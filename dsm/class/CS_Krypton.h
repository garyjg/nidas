/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-09-10 07:54:23 -0600 (Sat, 10 Sep 2005) $

    $LastChangedRevision: 2879 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/class/CSAT3_Sonic.h $

*/

#ifndef CS_KRYPTON_H
#define CS_KRYPTON_H

#include <VariableConverter.h>

#include <math.h>

namespace dsm {

/**
 * A class for making sense of data from a Campbell Scientific Inc
 * CSAT3 3D sonic anemometer.
 */
class CS_Krypton: public VariableConverter
{
public:

    CS_Krypton();

    ~CS_Krypton() {}

    CS_Krypton* clone() const;

    /**
     * @param val Kw parameter from sensor calibration.
     */
    void setKw(float val)
    {
        Kw = val;
	pathLengthKw = pathLength * Kw;
    }

    float getKw() const
    {
        return Kw;
    }

    /**
     * @param val V0 value in millivolts.
     */
    void setV0(float val)
    {
        V0 = val;
	logV0 = ::log(V0);
    }

    float getV0() const
    {
        return V0;
    }

    /**
     * @param val Pathlength of sensor, in cm.
     */
    void setPathLength(float val)
    {
        pathLength = val;
	pathLengthKw = pathLength * Kw;
    }

    float getPathLength() const
    {
        return pathLength;
    }

    /**
     * @param val Bias (g/m^3) to be removed from data values.
     */
    void setBias(float val)
    {
        bias = val;
    }

    float getBias() const
    {
        return bias;
    }

    /**
     * Convert a voltage to water vapor density in g/m^3.
     */
    float convert(float volts) const;

    std::string toString() const;

    void fromString(const std::string&) 
    	throw(atdUtil::InvalidParameterException);

protected:

    float Kw;

    float V0;

    float logV0;

    float pathLength;

    float bias;

    float pathLengthKw;

};

}

#endif
