/*
 * ClientManager.cpp
 *
 *  Created on: Apr 8, 2017
 *      Author: DK
 */

#include "debugmacros.h"
#include "ClientManager.h"
#include "DeviceClient.h"
#include "WebSocketsClient.h"
#include "WSClientWrapper.h"


char ClientManager::_deviceId[16];
char ClientManager::_deviceKey[8];



ClientManager::ClientManager() {
	// TODO Auto-generated constructor stub
	strcpy(_deviceId,DEFAULT_DEVICE_ID);
	_previousTimeStamp=millis();
}

void ClientManager::setup(void (*cbFunction)(CommandData &, WSClientWrapper *)) {
	DEBUG_PRINT(F("Starting clientmanager setup"));
	_clientStore.load(PAIRING_CONFIG_FILE);
	if(_clientStore.getCurrentSize()>0){
#ifdef DEBUG
		_clientStore.print();
#endif
		String strId=_clientStore.get("device_id");
		if(!strId.equals("")){
			strcpy(_deviceId,(char *)strId.c_str());
		}else{
			strcpy(_deviceId,DEFAULT_DEVICE_ID);
			_clientStore.store(PAIRING_CONFIG_FILE);
		}
		String strKey=_clientStore.get("device_key");
		if(!strKey.equals("")){
			strcpy(_deviceKey,(char *)strKey.c_str());
		}else{
			unsigned short key=(unsigned short)millis();//pairing id, should be returned in response
			String str(key,10);
			_clientStore.put("device_key", str);
			strcpy(_deviceKey, str.c_str());
			_clientStore.store(PAIRING_CONFIG_FILE);
		}

	}else{
		_clientStore.put("device_id", DEFAULT_DEVICE_ID);
		strcpy(_deviceId,DEFAULT_DEVICE_ID);
		unsigned short key=(unsigned short)millis();//pairing id, should be returned in response
		String str(key,10);
		_clientStore.put("device_key", str);
		_clientStore.store(PAIRING_CONFIG_FILE);
		strcpy(_deviceKey, str.c_str());
	}
	DEBUG_PRINTF_P("Default Deviceid is: %s, deviceKey is: %s\n", _deviceId, _deviceKey);
	//setting up udp broadcast socket
	_udpBroadcastClient.client.begin(LOCAL_UDP_PORT);
	_udpBroadcastClient.socket.ip=~WiFi.subnetMask() | WiFi.gatewayIP();
	_udpBroadcastClient.socket.port=REMOTE_UDP_PORT;
	_udpBroadcastClient.socket.connected=false;
	setCommandReceiveCallback(cbFunction);
}

ClientManager::~ClientManager() {
}


//TODO: to use MAC address as key
boolean ClientManager::pair() {
	char randBuf[8];
	DEBUG_PRINTLN(F("Enter pairing mode"));
	boolean retVal=false;
	//1. Request from device: UDP_PAIR_BROADCAST:deviceId:deviceKeyChallenge
	CommandData sendCommand, receiveCommand;
	unsigned short pairingId=(unsigned short)millis();//pairing id, should be returned in response

	sendCommand.setCommand(commands[UDP_PAIR_BROADCAST]);
	sendCommand.setPhoneId(_deviceId);
	sendCommand.setPhoneKey(itoa(pairingId, randBuf, 10));

	//broadcast pairing code
	boolean udpSendResp=udpTranscieve(_udpBroadcastClient, sendCommand, receiveCommand);
	//2. Response from phone: UDP_PAIR_BROADCAST:phoneId:phoneKey:ACK:deviceKeyChallenge
	if(udpSendResp){
		if(receiveCommand.getResponse()==true &&
				receiveCommand.getError()==false &&
				pairingId==atoi(receiveCommand.getData())){
			//correct challenge response
			//3. send UDP_PAIR_BROADCAST_ACCEPT:deviceId, deviceKey
			sendCommand.setCommand(commands[UDP_PAIR_BROADCAST_ACCEPT]);
			sendCommand.setPhoneId(_deviceId);
			sendCommand.setPhoneKey(_deviceKey);
			udpSendResp=udpTranscieve(_udpBroadcastClient, sendCommand, receiveCommand, false);
			if(udpSendResp){
				if(receiveCommand.getResponse()==true &&
						receiveCommand.getError()==false &&
						strcmp(_deviceKey, receiveCommand.getData())==0){
					if(strlen(receiveCommand.getPhoneId())>0 && strlen(receiveCommand.getPhoneKey())>0){
						DEBUG_PRINTF_P("Pairing: PhoneId: %s, PhoneKey: %s\n", receiveCommand.getPhoneId(), receiveCommand.getPhoneKey());
						//store pairing info in store
						_clientStore.put(String(receiveCommand.getPhoneId()), String(receiveCommand.getPhoneKey()));
						_clientStore.store(PAIRING_CONFIG_FILE);
						_clientStore.print();
						DEBUG_PRINTLN(F("Pairing successful"));
						//creating live device
						boolean deviceCreated=createDevice(receiveCommand.getPhoneId(),receiveCommand.getPhoneKey(), receiveCommand.getData(),_udpBroadcastClient.client.remoteIP());
						retVal=deviceCreated;
					}
				}
			}
		}
	}else{
		DEBUG_PRINTLN(F("No pairable device found"));
	}
	DEBUG_PRINTLN(F("Exiting pairing mode"));
	return retVal;
}

void ClientManager::unpair(char *clientId) {
	//remove from current device list
	DEBUG_PRINTF_P("Unpairing device: %s", clientId);
	int size=_clientList.size();
	for(int i=0;i<size;i++){
		DeviceClient *device=_clientList.get(i);
		if(strcmp(device->getClientId(),_currentClient->getClientId())==0){
			_clientList.remove(i);
			break;
		}
	}
	//remove from clientStore
	_clientStore.remove(String(_currentClient->getClientId()));
	_clientStore.store();
	_currentClient=NULL;
}

boolean ClientManager::createDevice(char * phoneId, char * phoneKey, char *sessionId, IPAddress phoneIp) {
	DEBUG_PRINTF_P("Creating device: %s, %s, IP:", phoneId, phoneKey);DEBUG_PRINTLN(phoneIp);

	DeviceClient *deviceClient=getDeviceClient(phoneId, phoneKey);
	if(deviceClient!=NULL && strcmp(deviceClient->getSessionId(), sessionId)==0){
		DEBUG_PRINTLN(F("device already connected in same session, device is not created"));
		return false;
	}
	if(deviceClient!=NULL){
		DEBUG_PRINTLN(F("device already connected, so remove the device"));
		remove(deviceClient);
	}
	deviceClient=new DeviceClient();
	deviceClient->setConnected(true);
	deviceClient->setClientId(phoneId);
	deviceClient->setClientPairingKey(phoneKey);
	deviceClient->setSessionId(sessionId);
	deviceClient->setRemoteIp(phoneIp);
	deviceClient->getWsClient().setClientManager(this);
	//deviceClient->getWsClient().beginSSL(phoneIp.toString(), REMOTE_HEARTBEAT_PORT, "/", "0a e7 9b bf cb 8a 5a 74 2d 43 e7 bc b7 02 cf 95 a1 19 77 c8");
	char deviceInfo[30];
	strcpy(deviceInfo, _deviceId);
	strcat(deviceInfo,":");
	strcat(deviceInfo,_deviceKey);
	deviceClient->getWsClient().begin(phoneIp.toString(), REMOTE_HEARTBEAT_PORT, deviceInfo);
	_clientList.add(deviceClient);
	_connectedDeviceCount++;
	DEBUG_PRINTLN(F("Device successfully created"));
	DEBUG_PRINT(F("Free heap is: "));DEBUG_PRINTLN(ESP.getFreeHeap());
	return true;

}



void ClientManager::remove(DeviceClient * deviceCLient) {
	int size=_clientList.size();
	for(int i=0;i<size;i++){
		DeviceClient *device=_clientList.get(i);
		if(strcmp(device->getClientId(),deviceCLient->getClientId())==0){
			DEBUG_PRINTF_P("Device found for removal: %s\n", deviceCLient->getClientId());
			device->setConnected(false);
			device->getWsClient().disconnect();
			_clientList.remove(i);
			delete(device);
			_connectedDeviceCount--;
			DEBUG_PRINT(F("Free heap is: "));DEBUG_PRINTLN(ESP.getFreeHeap());
			break;
		}
	}
}


boolean ClientManager::isPaired(CommandData &commandData){
	boolean retVal=false;
	if(strlen(commandData.getPhoneId())>0 && strlen(commandData.getPhoneKey())>0){
		String pKey=_clientStore.get(String(commandData.getPhoneId()));
		if(pKey.length()>0 && strcmp(pKey.c_str(), commandData.getPhoneKey())==0){
			retVal=true;
		}
	}
	DEBUG_PRINT(F("isPaired="));DEBUG_PRINTLN(retVal);
	return retVal;
}

void ClientManager::encryptRequest() {
}

/**
 * notify is handled via broadcast socket
 */
void ClientManager::notify() {

	char notifyCommand[64];
	strcpy(notifyCommand, commands[NOTIFY]);
	strcat(notifyCommand, ":");
	strcat(notifyCommand, _deviceId);
	strcat(notifyCommand, ":");
	strcat(notifyCommand, _deviceKey);
	int size=_clientList.size();
	for(int i=0;i<size;i++){
		DeviceClient *device=_clientList.get(i);
		if(device!=NULL){
			device->getWsClient().sendTXT(notifyCommand);
		}
	}
}

/**
 * This method processes all the data received on broadcast port, related to connect, pair, notify etc
 */
boolean ClientManager::processBroadcastData() {
	boolean retVal=false;
	//TODO: convert char * to static and remove delete call
	char * responseBuffer=new char[256];
	int readBytes=receiveUDPCommand(_udpBroadcastClient, responseBuffer, 256);
	if(readBytes>0){
		CommandData commandData;
		boolean parsed=commandData.parseCommandString(responseBuffer);
		if(parsed && isPaired(commandData)){
            /**
             * Connection handling
             * 1. If device sends UDP_CONN_BC_REQUEST_DEVICE
             *      then its a device initiated request, and connection established in two step handshaking
             *      phone sends UDP_CONN_BC_RESPONSE_DEVICE
             *      device starts web socket connection
             *      phone receives new web socket connection request
             *      Connection gets established.
             * 2. If phone sends UDP_CONN_BC_REQUEST_PHONE
             *      then its a phone initiated request, and connection established in three step handshaking
             *      device sends UDP_CONN_BC_RESPONSE_PHONE
             *      phone responds back with UDP_CONN_BC_RESPONSE_PHONE:ACK
             *      device starts web socket connection
             *      phone receives new web socket connection request
             *      Connection gets established.
             */
			if(strcmp(commands[UDP_CONN_BC_REQUEST_PHONE],commandData.getCommand())==0){
				//p.i.c
				DeviceClient *devClient=getDeviceClient(commandData.getPhoneId(), commandData.getPhoneKey());
				if(devClient!=NULL && strcmp(devClient->getSessionId(), commandData.getData())==0){
					DEBUG_PRINTLN(F("device already connected in same session"));
				}else{
					CommandData response;
					response.setCommand(commands[UDP_CONN_BC_RESPONSE_PHONE]);
					char responseArr[64];
					response.buildCommandString(responseArr);
					sendUDPCommand(responseArr,_udpBroadcastClient.client, _udpBroadcastClient.client.remoteIP(), REMOTE_UDP_PORT);
				}
			}else if(strcmp(commands[UDP_CONN_BC_RESPONSE_PHONE],commandData.getCommand())==0){
				//p.i.c
				//if(if ack then create device else do nothing)
				if(commandData.getResponse()){
					if(!commandData.getError()){
						createDevice(commandData.getPhoneId(), commandData.getPhoneKey(), commandData.getData(),_udpBroadcastClient.client.remoteIP());
					}else{
						//do nothing
					}
				}
			}else if(strcmp(commands[UDP_CONN_BC_RESPONSE_DEVICE],commandData.getCommand())==0){
				//d.i.c
				//UDP_CONN_BC_RESPONSE_DEVICE:phoneId:phoneKey
				createDevice(commandData.getPhoneId(), commandData.getPhoneKey(), commandData.getData(),_udpBroadcastClient.client.remoteIP());
			}
		}else{
			//dont send any response
			INFO_PRINTF_P("Device not registered: %s, %s\n", commandData.getPhoneId(), commandData.getPhoneKey());
		}
	}
	delete []responseBuffer;
	return retVal;
}



/**
 * It would process all internal commands that it can process, all other commands would be parsed and returned back to caller for its processing
 */
boolean ClientManager::update() {
	//0. poll the udpBroadcastClient to see any incoming connection
	processBroadcastData();
	//1. Poll all the udpClients to check if any data have arrived or not
	boolean retVal=false;
	if(_deviceToRemove){
		DeviceClient *_client=NULL;
		_client=getDeviceClient(_removeIP);
		if(_client!=NULL){
			remove(_client);
		}
		_deviceToRemove=false;
	}
	int size=_clientList.size();
	for(int i=_nextSocketToPoll;i<size;i++){
		DeviceClient *clientDevice=_clientList.get(i);
		if(clientDevice!=NULL && clientDevice->getConnected()){
			clientDevice->getWsClient().loop();
			_nextSocketToPoll++;
			if(_nextSocketToPoll==size){
				_nextSocketToPoll=0;
			}
		}
	}
	return retVal;
}

boolean ClientManager::initializeUDPConnection() {
	char _requestBuffer[256];
	boolean retVal=false;
	if(_connectedDeviceCount<=0){
		uint32_t currentTimeStamp=millis();
		if(currentTimeStamp-_previousTimeStamp>TCP_IP_PING_DELAY){
			_previousTimeStamp=currentTimeStamp;

#ifdef DEBUG
			DEBUG_PRINT(F("udp broadcast search message sent, IP: "));DEBUG_PRINT(_udpBroadcastClient.socket.ip);DEBUG_PRINT(F(":"));DEBUG_PRINTLN(_udpBroadcastClient.socket.port);
#endif
			CommandData commandData;
			//DEBUG_PRINTF_P("Input Command String is: %s\n", commands[UDP_CONNECT_BC_REQUEST]);
			commandData.setCommand(commands[UDP_CONN_BC_REQUEST_DEVICE]);
			commandData.setPhoneId(_deviceId);
			commandData.setPhoneKey(_deviceKey);
			commandData.buildCommandString(_requestBuffer);
			sendUDPCommand(_requestBuffer,_udpBroadcastClient);
		}
		processBroadcastData();
	}else{
		retVal= true;
	}
	return retVal;
}

void ClientManager::disconnectTCPIPConnection(TcpSocket &tcpSocketClient) {
	if(tcpSocketClient.client.status()){
		tcpSocketClient.client.stop();
		DEBUG_PRINTLN(F("TCPIP connection dropped"));
	}
	tcpSocketClient.socket.connected=false;
}

boolean ClientManager::establishTCPIPConnection(TcpSocket &tcpSocketClient) {
	DEBUG_PRINT(F("Attempt tcpip connection, current status: "));	DEBUG_PRINTLN(tcpSocketClient.client.status());
	if(tcpSocketClient.client.status()==0){
		if (!tcpSocketClient.client.connect(tcpSocketClient.socket.ip, tcpSocketClient.socket.port)) {
			DEBUG_PRINT(F("tcp/ip connection failed on IP"));	DEBUG_PRINT(tcpSocketClient.socket.ip);DEBUG_PRINT(F(":"));	DEBUG_PRINTLN(tcpSocketClient.socket.port);
			tcpSocketClient.socket.connected=false;
		}else{
			tcpSocketClient.socket.connected=true;
			DEBUG_PRINTLN(F("tcpIpSocketConnected=true"));
		}
	}
	return tcpSocketClient.socket.connected;
}

int ClientManager::sendUDPCommand(const char* command, WiFiUDP &client, IPAddress ip, uint16_t port) {
	DEBUG_PRINT(F("sendUDPCommand: "));DEBUG_PRINT(command);DEBUG_PRINT(F(", IP: "));DEBUG_PRINTLN(ip);
	client.beginPacket(ip, port);
	client.write(command);
	int udpSendResp=client.endPacket();
	return udpSendResp;

}

int ClientManager::sendUDPCommand(const char* command, UdpSocket &socket) {
	return sendUDPCommand(command, socket.client, socket.socket.ip, socket.socket.port);
}

void ClientManager::sendWSCommand(char* commandData) {
	sendWSCommand(commandData, NULL);
}


void ClientManager::sendWSCommand(char* commandData, WSClientWrapper *client) {
	if(client){
		client->sendTXT(commandData);
	}else{
		_currentClient->getWsClient().sendTXT(commandData);
	}
}

/**
 * TODO createDevice encryption handshaking
 */
int ClientManager::receiveUDPCommand(UdpSocket &socket, char* resBuf, int bufferSize) {
	int packetSize=socket.client.parsePacket();
	int readBytes=0;
	if (packetSize) {
#ifdef DEBUG
		IPAddress remoteIp=socket.client.remoteIP();
		DEBUG_PRINTF_P("Polling->packet Size:%d", packetSize);DEBUG_PRINT(F(", from"));DEBUG_PRINT(remoteIp);DEBUG_PRINT(F(":"));DEBUG_PRINTLN(socket.client.remotePort());
#endif
		// read the packet into packetBufffer
		readBytes = socket.client.read(resBuf, bufferSize);
		if (readBytes > 0) {
			resBuf[readBytes] = 0;//making it \0 terminated
			DEBUG_PRINTF_P("receiveUDPCommand Content: %s\n", resBuf);
		}
	}
	return readBytes;
}

char* ClientManager::parseCommandData(char* infoStr) {
	char *commandData=strtok(infoStr, ":");
	if(commandData!=NULL){//skipping the first one
		commandData=strtok(NULL,":");
	}
	DEBUG_PRINTF_P("Command Data: %s\n", commandData);
	return commandData;
}

boolean ClientManager::udpTranscieve(UdpSocket &socketData, CommandData &sendCommandData,
		CommandData &receiveCommandData, boolean broadcast, uint16_t timeout, uint16_t repeatCount) {
	int readBytes=0;
	boolean matchFound=false;
	unsigned long endMs=millis()+timeout;
	while(endMs>millis()){
		//if extra data is there append with command separated by colon
		char tempBuffer[256];
		sendCommandData.buildCommandString(tempBuffer);
		int udpSendResp=broadcast?sendUDPCommand(tempBuffer, socketData)://broadcasting
				sendUDPCommand(tempBuffer,socketData.client, socketData.client.remoteIP(), REMOTE_UDP_PORT);//unicasting
		int udpAttemptCount=0;
		if(udpSendResp){
			while(endMs>millis()){
				char responseBuffer[256];
				if((readBytes=receiveUDPCommand(socketData,responseBuffer,255))>0){
					receiveCommandData.parseCommandString(responseBuffer);
					if(strcmp(sendCommandData.getCommand(), receiveCommandData.getCommand())==0){
						matchFound=true;
						break;
					}else{
						delay(UDP_DELAY);
					}
				}else{
					DEBUG_PRINTLN(F("No response yet..."));
					udpAttemptCount++;
					if(udpAttemptCount>=repeatCount){
						break;
					}
					delay(UDP_DELAY);
				}
			}
			if(matchFound){
				DEBUG_PRINTLN(F("match found check"));
				break;
			}
		}else{
			delay(UDP_DELAY);
		}
	}
	return matchFound;
}


boolean ClientManager::openAudioChannel() {
	_audioClient.socket.ip=_currentClient->getRemoteIp();
	_audioClient.socket.connected=false;
	_audioClient.socket.port=REMOTE_AUDIO_PORT;
	return establishTCPIPConnection(_audioClient);
}

void ClientManager::closeAudioChannel() {
	disconnectTCPIPConnection(_audioClient);
}



DeviceClient* ClientManager::getDeviceClient(char* phoneId, char* phoneKey) {
	int size=_clientList.size();
	for(int i=0;i<size;i++){
		DeviceClient *client=_clientList.get(i);
		if(strcmp(client->getClientId(), phoneId)==0 && strcmp(client->getClientPairingKey(), phoneKey)==0){
			DEBUG_PRINTF_P("Device found:%s\n", client->getClientId());
			return client;
		}
	}
	return NULL;
}

DeviceClient* ClientManager::getDeviceClient(IPAddress remoteIP) {
	int size=_clientList.size();
	for(int i=0;i<size;i++){
		DeviceClient *client=_clientList.get(i);
		if(client->getRemoteIp()==remoteIP){
			DEBUG_PRINTF_P("Device found:%s\n", client->getClientId());
			return client;
		}
	}
	return NULL;
}


/**
 * websocket callback
 */
void ClientManager::webSocketEvent(WSClientWrapper * const client, WStype_t type, uint8_t * payload, size_t length) {
	DeviceClient *_client=NULL;
	switch(type) {
	case WStype_DISCONNECTED:
		DEBUG_PRINT(F("[WSc] IP: "));DEBUG_PRINTLN(client->getRemoteIp());
		_deviceToRemove=true;
		_removeIP=client->getRemoteIp();
		DEBUG_PRINTLN(F("[WSc] Disconnected!"));
		break;
	case WStype_CONNECTED:
		DEBUG_PRINTF_P("[WSc] Connected to url: %s\n",  payload);
		_client=getDeviceClient(client->getTCPClient()->remoteIP());
		if(_client!=NULL){
			_client->setConnected(true);
		}
		DEBUG_PRINTF_P("[WSc] total device count: %d\n",  _connectedDeviceCount);

		break;
	case WStype_TEXT:
		DEBUG_PRINTF_P("[WSc] get text: %s\n", payload);
		if(_commandCallback!=0){
			CommandData commandData;
			if(commandData.parseCommandString((char *)payload)){
				_client=getDeviceClient(commandData.getPhoneId(), commandData.getPhoneKey());
				if(_client!=NULL){
					_currentClient=_client;
					(*_commandCallback)(commandData, client);
				}
			}
		}
		break;
	case WStype_BIN:
		// send data to server
		// webSocket.sendBIN(payload, lenght);
		break;
	}

}
