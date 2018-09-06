/*
 * Command.h
 *
 *  Created on: Jun 16, 2017
 *      Author: debojitk
 */

#ifndef COMMANDDATA_H_
#define COMMANDDATA_H_
#include <Arduino.h>

extern char commands[][30];

enum UDPCommand:uint8_t{
	UDP_PAIR_BROADCAST,
	UDP_PAIR_BROADCAST_ACCEPT,
	UDP_PAIR_BROADCAST_REJECT,
	UDP_CONN_BC_REQUEST_PHONE,
	UDP_CONN_BC_RESPONSE_PHONE,
	UDP_CONN_BC_REQUEST_DEVICE,
	UDP_CONN_BC_RESPONSE_DEVICE,
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
	NOTIFY_ACCPET,
	NOTIFY_REJECT,
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

enum ConnectionStatus:uint8_t{
	CONNECTING,
	CONNECTED,
	DISCONNECTING,
	DISCONNECTED
};

class CommandData {
public:
	CommandData();
	virtual ~CommandData();

	char* getCommand() {
		return command;
	}

	void setCommand(const char * command){
		strcpy(this->command,command);
	}

	char* getData() {
		return data;
	}
	void setData(const char * data){
		strcpy(this->data, data);
	}

	boolean getError() {
		return error;
	}

	void setError(boolean error) {
		this->error = error;
	}

	char* getPhoneId() {
		return phoneId;
	}

	void setPhoneId(const char * phoneId){
		strcpy(this->phoneId, phoneId);
	}

	char* getPhoneKey() {
		return phoneKey;
	}

	void setPhoneKey(const char * phoneKey){
		strcpy(this->phoneKey, phoneKey);
	}

	char* getDeviceType() {
		return deviceType;
	}

	void setDeviceType(const char * deviceType){
		strcpy(this->deviceType, deviceType);
	}

	boolean getResponse() {
		return response;
	}

	void setResponse(boolean response) {
		this->response = response;
	}

	boolean parseCommandString(char *commnadStr);
	void buildCommandString(char *commnadStr);

private:
	char command[30];
	char phoneId[16];
	char phoneKey[8];
	char deviceType[8];
	char data[256];
	boolean response=false;//if the response is ACK/NACK, then response is true
	boolean error=false;//if NACK the error is true, and data contains error message
};

#endif /* COMMANDDATA_H_ */
