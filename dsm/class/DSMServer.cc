/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <DSMServer.h>

#include <Aircraft.h>

#include <DSMTime.h>
#include <XMLParser.h>
#include <DOMObjectFactory.h>

#include <atdUtil/InvalidParameterException.h>
#include <atdUtil/Logger.h>
#include <atdUtil/McSocket.h>

#include <unistd.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

/* static */
std::string DSMServer::xmlFileName;

/* static */
bool DSMServer::quit = false;

/* static */
bool DSMServer::restart = false;

/* static */
int DSMServer::main(int argc, char** argv) throw()
{
                                                                                
    DSMServerRunstring rstr(argc,argv);
    atdUtil::Logger* logger = 0;

    char hostname[MAXHOSTNAMELEN];
    gethostname(hostname,sizeof(hostname));
                                                                                
    if (rstr.debug) logger = atdUtil::Logger::createInstance(stderr);
    else {
        logger = atdUtil::Logger::createInstance(
                hostname,LOG_CONS,LOG_LOCAL5);
    }

    setXMLFileName(rstr.configFile);

    setupSignals();

    int result = 0;

    while (!quit) {

        Project* project = 0;
	try {
	    project = parseXMLConfigFile();
	}
	catch (const SAXException& e) {
	    logger->log(LOG_ERR,
		    XMLStringConverter(e.getMessage()));
	    result = 1;
	    break;
	}
	catch (const DOMException& e) {
	    logger->log(LOG_ERR,
		    XMLStringConverter(e.getMessage()));
	    result = 1;
	    break;
	}
	catch (const XMLException& e) {
	    logger->log(LOG_ERR,
		    XMLStringConverter(e.getMessage()));
	    result = 1;
	    break;
	}
	catch(const atdUtil::InvalidParameterException& e) {
	    logger->log(LOG_ERR,e.what());
	    result = 1;
	    break;
	}
	catch (const atdUtil::Exception& e) {
	    logger->log(LOG_ERR,e.what());
	    result = 1;
	    break;
	}

	const list<Aircraft*>& aclist = project->getAircraft();

	DSMServer* serverp = 0;

	try {
	    for (list<Aircraft*>::const_iterator ai=aclist.begin();
		ai != aclist.end(); ++ai) {
		Aircraft* aircraft = *ai;
		serverp = aircraft->findServer(hostname);
		if (serverp) break;
	    }

	    if (!serverp)
	    	throw atdUtil::InvalidParameterException("aircraft","server",
			string("Can't find server entry for ") + hostname);
	}
	catch (const atdUtil::Exception& e) {
	    logger->log(LOG_ERR,e.what());
	    result = 1;
	    break;
	}

	try {
	    serverp->scheduleServices();
	}
	catch (const atdUtil::Exception& e) {
	    logger->log(LOG_ERR,e.what());
	}

	serverp->waitOnServices();

	delete project;
    }

    cerr << "XMLCachingParser::destroyInstance()" << endl;
    XMLCachingParser::destroyInstance();

    cerr << "XMLImplementation::terminate()" << endl;
    XMLImplementation::terminate();
    return result;
}
                                                                                
/* static */
void DSMServer::setupSignals()
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset,SIGHUP);
    sigaddset(&sigset,SIGTERM);
    sigaddset(&sigset,SIGINT);
    sigaddset(&sigset,SIGUSR1);
    sigprocmask(SIG_UNBLOCK,&sigset,(sigset_t*)0);
                                                                                
    struct sigaction act;
    sigemptyset(&sigset);
    act.sa_mask = sigset;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = DSMServer::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
    sigaction(SIGUSR1,&act,(struct sigaction *)0);
}
                                                                                
void DSMServer::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    atdUtil::Logger::getInstance()->log(LOG_INFO,
        "received signal %s(%d), si_signo=%d, si_errno=%d, si_code=%d",
        strsignal(sig),sig,
        (siginfo ? siginfo->si_signo : -1),
        (siginfo ? siginfo->si_errno : -1),
        (siginfo ? siginfo->si_code : -1));
                                                                                
    switch(sig) {
    case SIGHUP:
	DSMServer::restart = true;
	break;
    case SIGTERM:
    case SIGINT:
    case SIGUSR1:
	DSMServer::quit = true;
	break;
    }
}

/* static */
Project* DSMServer::parseXMLConfigFile()
        throw(atdUtil::Exception,
        DOMException,SAXException,XMLException,
	atdUtil::InvalidParameterException)
{
    XMLCachingParser* parser = XMLCachingParser::getInstance();
    // throws Exception, DOMException
                                                                                
    // If parsing a local file, turn on validation
    parser->setDOMValidation(true);
    parser->setDOMValidateIfSchema(true);
    parser->setDOMNamespaces(true);
    parser->setXercesSchema(true);
    parser->setXercesSchemaFullChecking(true);
    parser->setDOMDatatypeNormalization(false);
    parser->setXercesUserAdoptsDOMDocument(true);
                                                                                
    // This document belongs to the caching parser
    DOMDocument* doc = parser->parse(getXMLFileName());
                                                                                
    Project* project = Project::getInstance();
                                                                                
    project->fromDOMElement(doc->getDocumentElement());
    // throws atdUtil::InvalidParameterException;

    return project;
}
                                                                                
DSMServer::DSMServer()
{
}

/*
 * Copy constructor.
 */
DSMServer::DSMServer(const DSMServer& x):
	aircraft(x.aircraft),name(x.name)
{
    list<DSMService*>::const_iterator si;
    for (si=x.services.begin(); si != x.services.end(); ++si) {
	DSMService* svc = *si;
	DSMService* newsvc = svc->clone();
	newsvc->setDSMServer(this);
	services.push_back(newsvc);
    }
}
DSMServer::~DSMServer()
{
    // delete services. These are the configured services,
    // not the cloned copies.
    list<DSMService*>::const_iterator si;
    // cerr << "~DSMServer services.size=" << services.size() << endl;
    for (si=services.begin(); si != services.end(); ++si) {
	DSMService* svc = *si;
	// cerr << "~DSMServer: deleting " << svc->getName() << endl;
	delete svc;
    }
}
void DSMServer::fromDOMElement(const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);

    if(node->hasAttributes()) {
    // get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    const std::string& aname = attr.getName();
	    const std::string& aval = attr.getValue();
	    if (!aname.compare("name")) setName(aval);
	}
    }
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();
	if (!elname.compare("service")) {
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0) 
		throw atdUtil::InvalidParameterException(
		    "DSMServer::fromDOMElement",
			elname,
			"does not have a class attribute");
	    DOMable* domable;
	    try {
		domable = DOMObjectFactory::createObject(classattr);
	    }
	    catch (const atdUtil::Exception& e) {
		throw atdUtil::InvalidParameterException("service",
		    classattr,e.what());
	    }
	    DSMService* service = dynamic_cast<DSMService*>(domable);
	    if (!service) {
		delete domable;
		throw atdUtil::InvalidParameterException("service",
		    classattr,"is not of type DSMService");
	    }
	    service->setDSMServer(this);
	    service->fromDOMElement((DOMElement*)child);
	    addService(service);
	}
    }
}

DOMElement* DSMServer::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

DOMElement* DSMServer::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

void DSMServer::scheduleServices() throw(atdUtil::Exception)
{

    list<DSMService*>::const_iterator si;
    for (si=services.begin(); si != services.end(); ++si) {
	DSMService* svc = *si;
	svc->schedule();
    }
}

void DSMServer::interruptServices() throw()
{
    list<DSMService*>::const_iterator si;
    for (si=services.begin(); si != services.end(); ++si) {
	DSMService* svc = *si;
	svc->interruptSubServices();
    }
}

void DSMServer::cancelServices() throw()
{
    list<DSMService*>::const_iterator si;
    for (si=services.begin(); si != services.end(); ++si) {
	DSMService* svc = *si;
	svc->cancelSubServices();
    }
}

void DSMServer::joinServices() throw()
{

    list<DSMService*>::const_iterator si;
    for (si=services.begin(); si != services.end(); ++si) {
	DSMService* svc = *si;
	svc->joinSubServices();
    }

}

void DSMServer::waitOnServices() throw()
{

    // We wake up every so often and check our threads.
    // An INTR,HUP or TERM signal to this process will
    // cause nanosleep to return. sigAction sets quit or
    // restart, and then we break, and cancel our threads.
    struct timespec sleepTime;
    sleepTime.tv_sec = 10;
    sleepTime.tv_nsec = 0;

    for (;;) {

	int nthreads = atdUtil::McSocketListener::check();

	list<DSMService*>::const_iterator si;
	for (si=services.begin(); si != services.end(); ++si) {
	    DSMService* svc = *si;
	    nthreads += svc->checkSubServices();
	}
	
	cerr << "DSMServer::wait nthreads=" << nthreads << endl;

	if (quit || restart) break;

	// cerr << "DSMServer::wait nanosleep" << endl;
        nanosleep(&sleepTime,0);

	if (quit || restart) break;
                                                                                
    }
    cerr << "Break out of wait loop, interrupting services" << endl;
    interruptServices();	

    // wait a bit, then cancel whatever is still running
    sleepTime.tv_sec = 1;
    sleepTime.tv_nsec = 0;
    nanosleep(&sleepTime,0);

    cancelServices();	
    joinServices();
}

DSMServerRunstring::DSMServerRunstring(int argc, char** argv) {
    debug = false;
    // extern char *optarg;	/* set by getopt() */
    extern int optind;		/* "  "     "     */
    int opt_char;		/* option character */
                                                                                
    while ((opt_char = getopt(argc, argv, "d")) != -1) {
        switch (opt_char) {
        case 'd':
            debug = true;
            break;
        case '?':
            usage(argv[0]);
        }
    }
    if (optind != argc - 1) usage(argv[0]);
    configFile = string(argv[optind++]);
}

/* static */
void DSMServerRunstring::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "[-d] config\n\
  -d: debug (optional). Send error messages to stderr, otherwise to syslog\n\
  config: name of DSM configuration file (required).\n\
" << endl;
    exit(1);
}

