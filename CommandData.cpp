/*
 * Command.cpp
 *
 *  Created on: Jun 16, 2017
 *      Author: debojitk
 */

#include "debugmacros.h"
#include <Arduino.h>
#include "CommandData.h"
#include "ClientManager.h"


char commands[][30]={
		"UDP_PAIR_BROADCAST",
		"UDP_PAIR_BROADCAST_ACCEPT",
		"UDP_PAIR_BROADCAST_REJECT",
		"UDP_CONNECT_BC_REQUEST",
		"UDP_CONNECT_BC_RESPONSE",
		"UDP_CONNECT_BC_RETRY",
		"UDP_CONNECT_BC_STARTHB",
		"START_COMM",
		"STOP_COMM",
		"START_RECORD",
		"STOP_RECORD",
		"MIC_RECORD_START",
		"MIC_RECORD_STOP",
		"START_PLAY",
		"STOP_PLAY",
		"RESET",
		"NOTIFY",
		"NOTIFY_ACCEPT",
		"NOTIFY_REJECT",
		"HELLO",
		"OPEN_DOOR",
		"CLOSE_DOOR",
		"SAVE_CONFIG",
		"LOAD_CONFIG",
		"DELETE_FILE",
		"GET_MESSAGES",
		"FORMAT",
		"FREE_SPACE",
		"SD_WRITE_TEST",
		"TEST_NOTIFY",
		"RESTORE"
};

CommandData::CommandData() {
	strcpy(phoneId,"");
	strcpy(phoneKey,"");
	strcpy(data,"");
	response=false;
	error=false;
}

CommandData::~CommandData() {
	// TODO Auto-generated destructor stub
}

/**
 * COMMAND:deviceid:deviceKey:ACK/NACK/DATA:DATA
 */
boolean CommandData::parseCommandString(char* commnadStr) {
	boolean retVal=false;
	DEBUG_PRINTF_P("Parsing input data: %s\n", commnadStr);
	const char *delim=":,";
	char *respData=strtok(commnadStr, delim);//commandResponse=<phoneId>:<phoneKey>
	if(respData!=NULL){
		strcpy(this->command, respData);//COMMAND
		respData=strtok(NULL,delim);
		if(respData!=NULL){
			strcpy(this->phoneId,respData);//PHONE_ID
			respData=strtok(NULL,delim);
			if(respData!=NULL){
				strcpy(this->phoneKey, respData);//PHONE_KEY
				respData=strtok(NULL,delim);
				if(respData!=NULL){
					strcpy(this->data, respData);
					if(strlen(this->data)>0){
						if(strcmp(this->data, "ACK")==0){//ack message
							this->response=true;
							this->error=false;
							respData=strtok(NULL,delim);
							if(respData!=NULL){
								strcpy(this->data, respData);
							}else{
								strcpy(this->data, "");
							}
						}else if(strcmp(this->data, "NACK")==0){//nack message
							this->response=false;
							this->error=true;
							respData=strtok(NULL,delim);
							if(respData!=NULL){
								strcpy(this->data, respData);
							}else{
								strcpy(this->data, "");
							}
						}
					}
				}else{
					//command only, no data
					this->response=false;
					this->error=false;
					strcpy(this->data,"");
				}
				retVal=true;
			}else{
				retVal=false;
			}
		}else{
			retVal=false;
		}
	}else{
		retVal=false;
	}

	return retVal;
}

void CommandData::buildCommandString(char* buffer) {
	if(this->command!=NULL ){
		strcpy(buffer, this->command);
		strcat(buffer, ":");
		if(strlen(this->phoneId)==0){
			strcat(buffer, ClientManager::getDeviceId());
		}else{
			strcat(buffer, this->phoneId);
		}
		strcat(buffer, ":");
		if(strlen(this->phoneKey)==0){
			strcat(buffer, ClientManager::getDeviceKey());
		}else{
			strcat(buffer, this->phoneKey);
		}
		if(this->response){
			if(this->error){
				strcat(buffer, ":NACK");
			}else{
				strcat(buffer, ":ACK");
			}
			if(strlen(this->data)>0){
				strcat(buffer, ":");
				strcat(buffer, this->data);
			}
		}else if(strlen(this->data)>0){
			strcat(buffer, ":");
			strcat(buffer, this->data);
		}
	}
	DEBUG_PRINTF_P("Command String is: %s\n", buffer);

}


