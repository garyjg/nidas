/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <SyncRecordOutput.h>
#include <SyncRecordServiceRequestor.h>
#include <DSMConfig.h>
#include <DSMSerialSensor.h>
#include <DSMTime.h>

#include <atdUtil/Logger.h>

#include <iostream>
#include <list>
#include <map>

using namespace dsm;
using namespace std;

CREATOR_ENTRY_POINT(SyncRecordOutput)

SyncRecordOutput::SyncRecordOutput()
{
}

SyncRecordOutput::~SyncRecordOutput()
{
}

void SyncRecordOutput::connect() throw(atdUtil::IOException)
{
    const list<DSMSensor*> sensors = getDSMConfig()->getSensors();

    map<int,DSMSensor*> sensorMap;

    for (list<DSMSensor*>::const_iterator si = sensors.begin();
    	si != sensors.end(); ++si) {
	DSMSensor* sensor = *si;
	const list<const Variable*>& vars = sensor->getVariables();
	DSMSerialSensor* ssensor = dynamic_cast<DSMSerialSensor*>(sensor);
	if (ssensor)
	{
	}
    }

    // If the outputStream is non-null, then we're already
    // connected, most likely to a fileset.
    if (!outputStream) {
	atdUtil::ServerSocket servsock;
	SyncRecordServiceRequestor requestor(servsock.getLocalPort());
	requestor.setSocketAddress(getSocketAddress());
	requestor.start();

	cerr << "accepting on " <<
	    servsock.getInet4SocketAddress().toString() << endl;
	atdUtil::Socket sock = servsock.accept();      // throws IOException
	cerr << "accepted connection, local addr=" <<
		sock.getLocalInet4SocketAddress().toString() << 
		" remote addr=" << sock.getInet4SocketAddress().toString() << 
		endl;

	servsock.close();

	cerr << "canceling requestor" << endl;
	try {
	    requestor.cancel();
	    requestor.join();
	}
	catch (const atdUtil::Exception& e)
	{
	    atdUtil::Logger::getInstance()->log(LOG_ERR,"%s: %s",
	    	requestor.getName().c_str(),e.what());
	}
	cerr << "requestor joined" << endl;

	setSocket(sock);
    }
}

bool SyncRecordOutput::receive(const Sample *samp)
         throw(SampleParseException, atdUtil::IOException)
{
    return true;
}

