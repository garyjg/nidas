// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/dynld/isff/GroundStation.h>

#include <iostream>

using namespace nidas::core;
using namespace nidas::dynld::isff;
using namespace std;

NIDAS_CREATOR_FUNCTION_NS(isff,GroundStation)

GroundStation::GroundStation():Site()
{
}

GroundStation::~GroundStation()
{
}

