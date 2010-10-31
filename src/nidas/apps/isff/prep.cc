/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <ctime>

#include <nidas/dynld/RawSampleInputStream.h>

#ifdef HAS_NC_SERVER_RPC_H
#include <nidas/dynld/isff/NetcdfRPCOutput.h>
#include <nidas/dynld/isff/NetcdfRPCChannel.h>
#endif

#include <nidas/core/FileSet.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/NearestResampler.h>
#include <nidas/core/NearestResamplerAtRate.h>
#include <nidas/core/SamplePipeline.h>
#include <nidas/core/XMLParser.h>

#include <nidas/core/ProjectConfigs.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/Variable.h>
#include <nidas/core/Version.h>
#include <nidas/core/Socket.h>

#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Process.h>

#include <set>
#include <map>
#include <iostream>
#include <iomanip>
#include <memory> // auto_ptr<>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

class DumpClient: public SampleClient 
{
public:

    typedef enum format { ASCII, BINARY1, BINARY2 } format_t;

    DumpClient(format_t,ostream&, int asciiPrecision);

    virtual ~DumpClient() {}

    bool receive(const Sample* samp) throw();

    void printHeader(vector<const Variable*> vars);

    void setStartTime(const n_u::UTime& val)
    {
        _startTime = val;
        _checkStart = true;
    }

    void setEndTime(const n_u::UTime& val)
    {
        _endTime = val;
        _checkEnd = true;
    }

    void setDOS(bool val)
    {
        _dosOut = val;
    }

    bool getDOS() const
    {
        return _dosOut;
    }

private:

    format_t _format;

    ostream& _ostr;

    n_u::UTime _startTime;

    n_u::UTime _endTime;

    bool _checkStart;

    bool _checkEnd;

    bool _dosOut;

    int _asciiPrecision;

};


class DataPrep
{
public:

    DataPrep();

    ~DataPrep();

    int parseRunstring(int argc, char** argv);

    int run() throw();

    static int main(int argc, char** argv);

    static int usage(const char* argv0);

    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

    vector<const Variable*> matchVariables(const Project&, set<const DSMConfig*>& activeDsms,
        set<DSMSensor*>& activeSensors) throw (n_u::InvalidParameterException);

    static void interrupt() { _interrupted = true; }

    static void finishUp() { _finished = true; }

    // default initialization values, which are displayed in usage() method.
    static const int defaultLogLevel = n_u::LOGGER_INFO;
    static const int defaultNCInterval = 1;
    static const int defaultNCLength = 86400;
    static const float defaultNCFillValue = 1.e37;
    static const int defaultNCTimeout = 60;
    static const int defaultNCBatchPeriod = 300;

private:

    string _progname;

    static bool _interrupted;

    static bool _finished;

    string _xmlFileName;

    list<string> _dataFileNames;

    auto_ptr<n_u::SocketAddress> _sockAddr;

    static const int DEFAULT_PORT = 30000;

    float _sorterLength;

    DumpClient::format_t _format;

    list<Variable*> _reqVars;

    n_u::UTime _startTime;

    n_u::UTime _endTime;

    std::string _configName;

    double _rate;

    bool _middleTimeTags;

    bool _dosOut;

    bool _doHeader;

    static const char* _rafXML;

    static const char* _isffXML;

    int _asciiPrecision;

    int _logLevel;

    string _ncserver;

    string _ncdir;

    string _ncfile;

    int _ncinterval;

    int _nclength;

    string _nccdl;

    float _ncfill;

    int _nctimeout;

    int _ncbatchperiod;

};


DataPrep::~DataPrep()
{
    list<Variable*>::const_iterator rvi = _reqVars.begin();
    for ( ; rvi != _reqVars.end(); ++rvi) {
        Variable* v = *rvi;
        delete v;
    }
}


/* static */
const char* DataPrep::_rafXML = "$PROJ_DIR/projects/$PROJECT/$AIRCRAFT/nidas/flights.xml";

/* static */
const char* DataPrep::_isffXML = "$ISFF/projects/$PROJECT/ISFF/config/configs.xml";

DumpClient::DumpClient(format_t fmt,ostream &outstr,int precision):
	_format(fmt),_ostr(outstr),_startTime((time_t)0),_endTime((time_t)0),
        _checkStart(false),_checkEnd(false),_dosOut(false),
        _asciiPrecision(precision)
{
}

void DumpClient::printHeader(vector<const Variable*>vars)
{
    // cout << "|--- date time -------| deltaT   bytes" << endl;
    vector<const Variable*>::const_iterator vi = vars.begin();
    for (; vi != vars.end(); ++vi) {
        const Variable* var = *vi;
        cout << var->getName() << ' ';
    }
    if (_dosOut) cout << '\r';
    cout << endl;
    vi = vars.begin();
    for (; vi != vars.end(); ++vi) {
        const Variable* var = *vi;
        if (var->getConverter())
            cout << '"' << var->getConverter()->getUnits() << "\" ";
        else
            cout << '"' << var->getUnits() << "\" ";
    }
    if (_dosOut) cout << '\r';
    cout << endl;
}

bool DumpClient::receive(const Sample* samp) throw()
{
    dsm_time_t tt = samp->getTimeTag();
    if (_checkStart && tt < _startTime.toUsecs()) return false;
    if (_checkEnd && tt > _endTime.toUsecs()) {
        DataPrep::finishUp();
        return false;
    }

#ifdef DEBUG
    cerr << "sampid=" << GET_DSM_ID(samp->getId()) << ',' <<
    	GET_SHORT_ID(samp->getId()) << endl;
#endif

    switch(_format) {
    case ASCII:
	{
	n_u::UTime ut(tt);
	_ostr << ut.format(true,"%Y %m %d %H:%M:%S.%4f");

	const float* fp =
		(const float*) samp->getConstVoidDataPtr();
	_ostr << setprecision(_asciiPrecision) << setfill(' ');
        // last value is number of non-NAs
	for (unsigned int i = 0;
		i < samp->getDataByteLength()/sizeof(float) - 1; i++)
	    _ostr << ' ' << setw(10) << fp[i];
        if (_dosOut) cout << '\r';
	_ostr << endl;
	}
        break;
    case BINARY1:
	{

	int fsecs = tt % USECS_PER_SEC;
	double ut = (double)((tt - fsecs) / USECS_PER_SEC) +
            (double) fsecs / USECS_PER_SEC;

	_ostr.write((const char*)&ut,sizeof(ut));
	const float* fp =
		(const float*) samp->getConstVoidDataPtr();
	for (unsigned int i = 0;
		i < samp->getDataByteLength()/sizeof(float) - 1; i++)
	    _ostr.write((const char*)(fp+i),sizeof(float));
	}
        break;
    case BINARY2:
	{

	_ostr.write((const char*)&tt,sizeof(tt));
	const float* fp =
		(const float*) samp->getConstVoidDataPtr();
	for (unsigned int i = 0;
		i < samp->getDataByteLength()/sizeof(float); i++)
	    _ostr.write((const char*)(fp+i),sizeof(float));
	}
        break;
    }
    return true;
}

DataPrep::DataPrep(): 
        _sorterLength(1.00),
	_format(DumpClient::ASCII),
        _startTime((time_t)0),_endTime((time_t)0),
        _rate(0.0),_middleTimeTags(true),_dosOut(false),_doHeader(true),
        _asciiPrecision(5),_logLevel(defaultLogLevel),
        _ncinterval(defaultNCInterval),_nclength(defaultNCLength),
        _ncfill(defaultNCFillValue),_nctimeout(defaultNCTimeout),
        _ncbatchperiod(defaultNCBatchPeriod)
{
}

int DataPrep::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */
    char* p1,*p2;

    _progname = argv[0];

    while ((opt_char = getopt(argc, argv, "AB:CD:dE:hHl:n:p:R:r:s:vx:")) != -1) {
	switch (opt_char) {
	case 'A':
	    _format = DumpClient::ASCII;
	    break;
	case 'B':
	    try {
		_startTime = n_u::UTime::parse(true,optarg);
	    }
	    catch(const n_u::ParseException& e) {
	        cerr << e.what() << endl;
		return usage(argv[0]);
	    }
	    break;
	case 'C':
	    _format = DumpClient::BINARY1;
	    break;
	case 'D':
	    {
		p1 = optarg;
		while ( (p2=strchr(p1,','))) {
		    Variable *var = new Variable();
                    char* ph = strchr(p1,'#');
                    if (ph) {
                        var->setName(string(p1,ph-p1));
                        ph++;
                        istringstream strm(string(ph,p2-ph));
                        int istn;
                        strm >> istn;
                        if (strm.fail()) {
                            cerr << "cannot parse station number for variable " <<
                                string(p1,p2-p1) << endl;
                            return usage(argv[0]);
                        }
                        var->setStation(istn);
                    } else var->setName(string(p1,p2-p1));
		    _reqVars.push_back(var);
		    p1 = p2 + 1;
		}

		Variable *var = new Variable();
                char* ph = strchr(p1,'#');
                if (ph) {
                    var->setName(string(p1,ph-p1));
                    ph++;
                    istringstream strm(ph);
                    int istn;
                    strm >> istn;
                    if (strm.fail()) {
                        cerr << "cannot parse station number for variable " <<
                            string(p1) << endl;
                        return usage(argv[0]);
                    }
                    var->setStation(istn);
                } else var->setName((p1));
		_reqVars.push_back(var);
	    }
	    break;
	case 'd':
	    _dosOut = true;
	    break;
	case 'E':
	    try {
		_endTime = n_u::UTime::parse(true,optarg);
	    }
	    catch(const n_u::ParseException& e) {
	        cerr << e.what() << endl;
		return usage(argv[0]);
	    }
	    break;
        case 'h':
            return usage(argv[0]);
            break;
        case 'H':
            _doHeader = false;
            break;
        case 'l':
            _logLevel = atoi(optarg);
            break;
        case 'n':
#ifdef HAS_NC_SERVER_RPC_H
	    {
		string ncarg(optarg);
                string::size_type i1=0,i2;

                i2 = ncarg.find(':',i1);
                if (i2 > i1) _ncserver = ncarg.substr(i1,i2-i1);
                if (i2 == string::npos) break;

                i1 = i2 + 1;
                i2 = ncarg.find(':',i1);
                if (i2 > i1) _ncdir = ncarg.substr(i1,i2-i1);
                if (i2 == string::npos) break;

                i1 = i2 + 1;
                i2 = ncarg.find(':',i1);
                if (i2 > i1) _ncfile = ncarg.substr(i1,i2-i1);
                if (i2 == string::npos) break;

                i1 = i2 + 1;
                i2 = ncarg.find(':',i1);
                if (i2 > i1) _ncinterval = atoi(ncarg.substr(i1,i2-i1).c_str());
                if (i2 == string::npos) break;

                i1 = i2 + 1;
                i2 = ncarg.find(':',i1);
                if (i2 > i1) _nclength = atoi(ncarg.substr(i1,i2-i1).c_str());
                if (i2 == string::npos) break;

                i1 = i2 + 1;
                i2 = ncarg.find(':',i1);
                if (i2 > i1) _nccdl = ncarg.substr(i1,i2-i1);
                if (i2 == string::npos) break;

                i1 = i2 + 1;
                i2 = ncarg.find(':',i1);
                if (i2 > i1) _ncfill = atof(ncarg.substr(i1,i2-i1).c_str());
                if (i2 == string::npos) break;

                i1 = i2 + 1;
                i2 = ncarg.find(':',i1);
                if (i2 > i1) _nctimeout = atoi(ncarg.substr(i1,i2-i1).c_str());
                if (i2 == string::npos) break;

                i1 = i2 + 1;
                i2 = ncarg.find(':',i1);
                if (i2 > i1) _ncbatchperiod = atoi(ncarg.substr(i1,i2-i1).c_str());
                if (i2 == string::npos) break;
            }
#else
            cerr << "-n option is not supported on this version of " << argv[0] << ", which was built without nc_server-devel package" << endl;
            return usage(argv[0]);
#endif
            break;
        case 'p':
            {
                istringstream ist(optarg);
                ist >> _asciiPrecision;
                if (ist.fail() || _asciiPrecision < 1) {
                    cerr << "Invalid precision: " << optarg << endl;
                    return usage(argv[0]);
                }
            }
            break;
        case 'r':
        case 'R':
            {
                istringstream ist(optarg);
                ist >> _rate;
                if (ist.fail() || _rate < 0) {
                    cerr << "Invalid resample rate: " << optarg << endl;
                    return usage(argv[0]);
                }
            }
            _middleTimeTags = opt_char == 'r';
            break;
        case 's':
            {
                istringstream ist(optarg);
                ist >> _sorterLength;
                if (ist.fail() || _sorterLength < 0 || _sorterLength > 10000) {
                    cerr << "Invalid sorter length: " << optarg << endl;
                    return usage(argv[0]);
                }
            }
            break;
	case 'v':
	    cout << "Version: " << Version::getSoftwareVersion() << endl;
	    exit(0);
	    break;
	case 'x':
	    _xmlFileName = optarg;
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }

    if (_reqVars.size() == 0) {
        cerr << "no variables requested, must have one or more -D options" <<
            endl;
        return usage(argv[0]);
    }

    for ( ; optind < argc; optind++) {
	string url(argv[optind]);
	if (url.length() > 5 && url.substr(0,5) == "sock:") {
	    url = url.substr(5);
	    string hostName = "127.0.0.1";
            int port = DEFAULT_PORT;
	    if (url.length() > 0) {
		size_t ic = url.find(':');
		hostName = url.substr(0,ic);
		if (ic < string::npos) {
		    istringstream ist(url.substr(ic+1));
		    ist >> port;
		    if (ist.fail()) {
			cerr << "Invalid port number: " << url.substr(ic+1) << endl;
			return usage(argv[0]);
		    }
		}
	    }
            try {
                n_u::Inet4Address addr = n_u::Inet4Address::getByName(hostName);
                _sockAddr.reset(new n_u::Inet4SocketAddress(addr,port));
            }
            catch(const n_u::UnknownHostException& e) {
                cerr << e.what() << endl;
                return usage(argv[0]);
            }
	}
	else if (url.length() > 5 && url.substr(0,5) == "unix:") {
	    url = url.substr(5);
            _sockAddr.reset(new n_u::UnixSocketAddress(url));
	}
	else _dataFileNames.push_back(url);
    }
    // must specify either:
    //	1. some data files to read, and optional begin and end times,
    //  2. a socket to connect to
    //	3. or a time period and a $PROJECT environment variable
    if (_dataFileNames.size() == 0 && !_sockAddr.get() &&
    	_startTime.toUsecs() == 0) return usage(argv[0]);
    if (_startTime.toUsecs() != 0 && _endTime.toUsecs() == 0)
        _endTime = _startTime + 90 * USECS_PER_DAY;
    return 0;
}

int DataPrep::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-A] [-C] -D var[,var,...] [-B time] [-E time]\n\
        [-h] [-r rate] [-s sorterLength] [-x xml_file] [input ...]\n\
    -A :ascii output (default)\n\
    -C :binary column output, double seconds since Jan 1, 1970, followed by floats for each var\n\
    -d : dos output (records terminated by CRNL instead of just NL)\n\
    -D var[,var,...]: One or more variable names to display\n\
    -B \"yyyy mm dd HH:MM:SS\": begin time (optional)\n\
    -E \"yyyy mm dd HH:MM:SS\": end time (optional)\n\
    -h : this help\n\
    -H : don't print out initial two line ASCII header of variable names and units\n\
    -l log_level: 7=debug,6=info,5=notice,4=warn,3=err, default=" << defaultLogLevel <<
#ifdef HAS_NC_SERVER_RPC_H
        "\n\
    -n server:dir:file:interval:length:cdlfile:missing:timeout:batchperiod\n\
        server: host name of system running nc_server RPC process\n\
        dir: directory on server to write files\n\
        file: format of NetCDF file names. For example: xxx_%Y%m%d.nc\n\
        interval: deltaT in seconds between time values in file. Default: " << defaultNCInterval << "\n\
        length: length of file, in seconds. 0 for no limit to the file size. Default: " << defaultNCInterval << "\n\
        cdlfile: name of NetCDF CDL file on server that is used for initialization of new files\n\
        missing: missing data value in file. Default: " << setprecision(2) << defaultNCFillValue << "\n\
        timeout: time in seconds that nc_server is expected to respond. Default: " << defaultNCTimeout << "\n\
        batchperiod: check for response back from server after this number of seconds.\n\
            Default: " << defaultNCInterval <<
#endif
        "\n\
    -p precision: number of digits in ASCII output values, default is 5\n\
    -r rate: optional resample rate, in Hz. Output timetags will be in middle of periods.\n\
    -R rate: optional resample rate, in Hz. Output timetags will be at integral deltaTs.\n\
    -s sorterLength: input data sorter length in seconds (optional)\n\
    -v : show version\n\
    -x xml_file: if not specified, the xml file name is determined by either reading\n\
       the data file header or from $ISFF/projects/$PROJECT/ISFF/config/configs.xml\n\
    input: data input (optional). One of the following:\n\
        sock:host[:port]          Default port is " << DEFAULT_PORT << "\n\
        unix:sockpath             unix socket name\n\
        file[,file,...]           one or more archive file names\n\
\n\
\n\
If no inputs are specified, then the -B time option must be given, and\n" <<
argv0 << " will read $ISFF/projects/$PROJECT/ISFF/config/configs.xml, to\n\
find an xml configuration for the begin time, read it to find a\n\
<fileset> archive for the given variables, and then open data files\n\
matching the <fileset> path descriptor and time period.\n\
\n" <<
argv0 << " does simple resampling, using the nearest sample to the times of the first\n\
variable requested, or if the -r or -R rate options are specified, to evenly spaced times\n\
at the given rate.\n\
\n\
Examples:\n" <<
	argv0 << " -D u.10m,v.10m,w.10m -B \"2006 jun 10 00:00\" -E \"2006 jul 3 00:00\"\n" <<
	argv0 << " -D u.10m,v.10m,w.10m sock:dsmhost\n" <<
	argv0 << " -D u.10m,v.10m,w.10m -r 60 unix:/tmp/data_socket\n" <<
        "\n\
Notes on choosing rates with -r or -R:\n\
    For rates less than 1 Hz it is best to choose a value such that 10^6/rate is an integer.\n\
    If you really want rate=1/3 Hz, specify rate to 7 significant figures,\n\
    0.3333333, and you will avoid round off errors in the time tag. \n\
    Output rates > 1 should be integers, or of a value with enough significant\n\
    figures such that 10^6/rate is an integer." << endl;
    return 1;
}

/* static */
bool DataPrep::_interrupted = false;

bool DataPrep::_finished = false;

/* static */
void DataPrep::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            DataPrep::_interrupted = true;
    break;
    }
}

/* static */
void DataPrep::setupSignals()
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
    act.sa_sigaction = DataPrep::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

/* static */
int DataPrep::main(int argc, char** argv)
{
    setupSignals();

    DataPrep dump;

    int res;

    if ((res = dump.parseRunstring(argc,argv))) return res;

    n_u::LogConfig lc;
    lc.level = dump._logLevel;
    n_u::Logger::getInstance()->setScheme(
        n_u::LogScheme("prep").addConfig (lc));

    return dump.run();
}

vector<const Variable*> DataPrep::matchVariables(const Project& project,set<const DSMConfig*>& activeDsms,
    set<DSMSensor*>& activeSensors) throw (n_u::InvalidParameterException)
{
    vector<const Variable*> variables;
    list<Variable*>::const_iterator rvi = _reqVars.begin();
    for ( ; rvi != _reqVars.end(); ++rvi) {
        Variable* reqvar = *rvi;
        bool match = false;

        DSMConfigIterator di = project.getDSMConfigIterator();
        for ( ; !match && di.hasNext(); ) {
            const DSMConfig* dsm = di.next();

            const list<DSMSensor*>& sensors = dsm->getSensors();
            list<DSMSensor*>::const_iterator si = sensors.begin();
            for (; !match && si != sensors.end(); ++si) {
                DSMSensor* sensor = *si;
                VariableIterator vi = sensor->getVariableIterator();
                for ( ; !match && vi.hasNext(); ) {
                    const Variable* var = vi.next();
// #define DEBUG
#ifdef DEBUG
                    cerr << "var=" << var->getName() <<
                        "(" << var->getStation() << "), " <<
                        var->getNameWithoutSite() <<
                        ", reqvar=" << reqvar->getName() <<
                        "(" << reqvar->getStation() << "), " <<
                        reqvar->getNameWithoutSite() <<
                        ", match=" << (*var == *reqvar) << endl;
#endif
                    if (*var == *reqvar) {
                        variables.push_back(var);
                        activeSensors.insert(sensor);
                        activeDsms.insert(dsm);
                        match = true;
                        break;
                    }
                }
            }
        }
        if (!match) throw
            n_u::InvalidParameterException(
                _progname,"cannot find variable",reqvar->getName());
    }
    return variables;
}

#ifdef PROJECT_IS_SINGLETON
class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};
#endif

int DataPrep::run() throw()
{
    try {

        Project project;

        IOChannel* iochan = 0;

        if (_xmlFileName.length() > 0) {

            _xmlFileName = n_u::Process::expandEnvVars(_xmlFileName);
            XMLParser parser;
            auto_ptr<xercesc::DOMDocument> doc(parser.parse(_xmlFileName));
            project.fromDOMElement(doc->getDocumentElement());
        }

        if (_sockAddr.get()) {
            if (_xmlFileName.length() == 0) {
                const char* re = getenv("PROJ_DIR");
                const char* pe = getenv("PROJECT");
                const char* ae = getenv("AIRCRAFT");
                const char* ie = getenv("ISFF");
                string configsXMLName;
                if (re && pe && ae) configsXMLName = n_u::Process::expandEnvVars(_rafXML);
                else if (ie && pe) configsXMLName = n_u::Process::expandEnvVars(_isffXML);
                if (configsXMLName.length() == 0)
                    throw n_u::InvalidParameterException("environment variables",
                        "PROJ_DIR,AIRCRAFT,PROJECT or ISFF,PROJECT","not found");
                ProjectConfigs configs;
                configs.parseXML(configsXMLName);
                // cerr << "parsed:" <<  configsXMLName << endl;
                // throws InvalidParameterException if no config for time
                const ProjectConfig* cfg = configs.getConfig(n_u::UTime());
                cfg->initProject(project);
                // cerr << "cfg=" <<  cfg->getName() << endl;
                _xmlFileName = cfg->getXMLName();
            }
            n_u::Socket* sock = 0;
            for (int i = 0; !sock && !_interrupted; i++) {
                try {
                    sock = new n_u::Socket(*_sockAddr.get());
                }
                catch(const n_u::IOException& e) {
                    if (i > 2)
                        n_u::Logger::getInstance()->log(LOG_WARNING,
                        "%s: retrying",e.what());
                    sleep(10);
                }
            }
            IOChannel* iosock = new nidas::core::Socket(sock);
            iochan = iosock->connect();
            if (iochan != iosock) {
                iosock->close();
                delete iosock;
            }
        }
        else {
            nidas::core::FileSet* fset;
            if (_dataFileNames.size() == 0) {
                // User has not specified the xml file. Get
                // the ProjectConfig from the configName or startTime
                // using the configs XML file, then parse the
                // XML of the ProjectConfig.
                if (_xmlFileName.length() == 0) {
                    string configsXML = n_u::Process::expandEnvVars(
                        "$ISFF/projects/$PROJECT/ISFF/config/configs.xml");

                    ProjectConfigs configs;
                    configs.parseXML(configsXML);
                    const ProjectConfig* cfg = 0;

                    if (_configName.length() > 0)
                        cfg = configs.getConfig(_configName);
                    else
                        cfg = configs.getConfig(_startTime);
                    cfg->initProject(project);
                    if (_startTime.toUsecs() == 0) _startTime = cfg->getBeginTime();
                    if (_endTime.toUsecs() == 0) _endTime = cfg->getEndTime();
                    _xmlFileName = cfg->getXMLName();
                }
                list<nidas::core::FileSet*> fsets = project.findSampleOutputStreamFileSets();
                if (fsets.size() == 0) {
                    n_u::Logger::getInstance()->log(LOG_ERR,"Cannot find a FileSet");
                    return 1;
                }
                // must clone, since fsets.front() belongs to project
                fset = fsets.front()->clone();

                if (_startTime.toUsecs() != 0) fset->setStartTime(_startTime);
                if (_endTime.toUsecs() != 0) fset->setEndTime(_endTime);
            }
            else {
                fset = nidas::core::FileSet::getFileSet(_dataFileNames);
            }
            iochan = fset;
        }

	RawSampleInputStream sis(iochan);
        SamplePipeline pipeline;
        pipeline.setRealTime(false);
        pipeline.setRawSorterLength(1.0);
        pipeline.setProcSorterLength(_sorterLength);
        pipeline.setRawHeapMax(1 * 1000 * 1000);
        pipeline.setProcHeapMax(1 * 1000 * 1000);

        if (_xmlFileName.length() == 0) {
            sis.readInputHeader();
            const SampleInputHeader& header = sis.getInputHeader();
	    _xmlFileName = header.getConfigName();
            _xmlFileName = n_u::Process::expandEnvVars(_xmlFileName);
            XMLParser parser;
	    auto_ptr<xercesc::DOMDocument> doc(parser.parse(_xmlFileName));

	    project.fromDOMElement(doc->getDocumentElement());
        }

        // match the variables.
        // on a match:
        //	1. save the var to pass to the resampler
        //  2. save the sensor in a set
        //  3. save the dsm in a set
        // later:
        //	1. init the sensors
        //	2. add the resampler as a processedSampleClient of the sensor

	vector<const Variable*> variables;
	set<DSMSensor*> activeSensors;
        set<const DSMConfig*> activeDsms;
        variables = matchVariables(project,activeDsms,activeSensors);

#ifdef DEBUG
        for (unsigned int i = 0; i < variables.size(); i++)
            cerr << "var=" << variables[i]->getName() <<
                        "(" << variables[i]->getStation() << ")" << endl;
#endif

        auto_ptr<Resampler> resampler;

        if (_rate > 0.0) {
            NearestResamplerAtRate* smplr =
                new NearestResamplerAtRate(variables);
            smplr->setRate(_rate);
            smplr->setFillGaps(true);
            smplr->setMiddleTimeTags(_middleTimeTags);
            resampler.reset(smplr);
        }
        else {
            resampler.reset(new NearestResampler(variables));
        }

	set<DSMSensor*>::const_iterator si = activeSensors.begin();
	for ( ; si != activeSensors.end(); ++si) {
	    DSMSensor* sensor = *si;
	    sensor->init();
            sis.addSampleTag(sensor->getRawSampleTag());
            SampleTagIterator sti = sensor->getSampleTagIterator();
            for ( ; sti.hasNext(); ) {
                const SampleTag* stag = sti.next();
                // sis.addSampleTag(stag);
                pipeline.getProcessedSampleSource()->addSampleTag(stag);
            }
	}

        pipeline.connect(&sis);
        resampler->connect(pipeline.getProcessedSampleSource());

        if (_ncserver.length() == 0) {
            DumpClient dumper(_format,cout,_asciiPrecision);
            dumper.setDOS(_dosOut);

            resampler->addSampleClient(&dumper);

            try {
                if (_startTime.toUsecs() != 0) {
                    DLOG(("searching for time ") <<
                        _startTime.format(true,"%Y %m %d %H:%M:%S"));
                    sis.search(_startTime);
                    DLOG(("search done."));
                    dumper.setStartTime(_startTime);
                }
                if (_endTime.toUsecs() != 0) dumper.setEndTime(_endTime);

                if (_doHeader) dumper.printHeader(variables);

                for (;;) {
                    sis.readSamples();
                    if (_finished || _interrupted) break;
                }
            }
            catch (n_u::EOFException& e) {
                cerr << "EOF received" << endl;
            }
            catch (n_u::IOException& e) {
                resampler->removeSampleClient(&dumper);
                resampler->disconnect(pipeline.getProcessedSampleSource());
                pipeline.disconnect(&sis);
                sis.close();
                throw e;
            }
            cerr << "flushing buffers" << endl;
            sis.flush();
            resampler->removeSampleClient(&dumper);
        }
#ifdef HAS_NC_SERVER_RPC_H
        else {
            nidas::dynld::isff::NetcdfRPCChannel* ncchan = new nidas::dynld::isff::NetcdfRPCChannel();
            ncchan->setServer(_ncserver);
            ncchan->setDirectory(_ncdir);
            ncchan->setFileNameFormat(_ncfile);
            ncchan->setTimeInterval(_ncinterval);
            ncchan->setFileLength(_nclength);
            ncchan->setCDLFileName(_nccdl);
            ncchan->setFillValue(_ncfill);
            ncchan->setRPCTimeout(_nctimeout);
            ncchan->setRPCBatchPeriod(_ncbatchperiod);

            SampleTag tag;
            tag.setRate(_rate);
            for (unsigned int i = 0; i < variables.size(); i++) {
                Variable* var = new Variable(*variables[i]);
                tag.addVariable(var);
            }
            tag.setDSMId(0);
            tag.setSampleId(32768);
            ncchan->addSampleTag(&tag);

            try {
                ncchan->connect();
            }
            catch (n_u::IOException& e) {
                cerr << "disconnect" << endl;
                resampler->disconnect(pipeline.getProcessedSampleSource());
                cerr << "disconnect" << endl;
                pipeline.disconnect(&sis);
                cerr << "close" << endl;
                sis.close();
                cerr << "delete ncchan" << endl;
                delete ncchan;
                throw e;
            }

            nidas::dynld::isff::NetcdfRPCOutput output(ncchan);

            resampler->addSampleClient(&output);

            try {
                if (_startTime.toUsecs() != 0) {
                    DLOG(("searching for time ") <<
                        _startTime.format(true,"%Y %m %d %H:%M:%S"));
                    sis.search(_startTime);
                    DLOG(("search done."));
                }
                for (;;) {
                    sis.readSamples();
                    if (_finished || _interrupted) break;
                }
            }
            catch (n_u::EOFException& e) {
                cerr << "EOF received" << endl;
            }
            catch (n_u::IOException& e) {
                resampler->removeSampleClient(&output);
                resampler->disconnect(pipeline.getProcessedSampleSource());
                pipeline.disconnect(&sis);
                sis.close();
                output.close();
                throw e;
            }

            cerr << "flushing buffers" << endl;
            sis.flush();
            resampler->removeSampleClient(&output);
            output.close();
        }
#endif  // HAS_NC_SERVER_RPC_H
        resampler->disconnect(pipeline.getProcessedSampleSource());
        pipeline.disconnect(&sis);
        sis.close();
    }
    catch (nidas::core::XMLException& e) {
	cerr << e.what() << endl;
	return 1;
    }
    catch (n_u::InvalidParameterException& e) {
	cerr << e.what() << endl;
	return 1;
    }
    catch (n_u::Exception& e) {
	cerr << e.what() << endl;
        cerr << "returning exception" << endl;
	return 1;
    }
    if (_interrupted) return 1;       // interrupted
    return 0;
}

int main(int argc, char** argv)
{
    return DataPrep::main(argc,argv);
}
