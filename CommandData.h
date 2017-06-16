/*
 * Command.h
 *
 *  Created on: Jun 16, 2017
 *      Author: debojitk
 */

#ifndef COMMANDDATA_H_
#define COMMANDDATA_H_

class CommandData {
public:
	CommandData(char *commandStr);
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
	char data[256];
	boolean response=false;//if the response is ACK/NACK, then response is true
	boolean error=false;//if NACK the error is true, and data contains error message

};

#endif /* COMMANDDATA_H_ */
