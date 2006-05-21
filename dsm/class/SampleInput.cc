/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <Project.h>
#include <SampleInput.h>
#include <DSMSensor.h>
#include <DSMService.h>

#include <atdUtil/Logger.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_FUNCTION(SampleInputStream)

/*
 * Constructor, with a IOChannel (which may be null).
 */
SampleInputStream::SampleInputStream(IOChannel* iochannel):
    service(0),iochan(iochannel),iostream(0),
    samp(0),leftToRead(0),dptr(0),
    unrecognizedSamples(0)
{
    if (iochan)
        iostream = new IOStream(*iochan,iochan->getBufferSize());
}

/*
 * Copy constructor, with a new IOChannel.
 */
SampleInputStream::SampleInputStream(const SampleInputStream& x,
	IOChannel* iochannel):
    service(x.service),
    iochan(iochannel),iostream(0),
    sampleTags(x.sampleTags),
    samp(0),leftToRead(0),dptr(0),
    unrecognizedSamples(0)
{
    if (iochan)
        iostream = new IOStream(*iochan,iochan->getBufferSize());
}

/*
 * Clone myself, with a new IOChannel.
 */
SampleInputStream* SampleInputStream::clone(IOChannel* iochannel)
{
    return new SampleInputStream(*this,iochannel);
}

SampleInputStream::~SampleInputStream()
{
    delete iostream;
    delete iochan;
}

string SampleInputStream::getName() const {
    if (iochan) return string("SampleInputStream: ") + iochan->getName();
    return string("SampleInputStream");
}

void SampleInputStream::requestConnection(DSMService* requester)
            throw(atdUtil::IOException)
{
    service = requester;
    iochan->requestConnection(this);
}

void SampleInputStream::connected(IOChannel* iochannel) throw()
{
    cerr << "SampleInputStream connected, iochannel=" <<
    	iochannel->getRemoteInet4Address().getHostAddress() << endl;
    // this create a clone of myself
    service->connected(clone(iochannel));
}

void SampleInputStream::addProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    sensorMapMutex.lock();
    sensorMap[sensor->getId()] = sensor;

    map<SampleClient*,list<DSMSensor*> >::iterator sci = 
    	sensorsByClient.find(client);
    if (sci != sensorsByClient.end()) sci->second.push_back(sensor);
    else {
        list<DSMSensor*> sensors;
	sensors.push_back(sensor);
	sensorsByClient[client] = sensors;
    }
    sensorMapMutex.unlock();

    sensor->addSampleClient(client);
}

void SampleInputStream::removeProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    if (!sensor) {		// remove client for all sensors
	sensorMapMutex.lock();
	map<SampleClient*,list<DSMSensor*> >::iterator sci = 
	    sensorsByClient.find(client);
	if (sci != sensorsByClient.end()) {
	    list<DSMSensor*>& sensors = sci->second;
	    for (list<DSMSensor*>::iterator si = sensors.begin();
	    	si != sensors.end(); ++si) {
		sensor = *si;
		sensor->removeSampleClient(client);
		if (sensor->getClientCount() == 0)
		    sensorMap.erase(sensor->getId());
	    }
	}
	sensorMapMutex.unlock();
    }
    else {
        sensor->removeSampleClient(client);
	if (sensor->getClientCount() == 0)
	    sensorMap.erase(sensor->getId());
    }
}

void SampleInputStream::init() throw()
{
#ifdef DEBUG
    cerr << "SampleInputStream::init(), buffer size=" << 
    	iochan->getBufferSize() << endl;
#endif
    if (!iostream)
	iostream = new IOStream(*iochan,iochan->getBufferSize());
}

void SampleInputStream::close() throw(atdUtil::IOException)
{
    delete iostream;
    iostream = 0;
    iochan->close();
}

atdUtil::Inet4Address SampleInputStream::getRemoteInet4Address() const
{
    if (iochan) return iochan->getRemoteInet4Address();
    else return atdUtil::Inet4Address();
}

void SampleInputStream::readHeader() throw(atdUtil::IOException)
{
    inputHeader.check(iostream);
}
/**
 * Read a buffer of data and process all samples in the buffer.
 * This is typically used when a select has determined that there
 * is data available on our file descriptor. Process all available
 * data from the InputStream and distribute() samples to the receive()
 * method of my SampleClients and to the receive() method of
 * DSMSenors.  This will perform only one physical
 * read of the underlying device.
 */
void SampleInputStream::readSamples() throw(atdUtil::IOException)
{
// #define DEBUG
#ifdef DEBUG
    static int nsamps = 0;

    cerr << "readSamples, iostream->read(), available=" << iostream->available() <<
    	", iostream=" << iostream << endl;
#endif
    iostream->read();		// read a buffer's worth
    if (iostream->isNewFile()) {	// first read from a new file
	readHeader();
	if (samp) samp->freeReference();
	samp = 0;
    }

    SampleHeader header;

    // process all in buffer
    for (;;) {
	if (!samp) {
#ifdef DEBUG
	    cerr << "available=" << iostream->available() << endl;
#endif
	    if (iostream->available() < header.getSizeOf()) break;

#ifndef DEBUG
	    iostream->read(&header,header.getSizeOf());
#else
	    size_t len = iostream->read(&header,header.getSizeOf());
	    assert(header.getSizeOf() == 16);
	    assert(len == 16);

	    cerr << "read header " <<
	    	" getTimeTag=" << header.getTimeTag() <<
	    	" getId=" << header.getId() <<
	    	" getType=" << (int) header.getType() <<
	    	" getDataByteLength=" << header.getDataByteLength() <<
		endl;
#endif
	    if (header.getType() >= UNKNOWN_ST || GET_DSM_ID(header.getId()) > 10) {
	        unrecognizedSamples++;
		atdUtil::Logger::getInstance()->log(LOG_WARNING,
		    "SampleInputStream UNKNOWN_ST unrecognizedSamples=%d",
			    unrecognizedSamples);
		cerr << "read header " <<
		    " getTimeTag=" << header.getTimeTag() <<
		    " getId=" << header.getId() << 
		    '(' << GET_DSM_ID(header.getId()) << ',' <<
		    	GET_SHORT_ID(header.getId()) <<
		    ") getType=" << (int) header.getType() <<
		    " getDataByteLength=" << header.getDataByteLength() <<
		    " chars=" << string((const char*)&header,16) << endl;
		cerr << "nbytes=" << iostream->getNBytes() << endl;
		throw atdUtil::IOException(iostream->getName(),"read","bad data");
	    }
	    else
		samp = dsm::getSample((sampleType)header.getType(),
		    header.getDataByteLength());

	    samp->setTimeTag(header.getTimeTag());
	    samp->setId(header.getId());
	    leftToRead = samp->getDataByteLength();
	    // cerr << "leftToRead=" << leftToRead << endl;
	    dptr = (char*) samp->getVoidDataPtr();
	}
	size_t len = iostream->available();
	if (len == 0) break;
	// cerr << "leftToRead=" << leftToRead << " available=" << len << endl;
	if (leftToRead < len) len = leftToRead;
	len = iostream->read(dptr, len);
	// cerr << "read len=" << len << endl;
	dptr += len;
	leftToRead -= len;
	if (leftToRead > 0) break;	// no more data in iostream buffer

#ifdef DEBUG
	if (!(nsamps++ % 100)) cerr << "read " << nsamps << " samples" << endl;
#endif

	distribute(samp);
	samp = 0;
    }
}

void SampleInputStream::distribute(const Sample* samp) throw()
{
    // pass samples to the appropriate sensor for processing
    // and distribution to processed sample clients
    dsm_sample_id_t sampid = samp->getId();
    sensorMapMutex.lock();
    if (sensorMap.size() > 0) {
	map<unsigned long,DSMSensor*>::const_iterator sensori;
	sensori = sensorMap.find(sampid);
	if (sensori != sensorMap.end()) sensori->second->receive(samp);
	else if (!(unrecognizedSamples++) % 100) {
	    atdUtil::Logger::getInstance()->log(LOG_WARNING,
		"SampleInputStream unrecognizedSamples=%d",
			unrecognizedSamples);
	}
    }
    sensorMapMutex.unlock();
    SampleSource::distribute(samp);
}

/**
 * Read the next sample. The caller must call freeReference on the
 * sample when they're done with it.
 */
Sample* SampleInputStream::readSample() throw(atdUtil::IOException)
{
    // user probably won't mix the two readSample methods on one stream,
    // but if they do, checking for non-null samp here should make things work.
restart:
    if (!samp) {
	SampleHeader header;
	while (iostream->available() < header.getSizeOf()) {
	    iostream->read();
	    if (iostream->isNewFile()) {
		inputHeader.check(iostream);
	    }
	}

	iostream->read(&header,header.getSizeOf());
	if (header.getType() >= UNKNOWN_ST) {
	    unrecognizedSamples++;
	    samp = dsm::getSample((sampleType)CHAR_ST,
		header.getDataByteLength());
	}
	else
	    samp = dsm::getSample((sampleType)header.getType(),
		header.getDataByteLength());

	samp->setTimeTag(header.getTimeTag());
	samp->setId(header.getId());
	leftToRead = samp->getDataByteLength();
	dptr = (char*) samp->getVoidDataPtr();
    }
    while (leftToRead > 0) {
	size_t len = iostream->read(dptr, leftToRead);
	if (iostream->isNewFile()) {
	    iostream->putback(dptr,len);
	    samp->freeReference();
	    samp = 0;
	    inputHeader.check(iostream);
	    goto restart;
	}
	dptr += len;
	leftToRead -= len;
	if (leftToRead == 0) break;
    }
    Sample* tmp = samp;
    samp = 0;
    return tmp;
}

/*
 * process <input> element
 */
void SampleInputStream::fromDOMElement(const DOMElement* node)
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
        }
    }

    // process <socket>, <fileset> child elements (should only be one)

    int niochan = 0;
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;

	iochan = IOChannel::createIOChannel((const DOMElement*)child);

	iochan->fromDOMElement((DOMElement*)child);

	if (++niochan > 1)
	    throw atdUtil::InvalidParameterException(
		    "SampleInputStream::fromDOMElement",
		    "input", "must have one child element");
    }
    if (!iochan)
        throw atdUtil::InvalidParameterException(
                "SampleInputStream::fromDOMElement",
		"input", "must have one child element");
}
                                                           
DOMElement* SampleInputStream::toDOMParent(
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
                                                                                
DOMElement* SampleInputStream::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

SampleInputMerger::SampleInputMerger() :
	name("SampleInputMerger"),
	inputSorter(name + "InputSorter"),
	procSampSorter(name + "ProcSampSorter"),
	unrecognizedSamples(0)
{
    inputSorter.setLengthMsecs(250);
    procSampSorter.setLengthMsecs(250);
}

SampleInputMerger::~SampleInputMerger()
{
    if (inputSorter.isRunning()) {
	inputSorter.interrupt();
	inputSorter.join();
    }
    if (procSampSorter.isRunning()) {
	procSampSorter.interrupt();
	procSampSorter.join();
    }
}

void SampleInputMerger::addInput(SampleInput* input)
{
    if (!inputSorter.isRunning()) inputSorter.start();
#ifdef DEBUG
    cerr << "SampleInputMerger: " << input->getName() << 
    	" addSampleClient, &inputSorter=" << &inputSorter << endl;
#endif
    input->addSampleClient(&inputSorter);

    SampleTagIterator si = input->getSampleTagIterator();
    for ( ; si.hasNext(); ) {
        addSampleTag(si.next());
        inputSorter.addSampleTag(si.next());
    }
}

void SampleInputMerger::removeInput(SampleInput* input)
{
    input->removeSampleClient(&inputSorter);
}

void SampleInputMerger::addProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    sensorMapMutex.lock();
    // samples with an Id equal to the sensor Id get forwarded to
    // the sensor
    sensorMap[sensor->getId()] = sensor;
    map<SampleClient*,list<DSMSensor*> >::iterator sci = 
    	sensorsByClient.find(client);
    if (sci != sensorsByClient.end()) sci->second.push_back(sensor);
    else {
        list<DSMSensor*> sensors;
	sensors.push_back(sensor);
	sensorsByClient[client] = sensors;
    }
    sensorMapMutex.unlock();

    // add sensor processed sample tags
    SampleTagIterator si = sensor->getSampleTagIterator();
    for ( ; si.hasNext(); ) {
	const SampleTag* stag = si.next();
        addSampleTag(stag);
	procSampSorter.addSampleTag(stag);
    }

    procSampSorter.addSampleClient(client);
    sensor->addSampleClient(&procSampSorter);
    inputSorter.addSampleClient(this);

    if (!procSampSorter.isRunning()) procSampSorter.start();
}

void SampleInputMerger::removeProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    procSampSorter.removeSampleClient(client);

    // check:
    //	are there still existing clients of procSampSorter for this sensor?
    //	simple way: remove procSampSorter as sampleClient of sensor
    //		if there are no more clients of procSampSorter
    if (procSampSorter.getClientCount() == 0) {
	if (!sensor) {		// remove client for all sensors
	    sensorMapMutex.lock();
	    map<SampleClient*,list<DSMSensor*> >::iterator sci = 
		sensorsByClient.find(client);
	    if (sci != sensorsByClient.end()) {
		list<DSMSensor*>& sensors = sci->second;
		for (list<DSMSensor*>::iterator si = sensors.begin();
		    si != sensors.end(); ++si) {
		    sensor = *si;
		    sensor->removeSampleClient(client);
		    if (sensor->getClientCount() == 0)
			sensorMap.erase(sensor->getId());
		}
	    }
	    sensorMapMutex.unlock();
	}
    	else {
	    sensor->removeSampleClient(&procSampSorter);
	    if (sensor->getClientCount() == 0) {
		sensorMapMutex.lock();
		sensorMap.erase(sensor->getId());
		sensorMapMutex.unlock();
	    }
	}
	inputSorter.removeSampleClient(this);
    }
}

/*
 * Redirect addSampleClient requests to inputSorter.
 */
void SampleInputMerger::addSampleClient(SampleClient* client) throw()
{
    inputSorter.addSampleClient(client);
}

void SampleInputMerger::removeSampleClient(SampleClient* client) throw()
{
    inputSorter.removeSampleClient(client);
}


void SampleInputMerger::addSampleTag(const SampleTag* stag)
{
    sampleTags.insert(stag);
#ifdef DO_DSM_CONFIG
    const DSMConfig* dsm =
	 Project::getInstance()->findDSM(stag->getDSMId());
    list<const DSMConfig*>::const_iterator di =
	find(dsmConfigs.begin(),dsmConfigs.end(),dsm);
    if (di == dsmConfigs.end()) dsmConfigs.push_back(dsm);
#endif
}


bool SampleInputMerger::receive(const Sample* samp) throw()
{
    // pass sample to the appropriate sensor for distribution.
    dsm_sample_id_t sampid = samp->getId();

    sensorMapMutex.lock();
    map<unsigned long,DSMSensor*>::const_iterator sensori
	    = sensorMap.find(sampid);

    if (sensori != sensorMap.end()) {
	DSMSensor* sensor = sensori->second;
	sensorMapMutex.unlock();
	sensor->receive(samp);
	return true;
    }
    sensorMapMutex.unlock();

    if (!(unrecognizedSamples++) % 100) {
	atdUtil::Logger::getInstance()->log(LOG_WARNING,
	    "SampleInputStream unrecognizedSamples=%d",
		    unrecognizedSamples);
    }

    return false;
}

void SampleInputStream::addSampleTag(const SampleTag* stag)
{
    sampleTags.insert(stag);
#ifdef DO_DSM_CONFIG
    const DSMConfig* dsm =
	 Project::getInstance()->findDSM(stag->getDSMId());
    list<const DSMConfig*>::const_iterator di =
	find(dsmConfigs.begin(),dsmConfigs.end(),dsm);
    if (di == dsmConfigs.end()) dsmConfigs.push_back(dsm);
#endif
}

/*
 * Constructor, with a IOChannel (which may be null).
 */
SortedSampleInputStream::SortedSampleInputStream(IOChannel* iochannel):
    SampleInputStream(iochannel),
    sorter1(0),sorter2(0),
    heapMax(10000000),heapBlock(false),
    sorterLengthMsecs(250)
{
}

/*
 * Copy constructor, with a new IOChannel.
 */
SortedSampleInputStream::SortedSampleInputStream(const SortedSampleInputStream& x,IOChannel* iochannel):
	SampleInputStream(x,iochannel),
	sorter1(0),sorter2(0),
	heapMax(x.heapMax),heapBlock(x.heapBlock),
	sorterLengthMsecs(x.sorterLengthMsecs)
{
}

/*
 * Clone myself, with a new IOChannel.
 */
SortedSampleInputStream* SortedSampleInputStream::clone(IOChannel* iochannel)
{
    return new SortedSampleInputStream(*this,iochannel);
}

SortedSampleInputStream::~SortedSampleInputStream()
{
    // cerr << "~SortedSampleInputStream" << endl;
    delete sorter1;
    delete sorter2;
}

void SortedSampleInputStream::addSampleClient(SampleClient* client) throw()
{
    if (!sorter1) {
        sorter1 = new SampleSorter("Sorter1");
	sorter1->setLengthMsecs(getSorterLengthMsecs());
	sorter1->setHeapBlock(getHeapBlock());
	sorter1->setHeapMax(getHeapMax());
    }
    SampleInputStream::addSampleClient(sorter1);
    sorter1->addSampleClient(client);
    if (!sorter1->isRunning()) sorter1->start();
}

void SortedSampleInputStream::removeSampleClient(SampleClient* client) throw()
{
    if (sorter1) {
        sorter1->removeSampleClient(client);
	SampleInputStream::removeSampleClient(sorter1);
    }
}
void SortedSampleInputStream::addProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    sensorMapMutex.lock();
    sensorMap[sensor->getId()] = sensor;
    map<SampleClient*,list<DSMSensor*> >::iterator sci = 
    	sensorsByClient.find(client);
    if (sci != sensorsByClient.end()) sci->second.push_back(sensor);
    else {
        list<DSMSensor*> sensors;
	sensors.push_back(sensor);
	sensorsByClient[client] = sensors;
    }
    sensorMapMutex.unlock();

    if (!sorter2) {
        sorter2 = new SampleSorter("Sorter2");
	sorter2->setLengthMsecs(getSorterLengthMsecs());
	sorter2->setHeapBlock(getHeapBlock());
	sorter2->setHeapMax(getHeapMax());
    }

    sensor->addSampleClient(sorter2);
    if (!sorter2->isRunning()) sorter2->start();

    SampleTagIterator si = sensor->getSampleTagIterator();
    for ( ; si.hasNext(); ) {
	const SampleTag* stag = si.next();
        addSampleTag(stag);
	sorter2->addSampleTag(stag,client);
    }
}

void SortedSampleInputStream::removeProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    if (!sensor) {		// remove client for all sensors
	sensorMapMutex.lock();
	map<SampleClient*,list<DSMSensor*> >::iterator sci = 
	    sensorsByClient.find(client);
	if (sci != sensorsByClient.end()) {
	    list<DSMSensor*>& sensors = sci->second;
	    for (list<DSMSensor*>::iterator si = sensors.begin();
	    	si != sensors.end(); ++si) {
		sensor = *si;
		sensor->removeSampleClient(sorter2);
		if (sensor->getClientCount() == 0)
		    sensorMap.erase(sensor->getId());
	    }
	}
	sensorMapMutex.unlock();
    }
    else {
        sensor->removeSampleClient(sorter2);
	if (sensor->getClientCount() == 0)
	    sensorMap.erase(sensor->getId());
    }
    if (sorter2) sorter2->removeSampleClient(client);
}

void SortedSampleInputStream::flush() throw()
{
    if (sorter1) sorter1->finish();
    if (sorter2) sorter2->finish();
}


void SortedSampleInputStream::close() throw(atdUtil::IOException)
{
    if (sorter1) {
        if (sorter1->isRunning()) sorter1->interrupt();
	sorter1->join();
    }
    if (sorter2) {
        if (sorter2->isRunning()) sorter2->interrupt();
	sorter2->join();
    }
}
void SortedSampleInputStream::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    SampleInputStream::fromDOMElement(node);
    XDOMElement xnode(node);
    const string& elname = xnode.getNodeName();
    if(node->hasAttributes()) {
    // get all the attributes of the node
        DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
	    if (!aname.compare("sorterLength")) {
	        istringstream ist(aval);
		int len;
		ist >> len;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(
		    	"SortedSampleOutputStream",
			attr.getName(),attr.getValue());
		setSorterLengthMsecs(len);
	    }
	}
    }
}

