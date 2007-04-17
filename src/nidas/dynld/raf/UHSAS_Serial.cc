/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision: 3654 $

    $LastChangedDate: 2007-02-01 14:40:14 -0700 (Thu, 01 Feb 2007) $

    $LastChangedRevision: 3654 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/dynld/raf/UHSAS_Serial.cc $

*/

#include <nidas/dynld/raf/UHSAS_Serial.h>
#include <nidas/core/PhysConstants.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>
#include <nidas/util/IOTimeoutException.h>

#include <sstream>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,UHSAS_Serial)

static const unsigned char setup_pkt[] =
	{
	0xff, 0xff, 0x10, 0xff, 0xff, 0x0e, 0x01, 0xff, 0xff, 0x0a, 0x04,
	0xff, 0xff, 0x0b, 0x00, 0x19, 0xff, 0xff, 0x18, 0xfa, 0x00, 0xff,
	0xff, 0x1c, 0x88, 0x13, 0xff, 0xff, 0x19, 0x32, 0x00, 0xff, 0xff,
	0xff, 0x1d, 0x88, 0x13, 0xff, 0xff, 0x1a, 0xfa, 0x00, 0xff, 0xff,
	0x1e, 0x88, 0x13, 0xff, 0xff, 0x1b, 0x32, 0x00, 0xff, 0xff, 0x1f,
	0xc4, 0x09, 0xff, 0xff, 0x08, 0x9a, 0x77, 0xff, 0xff, 0x08, 0xa7,
	0x35, 0xff, 0xff, 0x08, 0x22, 0x22, 0xff, 0xff, 0x08, 0x00, 0x00,
	0xff, 0xff, 0x08, 0xec, 0x11, 0xff, 0xff, 0x0d, 0x03, 0xff, 0xff,
	0x0c, 0x01, 0xff, 0xff, 0x09, 0x00, 0xff, 0xff, 0x14,

	0x00, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41,
	0x00, 0x41, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41, 0x00,
	0x42, 0x00, 0x42, 0x00, 0x42, 0x00, 0x42, 0x00, 0x42, 0x00, 0x42,
	0x00, 0x42, 0x00, 0x43, 0x00, 0x43, 0x00, 0x43, 0x00, 0x44, 0x00,
	0x44, 0x00, 0x45, 0x00, 0x45, 0x00, 0x46, 0x00, 0x47, 0x00, 0x48,
	0x00, 0x49, 0x00, 0x4a, 0x00, 0x4c, 0x00, 0x4e, 0x00, 0x50, 0x00,
	0x53, 0x00, 0x56, 0x00, 0x59, 0x00, 0x5d, 0x00, 0x62, 0x00, 0x68,
	0x00, 0x6e, 0x00, 0x76, 0x00, 0x7f, 0x00, 0x89, 0x00, 0x96, 0x00,
	0xa4, 0x00, 0xb5, 0x00, 0xc8, 0x00, 0xdf, 0x00, 0xfa, 0x00, 0x1a,
	0x01, 0x3f, 0x01, 0x6a, 0x01, 0x9c, 0x01, 0xd7, 0x01, 0x1c, 0x02,
	0x69, 0x02, 0xc0, 0x02, 0x25, 0x03, 0x98, 0x03, 0x1b, 0x04, 0xaf,
	0x04, 0x57, 0x05, 0x15, 0x06, 0xe8, 0x06, 0xd5, 0x07, 0xdc, 0x08,
	0xfe, 0x09, 0x3c, 0x0b, 0x99, 0x0c, 0x15, 0x0e, 0xb4, 0x0f, 0x75,
	0x11, 0x5f, 0x13, 0x71, 0x15, 0xb4, 0x17, 0x2e, 0x1a, 0xe3, 0x1c,
	0xd9, 0x1f, 0x0e, 0x23, 0x7a, 0x26, 0x08, 0x2a, 0x9b, 0x2d, 0x10,
	0x31, 0x52, 0x34, 0x44, 0x37, 0xf2, 0x39, 0x65, 0x3c, 0xb4, 0x3e,
	0xf5, 0x40, 0x44, 0x43, 0xc2, 0x45, 0xa0, 0x48, 0x28, 0x4c, 0xac,
	0x50, 0x3d, 0x56, 0x69, 0x5c, 0x55, 0x62, 0x08, 0x67, 0x80, 0x6a,
	0x9d, 0x6c, 0xb3, 0x6d, 0xb3, 0x6d,
// Block 2.
	0xff, 0xff, 0x0b, 0x00, 0x19, 0xff, 0xff, 0x18, 0xfa, 0x00, 0xff,
	0xff, 0x1c, 0x88, 0x13, 0xff, 0xff, 0x19, 0x32, 0x00, 0xff, 0xff,
	0x1d, 0x88, 0x13, 0xff, 0xff, 0x1a, 0xfa, 0x00, 0xff, 0xff, 0x1e,
	0x88, 0x13, 0xff, 0xff, 0x1b, 0x32, 0x00, 0xff, 0xff, 0x1f, 0xc4,
	0x09, 0xff, 0xff, 0x08, 0x9a, 0x77, 0xff, 0xff, 0x08, 0xa7, 0x35,
	0xff, 0xff, 0x08, 0x22, 0x22, 0xff, 0xff, 0x08, 0x00, 0x00, 0xff,
	0xff, 0x08, 0xec, 0x11, 0xff, 0xff, 0x0d, 0x00, 0xff, 0xff, 0x0c,
	0x01, 0xff, 0xff, 0x09, 0x00, 0xff, 0xff, 0x15,

	0x00, 0x00, 0x7a, 0x00, 0x7a, 0x00, 0x7b, 0x00, 0x7b, 0x00, 0x7c,
	0x00, 0x7c, 0x00, 0x7d, 0x00, 0x7e, 0x00, 0x80, 0x00, 0x81, 0x00,
	0x83, 0x00, 0x85, 0x00, 0x87, 0x00, 0x8a, 0x00, 0x8d, 0x00, 0x90,
	0x00, 0x95, 0x00, 0x9a, 0x00, 0xa0, 0x00, 0xa6, 0x00, 0xae, 0x00,
	0xb8, 0x00, 0xc3, 0x00, 0xd0, 0x00, 0xdf, 0x00, 0xf0, 0x00, 0x05,
	0x01, 0x1d, 0x01, 0x39, 0x01, 0x5a, 0x01, 0x80, 0x01, 0xad, 0x01,
	0xe2, 0x01, 0x20, 0x02, 0x68, 0x02, 0xbc, 0x02, 0x1f, 0x03, 0x92,
	0x03, 0x1a, 0x04, 0xb8, 0x04, 0x71, 0x05, 0x49, 0x06, 0x47, 0x07,
	0x6f, 0x08, 0xca, 0x09, 0x5f, 0x0b, 0x3a, 0x0d, 0x65, 0x0f, 0xee,
	0x11, 0xe6, 0x14, 0x60, 0x18, 0x6f, 0x1c, 0x30, 0x21, 0xc0, 0x26,
	0xe5, 0x2c, 0xf6, 0x33, 0x0d, 0x3c, 0x4f, 0x45, 0xdb, 0x4f, 0xd5,
	0x5b, 0x5e, 0x69, 0x99, 0x78, 0xaa, 0x89, 0xb7, 0x9c, 0xdf, 0xb1,
	0x39, 0xc9, 0xe1, 0xe2, 0xfb, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
	0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
	0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
	0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
	0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
	0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
	0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
// Block 3.
	0xff, 0xff, 0x0b, 0x00, 0x19, 0xff, 0xff, 0x18, 0xfa, 0x00, 0xff,
	0xff, 0x1c, 0x88, 0x13, 0xff, 0xff, 0x19, 0x32, 0x00, 0xff, 0xff,
	0x1d, 0x88, 0x13, 0xff, 0xff, 0x1a, 0xfa, 0x00, 0xff, 0xff, 0x1e,
	0x88, 0x13, 0xff, 0xff, 0x1b, 0x32, 0x00, 0xff, 0xff, 0x1f, 0xc4,
	0x09, 0xff, 0xff, 0x08, 0x9a, 0x77, 0xff, 0xff, 0x08, 0xa7, 0x35,
	0xff, 0xff, 0x08, 0x22, 0x22, 0xff, 0xff, 0x08, 0x00, 0x00, 0xff,
	0xff, 0x08, 0xec, 0x11, 0xff, 0xff, 0x0d, 0x00, 0xff, 0xff, 0x0c,
	0x01, 0xff, 0xff, 0x09, 0x00, 0xff, 0xff, 0x16,

	0x00, 0x00, 0x5b, 0x00, 0x64, 0x00, 0x6f, 0x00, 0x7b, 0x00, 0x89,
	0x00, 0x9a, 0x00, 0xad, 0x00, 0xc4, 0x00, 0xdf, 0x00, 0xfe, 0x00,
	0x23, 0x01, 0x4e, 0x01, 0x80, 0x01, 0xbb, 0x01, 0xff, 0x01, 0x4f,
	0x02, 0xad, 0x02, 0x1b, 0x03, 0x9c, 0x03, 0x32, 0x04, 0xe2, 0x04,
	0xb0, 0x05, 0xa1, 0x06, 0xbb, 0x07, 0x05, 0x09, 0x87, 0x0a, 0x4a,
	0x0c, 0x5b, 0x0e, 0xc5, 0x10, 0x97, 0x13, 0xe5, 0x16, 0xc4, 0x1a,
	0x48, 0x1f, 0x92, 0x24, 0xc5, 0x2a, 0x04, 0x32, 0x7e, 0x3a, 0x69,
	0x44, 0x01, 0x50, 0x91, 0x5d, 0x75, 0x6d, 0x08, 0x80, 0xcc, 0x95,
	0x34, 0xaf, 0xfd, 0xcc, 0xc4, 0xef, 0xff, 0xfe, 0xff, 0xfe, 0xff,
	0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
	0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
	0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
	0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
	0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
	0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
	0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
	0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
	0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
	0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
// Block 4.
        0xff, 0xff, 0x0b, 0x00, 0x19, 0xff, 0xff, 0x18, 0xfa, 0x00, 0xff,
        0xff, 0x1c, 0x88, 0x13, 0xff, 0xff, 0x19, 0x32, 0x00, 0xff, 0xff,
        0x1d, 0x88, 0x13, 0xff, 0xff, 0x1a, 0xfa, 0x00, 0xff, 0xff, 0x1e,
        0x88, 0x13, 0xff, 0xff, 0x1b, 0x32, 0x00, 0xff, 0xff, 0x1f, 0xc4,
        0x09, 0xff, 0xff, 0x08, 0x9a, 0x77, 0xff, 0xff, 0x08, 0xa7, 0x35,
        0xff, 0xff, 0x08, 0x22, 0x22, 0xff, 0xff, 0x08, 0x00, 0x00, 0xff,
        0xff, 0x08, 0xec, 0x11, 0xff, 0xff, 0x0d, 0x00, 0xff, 0xff, 0x0c,
        0x01, 0xff, 0xff, 0x09, 0x00, 0xff, 0xff, 0x17,

        0x00, 0x00, 0x06, 0x0a, 0xbb, 0x0b, 0xb9, 0x0d, 0x0c, 0x10, 0xc6,
        0x12, 0xf8, 0x15, 0xb6, 0x19, 0x12, 0x1e, 0x30, 0x23, 0x2a, 0x29,
        0x2d, 0x30, 0x5a, 0x38, 0xed, 0x41, 0x2c, 0x4d, 0x45, 0x5a, 0xa0,
        0x69, 0x8e, 0x7b, 0x8b, 0x90, 0x18, 0xa9, 0xe5, 0xc5, 0x8c, 0xe7,
        0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
        0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
        0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
        0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
        0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
        0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
        0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
        0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
        0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
        0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
        0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
        0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
        0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
        0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
        0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
        0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
        0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
        0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
        0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
        0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
        0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
        0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
        0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
        0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
        0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
        0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
        0xff, 0xfe, 0xff
	};


void UHSAS_Serial::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    DSMSerialSensor::fromDOMElement(node);

}

void UHSAS_Serial::sendInitString() throw(n_u::IOException)
{
std::cerr << "UHSAS:: sendInitString\n";
    // clear whatever junk may be in the buffer til a timeout
    try {
        for (;;) {
            readBuffer(MSECS_PER_SEC / 100);
            clearBuffer();
        }
    }
    catch (const n_u::IOTimeoutException& e) {}

    n_u::UTime twrite;
    write(setup_pkt, sizeof(setup_pkt));
}

bool UHSAS_Serial::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
/*
    SampleT<float>* outs = getSample<float>(_noutValues);
    float* dout = outs->getDataPtr();
    const UHSAS_blk *input = (UHSAS_blk *) samp->getConstVoidDataPtr();



    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 1);


    // these values must correspond to the sequence of
    // <variable> tags in the <sample> for this sensor.
    *dout++ = (input->cabinChan[FREF_INDX] - 2048) * 4.882812e-3;
    *dout++ = (input->cabinChan[FTMP_INDX] - 2328) * 0.9765625;
    *dout++ = _range;
    *dout++ = fuckedUpLongFlip((char *)&input->rejDOF);
    *dout++ = fuckedUpLongFlip((char *)&input->rejAvgTrans);
    *dout++ = fuckedUpLongFlip((char *)&input->ADCoverflow);

    // DMT fucked up the word count.  Re-align long data on mod 4 boundary.
    const char * p = (char *)input->OPCchan - 2;

    for (int iout = 0; iout < _nChannels; ++iout)
    {
      *dout++ = fuckedUpLongFlip(p);
      p += sizeof(unsigned long);
    }

    results.push_back(outs);
*/
    return true;
}
