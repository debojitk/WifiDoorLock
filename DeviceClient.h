/*
 * DeviceClient.h
 *
 *  Created on: Apr 8, 2017
 *      Author: DK
 */

#ifndef DEVICECLIENT_H_
#define DEVICECLIENT_H_

#include "SocketData.h"
#include "Properties.h"
#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "WiFiClient.h"
#include "ESP8266WebServer.h"
#include "WiFiUdp.h"
#include <Hash.h>
#include "WSClientWrapper.h"
//class WSClientWrapper;
class DeviceClient {
public:

	DeviceClient();
	virtual ~DeviceClient();

	boolean getConnected()  {
		return _connected;
	}

	void setConnected(boolean connected) {
		this->_connected = connected;
	}

	char* getClientId()  {
		return _clientId;
	}


	char* getClientPairingKey() {
		return _clientPairingKey;
	}

	void setClientId(char *clientId){
		strcpy(this->_clientId, clientId);
	}
	void setClientPairingKey(char *clientPairingKey){
		strcpy(this->_clientPairingKey, clientPairingKey);
	}


	const IPAddress& getRemoteIp() {
		return remoteIp;
	}

	void setRemoteIp(IPAddress& remoteIp) {
		this->remoteIp = remoteIp;
	}

	WSClientWrapper& getWsClient() {
		return _wsClient;
	}

	void setWsClient(WSClientWrapper& wsClient) {
		_wsClient = wsClient;
	}

	char* getSessionId(){
		return _sessionId;
	}
	void setSessionId(char * sessionId){
		strcpy(this->_sessionId, sessionId);
	}

private:
	char _clientId[16];
	char _clientPairingKey[8];
	boolean _connected;
	IPAddress remoteIp;
	WSClientWrapper _wsClient;
	char _sessionId[16];
};

#endif /* DEVICECLIENT_H_ */
