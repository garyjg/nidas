// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/* LamsNetSensor.h
 *
 * Copyright 2007-2012 UCAR, NCAR, All Rights Reserved
 * Revisions:
 * $LastChangedRevision: $
 * $LastChangedDate:  $
 * $LastChangedBy: cjw $
 * $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynid/LamsNetSensor.h $
 */


#ifndef NIDAS_DYNLD_RAF_LAMSNETSENSOR_H
#define NIDAS_DYNLD_RAF_LAMSNETSENSOR_H

#include <iostream>
#include <iomanip>

#include <nidas/dynld/UDPSocketSensor.h>
#include <nidas/util/EndianConverter.h>

#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Sensor class supporting the NCAR/EOL Laser Air Motion Sensor (LAMS 3-beam)
 * via Ethernet UDP connection.
 */
class LamsNetSensor : public CharacterSensor
{
public:
    LamsNetSensor();
    ~LamsNetSensor();

    bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();

private:

    static const int nBeams = 4;

    static const int LAMS_SPECTRA_SIZE = 512;

    const Sample *_saveSamps[nBeams];

    size_t _unmatchedSamples;

    size_t _outOfSequenceSamples;

    /// beams come in two packets, so need to re-enter process().
    size_t _beam;

    uint32_t _prevSeqNum[nBeams];

    static const nidas::util::EndianConverter * _fromLittle;

    /** No copying. */
    LamsNetSensor(const LamsNetSensor&);

    /** No assignment. */
    LamsNetSensor& operator=(const LamsNetSensor&);
};

}}}

#endif
