/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_REMOTESERIALCONNECTION_H
#define DSM_REMOTESERIALCONNECTION_H

#include <atdUtil/IOException.h>
#include <atdUtil/EOFException.h>
#include <atdUtil/Socket.h>
#include <SampleClient.h>
#include <DSMSensor.h>

namespace dsm {

class RemoteSerialConnection : public SampleClient {
public:
    RemoteSerialConnection(const atdUtil::Socket& sock, const std::string& d) :
	socket(sock),devname(d),sensor(0) {}
    virtual ~RemoteSerialConnection();

    int getFd() const { return socket.getFd(); }
    const std::string& getSensorName() const { return devname; }

    void setSensor(DSMSensor* val) {
	if (val) val->addSampleClient(this);
	else if (sensor) sensor->removeSampleClient(this);
	sensor = val;
    }

    DSMSensor* getPort() const { return sensor; }

    /**
     * Receive a sample from the DSMSensor, write data portion to socket.
     */
    bool receive(const Sample* s)
		throw(SampleParseException,atdUtil::IOException);

    /**
     * Read data from socket, write to DSMSensor.
     */
    void read() throw(atdUtil::IOException);
  
private:
    atdUtil::Socket socket;
    std::string devname;
    DSMSensor* sensor;
};

}
#endif
