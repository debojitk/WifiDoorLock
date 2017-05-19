/*
 * SocketData.h
 *
 *  Created on: Aug 6, 2016
 *      Author: debojitk
 */

#ifndef SOCKETDATA_H_
#define SOCKETDATA_H_
#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "WiFiClient.h"
#include "ESP8266WebServer.h"
#include "WiFiUdp.h"


struct SocketData{
	IPAddress ip;
	uint16_t port;
	boolean connected;
};

struct UdpSocket{
	WiFiUDP client;
	SocketData socket;
};

struct TcpSocket{
	WiFiClient client;
	SocketData socket;
};

struct CommandData{
	char command[30];
	char phoneId[16];
	char phoneKey[8];
	char data[256];
};
extern char commands[][30];

enum UDPCommand:uint8_t{
	UDP_PAIR_BROADCAST,
	UDP_CONNECT_BC_REQUEST,
	UDP_CONNECT_BC_RESPONSE,
	UDP_CONNECT_BC_RETRY,
	UDP_CONNECT_BC_STARTHB,
	START_COMM,
	STOP_COMM,
	START_RECORD,
	STOP_RECORD,
	MIC_RECORD_START,
	MIC_RECORD_STOP,
	START_PLAY,
	STOP_PLAY,
	RESET,
	NOTIFY,
	HELLO,
	OPEN_DOOR,
	CLOSE_DOOR,
	SAVE_CONFIG,
	LOAD_CONFIG,
	DELETE_FILE,
	GET_MESSAGES,
	FORMAT,
	FREE_SPACE,
	SD_WRITE_TEST,
	TEST_NOTIFY,
	RESTORE

};
void trimPath(char * sourceStr, char *retstr, bool create=false);

#endif /* SOCKETDATA_H_ */
