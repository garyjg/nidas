/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-08-29 15:10:54 -0600 (Mon, 29 Aug 2005) $

    $LastChangedRevision: 2753 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.atd.ucar.edu/svn/hiaper/ads3/dsm/src/data_dump.cc $
 ********************************************************************

*/

#define _XOPEN_SOURCE	/* glibc2 needs this */
#include <time.h>

#include <FileSet.h>
#include <Socket.h>
#include <SampleInput.h>
#include <DSMEngine.h>
#include <SyncRecordGenerator.h>

#include <set>
#include <map>
#include <iostream>
#include <iomanip>

using namespace dsm;
using namespace std;


class SyncServer
{
public:

    SyncServer();

    int parseRunstring(int argc, char** argv) throw();

    int run() throw();

    void simLoop(SampleInputStream& input,SampleOutputStream* output,
	SyncRecordGenerator& syncGen) throw(atdUtil::IOException);

    void normLoop(SampleInputStream& input,SampleOutputStream* output,
	SyncRecordGenerator& syncGen) throw(atdUtil::IOException);


// static functions
    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

    static int main(int argc, char** argv) throw();

    static int usage(const char* argv0);

    static const int DEFAULT_PORT;

private:

    static bool interrupted;

    string xmlFileName;

    list<string> dataFileNames;

    int port;

    bool simulationMode;
};

int main(int argc, char** argv)
{
    return SyncServer::main(argc,argv);
}


/* static */
bool SyncServer::interrupted = false;

const int SyncServer::DEFAULT_PORT = 30001;

/* static */
void SyncServer::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            SyncServer::interrupted = true;
    break;
    }
}

/* static */
void SyncServer::setupSignals()
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset,SIGHUP);
    sigaddset(&sigset,SIGTERM);
    sigaddset(&sigset,SIGINT);
    sigprocmask(SIG_UNBLOCK,&sigset,(sigset_t*)0);
                                                                                
    struct sigaction act;
    sigemptyset(&sigset);
    act.sa_mask = sigset;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = SyncServer::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

/* static */
int SyncServer::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-x xml_file] [-p port] raw_data_file ...\n\
    -p port: sync record output socket port number: default=" << DEFAULT_PORT << "\n\
    -s: simulation mode (pause a second before sending each sync record)\n\
    -x xml_file (optional), default: \n\
	$ADS3_CONFIG/projects/<project>/<aircraft>/flights/<flight>/ads3.xml\n\
	where <project>, <aircraft> and <flight> are read from the input data header\n\
    raw_data_file: names of one or more raw data files, separated by spaces\n\
" << endl;
    return 1;
}

/* static */
int SyncServer::main(int argc, char** argv) throw()
{
    setupSignals();

    SyncServer sync;

    int res;
    
    if ((res = sync.parseRunstring(argc,argv)) != 0) return res;
    return sync.run();
}


SyncServer::SyncServer():
	port(DEFAULT_PORT),simulationMode(false)
{
}

int SyncServer::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "p:sx:")) != -1) {
	switch (opt_char) {
	case 'p':
	    {
		istringstream ist(optarg);
		ist >> port;
		if (ist.fail()) {
		    cerr << "Invalid port number: " << optarg << endl;
		    usage(argv[0]);
		}
	    }
	    break;
	case 's':
	    simulationMode = true;
	    break;
	case 'x':
	    xmlFileName = optarg;
	    break;
	case '?':
	    usage(argv[0]);
	}
    }
    for (; optind < argc; ) dataFileNames.push_back(argv[optind++]);
    if (dataFileNames.size() == 0) usage(argv[0]);
    return 0;
}

int SyncServer::run() throw()
{

    IOChannel* iochan = 0;

    try {
	dsm::FileSet* fset = new dsm::FileSet();
	iochan = fset;

#ifdef USE_FILESET_TIME_CAPABILITY
	struct tm tm;
	strptime("2005 04 05 00:00:00","%Y %m %d %H:%M:%S",&tm);
	time_t start = timegm(&tm);

	strptime("2005 04 06 00:00:00","%Y %m %d %H:%M:%S",&tm);
	time_t end = timegm(&tm);

	fset->setDir("/tmp/RICO/hiaper");
	fset->setFileName("radome_%Y%m%d_%H%M%S.dat");
	fset->setStartTime(start);
	fset->setEndTime(end);
#else
	list<string>::const_iterator fi;
	for (fi = dataFileNames.begin(); fi != dataFileNames.end(); ++fi)
	    fset->addFileName(*fi);
#endif

	// SortedSampleStream owns the iochan ptr.
	SortedSampleInputStream input(iochan);
	input.setSorterLengthMsecs(2000);

	// Block while waiting for heapSize to become less than heapMax.
	input.setHeapBlock(true);
	input.setHeapMax(10000000);
	input.init();

	input.readHeader();
	SampleInputHeader header = input.getHeader();

	if (xmlFileName.length() == 0) {
	    if (getenv("ISFF") != 0)
		xmlFileName = Project::getConfigName("$ISFF",
		    "projects", header.getProjectName(),
		    header.getSiteName(),"ops",
		    header.getObsPeriodName(),"ads3.xml");
	    else
		xmlFileName = Project::getConfigName("$ADS3_CONFIG",
		    "projects", header.getProjectName(),
		    header.getSiteName(),"flights",
		    header.getObsPeriodName(),"ads3.xml");
	}

	auto_ptr<Project> project;
	auto_ptr<xercesc::DOMDocument> doc(
		DSMEngine::parseXMLConfigFile(xmlFileName));

	project = auto_ptr<Project>(Project::getInstance());
	project->fromDOMElement(doc->getDocumentElement());

	Site* site = 0;
	const list<Site*>& sites = project->getSites();

	string aircraft = header.getSiteName();

	list<Site*>::const_iterator ai = sites.begin();
	if (sites.size() == 1) site = *ai;
	else {
	    for (ai = sites.begin(); ai != sites.end(); ++ai) {
		Site* tmpsite = *ai;
		if (!tmpsite->getName().compare(aircraft)) {
		    site = tmpsite;
		    break;
		}
	    }
	}
	if (!site) throw atdUtil::InvalidParameterException("site",aircraft,
		"not found");

	set<DSMSensor*> sensors;
	SampleTagIterator ti = site->getSampleTagIterator();
	for ( ; ti.hasNext(); ) {
	    const SampleTag* stag = ti.next();
	    input.addSampleTag(stag);

	    DSMSensor* sensor = site->findSensor(stag->getId());
	    if (!sensor) {
	        cerr << "sensor with dsm id=" << stag->getDSMId() <<
		", sensor id=" << stag->getSensorId() << " not found" << endl;
		continue;
	    }
	    set<DSMSensor*>::const_iterator si = sensors.find(sensor);
	    if (si == sensors.end()) {
	        sensors.insert(sensor);
		sensor->init();
	    }
	}

	SyncRecordGenerator syncGen;
	syncGen.connect(&input);

	dsm::ServerSocket* servSock = new dsm::ServerSocket(port);
	SampleOutputStream* output = new SampleOutputStream(servSock);

	output->connect();
	syncGen.connected(output);

	if (simulationMode) simLoop(input,output,syncGen);
	else normLoop(input,output,syncGen);
    }
    catch (atdUtil::EOFException& eof) {
        cerr << eof.what() << endl;
	return 1;
    }
    catch (atdUtil::IOException& ioe) {
        cerr << ioe.what() << endl;
	return 1;
    }
    catch (atdUtil::InvalidParameterException& ioe) {
        cerr << ioe.what() << endl;
	return 1;
    }
    catch (dsm::XMLException& e) {
        cerr << e.what() << endl;
	return 1;
    }
    catch (atdUtil::Exception& e) {
        cerr << e.what() << endl;
	return 1;
    }
    return 0;
}

void SyncServer::simLoop(SampleInputStream& input,SampleOutputStream* output,
	SyncRecordGenerator& syncGen) throw(atdUtil::IOException)
{

    try {
	Sample* samp = input.readSample();
	dsm_time_t tt = samp->getTimeTag();
	syncGen.sendHeader(tt,output);
	input.distribute(samp);
	samp->freeReference();

	int simClockRes = USECS_PER_SEC / 10;	// simulated clock resolution

	// simulated data clock. Round it up to next simClockRes.
	dsm_time_t simClock = tt + simClockRes - (tt % simClockRes);

	const int MAX_WAIT = 5;

	for (;;) {
	    if (!output->getIOStream()) break;	 // check for disconnect
	    if (interrupted) break;

	    samp = input.readSample();

	    tt = samp->getTimeTag();

	    while (tt > simClock) {	// getting ahead of simulated data clock

#ifdef DEBUG
	        cerr << "tt=" << tt / USECS_PER_SEC << '.' <<
			setfill('0') << setw(3) << (tt % USECS_PER_SEC) / 1000 <<
		    " simClock=" <<  simClock / USECS_PER_SEC << '.' <<
		    	setfill('0') << setw(3) <<  (simClock % USECS_PER_SEC) / 1000 <<
		    endl;
#endif
			
		// correct for drift
		long tsleep = simClockRes - (getSystemTime() % simClockRes);
		struct timespec nsleep;
		nsleep.tv_sec = tsleep / USECS_PER_SEC;
		nsleep.tv_nsec = (tsleep % USECS_PER_SEC) * 1000;
		if (nanosleep(&nsleep,0) < 0 && errno == EINTR) break;

		simClock += simClockRes;

		int tdiff = (int)((tt - simClock) / USECS_PER_SEC);
		// if a big jump in the data, wait a max of 5 seconds for the impatient.
		if (tdiff > MAX_WAIT) 
		    simClock += (tdiff - MAX_WAIT) * USECS_PER_SEC;
	    }
	    input.distribute(samp);
	    samp->freeReference();
	}
    }
    catch (atdUtil::EOFException& e) {
	input.close();
	syncGen.disconnect(&input);
	syncGen.disconnected(output);
	throw e;
    }
    catch (atdUtil::IOException& e) {
	input.close();
	syncGen.disconnect(&input);
	syncGen.disconnected(output);
	throw e;
    }
}

void SyncServer::normLoop(SampleInputStream& input,SampleOutputStream* output,
	SyncRecordGenerator& syncGen) throw(atdUtil::IOException)
{

    Sample* samp = input.readSample();
    dsm_time_t tt = samp->getTimeTag();
    syncGen.sendHeader(tt,output);
    input.distribute(samp);
    samp->freeReference();

    try {
	for (;;) {
	    if (interrupted) break;
	    input.readSamples();
	}
    }
    catch (atdUtil::EOFException& e) {
	cerr << "EOF received: flushing buffers" << endl;
	input.flush();
	syncGen.disconnect(&input);

	input.close();
	syncGen.disconnected(output);
	throw e;
    }
    catch (atdUtil::IOException& e) {
	input.close();
	syncGen.disconnect(&input);
	syncGen.disconnected(output);
	throw e;
    }
}
