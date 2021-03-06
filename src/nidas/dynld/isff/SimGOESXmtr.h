// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#ifndef NIDAS_DYNLD_ISFF_SIMGOESXMTR_H
#define NIDAS_DYNLD_ISFF_SIMGOESXMTR_H

#include "GOESXmtr.h"
#include <nidas/core/IOChannel.h>
#include <nidas/core/SampleTag.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>

#include <string>
#include <iostream>
#include <vector>

namespace nidas { namespace dynld { namespace isff {

using namespace nidas::core;

/**
 * An IOChannel implementation to simulate a GOES transmitter.
 */
class SimGOESXmtr: public GOESXmtr {

public:

    /**
     * Constructor.
     */
    SimGOESXmtr();

    /**
     * Copy constructor.
     */
    SimGOESXmtr(const SimGOESXmtr&);

    /**
     * Destructor.
     */
    ~SimGOESXmtr();

    /**
     * Clone invokes default copy constructor.
     */
    SimGOESXmtr* clone() const { return new SimGOESXmtr(*this); }

    /**
     * Set the RF baud rate
     * @param val RF baud, in bits/sec.
     */
    void setRFBaud(long val) throw(nidas::util::InvalidParameterException)
    {
        _rfBaud = val;
    }

    int getRFBaud() const
    {
        return _rfBaud;
    }
    

    void open() throw(nidas::util::IOException);

    /**
     * Do the actual hardware read.
     */
    size_t read(void*, size_t) throw (nidas::util::IOException)
    {
        throw nidas::util::IOException(getName(),"read","not supported");
    }

    /**
    * Do the actual hardware write.
    */
    size_t write(const void*, size_t) throw (nidas::util::IOException)
    {
        throw nidas::util::IOException(getName(),"write","not supported");
    }

    /**
    * Do the actual hardware write.
    */
    size_t write(const struct iovec*, int) throw (nidas::util::IOException)
    {
        throw nidas::util::IOException(getName(),"write","not supported");
    }

    /**
     * Queue a sample for writing to a GOES transmitter.
    */
    void transmitData(const nidas::util::UTime& at,
    	int configid,const Sample*) throw (nidas::util::IOException);

    unsigned long checkId() throw(nidas::util::IOException);

    int checkClock() throw(nidas::util::IOException);

    void printStatus() throw();

    void reset() throw(nidas::util::IOException) {}

    void init() throw(nidas::util::IOException) {}

private:

    nidas::util::UTime _transmitQueueTime;

    nidas::util::UTime _transmitAtTime;

    nidas::util::UTime _transmitSampleTime;

    int _rfBaud;
    
};


}}}	// namespace nidas namespace dynld namespace isff

#endif
