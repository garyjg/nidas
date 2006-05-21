/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <fcntl.h>
#include <iostream>
#include <string.h>

#include <Looper.h>

#include <atdTermio/SerialPort.h>

using namespace std;
using namespace dsm;

int createPtyLink(const std::string& link) throw(atdUtil::IOException)
{
    int fd;
    const char* ptmx = "/dev/ptmx";

    if ((fd = open(ptmx,O_RDWR)) < 0) 
    	throw atdUtil::IOException(ptmx,"open",errno);

    char* slave = ptsname(fd);
    if (!slave) throw atdUtil::IOException(ptmx,"ptsname",errno);

    cerr << "slave pty=" << slave << endl;

    if (grantpt(fd) < 0) throw atdUtil::IOException(ptmx,"grantpt",errno);
    if (unlockpt(fd) < 0) throw atdUtil::IOException(ptmx,"unlockpt",errno);

    struct stat linkstat;
    if (lstat(link.c_str(),&linkstat) < 0) {
        if (errno != ENOENT)
		throw atdUtil::IOException(link,"stat",errno);
    }
    else {
        if (S_ISLNK(linkstat.st_mode)) {
	    cerr << link << " is a symbolic link, deleting" << endl;
	    if (unlink(link.c_str()) < 0)
		throw atdUtil::IOException(link,"unlink",errno);
	}
	else
	    throw atdUtil::IOException(link,
	    	"exists and is not a symbolic link","");

    }
    if (symlink(slave,link.c_str()) < 0)
	throw atdUtil::IOException(link,"symlink",errno);
    return fd;
}

class MensorSim: public LooperClient
{
public:
    MensorSim(atdTermio::SerialPort* p):port(p) {}
    void looperNotify() throw();
private:
    atdTermio::SerialPort* port;
};

void MensorSim::looperNotify() throw()
{
    char outbuf[128];
    sprintf(outbuf,"1%f\r\n",1000.0);
    port->write(outbuf,strlen(outbuf));
}

class ParoSim: public LooperClient
{
public:
    ParoSim(atdTermio::SerialPort* p):port(p) {}
    void looperNotify() throw();
private:
    atdTermio::SerialPort* port;
};

void ParoSim::looperNotify() throw()
{
    char outbuf[128];
    sprintf(outbuf,"*0001%f\r\n",1000.0);
    port->write(outbuf,strlen(outbuf));
}

class BuckSim: public LooperClient
{
public:
    BuckSim(atdTermio::SerialPort* p):port(p) {}
    void looperNotify() throw();
private:
    atdTermio::SerialPort* port;
};

void BuckSim::looperNotify() throw()
{
    const char* outbuf =
    	"14354,-14.23,0,0,-56,0, 33.00,05/08/2003, 17:47:08\r\n";
    port->write(outbuf,strlen(outbuf));
}

class Csat3Sim: public LooperClient
{
public:
    Csat3Sim(atdTermio::SerialPort* p):port(p),counter(0) {}
    void looperNotify() throw();
private:
    atdTermio::SerialPort* port;
    unsigned char counter;
};

void Csat3Sim::looperNotify() throw()
{
    unsigned char outbuf[] =
    	{0,4,0,4,0,4,0,4,0x40,0x05,0x55,0xaa};

    outbuf[8] = (outbuf[8] & 0xc0) + counter++;
    counter &= 0x3f;
    port->write((const char*)outbuf,12);
}

class SensorSim {
public:
    SensorSim();
    int parseRunstring(int argc, char** argv);
    int run();
    static int usage(const char* argv0);
private:
    string device;
    enum sens_type { MENSOR_6100, PARO_1000, BUCK_DP, CSAT3, UNKNOWN } type;
    bool openpty;
    float rate;
};

SensorSim::SensorSim(): type(UNKNOWN),openpty(false),rate(50.0)
{
}

int SensorSim::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    openpty = false;

    while ((opt_char = getopt(argc, argv, "cdmpr:t")) != -1) {
	switch (opt_char) {
	case 'c':
	    type = CSAT3;
	    break;
	case 'd':
	    type = BUCK_DP;
	    break;
	case 'm':
	    type = MENSOR_6100;
	    break;
	case 'p':
	    type = PARO_1000;
	    break;
	case 'r':
	    rate = atof(optarg);
	    break;
	case 't':
	    openpty = true;
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    if (optind == argc - 1) device = string(argv[optind++]);
    if (device.length() == 0) return usage(argv[0]);
    if (type == UNKNOWN) return usage(argv[0]);
    if (optind != argc) return usage(argv[0]);
    return 0;
}

int SensorSim::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "[-p | -m]  device\n\
  -c: simulate CSAT3 sonic anemometer (9600n81, unprompted)\n\
  -d: simulate Buck dewpointer (9600n81, unprompted)\n\
  -m: simulate Mensor 6100 (57600n81,prompted)\n\
  -p: simulate ParoScientific DigiQuartz 1000 (57600n81, unprompted)\n\
  -r rate: generate data at given rate, in Hz (for unprompted sensor)\n\
  -t: open pseudo-terminal device\n\
  device: Name of serial device or pseudo-terminal, e.g. /dev/ttyS1, or /tmp/pty/dev0\n\
" << endl;
    return 1;
}

int SensorSim::run()
{
    try {
	auto_ptr<atdTermio::SerialPort> port;
	auto_ptr<LooperClient> sim;

	if (openpty) {
	    int fd = createPtyLink(device);
	    port.reset(new atdTermio::SerialPort("/dev/ptmx",fd));
	}
	else port.reset(new atdTermio::SerialPort(device));

	unsigned long msecPeriod =
		(unsigned long)rint(MSECS_PER_SEC / rate);
	// cerr << "msecPeriod=" << msecPeriod << endl;

	string promptStrings[] = { "#1?\n", "","",""};

	switch (type) {
	case MENSOR_6100:
	    port->setBaudRate(57600);
	    port->iflag() = ICRNL;
	    port->oflag() = OPOST;
	    port->lflag() = ICANON;
	    sim.reset(new MensorSim(port.get()));
	    break;
	case PARO_1000:
	    port->setBaudRate(57600);
	    port->iflag() = 0;
	    port->oflag() = OPOST;
	    port->lflag() = ICANON;
	    sim.reset(new ParoSim(port.get()));
	    break;
	case BUCK_DP:
	    port->setBaudRate(9600);
	    port->iflag() = 0;
	    port->oflag() = OPOST;
	    port->lflag() = ICANON;
	    sim.reset(new BuckSim(port.get()));
	    break;
	case CSAT3:
	    port->setBaudRate(9600);
	    port->iflag() = 0;
	    port->oflag() = 0;
	    port->lflag() = ICANON;
	    sim.reset(new Csat3Sim(port.get()));
	    break;
	case UNKNOWN:
	    return 1;
	}

	if (!openpty) port->open(O_RDWR);

	Looper* looper = 0;
	if (promptStrings[type].length() == 0) {
	    looper = Looper::getInstance();
	    looper->addClient(sim.get(),msecPeriod);
	    looper->join();
	}
	else {

	    for (;;) {
		char inbuf[128];
		int l = port->readline(inbuf,sizeof(inbuf));
		inbuf[l] = '\0';
		if (!strcmp(inbuf,promptStrings[type].c_str()))
		    sim->looperNotify();
		else cerr << "unrecognized prompt: \"" << inbuf << "\"" << endl;
	    }
	}
    }
    catch(atdUtil::IOException& ioe) {
	cerr << ioe.what() << endl;
	return 1;
    }
    return 0;
}
int main(int argc, char** argv)
{
    SensorSim sim;
    int res;
    if ((res = sim.parseRunstring(argc,argv)) != 0) return res;
    sim.run();
}

