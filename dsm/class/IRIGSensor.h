/*
 ******************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-11-22 14:41:27 -0700 (Mon, 22 Nov 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/DSMSerialSensor.h $

 ******************************************************************
*/
#ifndef IRIGSENSOR_H
#define IRIGSENSOR_H

#include <dsm_serial.h>

#include <RTL_DSMSensor.h>
#include <ClockSensor.h>
#include <atdUtil/InvalidParameterException.h>

namespace dsm {
/**
 * Sensor class for controlling and recieving data from an IRIG clock.
 */
class IRIGSensor : public RTL_DSMSensor, public ClockSensor {

public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    IRIGSensor();

    ~IRIGSensor();

    /**
     * Return true if this is a ClockSensor.
     */
    bool isClock() const { return true; }

    /**
     * Implementation of ClockSensor::processClockSample method.
     */
    SampleT<dsm_sys_time_t>* processClockSample(const Sample* samp)
	    throw(SampleParseException,atdUtil::IOException);

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(atdUtil::IOException);

    /*
     * Close the device connected to the sensor.
     */
    void close() throw(atdUtil::IOException);

    /**
     * Process a raw sample.
     */
    const Sample*  process(const Sample*)
    	throw(dsm::SampleParseException,atdUtil::IOException);

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:
    const int GOOD_CLOCK_LIMIT;
    int questionableClock;

};

}

#endif
