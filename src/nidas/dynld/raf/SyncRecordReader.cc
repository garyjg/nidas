/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/dynld/raf/SyncRecordReader.h>
#include <nidas/dynld/raf/SyncRecordSource.h>
#include <nidas/util/EOFException.h>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

SyncRecordReader::SyncRecordReader(IOChannel*iochan):
	inputStream(iochan),headException(0),
	numFloats(0),startTime(0),_debug(false)
{
    try {
	// inputStream.init();
	for (;;) {
	    const Sample* samp = inputStream.readSample();
	    // read/parse SyncRec header, full out variables list
	    if (samp->getId() == SYNC_RECORD_HEADER_ID) {
                if (_debug)
		    cerr << "received SYNC_RECORD_HEADER_ID" << endl;
		scanHeader(samp);
		samp->freeReference();
		break;
	    }
	    else samp->freeReference();
	}
    }
    catch(const n_u::IOException& e) {
        headException = new SyncRecHeaderException(e.what());
    }
}

SyncRecordReader::~SyncRecordReader()
{
    list<SampleTag*>::iterator si;
    for (si = sampleTags.begin(); si != sampleTags.end(); ++si)
	delete *si;
    delete headException;
}

/* local map class with a destructor which cleans up any
 * pointers left hanging.
 */
namespace {
class LocalVarMap: public map<string,SyncRecordVariable*>
{
public:
    ~LocalVarMap() {
	map<string,SyncRecordVariable*>::const_iterator vi;
	for (vi = begin(); vi != end(); ++vi) {
	    SyncRecordVariable* var = vi->second;
	    delete var;
	}
    }
};
}

void SyncRecordReader::scanHeader(const Sample* samp) throw()
{
    size_t offset = 0;
    size_t lagoffset = 0;
    delete headException;
    headException = 0;

    startTime = (time_t)(samp->getTimeTag() / USECS_PER_SEC);

    istringstream header(
    	string((const char*)samp->getConstVoidDataPtr(),
		samp->getDataLength()));

    if (_debug)
        cerr << "header=\n" << string((const char*)samp->getConstVoidDataPtr(),
		samp->getDataLength()) << endl;

    try {
        readKeyedValues(header);
    }
    catch (const SyncRecHeaderException& e) {
        headException = new SyncRecHeaderException(e);
	return;
    }

    string tmpstr;
    string section = "variables";
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("variables")) {
    	headException = new SyncRecHeaderException("variables {",
	    tmpstr);
	return;
    }

    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("{")) {
    	headException = new SyncRecHeaderException("variables {",
	    string("variables ") + tmpstr);
	return;
    }

    list<const SyncRecordVariable*> newvars;
    LocalVarMap varmap;
    map<string,SyncRecordVariable*>::const_iterator vi;
    list<const SyncRecordVariable*>::const_iterator vli;

    for (;;) {

	string vname;
	header >> vname;

        if (_debug)
	    cerr << "vname=" << vname << endl;

	if (header.eof()) goto eof;

	// look for end paren
	if (!vname.compare("}")) break;

	string vtypestr;
	int vlen;

	header >> vtypestr >> vlen;
        if (_debug) {
	    cerr << "vtypestr=" << vtypestr << endl;
	    cerr << "vlen=" << vlen << endl;
        }
	if (header.eof()) goto eof;

	// screen bad variable types here
	if (vtypestr.length() != 1) {
	    headException = new SyncRecHeaderException(
	    	string("unexpected variable type: ") + vtypestr);
	    return;
	}

	Variable::type_t vtype = Variable::CONTINUOUS;
	switch (vtypestr[0]) {
	case 'n':
	    vtype = Variable::CONTINUOUS;
	    break;
	case 'c':
	    vtype = Variable::COUNTER;
	    break;
	case 't':
	    vtype = Variable::CLOCK;	// shouldn't happen
	    break;
	case 'o':
	    vtype = Variable::OTHER;	// shouldn't happen
	    break;
	default:
	    headException = new SyncRecHeaderException(
	    	string("unexpected variable type: ") + vtypestr);
	    return;
	}

	string vunits = getQuotedString(header);
        if (_debug)
	    cerr << "vunits=" << vunits << endl;
	if (header.eof()) goto eof;

	string vlongname = getQuotedString(header);
        if (_debug)
	    cerr << "vlongname=\"" << vlongname << "\"" << endl;
	if (header.eof()) goto eof;

	vector<float> coefs;

	for (;;) {
	    float val;
	    header >> val;
	    if (header.fail()) break;
            if (_debug)
	        cerr << "val=" << val << endl;
	    coefs.push_back(val);
	}
	if (header.eof()) goto eof;
	header.clear();

	// converted units
	string cunits = getQuotedString(header);
        if (_debug)
	    cerr << "cunits=" << vunits << endl;
	if (header.eof()) goto eof;

	char semicolon = 0;
	for (;;) {
	    header >> semicolon;
	    if (header.eof()) goto eof;
	    if (semicolon != ' ' && semicolon != '\t' &&
	    	semicolon != '\n' && semicolon != '\r')
	    	break;
	}

	if (semicolon != ';') {
	    headException =
	    	new SyncRecHeaderException(";",string(semicolon,1));
	    return;
	}

	// variable fields parsed, now create SyncRecordVariable.
	SyncRecordVariable* var = new SyncRecordVariable();

	var->setName(vname);

	var->setType(vtype);

	var->setLength(vlen);
	var->setUnits(vunits);
	var->setLongName(vlongname);

	if (coefs.size() == 2) {
	    Linear* linear = new Linear();
	    linear->setIntercept(coefs[0]);
	    linear->setSlope(coefs[1]);
	    linear->setUnits(cunits);
	    var->setConverter(linear);
	}
	else if (coefs.size() > 2) {
	    Polynomial* poly = new Polynomial();
	    poly->setCoefficients(coefs);
	    poly->setUnits(cunits);
	    var->setConverter(poly);
	}
	
	varmap[vname] = var;
	newvars.push_back(var);
    }
    if (_debug)
        cerr << "done with variable loop" << endl;
    section = "rates";

    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("rates")) {
    	headException = new SyncRecHeaderException("rates {",
	    tmpstr);
	return;
    }

    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("{")) {
    	headException = new SyncRecHeaderException("rates {",
	    string("rates ") + tmpstr);
        return;
    }

    // read rate groups
    for (;;) {
	float rate;
        header >> rate;
        if (_debug)
	    cerr << "read rate=" << rate << endl;
	if (header.fail()) break;
	if (header.eof()) goto eof;

	size_t groupSize = 1;	// number of float values in a group
	offset++;		// first float in group is the lag value

	SampleTag* stag = new SampleTag();
	sampleTags.push_back(stag);
	stag->setRate(rate);

	// read variable names of this rate
	for (;;) {
	    string vname;
	    header >> vname;
            if (_debug)
	        cerr << "variable of rate: " << vname << endl;
	    if (header.eof()) goto eof;
	    if (!vname.compare(";")) break;
	    vi = varmap.find(vname);
	    if (vi == varmap.end()) {
		ostringstream ost;
		ost << rate;
	        headException = new SyncRecHeaderException(
			string("variable ") + vname +
			" not found for rate " + ost.str());
		return;
	    }
	    SyncRecordVariable* var = vi->second;
	    if (!var) {
		ostringstream ost;
		ost << rate;
	        headException = new SyncRecHeaderException(
			string("variable ") + vname +
			" listed under more than one rate " + ost.str());
		return;
	    }

	    stag->addVariable(var);
	    var->setSyncRecOffset(offset);
	    var->setLagOffset(lagoffset);
	    int nfloat = var->getLength() * (int)ceil(rate);
	    offset += nfloat;
	    groupSize += nfloat;
	    varmap[vname] = 0;
	}
	lagoffset += groupSize;
    }

    header.clear();	// clear fail bit
    tmpstr.clear();
    header >> tmpstr;
    if (header.eof() || tmpstr.compare("}"))
	headException = new SyncRecHeaderException("}",
	    tmpstr);

    // check that all variables have been found in a rate group
    for (vi = varmap.begin(); vi != varmap.end(); ++vi) {
        SyncRecordVariable* var = vi->second;
	if (var) {
	    headException = new SyncRecHeaderException(
	    	string("variable ") + var->getName() +
		" not found in a rate group");
            if (_debug)
	        cerr << headException->what() << endl;
	    return;
	}
    }

    variables = newvars;

    // make the variableMap for quick lookup.
    for (vli = variables.begin(); vli != variables.end(); ++vli) {
	const SyncRecordVariable* varp = *vli;
    	variableMap[varp->getName()] = varp;
    }

    numFloats = offset;
    return;

eof: 
    headException = new SyncRecHeaderException(section,"end of header");
    return;
}

string SyncRecordReader::getQuotedString(istringstream& istr)
{
    string val;

    char dquote = 0;
    do {
	istr >> dquote;
	if (istr.eof()) return val;
    } while (dquote != '\"');


    for (;;) {
	istr.get(dquote);
	if (istr.eof() || dquote == '\"') break;
	val += dquote;
    }
    return val;
}

void SyncRecordReader::readKeyedValues(istringstream& header)
	throw(SyncRecHeaderException)
{

    for (;;) {
        string key,value;
        header >> key;
        if (header.eof())
            throw SyncRecHeaderException("end of header when reading keyed values");
        if (key.length() > 0 && key[0] == '#') break;
        header >> value;
        if (header.eof())
            throw SyncRecHeaderException(string("end of header when reading value for ") +
		key);

        if (key == "project") projectName = value;
        else if (key == "aircraft") aircraftName = value;
        else if (key == "flight") flightName = value;
    }
}

size_t SyncRecordReader::read(dsm_time_t* tt,float* dest,size_t len) throw(n_u::IOException)
{

    for (;;) {
	const Sample* samp = inputStream.readSample();
	if (samp->getId() == SYNC_RECORD_ID) {

	    if (len > samp->getDataLength()) len = samp->getDataLength();

	    *tt = samp->getTimeTag();
	    memcpy(dest,samp->getConstVoidDataPtr(),len * sizeof(float));

	    samp->freeReference();
	    return len;
	}
	else samp->freeReference();
    }
}

const list<const SyncRecordVariable*> SyncRecordReader::getVariables() throw(n_u::Exception)
{
    if (headException) throw *headException;
    return variables;
}

const SyncRecordVariable* SyncRecordReader::getVariable(const std::string& name) const
{
    map<string,const SyncRecordVariable*>::const_iterator vi;
    vi = variableMap.find(name);
    if (vi == variableMap.end()) return 0;
    return vi->second;
}

