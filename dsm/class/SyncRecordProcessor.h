
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_SYNCRECORDPROCESSOR_H
#define DSM_SYNCRECORDPROCESSOR_H

#include <SampleIOProcessor.h>
#include <SyncRecordGenerator.h>
#include <SampleSorter.h>

#include <vector>
#include <map>
#include <string>

namespace dsm {

class DSMSensor;

class SyncRecordProcessor: public SampleIOProcessor
{
public:
    
    SyncRecordProcessor();

    virtual ~SyncRecordProcessor();

    SampleIOProcessor* clone() const;

    // void setDSMConfig(const DSMConfig* val);

    // void setDSMService(const DSMService* val);

    virtual bool singleDSM() const { return false; }

    void connect(SampleInput* input) throw(atdUtil::IOException);
    
    void disconnect(SampleInput* input) throw(atdUtil::IOException);

    void connected(SampleOutput* output) throw();

    void disconnected(SampleOutput* output) throw();

    void newFileCallback(dsm_time_t thead) throw();

protected:
    void init() throw();

    void scanSensors(const std::list<DSMSensor*>& sensors);

    void allocateRecord(dsm_time_t timetag);

protected:

    SampleSorter sorter;

    SyncRecordGenerator generator;

    bool initialized;
    atdUtil::Mutex initMutex;

};

}

#endif
