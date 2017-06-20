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

/*struct CommandData{
	char command[30];
	char phoneId[16];
	char phoneKey[8];
	char data[256];
	boolean response;//if the response is ACK/NACK, then response is true
	boolean error;//if NACK the error is true, and data contains error message
};*/

void trimPath(char * sourceStr, char *retstr, bool create=false);

#endif /* SOCKETDATA_H_ */
