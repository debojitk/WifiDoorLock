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

ClientManager::ClientManager() {
	// TODO Auto-generated constructor stub
	strcpy(_deviceId,DEFAULT_DEVICE_ID);
	_previousTimeStamp=millis();
}

void ClientManager::setup(void (*cbFunction)(CommandData, WSClientWrapper *)) {
	DEBUG_PRINT("Starting clientmanager setup");
	_clientStore.load(PAIRING_CONFIG_FILE);
	if(_clientStore.getCurrentSize()>0){
#ifdef DEBUG
		_clientStore.print();
#endif
		String str=_clientStore.get("device_id");
		if(!str.equals("")){
			strcpy(_deviceId,(char *)str.c_str());
		}else{
			strcpy(_deviceId,DEFAULT_DEVICE_ID);
		}
	}else{
		_clientStore.put("device_id", DEFAULT_DEVICE_ID);
		_clientStore.store(PAIRING_CONFIG_FILE);
	}
	DEBUG_PRINTF("Default Deviceid is: %s\n", _deviceId);
	//setting up udp broadcast socket
	_udpBroadcastClient.client.begin(LOCAL_UDP_PORT);
	_udpBroadcastClient.socket.ip=~WiFi.subnetMask() | WiFi.gatewayIP();
	_udpBroadcastClient.socket.port=REMOTE_UDP_PORT;
	_udpBroadcastClient.socket.connected=false;
	setCommandReceiveCallback(cbFunction);
}

ClientManager::~ClientManager() {
}

boolean ClientManager::pair() {
	char randBuf[8];
	DEBUG_PRINTLN("Enter pairing mode");
	boolean retVal=false;
	unsigned short pairingId=(unsigned short)millis();//pairing id, should be returned in response
	strcpy(_requestBuffer, (const char *)_deviceId);
	strcat(_requestBuffer, ":");
	strcat(_requestBuffer,itoa(pairingId, randBuf, 10));
	//broadcast pairing code
	boolean udpSendResp=udpTranscieve(UDP_PAIR_BROADCAST,_udpBroadcastClient, _requestBuffer, _responseBuffer, 10000, false);
	if(udpSendResp){
		char *respData=strtok(_responseBuffer, delim);//read and skip the command part from UDP_PAIR_BROADCAST:ACK:<pairingId>:<phoneId>:<phoneKey>
		if(respData!=NULL){
			respData=strtok(NULL,delim);//this should contain ACK
			char phoneId[16]="";
			char phoneKey[8]="";
			if(respData!=NULL){
				if(strcmp("ACK", respData)==0){//if response is ACK then only consider the response
					respData=strtok(NULL,delim);
					if(respData!=NULL){
						if(pairingId==atoi(respData)){//if device paring id is present in response then consider the response
							respData=strtok(NULL,delim);
							if(respData!=NULL){
								strcpy(phoneId,respData);
								respData=strtok(NULL,delim);
							}
							if(respData!=NULL){
								strcpy(phoneKey, respData);
								respData=strtok(NULL,delim);
							}
						}
					}
				}
			}
			if(strlen(phoneId)>0 && strlen(phoneKey)>0){
				DEBUG_PRINTF("Pairing: PhoneId: %s, PhoneKey: %s\n", phoneId, phoneKey);
				//store pairing info in store
				_clientStore.put(String(phoneId), String(phoneKey));
				_clientStore.store(PAIRING_CONFIG_FILE);
				_clientStore.print();
				DEBUG_PRINTLN("Pairing successful");
				retVal=true;
			}
		}
		strcpy(_requestBuffer, commands[UDP_PAIR_BROADCAST]);
		if(retVal==true){
			strcat(_requestBuffer, ":ACK");
		}else{
			DEBUG_PRINTLN("Pairing failed");
			strcat(_requestBuffer, ":NACK:Pairing failed");
		}
		sendUDPCommand(_requestBuffer,_udpBroadcastClient.client, _udpBroadcastClient.client.remoteIP(), REMOTE_UDP_PORT);
	}else{
		DEBUG_PRINTLN("No pairable device found");
	}
	DEBUG_PRINTLN("Exiting pairing mode");
	return retVal;
}

void ClientManager::unpair(char *clientId) {
	//remove from current device list
	DEBUG_PRINTF("Unpairing device: %s", clientId);
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

boolean ClientManager::createDevice(char * phoneId, char * phoneKey, IPAddress phoneIp) {
	DEBUG_PRINTF("Creating device: %s, %s, IP:", phoneId, phoneKey);DEBUG_PRINTLN(phoneIp);

	DeviceClient *deviceClient=getDeviceClient(phoneId, phoneKey);
	if(deviceClient!=NULL){
		remove(deviceClient);
	}
	deviceClient=new DeviceClient();
	deviceClient->setConnected(true);
	deviceClient->setClientId(phoneId);
	deviceClient->setClientPairingKey(phoneKey);
	deviceClient->setRemoteIp(phoneIp);
	deviceClient->getWsClient().setClientManager(this);
	deviceClient->getWsClient().beginSSL(phoneIp.toString(), REMOTE_HEARTBEAT_PORT, "/", "0a e7 9b bf cb 8a 5a 74 2d 43 e7 bc b7 02 cf 95 a1 19 77 c8");
	_clientList.add(deviceClient);
	_connectedDeviceCount++;
	DEBUG_PRINTLN("Device successfully created");
	DEBUG_PRINT("Free heap is: ");DEBUG_PRINTLN(ESP.getFreeHeap());
	return true;

}



void ClientManager::remove(DeviceClient * deviceCLient) {
	int size=_clientList.size();
	for(int i=0;i<size;i++){
		DeviceClient *device=_clientList.get(i);
		if(strcmp(device->getClientId(),deviceCLient->getClientId())==0){
			DEBUG_PRINTF("Device found for removal: %s\n", deviceCLient->getClientId());
			device->setConnected(false);
			_clientList.remove(i);
			delete(device);
			_connectedDeviceCount--;
			DEBUG_PRINT("Free heap is: ");DEBUG_PRINTLN(ESP.getFreeHeap());
			break;
		}
	}

}

boolean ClientManager::parseAndValidateRequest(char * responseBuffer, CommandData &commandData){
	boolean retVal=false;
	DEBUG_PRINTF("Parsing input data: %s\n", responseBuffer);
	char *respData=strtok(responseBuffer, delim);//commandResponse=<phoneId>:<phoneKey>
	if(respData!=NULL){
		strcpy(commandData.command, respData);
		respData=strtok(NULL,delim);
		if(respData!=NULL){
			strcpy(commandData.phoneId,respData);
			respData=strtok(NULL,delim);
		}
		if(respData!=NULL){
			strcpy(commandData.phoneKey, respData);
			respData=strtok(NULL,delim);
		}
		if(respData!=NULL){
			strcpy(commandData.data, respData);
		}
		if(strlen(commandData.phoneId)>0 && strlen(commandData.phoneKey)>0){
			String pKey=_clientStore.get(String(commandData.phoneId));
			if(pKey.length()>0 && strcmp(pKey.c_str(), commandData.phoneKey)==0){
				retVal=true;
			}
		}
	}

	return retVal;
}

void ClientManager::encryptRequest() {
}

/**
 * notify is handled via broadcast socket
 */
char * ClientManager::notify(uint32_t timeout) {
	strcpy(_responseBuffer,"");
	strcpy(_requestBuffer, (const char *)_deviceId);
	boolean udpSendResp = udpTranscieve(NOTIFY, _udpBroadcastClient, _requestBuffer, _responseBuffer, timeout, true);
	if(udpSendResp){
		strcpy(_responseBuffer,"");
	}
	return _responseBuffer;
}

/**
 * This method processes all the data received on broadcast port, related to connect, pair, notify etc
 */
boolean ClientManager::processBroadcastData() {
	boolean retVal=false;
	int readBytes=receiveUDPCommand(_udpBroadcastClient, _responseBuffer, 256);
	if(readBytes>0){
		CommandData commandData;
		boolean validated=parseAndValidateRequest(_responseBuffer, commandData);
		if(validated){
			if(strcmp(commands[UDP_CONNECT_BC_REQUEST],commandData.command)==0){
				//p.i.c
				//create device partially
				createDevice(commandData.phoneId, commandData.phoneKey, _udpBroadcastClient.client.remoteIP());
				sendUDPCommand(commands[UDP_CONNECT_BC_RESPONSE],_udpBroadcastClient.client, _udpBroadcastClient.client.remoteIP(), REMOTE_UDP_PORT);
			}else if(strcmp(commands[UDP_CONNECT_BC_RESPONSE],commandData.command)==0){
				//d.i.c
				//UDP_CONNECT_BC_RESPONSE:phoneId:phoneKey
				createDevice(commandData.phoneId, commandData.phoneKey, _udpBroadcastClient.client.remoteIP());
			}
		}else{
			//dont send any response
			INFO_PRINTF("Device not registered: %s, %s\n", commandData.phoneId, commandData.phoneKey);
		}
	}
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
	boolean retVal=false;
	if(_connectedDeviceCount<=0){
		uint32_t currentTimeStamp=millis();
		if(currentTimeStamp-_previousTimeStamp>TCP_IP_PING_DELAY){
			_previousTimeStamp=currentTimeStamp;

#ifdef DEBUG
			DEBUG_PRINT("udp broadcast search message sent, IP: ");DEBUG_PRINT(_udpBroadcastClient.socket.ip);DEBUG_PRINT(":");DEBUG_PRINTLN(_udpBroadcastClient.socket.port);
#endif
			strcpy(_requestBuffer, commands[UDP_CONNECT_BC_REQUEST]);
			strcat(_requestBuffer, ":");
			strcat(_requestBuffer, (const char *)_deviceId);
			sendUDPCommand(_requestBuffer,_udpBroadcastClient);
		}
		processBroadcastData();
	}else{
		retVal= true;
	}
	return retVal;
}
/**
 * This method checks if the phoneid and phone key is already registered
 */
boolean ClientManager::validateResponse(UDPCommand command, char * responseBuffer, char *phoneId, char *phoneKey){
	boolean retVal=false;
	if(strstr(responseBuffer, commands[command])!=NULL){
		char *respData=strtok(responseBuffer, delim);//skipping the first part that is command
		if(respData!=NULL){
			respData=strtok(NULL, delim);
			if(respData!=NULL && strstr("ACK", respData)!=NULL){//if response is ACK or NACK then only consider the response
				respData=strtok(NULL,delim);
				if(respData!=NULL){
					strcpy(phoneId,respData);
					respData=strtok(NULL,delim);
				}
				if(respData!=NULL){
					strcpy(phoneKey, respData);
					respData=strtok(NULL,delim);
				}
				if(strlen(phoneId)>0 && strlen(phoneKey)>0){
					String pKey=_clientStore.get(String(phoneId));
					if(pKey.length()>0 && strcmp(pKey.c_str(), phoneKey)==0){
						retVal=true;
					}
				}
			}
		}
	}

	return retVal;
}


void ClientManager::disconnectTCPIPConnection(TcpSocket &tcpSocketClient) {
	if(tcpSocketClient.client.status()){
		tcpSocketClient.client.stop();
#ifdef DEBUG
		DEBUG_PRINTLN("TCPIP connection dropped");
#endif
	}
	tcpSocketClient.socket.connected=false;
}

boolean ClientManager::establishTCPIPConnection(TcpSocket &tcpSocketClient) {
#ifdef DEBUG
	DEBUG_PRINT("Attempt tcpip connection, current status: ");	DEBUG_PRINTLN(tcpSocketClient.client.status());
#endif
	if(tcpSocketClient.client.status()==0){
		if (!tcpSocketClient.client.connect(tcpSocketClient.socket.ip, tcpSocketClient.socket.port)) {
#ifdef DEBUG
			DEBUG_PRINT("tcp/ip connection failed on IP");	DEBUG_PRINT(tcpSocketClient.socket.ip);DEBUG_PRINT(":");	DEBUG_PRINTLN(tcpSocketClient.socket.port);
#endif
			tcpSocketClient.socket.connected=false;
		}else{
			tcpSocketClient.socket.connected=true;
#ifdef DEBUG
			DEBUG_PRINTLN("tcpIpSocketConnected=true");
#endif
		}
	}
	return tcpSocketClient.socket.connected;
}

int ClientManager::sendUDPCommand(const char* command, WiFiUDP &client, IPAddress ip, uint16_t port) {
#ifdef DEBUG
	DEBUG_PRINT("sendUDPCommand: ");DEBUG_PRINT(command);DEBUG_PRINT(", IP: ");DEBUG_PRINTLN(ip);
#endif
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
		DEBUG_PRINTF("Polling->packet Size:%d", packetSize);DEBUG_PRINT(", from");DEBUG_PRINT(remoteIp);DEBUG_PRINT(":");DEBUG_PRINTLN(socket.client.remotePort());
#endif
		// read the packet into packetBufffer
		readBytes = socket.client.read(resBuf, bufferSize);
		if (readBytes > 0) {
			resBuf[readBytes] = 0;//making it \0 terminated
			DEBUG_PRINTF("receiveUDPCommand Content: %s\n", resBuf);
		}
	}
	return readBytes;
}

SocketData ClientManager::parseTCPIPHostInfo(char* infoStr) {
	uint8_t ipPart1=0, ipPart2=0, ipPart3=0, ipPart4=0;
	uint16_t tcpIpPort=0;

	char *ip=strtok(infoStr, delim);
	if(ip!=NULL){//skipping the first one
		//ipPart1=atoi(ip);
		ip=strtok(NULL,delim);
	}
	if(ip!=NULL){
		ipPart1=atoi(ip);
		ip=strtok(NULL,delim);
	}
	if(ip!=NULL){
		ipPart2=atoi(ip);
		ip=strtok(NULL,delim);
	}
	if(ip!=NULL){
		ipPart3=atoi(ip);
		ip=strtok(NULL,delim);
	}
	if(ip!=NULL){
		ipPart4=atoi(ip);
		ip=strtok(NULL,delim);
	}
	if(ip!=NULL){
		tcpIpPort=atoi(ip);
		ip=strtok(NULL,delim);
	}

	IPAddress addr(ipPart1,ipPart2, ipPart3, ipPart4);
	SocketData sock={addr, tcpIpPort, false};
#ifdef DEBUG
	DEBUG_PRINT("Received IP: ");
	DEBUG_PRINT(addr);
	DEBUG_PRINT(",Received port: ");
	DEBUG_PRINTLN(tcpIpPort);
#endif
	return sock;

}

char* ClientManager::parseCommandData(char* infoStr) {
	char *commandData=strtok(infoStr, ":");
	if(commandData!=NULL){//skipping the first one
		commandData=strtok(NULL,":");
	}
	DEBUG_PRINTF("Command Data: %s\n", commandData);
	return commandData;
}

boolean ClientManager::udpTranscieve(UDPCommand commandId, UdpSocket &socketData,char * requestBuffer,
		char* responseBuffer, uint16_t timeout, boolean validate) {
	int udpAttemptCount=0;
	int readBytes=0;
	boolean matchFound=false;
	unsigned long endMs=millis()+timeout;
	while(endMs-millis()>0){
		//if extra data is there append with command separated by colon
		strcpy(_tempBuffer, commands[commandId]);
		if(strlen((const char *)requestBuffer)>0){
			strcat(_tempBuffer, ":");
			strcat(_tempBuffer, requestBuffer);
		}
		int udpSendResp=sendUDPCommand(_tempBuffer, socketData);
		DEBUG_PRINT("udp send response: ");DEBUG_PRINTLN(udpSendResp);
		if(udpSendResp){
			while(endMs-millis()>0){
				if((readBytes=receiveUDPCommand(socketData,responseBuffer,255))>0){
					if(strstr(responseBuffer, commands[commandId])!=NULL){
						if(validate){
							char phoneId[16]="";
							char phoneKey[8]="";
							if(validateResponse(commandId,responseBuffer, phoneId, phoneKey)){
								matchFound= true;
								break;
							}else{
								matchFound= false;
							}

						}else{
							DEBUG_PRINT("Matching response found: ");DEBUG_PRINTLN(responseBuffer);
							matchFound=true;
							break;
						}
					}else{
						DEBUG_PRINTLN("Matching response not found");
					}
				}else{
					DEBUG_PRINTLN("No response yet...");
					udpAttemptCount++;
					if(udpAttemptCount>=5){
						break;
					}
					delay(UDP_DELAY);
				}
			}
			if(matchFound){
				DEBUG_PRINTLN("match found check");
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
			DEBUG_PRINTF("Device found:%s\n", client->getClientId());
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
			DEBUG_PRINTF("Device found:%s\n", client->getClientId());
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
		DEBUG_PRINTLN("In WStype_DISCONNECTED");
		DEBUG_PRINT("[WSc] IP: ");DEBUG_PRINTLN(client->getRemoteIp());
		_deviceToRemove=true;
		_removeIP=client->getRemoteIp();
		DEBUG_PRINTF("[WSc] Disconnected!\n");
		break;
	case WStype_CONNECTED:
		DEBUG_PRINTF("[WSc] Connected to url: %s\n",  payload);
		_client=getDeviceClient(client->getTCPClient()->remoteIP());
		if(_client!=NULL){
			_client->setConnected(true);
		}
		DEBUG_PRINTF("[WSc] total device count: %d\n",  _connectedDeviceCount);

		break;
	case WStype_TEXT:
		DEBUG_PRINTF("[WSc] get text: %s\n", payload);
		if(_commandCallback!=0){
			CommandData commandData;
			if(parseAndValidateRequest((char *)payload, commandData)){
				_currentClient=getDeviceClient(commandData.phoneId, commandData.phoneKey);

				(*_commandCallback)(commandData, client);
			}
		}
		break;
	case WStype_BIN:
		// send data to server
		// webSocket.sendBIN(payload, lenght);
		break;
	}

}
