// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_XMLRPCTHREAD_H
#define NIDAS_CORE_XMLRPCTHREAD_H

#include <nidas/util/Thread.h>
#include <xmlrpcpp/XmlRpc.h>

namespace nidas { namespace core {

/**
 * A thread that provides XML-based Remote Procedure Calls
 * to web interfaces.
 */
class XmlRpcThread: public nidas::util::Thread
{
public:
    
    /** Constructor. */
    XmlRpcThread(const std::string& name);

    ~XmlRpcThread();

    void interrupt();

protected:

    XmlRpc::XmlRpcServer* _xmlrpc_server;

private:

    /** Copy not needed */
    XmlRpcThread(const XmlRpcThread&);

    /** Assignment not needed */
    XmlRpcThread& operator=(const XmlRpcThread&);
};

}}	// namespace nidas namespace core

#endif
