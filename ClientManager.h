/*
 * ClientManager.h
 *
 *  Created on: Apr 8, 2017
 *      Author: DK
 */

#ifndef CLIENTMANAGER_H_
#define CLIENTMANAGER_H_
#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "WiFiClient.h"
#include "ESP8266WebServer.h"
#include "WiFiUdp.h"
#include "LinkedList.h"
#include "SocketData.h"
#include "Properties.h"
#include <WebSocketsClient.h>
#include "DeviceClient.h"
#define LOCAL_UDP_PORT 6666
#define REMOTE_UDP_PORT 8888
#define REMOTE_AUDIO_PORT 8086
#define REMOTE_HEARTBEAT_PORT 8087
#define UDP_CONNECT_ATTEMPT_COUNT 10
#define UDP_DELAY 1000
#define TCP_IP_PING_DELAY 5000 //5 secs


#define PAIRING_CONFIG_FILE "/paired.txt"
#define DEFAULT_DEVICE_ID "MY_DEVICE"
#define DEFAULT_DEVICE_KEY "123456"

//class DeviceClient;
class ClientManager {
public:
	ClientManager();
	virtual ~ClientManager();
	//This method should be called in paring mode
	boolean pair();
	//This method is required to unpair a paired device
	void unpair(char *clientId);
	//This method adds a client to active device list if the device is already paired
	boolean createDevice(char * phoneId, char * phoneKey, IPAddress phoneIp);
	void remove(DeviceClient * deviceCLient);
	void encryptRequest();
	void notify(uint32_t timeout, char *_responseBuffer);
	boolean update();
	boolean processBroadcastData();
	boolean startHeartbeatProcess(char* phoneId, char* phoneKey, int remoteHeartbeatPort, int remoteUdpPort);
	boolean initializeUDPConnection();
	void disconnectTCPIPConnection(TcpSocket &tcpSocketClient);
	boolean establishTCPIPConnection(TcpSocket &tcpSocketClient);
	boolean isPaired(CommandData & commandData);
	boolean openAudioChannel();
	void closeAudioChannel();
	void sendWSCommand(char *commandData, WSClientWrapper *client);
	void sendWSCommand(char *commandData);
	void setup(void (*cbFunction)(CommandData &, WSClientWrapper *));
	void webSocketEvent(WSClientWrapper *const client, WStype_t type, uint8_t * payload, size_t length);
	DeviceClient *getDeviceClient(char *phoneId, char *phoneKey);
	DeviceClient *getDeviceClient(IPAddress remoteIP);
	WiFiClient &audioSocket=_audioClient.client;
	void setCommandReceiveCallback(void (*cbFunction)(CommandData &, WSClientWrapper *)){
		_commandCallback=cbFunction;
	}

	DeviceClient* getCurrentClient()  {
		return _currentClient;
	}

	void setCurrentClient( DeviceClient* currentClient) {
		_currentClient = currentClient;
	}

	TcpSocket& getAudioClient() {
		return _audioClient;
	}

	void setAudioClient(TcpSocket& audioClient) {
		_audioClient = audioClient;
	}

	void setDeviceId(char *deviceId){
		strcpy(this->_deviceId, deviceId);
	}
	char *getDeviceId(){
		return _deviceId;
	}
private:
	LinkedList<DeviceClient *>_clientList=LinkedList<DeviceClient *>();
	Properties _clientStore=Properties(10);//to store paired devices
	DeviceClient *_currentClient=NULL;
	UdpSocket _udpBroadcastClient;
	TcpSocket _audioClient;

	uint8_t _connectedDeviceCount=0;
	uint32_t _previousTimeStamp;
	uint8_t _nextSocketToPoll=0;

	char _deviceId[16];
	char _deviceKey[8];
	const char *delim=".:";

	void (*_commandCallback)(CommandData &, WSClientWrapper *)=0;
	int sendUDPCommand(const char* command, WiFiUDP &client, IPAddress ip, uint16_t port);
	int sendUDPCommand(const char* command, UdpSocket &socket);
	int receiveUDPCommand(UdpSocket &socket, char *resBuf, int bufferSize);
	char * parseCommandData(char * infoStr);
	boolean udpTranscieve(UdpSocket &socketData, CommandData &sendCommandData, CommandData &receiveCommandData,
			boolean broadcast=true,
			uint16_t timeout=30000, uint16_t repeatCount=5);
	boolean _deviceToRemove=false;
	IPAddress _removeIP;

};
#endif /* CLIENTMANAGER_H_ */
