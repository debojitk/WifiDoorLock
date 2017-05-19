/*
 * WSClientWrapper.cpp
 *
 *  Created on: May 6, 2017
 *      Author: DK
 */

#include "WSClientWrapper.h"
#include "ClientManager.h"
#include "debugmacros.h"
WSClientWrapper::WSClientWrapper() {
	// TODO Auto-generated constructor stub
}

WSClientWrapper::~WSClientWrapper() {
	// TODO Auto-generated destructor stub
}

void WSClientWrapper::setClientManager(ClientManager* clientManager) {
	this->clientManager=clientManager;
}

WiFiClient* WSClientWrapper::getTCPClient() {
	return this->_client.tcp;
}

void WSClientWrapper::runCbEvent(WStype_t type, uint8_t * payload, size_t length) {
	if(clientManager){
		DEBUG_PRINTF("runCbEvent: received message of size %d, type:%d \n", length, type);
		if(this->getTCPClient()!=NULL){
			DEBUG_PRINTLN("runCbEvent: setting device ip");
			this->_remoteIp=this->getTCPClient()->remoteIP();
			DEBUG_PRINTLN(_remoteIp);
		}
		clientManager->webSocketEvent(this, type, payload, length);
		DEBUG_PRINTLN("runCbEvent: completed");

	}
}
