/*
 * Command.cpp
 *
 *  Created on: Jun 16, 2017
 *      Author: debojitk
 */

#include "CommandData.h"
#include "debugmacros.h"
#include <Arduino.h>


namespace std {

CommandData::CommandData() {
	// TODO Auto-generated constructor stub
}

CommandData::CommandData(char* commandStr) {
	this->parseCommandString(commandStr);
}

CommandData::~CommandData() {
	// TODO Auto-generated destructor stub
}

/**
 * COMMAND:deviceid:deviceKey:ACK/NACK/DATA:DATA
 */
boolean CommandData::parseCommandString(char* commnadStr) {
	boolean retVal=false;
		DEBUG_PRINTF_P("Parsing input data: %s\n", responseBuffer);
		char *respData=strtok(responseBuffer, delim);//commandResponse=<phoneId>:<phoneKey>
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
	if(this->command!=NULL && this->phoneId!=NULL && this->phoneKey!=NULL){
		strcpy(buffer, this->command);
		strcat(buffer, ":");
		strcat(buffer, this->phoneId);
		strcat(buffer, ":");
		strcat(buffer, this->phoneKey);
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

} /* namespace std */
