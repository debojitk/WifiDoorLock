/*
 * WSClientWrapper.h
 *
 *  Created on: May 6, 2017
 *      Author: DK
 */
#ifndef WSCLIENTWRAPPER_H_
#define WSCLIENTWRAPPER_H_
#include <WebSocketsClient.h>

class ClientManager;
class WSClientWrapper: public WebSocketsClient {
public:
	WSClientWrapper();
	virtual ~WSClientWrapper();
	void setClientManager(ClientManager *clientManager);
	WiFiClient * getTCPClient();

	IPAddress& getRemoteIp() {
		return _remoteIp;
	}

protected:
	virtual void runCbEvent(WStype_t type, uint8_t * payload, size_t length) override;

private:
	ClientManager *clientManager=0;
	IPAddress _remoteIp;
};


#endif /* WSCLIENTWRAPPER_H_ */
