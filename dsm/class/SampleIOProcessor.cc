/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <SampleIOProcessor.h>
#include <NidsIterators.h>
#include <atdUtil/Logger.h>

using namespace dsm;
using namespace std;

SampleIOProcessor::SampleIOProcessor(): id(0),optional(false),service(0)
{
}

/*
 * Copy constructor
 */

SampleIOProcessor::SampleIOProcessor(const SampleIOProcessor& x):
	name(x.name),id(x.id),optional(x.optional),service(x.service)
{
#ifdef DEBUG
    cerr << "SampleIOProcessor copy ctor" << endl;
#endif
    list<SampleOutput*>::const_iterator oi;
    for (oi = x.dconOutputs.begin(); oi != x.dconOutputs.end(); ++oi) {
        SampleOutput* output = *oi;
        addDisconnectedOutput(output->clone());
    }
}

SampleIOProcessor::~SampleIOProcessor()
{
#ifdef DEBUG
    cerr << "~SampleIOProcessor, this=" << this << ", dconOutputs.size=" << dconOutputs.size() << endl;
#endif

    list<SampleOutput*>::const_iterator oi;
    for (oi = dconOutputs.begin(); oi != dconOutputs.end(); ++oi) {
        SampleOutput* output = *oi;

#ifdef DEBUG
	cerr << "~SampleIOProcessor, deleting output=" << output->getName() << endl;
#endif

	delete output;
    }
    for (oi = conOutputs.begin(); oi != conOutputs.end(); ++oi) {
        SampleOutput* output = *oi;
	output->close();
#ifdef DEBUG
	cerr << "~SampleIOProcessor, deleting output=" << output->getName() << endl;
#endif
	delete output;
    }
    set<SampleTag*>::const_iterator ti;
    for (ti = sampleTags.begin(); ti != sampleTags.end(); ++ti)
	delete *ti;
}

void SampleIOProcessor::addSampleTag(SampleTag* tag)
	throw(atdUtil::InvalidParameterException)
{
    sampleTags.insert(tag);
    constSampleTags.insert(tag);
}


SampleTagIterator SampleIOProcessor::getSampleTagIterator() const
{
    return SampleTagIterator(this);
}

VariableIterator SampleIOProcessor::getVariableIterator() const
{
    return VariableIterator(this);
}

const std::string& SampleIOProcessor::getName() const { return name; }

void SampleIOProcessor::setName(const std::string& val) { name = val; }

void SampleIOProcessor::connect(SampleInput* input) throw(atdUtil::IOException)
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s",
	input->getName().c_str(), getName().c_str());

    list<SampleOutput*> tmpOutputs = dconOutputs;
    list<SampleOutput*>::const_iterator oi;
    for (oi = tmpOutputs.begin(); oi != tmpOutputs.end(); ++oi) {
	SampleOutput* output = *oi;

	SampleTagIterator sti = getSampleTagIterator();
	for (; sti.hasNext(); ) output->addSampleTag(sti.next());

	output->requestConnection(this);
    }
}
 
void SampleIOProcessor::disconnect(SampleInput* input) throw(atdUtil::IOException)
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s is disconnecting from %s",
	input->getName().c_str(),getName().c_str());

    list<SampleOutput*>::iterator oi;
    for (oi = conOutputs.begin(); oi != conOutputs.end(); ++oi) {
        SampleOutput* output = *oi;
        output->finish();
    }
}
 
void SampleIOProcessor::connected(SampleOutput* output) throw()
{
    addConnectedOutput(output);
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s, #outputs=%d",
	output->getName().c_str(),
	getName().c_str(),
	conOutputs.size());
    try {
	output->init();
    }
    catch( const atdUtil::IOException& ioe) {
	atdUtil::Logger::getInstance()->log(LOG_ERR,
	    "%s: error: %s",
	    output->getName().c_str(),ioe.what());
	disconnected(output);
	return;
    }
}
 
void SampleIOProcessor::disconnected(SampleOutput* output) throw()
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	output->getName().c_str(),
	getName().c_str());
    output->close();
    removeConnectedOutput(output);		// deletes it
}

void SampleIOProcessor::addConnectedOutput(SampleOutput* val)
{
    list<SampleOutput*>::iterator oi;
    for (oi = dconOutputs.begin(); oi != dconOutputs.end(); ) {
        SampleOutput* output = *oi;
	if (output == val) {
	    cerr << "SampleIOProcessor, connected a disconnected output=" <<
	    	val->getName() << endl;
	    oi = dconOutputs.erase(oi);
	}
	else ++oi;
    }
    conOutputs.push_back(val);
}

void SampleIOProcessor::removeConnectedOutput(SampleOutput* val)
{
    list<SampleOutput*>::iterator oi;
    bool found = false;
    for (oi = conOutputs.begin(); oi != conOutputs.end(); ) {
	SampleOutput* output = *oi;
	if (output == val) {
	    atdUtil::Logger::getInstance()->log(LOG_DEBUG,
		"SampleIOProcessor, disconnected %s\n",val->getName().c_str());
	    oi = conOutputs.erase(oi);
	    delete output;
	    found = true;
	}
	else ++oi;
    }
    if (!found) atdUtil::Logger::getInstance()->log(LOG_WARNING,
	"SampleIOProcessor, cannot find output %s\n",val->getName().c_str());
}

/*
 * process <processor> element
 */
void SampleIOProcessor::fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
        // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
           if (!attr.getName().compare("id")) {
               istringstream ist(attr.getValue());
               // If you unset the dec flag, then a leading '0' means
               // octal, and 0x means hex.
               ist.unsetf(ios::dec);
               unsigned long val;
               ist >> val;
               if (ist.fail())
                   throw atdUtil::InvalidParameterException("sensor",
                       attr.getName(),attr.getValue());
               setShortId(val);
           }
           else if (!attr.getName().compare("optional")) {
               istringstream ist(attr.getValue());
		bool val;
		ist >> boolalpha >> val;
		if (ist.fail()) {
		    ist.clear();
		    ist >> noboolalpha >> val;
		    if (ist.fail())
			throw atdUtil::InvalidParameterException(
				"SampleIOProcessor", attr.getName(),
					attr.getValue());
		}
		setOptional(val);
	    }
        }
    }

    // process <output> child elements
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();
	DOMable* domable;
        if (!elname.compare("output")) {
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0)
		throw atdUtil::InvalidParameterException(
		    "SampleIOProcessor::fromDOMElement",
		    elname, "class not specified");
            try {
                domable = DOMObjectFactory::createObject(classattr);
            }
            catch (const atdUtil::Exception& e) {
                throw atdUtil::InvalidParameterException("service",
                    classattr,e.what());
            }
	    SampleOutput* output = dynamic_cast<SampleOutput*>(domable);
            if (!output) {
		delete domable;
                throw atdUtil::InvalidParameterException("service",
                    classattr,"is not a SampleOutput");
	    }
            output->fromDOMElement((xercesc::DOMElement*)child);
	    addDisconnectedOutput(output);
	}
	else if (!elname.compare("sample")) {
	    SampleTag* stag = new SampleTag();
	    stag->fromDOMElement((xercesc::DOMElement*)child);
	    stag->setDSMId(getDSMId());
	    stag->setSensorId(getShortId());
	    addSampleTag(stag);
	}
        else throw atdUtil::InvalidParameterException(
                "SampleIOProcessor::fromDOMElement",
                elname, "unsupported element");
    }
    if (dconOutputs.size() == 0)
        throw atdUtil::InvalidParameterException(
                "SampleIOProcessor::fromDOMElement",
                "output", "no output specified");

}

xercesc::DOMElement* SampleIOProcessor::toDOMParent(
    xercesc::DOMElement* parent)
    throw(xercesc::DOMException)
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
                        DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

xercesc::DOMElement* SampleIOProcessor::toDOMElement(xercesc::DOMElement* node)
    throw(xercesc::DOMException)
{
    return node;
}

