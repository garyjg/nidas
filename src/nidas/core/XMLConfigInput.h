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


#ifndef NIDAS_CORE_XMLCONFIGINPUT_H
#define NIDAS_CORE_XMLCONFIGINPUT_H

#include <nidas/util/McSocket.h>
#include <nidas/core/Datagrams.h>

namespace nidas { namespace core {

class XMLConfigInput: public nidas::util::McSocket<nidas::util::Socket>
{
public:
    XMLConfigInput()
    {
        setRequestType(XML_CONFIG);
    }

protected:
};

}}	// namespace nidas namespace core

#endif
