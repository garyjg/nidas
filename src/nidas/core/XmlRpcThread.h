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
