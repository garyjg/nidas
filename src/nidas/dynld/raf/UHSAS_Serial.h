// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3650 $

    $LastChangedDate: 2007-01-31 16:00:23 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3650 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/UHSAS_Serial.h $

*/

#ifndef NIDAS_DYNLD_RAF_UHSAS_SERIAL_H
#define NIDAS_DYNLD_RAF_UHSAS_SERIAL_H

#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/util/EndianConverter.h>

#include <iostream>

namespace nidas { namespace dynld { namespace raf {

/**
 * A class for reading the UHSAS probe.  This appears to be an updated PCASP.
 * RS-232 @ 115,200 baud.
 */
class UHSAS_Serial : public DSMSerialSensor
{
public:

  UHSAS_Serial();

  ~UHSAS_Serial();

  void fromDOMElement(const xercesc::DOMElement* node)
      throw(nidas::util::InvalidParameterException);

  void sendInitString() throw(nidas::util::IOException);

  bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

  void addSampleTag(SampleTag* tag)
        throw(nidas::util::InvalidParameterException);

    void setSendInitBlock(bool val)
    {
        _sendInitBlock = val;
    }

    bool getSendInitBlock() const
    {
        return _sendInitBlock;
    }

    static unsigned const char* findMarker(unsigned const char* ip,unsigned const char* eoi,
        unsigned char* marker, int len);

private:

  static const nidas::util::EndianConverter * fromLittle;

  /**
   * Total number of floats in the processed output sample.
   */
  int _noutValues;

  /**
   * Number of histogram bins to be read. Probe puts out 100
   * histogram values, but it appears that largest bin is not to be used,
   * so there are 99 usable bins on this probe.
   * To be compatible with old datasets, the XML may specify 100 bins, and
   * first bin will be zeroed.
   */
  int _nValidChannels;

  /**
   * Number of housekeeping channels.  9 of 12 possible are unpacked.
   */
  int _nHousekeep;

  /**
   * Housekeeping scale factors.
   */
  float _hkScale[12];

  /**
   * UHSAS sample-rate, currently used for scaling the sum of the bins.
   */
  float _sampleRate;

  bool _sendInitBlock;

  int _nOutBins;

  bool _sumBins;

  unsigned int _nDataErrors;

  /**
   * sample period in microseconds.
   */
  int _dtUsec;

  /**
   * number of stitch sequences encountered.
   */
  unsigned int _nstitch;

  /**
   * Number of times that the number of bytes between the histogram markers
   * (ffff04 and ffff05) exceeds the expected number of 200.
   */
  unsigned int _largeHistograms;

  unsigned int _totalHistograms;

};

}}}	// namespace nidas namespace dynld raf

#endif
