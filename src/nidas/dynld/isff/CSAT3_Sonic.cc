// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
   Copyright 2005 UCAR, NCAR, All Rights Reserved

   $LastChangedDate$

   $LastChangedRevision$

   $LastChangedBy$

   $HeadURL$

*/

#include <nidas/dynld/isff/CSAT3_Sonic.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/Variable.h>
#include <nidas/core/PhysConstants.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>
#include <nidas/util/IOTimeoutException.h>

#include <sstream>
#include <fstream>

using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,CSAT3_Sonic)

CSAT3_Sonic::CSAT3_Sonic():
    _windInLen(12),	// two bytes each for u,v,w,tc,diag, and 0x55aa
    _totalInLen(12),
    _windNumOut(0),
    _ldiagIndex(-1),
    _spdIndex(-1),
    _dirIndex(-1),
    _spikeIndex(-1),
    _windSampleId(0),
    _extraSampleTags(),
    _nttsave(-2),
    _counter(-1),
#if __BYTE_ORDER == __BIG_ENDIAN
    _swapBuf(),
#endif
    _rate(0),
    _oversample(false),
    _serialNumber(),_sonicLogFile(),
    _gapDtUsecs(0),
    _ttlast(0),
    _nanIfDiag(true),
    _consecutiveOpenFailures(0)
{
    /* index and sign transform for usual sonic orientation.
     * Normal orientation, no component change: 0 to 0, 1 to 1 and 2 to 2,
     * with no sign change. */
    for (int i = 0; i < 3; i++) {
        _tx[i] = i;
        _sx[i] = 1;
    }
}

CSAT3_Sonic::~CSAT3_Sonic()
{
}

bool CSAT3_Sonic::stopSonic() throw(n_u::IOException)
{
    try {
        setMessageParameters(1,"",true);
    }
    catch(const n_u::InvalidParameterException& e) {
        throw n_u::IOException(getName(),"stop",e.what());
    }

    clearBuffer();
    for (int i = 0; i < 10; i++) {
        // clear whatever junk may be in the buffer til a timeout
        // If we get a timeout then there is no need to send a &
        try {
            for (int i = 0; i < 2; i++) {
                readBuffer(MSECS_PER_SEC + 10);
                clearBuffer();
            }
        }
        catch (const n_u::IOTimeoutException& e) {
            DLOG(("%s: timeout",getName().c_str()));
            return true;
        }
        DLOG(("%s: sending &",getName().c_str()));
        write("&",1);
    }
    return false;
}

bool CSAT3_Sonic::startSonic() throw(n_u::IOException)
{
    clearBuffer();

    DLOG(("%s: sending D& (nocr)",getName().c_str()));
    write("D&",2);
    size_t ml = getMessageLength() + getMessageSeparator().length();

    size_t tlen = 0;
    const int NTRY = 5;
    // read until we get an actual sample, ml number of characters, or NTRY timeouts
    for (int ntry = 0; tlen < ml; ) {
        try {
            size_t l = readBuffer(3 * MSECS_PER_SEC);
            DLOG(("%s: read, l=%zd",getName().c_str(),l));
            tlen += l;
            int nsamp = 0;
            for (Sample* samp = nextSample(); samp; samp = nextSample()) {
                distributeRaw(samp);
                nsamp++;
            }
            if (nsamp > 0) return true;
        }
        catch (const n_u::IOTimeoutException& e) {
            DLOG(("%s: timeout, sending &",getName().c_str()));
            if (++ntry == NTRY) throw e;
            write("D&",2);
        }
        // DLOG(("%s: sending D",getName().c_str()));
        // write("D",1);
    }
    return false;
}

string CSAT3_Sonic::querySonic(int &acqrate,char &osc, string& serialNumber, string& revision)
throw(n_u::IOException)
{
    string result;

    acqrate = 0;
    osc = ' ';
    serialNumber = "unknown";
    revision = "unknown";

    /*
     * Make this robust around the following situations:
     * 1. No sonic connected to the port. Will result in a timeout and a break
     *    out of the inner loop. Will try up to the number of times in the
     *    outer loop, and return an empty result, and the returned parameters will
     *    have the above initial values.
     *    If a timeout is set for this sensor in the config, the open() will be
     *    retried again and things should succeed once a sonic, with power is connected.
     * 2. Sonic is connected but it isn't responding to commands, just spewing good data.
     *    result will a string of binary jibberish, and the returned parameters
     *    will have the above initial values since the keywords won't be found.
     *    An attempt may be made to set the rate, which will also not succeed,
     *    the open() will return anyway and the good data will be read as usual
     *    but it may have the wrong rate.
     * 3. sonic isn't responding to commands and is sending jibberish data, i.e.
     *    a wrong baud rate or bad signal connection. result will be as in
     *    2 above, but the data read will be junk. User is expected to notice
     *    the bad data and resolve the issue. Software can't do anything about it
     *    (could try other baud rates, but that ain't worth doing...).
     * 4. Operational sonic. All should be happy.
     */

    for (int j = 0; j < 5; j++) {
        DLOG(("%s: sending ?? CR",getName().c_str()));
        write("??\r",3);    // must send CR
        // sonic takes about 2 seconds to respond to ??
        int timeout = 2 * MSECS_PER_SEC;

        // read till timeout, or 10 times.
        for (int i = 0; i < 10; i++) {
            try {
                readBuffer(timeout);
            }
            catch (const n_u::IOTimeoutException& e) {
                DLOG(("%s: timeout",getName().c_str()));
                break;
            }
            for (Sample* samp = nextSample(); samp; samp = nextSample()) {
                distributeRaw(samp);
                int l = samp->getDataByteLength();
                // strings will not be null terminated
                const char * cp = (const char*)samp->getConstVoidDataPtr();
                if (result.length() == 0)
                    while (l && (*cp == 'T' || ::isspace(*cp))) { cp++; l--; }
                result += string(cp,l);
            }
        }
        if (result.length() > 100) break;
    }
    clearBuffer();

    // Version 3 output starts with "ET=", version 4 with "SNXXXX"
    string::size_type fs = std::min(result.find("ET="),result.find("SN"));
    if (fs != string::npos && fs > 0) result = result.substr(fs);

    while (result.length() > 0 && result[result.length() - 1] == '>')
        result.resize(result.length()-1);

    unsigned int ql = result.length();
    DLOG(("%s: query=",getName().c_str()) << n_u::addBackslashSequences(result) << " result length=" << ql);
    if (ql == 0) return result;

    // find and get AQ parameter, e.g. AQ=1.0 (raw sampling rate)
    fs = result.find("AQ=");
    if (fs != string::npos && fs + 3 < ql)
        acqrate = atoi(result.substr(fs+3).c_str());

    // get os parameter, e.g. "os=g"
    // g for 10Hz 6x oversampling, h for 20Hz 3x oversampling, ' ' otherwise
    // For version 4, os=0 means no oversampling
    fs = result.find("os=");
    if (fs != string::npos && fs + 3 < ql) osc = result[fs+3];

    // get serial number, e.g. "SN1124" (hopefully is the only string with "SN")
    fs = result.find("SN");
    string::size_type bl = result.find(' ',fs);
    if (fs != string::npos && bl != string::npos)
        serialNumber = result.substr(fs,bl-fs);

    // get software revision, e.g. "rev 3.0f"
    fs = result.find("rev");
    if (fs != string::npos && fs + 4 < ql) {
        bl = result.find(' ',fs+4);
        revision = result.substr(fs+4,bl-fs-4);
    }
    return result;
}

string CSAT3_Sonic::sendRateCommand(const char* cmd)
throw(n_u::IOException)
{
    DLOG(("%s: sending %s",getName().c_str(),cmd));
    write(cmd,2);
    int timeout = MSECS_PER_SEC * 4;

    string result;
    // do up to 10 reads or a timeout.
    for (int i = 0; i < 10; i++) {
        try {
            readBuffer(timeout);
        }
        catch (const n_u::IOTimeoutException& e) {
            DLOG(("%s: timeout",getName().c_str()));
            break;
        }
        for (Sample* samp = nextSample(); samp; samp = nextSample()) {
            distributeRaw(samp);
            // strings will not be null terminated
            const char * cp = (const char*)samp->getConstVoidDataPtr();
            result += string(cp,samp->getDataByteLength());
        }
    }
    clearBuffer();
    while (result.length() > 0 && result[result.length() - 1] == '>') result.resize(result.length()-1);
    return result;
}

const char* CSAT3_Sonic::getRateCommand(int rate,bool oversample)
{
    struct acqSigTable {
        int rate;
        bool oversample;
        const char* cmd;
    };
    static const struct acqSigTable acqSigCmds[] = {
        {1,false,"A2"},
        {2,false,"A5"},
        {3,false,"A6"},
        {5,false,"A7"},
        {6,false,"A8"},
        {10,false,"A9"},
        {12,false,"Aa"},
        {15,false,"Ab"},
        {20,false,"Ac"},
        {30,false,"Ad"},
        {60,false,"Ae"},
        {10,true,"Ag"},
        {20,true,"Ah"},
    };

    int nr =  (signed)(sizeof(acqSigCmds)/sizeof(acqSigCmds[0]));
    for (int i = 0; i < nr; i++) {
        if (acqSigCmds[i].rate == rate &&
                (oversample == acqSigCmds[i].oversample)) {
            return acqSigCmds[i].cmd;
        }
    }
    return 0;
}

void CSAT3_Sonic::open(int flags)
throw(n_u::IOException,n_u::InvalidParameterException)
{
    DSMSerialSensor::open(flags);

    const int NOPEN_TRY = 5;

    const char* rateCmd = 0;
    DLOG(("%s: _rate=%d",getName().c_str(),_rate));
    if (_rate > 0) {
        rateCmd = getRateCommand(_rate,_oversample);
        if (!rateCmd) {
            ostringstream ost;
            ost << "rate=" << _rate << " Hz not supported with oversample=" << _oversample;
            throw n_u::InvalidParameterException(getName(),
                    "sample rate",ost.str());
        }
    }


    int ml = getMessageLength();
    string sep = getMessageSeparator();
    bool eom = getMessageSeparatorAtEOM();

    stopSonic();

    try {
        setMessageParameters(0,">",true);
    }
    catch(const n_u::InvalidParameterException& e) {
        throw n_u::IOException(getName(),"open",e.what());
    }

    // put in "terminal" mode
    DLOG(("%s: sending T (nocr)",getName().c_str()));
    write("T",1);
    usleep(USECS_PER_SEC);

    int acqrate = 0;
    string serialNumber = "unknown";
    char osc = ' ';
    string revision;

    const int NTRY = 2;
    for (int ntry = 0; serialNumber == "unknown" && ntry < NTRY; ntry++) {
        string query = querySonic(acqrate,osc,serialNumber,revision);
        DLOG(("%s: AQ=%d,os=%c,serial number=",getName().c_str(),acqrate,osc) << serialNumber << " rev=" << revision);

        // Is current sonic rate OK?  If requested rate is 0, don't change.
        bool rateOK = _rate == 0;
        if (!_oversample && acqrate == _rate) rateOK = true;
        if (_oversample && acqrate == 60) {
            if (_rate == 10 && osc == 'g') rateOK = true;
            if (_rate == 20 && osc == 'h') rateOK = true;
        }

        string rateResult;
        if (!rateOK) {
            assert(rateCmd != 0);
            rateResult = sendRateCommand(rateCmd);
            query = querySonic(acqrate,osc,serialNumber,revision);
            DLOG(("%s: AQ=%d,os=%c,serial number=",getName().c_str(),acqrate,osc) << serialNumber << " rev=" << revision);
        }

        if (serialNumber != "unknown") {
            // On rate or serial number change, log to file.
            if ((!rateOK || serialNumber != _serialNumber) && _sonicLogFile.length() > 0) {
                n_u::UTime now;
                string fname = getDSMConfig()->expandString(_sonicLogFile);
                ofstream fst(fname.c_str(),ios_base::out | ios_base::app);
                fst << "csat3: " << getName() <<
                    ", id=" << getDSMId() << ',' << getSensorId() <<
                    ", " << serialNumber << ", " << revision << endl;
                fst << "time: " << now.format(true,"%Y %m %d %H:%M:%S") << endl;
                n_u::trimString(rateResult);
                if (rateResult.length() > 0) fst << rateResult << endl;
                n_u::trimString(query);
                fst << query << endl;
                fst << "##################" << endl;
                if (fst.fail()) PLOG(("%s: write failed",_sonicLogFile.c_str()));
                fst.close();
            }
            _serialNumber = serialNumber;
        }
    }

    try {
        setMessageParameters(ml,sep,eom);
    }
    catch(const n_u::InvalidParameterException& e) {
        throw n_u::IOException(getName(),"open",e.what());
    }

    // returns true if some recognizeable samples are received.
    bool dataok = startSonic();

    // An IOTimeException is thrown, which will cause another open attempt to
    // be scheduled, in the following circumstances:
    //    * no serial number and no data. We don't add any extra log messages
    //      in this case to avoid filling up the logs when a sonic simply
    //      isn't connected. We don't want to give up in this case, and 
    //      return without an exception, because then a sonic might later be
    //      connected and the data read without knowing the serial number, which
    //      is only determined in this open method. The serial number is important
    //      to know for later analysis purposes.
    //    * if no serial number, but some data is received and less than NOPEN_TRY
    //      attempts have been made.  After NOPEN_TRY consecutive failures, just
    //      return from this open method without an exception, which will pass
    //      this DSMSensor to the select loop for reading, i.e. punt on getting
    //      the serial number - the data is more important.
    //    * if serial number, but no data is received, and less than NOPEN_TRY
    //      attempts have been tried. This should be a rare situation. If the
    //      sonic responds to serial number queries, it generally should spout data.
    //      After NOPEN_TRY consecutive failures, just return from this open method
    //      which will pass this DSMSensor to the select loop for reading. If a
    //      timeout value is defined for this DSMSensor, and data doesn't start
    //      arriving in that amount of time, then this DSMSensor will be scheduled
    //      for another open.
    if (serialNumber == "unknown") {
        // can't query sonic
        _consecutiveOpenFailures++;
        if (dataok) {
            if (_consecutiveOpenFailures >= NOPEN_TRY) {
                WLOG(("%s: Cannot query sonic serial number. but data received. %d open failures. Will try to read.",
                            getName().c_str(),_consecutiveOpenFailures));
                return; // return from open, proceed to read data.
            }
            WLOG(("%s: Cannot query sonic serial number, but data received. %d open failures.",
                        getName().c_str(),_consecutiveOpenFailures));
        }
        throw n_u::IOTimeoutException(getName(),"open");
    }
    else {
        // serial number query success, but no data - should be a rare occurence.
        if (!dataok) {
            _consecutiveOpenFailures++;
            if (_consecutiveOpenFailures >= NOPEN_TRY) {
                WLOG(("%s: Sonic serial number=%s, but no data received. %d open failures. Will try to read.",
                            getName().c_str(),serialNumber.c_str(),_consecutiveOpenFailures));
                return; // return from open, proceed to read data.
            }
            WLOG(("%s: Sonic serial number=%s, but no data received. %d open failures.",
                        getName().c_str(),serialNumber.c_str(),_consecutiveOpenFailures));
            throw n_u::IOTimeoutException(getName(),"open");
        }
        _consecutiveOpenFailures = 0;   // what-a-ya-know, success!
    }
}

void CSAT3_Sonic::validate()
    throw(n_u::InvalidParameterException)
{
    SonicAnemometer::validate();

    std::list<const SampleTag*> tags= getSampleTags();

    if (tags.size() > 2 || tags.size() < 1)
        throw n_u::InvalidParameterException(getName() +
                " can only create two samples (wind and extra)");

    std::list<const SampleTag*>::const_iterator si = tags.begin();
    for ( ; si != tags.end(); ++si) {
        const SampleTag* stag = *si;
        /*
         * nvars
         * 5	u,v,w,tc,diag
         * 6	u,v,w,tc,diag,ldiag
         * 7	u,v,w,tc,diag,spd,dir
         * 8	u,v,w,tc,diag,ldiag,spd,dir
         * 9	u,v,w,tc,diag,uflag,vflag,wflag,tcflag
         * 11	u,v,w,tc,diag,spd,dir,uflag,vflag,wflag,tcflag
         */
        if (_windSampleId == 0) {
            size_t nvars = stag->getVariables().size();
            _rate = (int)rint(stag->getRate());
            _gapDtUsecs = 5 * USECS_PER_SEC;

            _windSampleId = stag->getId();
            _windNumOut = nvars;
            switch(nvars) {
            case 5:
            case 6:
            case 9:
                if (nvars == 9) _spikeIndex = 5;
                if (nvars == 6) _ldiagIndex = 5;
                break;
            case 11:
            case 7:
            case 8:
                if (nvars == 8) _ldiagIndex = 5;
                if (nvars == 11) _spikeIndex = 7;
                {
                    VariableIterator vi = stag->getVariableIterator();
                    for (int i = 0; vi.hasNext(); i++) {
                        const Variable* var = vi.next();
                        const string& vname = var->getName();
                        if (vname.length() > 2 && vname.substr(0,3) == "spd")
                            _spdIndex = i;
                        else if (vname.length() > 2 && vname.substr(0,3) == "dir")
                            _dirIndex = i;
                    }
                }
                if (_spdIndex < 0 || _dirIndex < 0)
                    throw n_u::InvalidParameterException(getName() +
                            " CSAT3 cannot find speed or direction variables");
                break;
            default:
                throw n_u::InvalidParameterException(getName() +
                        " unsupported number of variables. Must be: u,v,w,tc,diag,[spd,dir][4xflags]]");
            }
        }
        else {
            _extraSampleTags.push_back(stag);
            _totalInLen += 2;	// 2 bytes for each additional input
        }
    }
#if __BYTE_ORDER == __BIG_ENDIAN
    _swapBuf.resize(_totalInLen/2);
#endif
}

float CSAT3_Sonic::correctTcForPathCurvature(float tc, float, float, float)
{
    // no correction necessary. CSAT outputs speed of sound
    // that is already corrected for path curvature.
    return tc;
}

bool CSAT3_Sonic::process(const Sample* samp,
        std::list<const Sample*>& results) throw()
{

    size_t inlen = samp->getDataByteLength();
    if (inlen < _windInLen) return false;	// not enough data

    const char* dinptr = (const char*) samp->getConstVoidDataPtr();
    // check for correct termination bytes
#ifdef DEBUG
    cerr << "inlen=" << inlen << ' ' << hex << (int)dinptr[inlen-2] <<
        ',' << (int)dinptr[inlen-1] << dec << endl;
#endif

    // Sometimes a serializer is connected to a sonic, in which
    // case it generates records longer than 12 bytes.
    // Check here that the record ends in 0x55 0xaa.
    if (dinptr[inlen-2] != '\x55' || dinptr[inlen-1] != '\xaa') return false;

    if (inlen > _totalInLen) inlen = _totalInLen;

#if __BYTE_ORDER == __BIG_ENDIAN
    /* Swap bytes of input. Campbell output is little endian */
    swab(dinptr,(char *)&_swapBuf.front(),inlen-2);     // dont' swap 0x55 0xaa
    const short* win = &_swapBuf.front();
#else
    const short* win = (const short*) dinptr;
#endif

    /*
     * CSAT3 has an internal two sample buffer, so shift
     * wind time tags backwards by two samples.
     */

    /* restart sample time shifting on a data gap */
    if (_gapDtUsecs > 0 && (samp->getTimeTag() - _ttlast) > _gapDtUsecs) _nttsave = -2;
    _ttlast = samp->getTimeTag();

    if (_nttsave < 0)
        _timetags[_nttsave++ + 2] = samp->getTimeTag();
    else {
        SampleT<float>* wsamp = getSample<float>(_windNumOut);
        wsamp->setTimeTag(_timetags[_nttsave]);
        wsamp->setId(_windSampleId);

        _timetags[_nttsave] = samp->getTimeTag();
        _nttsave = (_nttsave + 1) % 2;

        float* dout = wsamp->getDataPtr();

        unsigned short diag = (unsigned) win[4];
        int cntr = (diag & 0x003f);

        // special NaN encodings of diagnostic value
        // (F03F=61503 and F000=61440), where diag
        // bits (12 to 15) are set and a counter
        // value of 0 or 63. Range codes all 0.
        if (diag == 61503 || diag == 61440) {
            for (int i = 0; i < 4; i++) 
                dout[i] = floatNAN;
            diag = (diag & 0xf000) >> 12;
        }
        else {
            int range[3];
            range[0] = (diag & 0x0c00) >> 10;
            range[1] = (diag & 0x0300) >> 8;
            range[2] = (diag & 0x00c0) >> 6;
            diag = (diag & 0xf000) >> 12;

            if (diag && _nanIfDiag) {
                for (int i = 0; i < 4; i++) {
                        dout[i] = floatNAN;
                }
            }
            else {
                const float scale[] = {0.002,0.001,0.0005,0.00025};
                int nmissing = 0;
                for (int i = 0; i < 3; i++) {
                    int ix = _tx[i];
                    if (win[ix] == -32768) {
                        dout[i] = floatNAN;
                        nmissing++;
                    }
                    else {
                        dout[i] = _sx[i] * win[ix] * scale[range[ix]];
                    }
                }

                /*
                 * Documentation also says speed of sound should be a NaN if
                 * ALL the wind components are NaNs.
                 */
                if (nmissing == 3 || win[3] == -32768)
                    dout[3] = floatNAN;
                else {
                    /* convert raw word to speed of sound in m/s */
                    float c = (win[3] * 0.001) + 340.0;
                    /* Convert speed of sound to Tc */
                    dout[3] = (c * c) / GAMMA_R - KELVIN_AT_0C;
                }
            }
        }

        if (_counter >=0 && ((++_counter % 64) != cntr) ) diag += 16;
        _counter = cntr;
        dout[4] = diag;

        // logical diagnostic value: set to 0 if all sonic
        // diagnostics are zero, otherwise one.
        if (_ldiagIndex >= 0) dout[_ldiagIndex] = (float)(diag != 0);

        if (_spikeIndex >= 0 || getDespike()) {
            bool spikes[4] = {false,false,false,false};
            despike(wsamp->getTimeTag(),dout,4,spikes);

            if (_spikeIndex >= 0) {
                for (int i = 0; i < 4; i++) 
                    dout[i + _spikeIndex] = (float) spikes[i];
            }
        }

        offsetsAndRotate(wsamp->getTimeTag(), dout);

        if (_spdIndex >= 0)
            dout[_spdIndex] = sqrt(dout[0] * dout[0] + dout[1] * dout[1]);
        if (_dirIndex >= 0) {
            float dr = atan2f(-dout[0],-dout[1]) * 180.0 / M_PI;
            if (dr < 0.0) dr += 360.;
            dout[_dirIndex] = dr;
        }
        results.push_back(wsamp);
    }

    // inlen is now less than or equal to the expected input length
    bool goodterm = dinptr[inlen-2] == '\x55' && dinptr[inlen-1] == '\xaa';

    for (unsigned int i = 0; i < _extraSampleTags.size(); i++) {

        if (inlen < _windInLen + (i+1) * 2) break;

        const SampleTag* stag = _extraSampleTags[i];
        const vector<const Variable*>& vars = stag->getVariables();
        size_t nvars = vars.size();
        SampleT<float>* hsamp = getSample<float>(nvars);
        hsamp->setTimeTag(samp->getTimeTag());
        hsamp->setId(stag->getId());

        unsigned short counts = ((const unsigned short*) win)[i+5];
        float volts;
        if (counts < 65500 && goodterm) volts = counts * 0.0001;
        else volts = floatNAN;

        for (unsigned int j = 0; j < nvars; j++) {
            const Variable* var = vars[j];
            VariableConverter* conv = var->getConverter();
            if (!conv) hsamp->getDataPtr()[j] = volts;
            else hsamp->getDataPtr()[j] =
                conv->convert(hsamp->getTimeTag(),volts);
        }
        results.push_back(hsamp);
    }
    return true;
}

void CSAT3_Sonic::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    SonicAnemometer::fromDOMElement(node);

    const list<const Parameter*>& params = getParameters();
    list<const Parameter*>::const_iterator pi = params.begin();

    for ( ; pi != params.end(); ++pi) {
        const Parameter* parameter = *pi;

        if (parameter->getName() == "orientation") {
            bool pok = parameter->getType() == Parameter::STRING_PARAM &&
                parameter->getLength() == 1;
            if (pok && parameter->getStringValue(0) == "normal") {
                _tx[0] = 0;
                _tx[1] = 1;
                _tx[2] = 2;
                _sx[0] = 1;
                _sx[1] = 1;
                _sx[2] = 1;
            }
            else if (pok && parameter->getStringValue(0) == "down") {
                /* When the sonic is hanging down, the usual sonic w axis
                 * becomes the new u axis, u becomes w, and v becomes -v. */
                _tx[0] = 2;     // new u is normal w
                _tx[1] = 1;     // v is -v
                _tx[2] = 0;     // new w is normal u
                _sx[0] = 1;
                _sx[1] = -1;    // v is -v
                _sx[2] = 1;
            }
            else if (pok && parameter->getStringValue(0) == "flipped") {
                /* Sonic flipped over, w becomes -w, v becomes -v. */
                _tx[0] = 0;
                _tx[1] = 1;
                _tx[2] = 2;
                _sx[0] = 1;
                _sx[1] = -1;
                _sx[2] = -1;
            }
            else
                throw n_u::InvalidParameterException(getName(),
                        "orientation parameter",
                        "must be one string: \"normal\" (default), \"down\" or \"flipped\"");
        }
        else if (parameter->getName() == "oversample") {
            if (parameter->getType() != Parameter::BOOL_PARAM ||
                    parameter->getLength() != 1)
                throw n_u::InvalidParameterException(getName(),
                        "oversample parameter",
                        "must be boolean true or false");
            _oversample = (int)parameter->getNumericValue(0);
        }
        else if (parameter->getName() == "soniclog") {
            if (parameter->getType() != Parameter::STRING_PARAM ||
                    parameter->getLength() != 1)
                throw n_u::InvalidParameterException(getName(),
                        "soniclog parameter",
                        "must be a string");
            _sonicLogFile = parameter->getStringValue(0);
        }
    }
}
