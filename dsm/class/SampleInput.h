/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef DSM_SAMPLEINPUT_H
#define DSM_SAMPLEINPUT_H


#include <SampleSource.h>
#include <IOStream.h>
#include <ConnectionRequester.h>
#include <SampleSorter.h>
#include <SampleInputHeader.h>

namespace dsm {

class DSMConfig;
class DSMSensor;

/**
 * Interface of an input SampleSource. Typically a SampleInput is
 * reading serialized samples from a socket or file, and
 * the sending them on.
 *
 */
class SampleInput: public SampleSource
{
public:

    virtual ~SampleInput() {}

    virtual std::string getName() const = 0;

    virtual atdUtil::Inet4Address getRemoteInet4Address() const = 0;

    /**
     * Client wants samples from the process() method of the
     * given DSMSensor.
     */
    virtual void addProcessedSampleClient(SampleClient*,DSMSensor*) = 0;

    virtual void removeProcessedSampleClient(SampleClient*,DSMSensor* = 0) = 0;

};

/**
 * SampleInputMerger sorts samples that are coming from one
 * or more inputs.  Samples can then be passed onto sensors
 * for processing, and then sorted again.
 *
 * SampleInputMerger makes use of two SampleSorters, inputSorter
 * and procSampleSorter.
 *
 * inputSorter is a client of one or more inputs (the text arrows
 * show the sample flow):
 *
 * input ----v
 * input --> inputSorter
 * input ----^
 *
 * After sorting the samples, inputSorter passes them onto the two
 * types of SampleClients that have registered with SampleInputMerger.
 * SampleClients that have registered with
 * SampleInputMerger::addSampleClient will receive their raw samples
 * directly from inputSorter.
 *
 * inputSorter -> sampleClients
 *
 * SampleClients that have registered with
 * SampleInputMerger::addProcessedSampleClient will receive their
 * samples indirectly:
 * 
 * inputSorter -> this -> sensor -> procSampSorter -> processedSampleClients
 *
 * inputSorter provides sorting of the samples from the various inputs.
 *
 * procSampSorter provides sorting of the processed samples.
 * Sensors are apt to create processed samples with different
 * time-tags than the input raw samples, therefore they need
 * to be sorted again.
 */
class SampleInputMerger: public SampleInput , protected SampleClient
{
public:
    SampleInputMerger();

    virtual ~SampleInputMerger();

    std::string getName() const { return name; }

    atdUtil::Inet4Address getRemoteInet4Address() const
    {
        return atdUtil::Inet4Address(INADDR_ANY);
    }

    /**
     * Add an input to be merged and sorted.
     */
    void addInput(SampleInput* input);

    void removeInput(SampleInput* input);

    /**
     * Add a SampleClient that wants samples which have been
     * merged from various inputs, sorted, processed through a
     * certain DSMSensor, and then re-sorted again.
     */
    void addProcessedSampleClient(SampleClient*,DSMSensor*);

    void removeProcessedSampleClient(SampleClient*,DSMSensor* = 0);

    /**
     * Add a SampleClient that wants samples which have been
     * merged from various inputs and then sorted.
     */
    void addSampleClient(SampleClient* client) throw();

    void removeSampleClient(SampleClient* client) throw();

    bool receive(const Sample*) throw();

    void addSampleTag(const SampleTag* stag);

    const std::set<const SampleTag*>& getSampleTags() const
    {
        return sampleTags;
    }

    /**
     * What DSMConfigs are associated with this SampleInput.
     */
    const std::list<const DSMConfig*>& getDSMConfigs() const
    {
        return dsmConfigs;
    }

protected:

    std::string name;

    std::map<unsigned long int, DSMSensor*> sensorMap;

    std::map<SampleClient*, std::list<DSMSensor*> > sensorsByClient;

    atdUtil::Mutex sensorMapMutex;

    SampleSorter inputSorter;

    SampleSorter procSampSorter;

    size_t unrecognizedSamples;

    std::set<const SampleTag*> sampleTags;

    std::list<const DSMConfig*> dsmConfigs;

};


/**
 * Extension of the interface to a SampleInput providing the
 * methods needed to establish the connection to the source
 * of samples (socket or files) and actually read samples
 * from the connection.
 */
class SampleInputReader: public SampleInput, public ConnectionRequester, public DOMable
{
public:

    virtual ~SampleInputReader() {}

    virtual void requestConnection(DSMService*)
        throw(atdUtil::IOException) = 0;

    virtual void init() throw(atdUtil::IOException) = 0;

    /**
     * Read a buffer of data, serialize the data into samples,
     * and distribute() samples to the receive() method of my SampleClients.
     * This will perform only one physical read of the underlying device
     * and so is appropriate to use when a select() has determined
     * that there is data availabe on our file descriptor.
     */
    virtual void readSamples() throw(atdUtil::IOException) = 0;

    /**
     * Blocking read of the next sample from the buffer. The caller must
     * call freeReference on the sample when they're done with it.
     */
    virtual Sample* readSample() throw(atdUtil::IOException) = 0;

    virtual void close() throw(atdUtil::IOException) = 0;

};

/**
 * An implementation of a SampleInputReader.
 *
 * The readSamples method converts raw bytes from the iochannel
 * into Samples.
 *
 * If a SampleClient has requested processed Samples via
 * addProcessedSampleClient, then SampleInputStream will pass
 * Samples to the respective DSMSensor for processing,
 * and then the DSMSensor passes the processed Samples to
 * the SampleClient:
 *
 * iochannel -> readSamples method -> DSMSensor -> SampleClient
 * 
 * If a SampleClient has requested non-processed Samples,
 * via the simple addSampleClient method, then SampleInput
 * stream passes the Samples straight to the SampleClient:
 *
 * iochannel -> readSamples method -> SampleClient
 *
 */
class SampleInputStream: public SampleInputReader
{

public:

    /**
     * Constructor.
     * @param iochannel The IOChannel that we use for data input.
     *   SampleInputStream will own the pointer to the IOChannel,
     *   and will delete it in ~SampleInputStream(). If 
     *   it is a null pointer, then it must be set within
     *   the fromDOMElement method.
     */
    SampleInputStream(IOChannel* iochannel = 0);

    /**
     * Copy constructor, with a new, connected IOChannel.
     */
    SampleInputStream(const SampleInputStream& x,IOChannel* iochannel);

    /**
     * Create a clone, with a new, connected IOChannel.
     */
    virtual SampleInputStream* clone(IOChannel* iochannel);

    virtual ~SampleInputStream();

    std::string getName() const;

    const std::set<const SampleTag*>& getSampleTags() const
    {
        return sampleTags;
    }

    void addSampleTag(const SampleTag* stag);

    void readHeader() throw(atdUtil::IOException);

    const SampleInputHeader& getHeader() const { return inputHeader; }

    void addProcessedSampleClient(SampleClient*,DSMSensor*);

    void removeProcessedSampleClient(SampleClient*,DSMSensor* = 0);

    void requestConnection(DSMService*) throw(atdUtil::IOException);

    /**
     * Implementation of ConnectionRequester::connected.
     */
    void connected(IOChannel* iochan) throw();

    atdUtil::Inet4Address getRemoteInet4Address() const;

    void init() throw();

    /**
     * Read a buffer of data, serialize the data into samples,
     * and distribute() samples to the receive() method of my
     * SampleClients and DSMSensors.
     * This will perform only one physical read of the underlying
     * IOChannel and so is appropriate to use when a select()
     * has determined that there is data available on our file
     * descriptor.
     */
    void readSamples() throw(atdUtil::IOException);

    /**
     * Read the next sample from the InputStream. The caller must
     * call freeReference on the sample when they're done with it.
     * This method may perform zero or more reads of the IOChannel.
     */
    Sample* readSample() throw(atdUtil::IOException);

    void distribute(const Sample* s) throw();

    size_t getUnrecognizedSamples() const { return unrecognizedSamples; }

    void close() throw(atdUtil::IOException);

    void newFile() throw(atdUtil::IOException);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
    	throw(xercesc::DOMException);
                                                                                
    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
    	throw(xercesc::DOMException);

protected:

    /**
     * Service that has requested my input.
     */
    DSMService* service;

    IOChannel* iochan;

    IOStream* iostream;

    std::map<unsigned long int, DSMSensor*> sensorMap;

    std::map<SampleClient*, std::list<DSMSensor*> > sensorsByClient;

    atdUtil::Mutex sensorMapMutex;

    std::set<const SampleTag*> sampleTags;

private:

    /**
     * Will be non-null if we have previously read part of a sample
     * from the stream.
     */
    Sample* samp;

    /**
     * How many bytes left to read from the stream into the data
     * portion of samp.
     */
    size_t leftToRead;

    /**
     * Pointer into the data portion of samp where we will read next.
     */
    char* dptr;

    size_t unrecognizedSamples;

    /**
     * Copy constructor.
     */
    SampleInputStream(const SampleInputStream&);

    SampleInputHeader inputHeader;

};

class SortedSampleInputStream: public SampleInputStream
{
public:
    SortedSampleInputStream(IOChannel* iochannel = 0);
    /**
     * Copy constructor, with a new, connected IOChannel.
     */
    SortedSampleInputStream(const SortedSampleInputStream& x,IOChannel* iochannel);

    virtual ~SortedSampleInputStream();

    /**
     * Create a clone, with a new, connected IOChannel.
     */
    SortedSampleInputStream* clone(IOChannel* iochannel);

    void addSampleClient(SampleClient* client) throw();

    void removeSampleClient(SampleClient* client) throw();

    void addProcessedSampleClient(SampleClient* client, DSMSensor* sensor);

    void removeProcessedSampleClient(SampleClient* client, DSMSensor* sensor = 0);

    void close() throw(atdUtil::IOException);

    void flush() throw();

    /**
     * Set the maximum amount of heap memory to use for sorting samples.
     * @param val Maximum size of heap in bytes.
     * @see SampleSorter::setHeapMax().
     */
    void setHeapMax(size_t val) { heapMax = val; }

    size_t getHeapMax() const { return heapMax; }

    /**
     * @param val If true, and heapSize exceeds heapMax,
     *   then wait for heapSize to be less then heapMax,
     *   which will block any SampleSources that are inserting
     *   samples into this sorter.  If false, then discard any
     *   samples that are received while heapSize exceeds heapMax.
     * @see SampleSorter::setHeapBlock().
     */
    void setHeapBlock(bool val) { heapBlock = val; }

    bool getHeapBlock() const { return heapBlock; }

    /**
     * Set length of SampleSorter, in milliseconds.
     */
    void setSorterLengthMsecs(int val)
    {
        sorterLengthMsecs = val;
    }

    int getSorterLengthMsecs() const
    {
        return sorterLengthMsecs;
    }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);


private:

    SampleSorter *sorter1;

    SampleSorter *sorter2;

    size_t heapMax;

    bool heapBlock;

    /**
     * No copying.
     */
    SortedSampleInputStream(const SortedSampleInputStream&);

    /**
     * No assignment.
     */
    SortedSampleInputStream& operator=(const SortedSampleInputStream&);

    /**
     * Length of SampleSorter, in milli-seconds.
     */
    int sorterLengthMsecs;

};

}

#endif
