/**
 * This version adds sd card
 */
#include "debugmacros.h"
#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "WiFiClient.h"
#include "ESP8266WebServer.h"
#include "WiFiUdp.h"
#include "SocketData.h"
#include <FS.h>
#include "Properties.h"
#include "SD.h"
#include "SPI.h"
#include "ClientManager.h"


//tcp/ip comm attributes
#define ICACHE_RAM_ATTR     __attribute__((section(".iram.text")))//required for timer
#define TIMER_DELAY 45  //19.25khz //46 for 22050 (1,000,000/22050)
//#define TIMER_DELAY 500000// 1 sec
#define INBUFSIZE 2920
#define OUTBUFSIZE 2920
#define DEFAULT_SOFT_AP_SSID "ESPAP"
//#define MAX_REC_LENGTH 500000
#define CONFIG_FILE "/config.txt"
#define NOBODY_RESPONDING "nobody_responding.raw"
#define THANK_YOU "thank_you.raw"
#define NOT_AT_HOME "not_at_home.raw"
#define PLEASE_WAIT "please_wait.raw"
#define REQUEST_SOURCE_PHONE "phone"
#define REQUEST_SOURCE_NOTIFY "notify"
#define REQUEST_SOURCE_NOTIFY_STOP "stop_notify"
#define NOTIFY_PROCESSING_TIME 60000 //1 min, if no one responds within 1 minute, then notification processing flag gets reset, and user can again send notify event
#define MAXRECLEN 200000


#define NOTIFY_GPIO_PIN D2//D4 of nodemcu
#define PAIR_GPIO_PIN D4//D3 of nodemcu

#define NOTIFY_ARDUINO_PIN D1//D6 of nodemcu
#define OPEN_DOOR_PIN D1//D7 of nodemcu
#define CLOSE_DOOR_PIN D0//D8 of nodemcu
#define SD_CARD_CHIP_SELECT D8
//SD configuration
//MISO-D6, MOSI-D7, SCK-D5, CS-D8

//const char* ssid = "debojit-dlink";
//const char* password = "India@123";

char responseBuffer[256];       // a string to send back

uint32_t maxRecLen=500000;
uint32_t recordLength=0;


boolean notifyRequested=false;
boolean notifyFlowInProgress=false;

boolean pairingRequested=false;
boolean pairingFlowInProgress=false;

IPAddress softAPIP(192, 168, 4, 1);//way to set gateway ip manually

boolean sdCardPresent=false;
boolean playingFromSD=false;

boolean recordInProgress=false;
boolean playbackInProgress=false;
boolean restoreInProgress=false;
boolean recordFromMicInProgress=false;
boolean someHeavyProcessingInProgress=false;
boolean initiateMessageRecordingfromMic=false;//part of workflow

char playRequestSource[10];//values can be notify, phone
char recordRequestSource[10];//values can be notify, phone
char fileNameBuffer[30], fileNameBuffer1[30];

SDFileWrapper recordingFile;
SDFileWrapper recordingFileFromMic;
SDFileWrapper playingFileFromSD;
SDFileWrapper msgsDir;

File playingFileFromFlash;

Properties properties(10);

uint32_t currentTimeStamp=0, previousTimeStamp=0;
ClientManager clientManager;
WSClientWrapper *heavyProcessResponseClient;
WSClientWrapper *currentWSClient;


boolean tcpIpSocketConnected=false;
boolean tcpIpCommStarted=false;

//double buffering
uint8_t rbuffer[2][INBUFSIZE];
uint8_t rbufferState[2]={0,0};
int readBufferStart[2]={0,0};
int readBufferLast[2]={0,0};
uint8_t readBufferNextToRead=0;//0/1
uint8_t readBufferNextToFill=0;//0/1

uint8_t wbuffer[2][OUTBUFSIZE];
uint8_t wbufferState[2]={0,0};
int writeBufferStart[2]={0,0};
int writeBufferLast[2]={OUTBUFSIZE,OUTBUFSIZE};
uint8_t writeBufferNextToRead=0;//0/1
uint8_t writeBufferNextToFill=0;//0/1
uint8_t sample=0, prevSample=0;

uint32_t lastNotificationTimestamp=0;




void setup(){
	Serial.begin(921600);//20usec/byte
	//Serial.begin(1000000);
	INFO_PRINTLN(F("\nStarting TestESPTransceiverSoftAPV5..."));
	INFO_PRINTLN(F("Configuring SPI flash"));
	SPIFFS.begin();
	setupSDCard();
	readConfigFileAndConfigureWifi();
	//starting up clientManager
	clientManager.setup(processIncomingWSCommands);
	//setupNotifyGPIO();
	setupPairingGPIO();
	setupDoorControl();
	setupNotifyArduino();
	setupTimer1();
	currentTimeStamp=previousTimeStamp=millis();
}

void setupSDCard(){
	DEBUG_PRINTLN(F("Initializing SD card..."));

	if (!SD.begin(SD_CARD_CHIP_SELECT)) {
		DEBUG_PRINTLN(F("SD card initialization failed!"));
		return;
	}
	sdCardPresent=true;
	DEBUG_PRINTLN(F("sd card initialization done."));

	recordingFileFromMic=SD.open("testsd.txt", FILE_WRITE_NO_APPEND);
	if(recordingFileFromMic){
		DEBUG_PRINTF_P("File Open Success: %s\n",recordingFileFromMic.name());
		recordingFileFromMic.println("testing file write");
		recordingFileFromMic.close();
		SD.remove("testsd.txt");
	}else{
		DEBUG_PRINTLN(F("File open failed, restarting..."));
		delay(1000);
		ESP.restart();
	}
}

void readConfigFileAndConfigureWifi(){
	properties.load(CONFIG_FILE);
	properties.print();
	if(properties.getCurrentSize()>0){
		String apMode=properties.get("mode");
		String softapSsid=properties.get("softap_ssid");
		INFO_PRINTLN("AP mode is: "+apMode);
		INFO_PRINTLN("softapSsid: "+softapSsid);
		//String maxRecLenStr=properties.get("max_rec_len");
		//maxRecLen=maxRecLenStr.toInt();
		//PRINTLN("maxRecLen: "+maxRecLen);
		if(softapSsid.equals("")){
			softapSsid=DEFAULT_SOFT_AP_SSID;
		}
		if(apMode.equals("softap")){
			setupEspRadioAsSoftAP(softapSsid.c_str());
		}else{
			String ssid=properties.get("station_ssid");
			String password=properties.get("station_pwd");
			setupEspRadioAsStation(softapSsid.c_str(),ssid.c_str(), password.c_str());
		}
		String deviceId=properties.get("device_id");
		if(deviceId.length()>0){
			clientManager.setDeviceId((char *)deviceId.c_str());
		}
	}else{
		//properties.put("mode","softap");
		properties.put("mode","station");
		properties.put("softap_ssid","ESPAP");
		properties.put("station_ssid","debojit-dlink");
		properties.put("station_pwd","India@123");
		properties.put("not_at_home","false");
		properties.put("max_rec_len","200000");
		properties.put("device_id",DEFAULT_DEVICE_ID);
		properties.put("device_type",DEVICE_TYPE_COMM);

		properties.store(CONFIG_FILE);
		INFO_PRINTLN(F("Properties file not found, creating default, restarting"));
		delay(1000);
		ESP.restart();
	}
}


void setupEspRadioAsSoftAP(const char *ssid){
	yield();
	INFO_PRINTLN(F("Configuring ESP radio access point as SOFT_AP mode..."));
	WiFi.mode(WIFI_AP);//Access point mode only
	/* You can remove the password parameter if you want the AP to be open. */
	WiFi.softAPConfig(softAPIP, softAPIP, IPAddress(255, 255, 255, 0));//use to set custom IP
	WiFi.softAP(ssid);//starting the soft ap

	IPAddress myIP = WiFi.softAPIP();//get the soft AP IP
	INFO_PRINT(F("AP IP address: "));INFO_PRINTLN(myIP);
	//!!VVI!!
	WiFi.disconnect();//disconnect the wifi so that it does not search for wifi host to connect to!
	//if it is not disconnected it would scan for wifi hosts and reduce performance.
#ifdef INFO
	WiFi.printDiag(Serial);//printing wifi details
	INFO_PRINTLN(F("#################"));
#endif
}

void setupEspRadioAsStation(const char *softAPSsid, const char *ssid, const char * password){
	INFO_PRINTF("Connecting to %s-%s\nConfiguring access point...", ssid, password);
	/* You can remove the password parameter if you want the AP to be open. */
	WiFi.softAP(softAPSsid);
	IPAddress myIP = WiFi.softAPIP();
	INFO_PRINT(F("AP IP address: "));DEBUG_PRINTLN(myIP);
	WiFi.begin(ssid, password);
	uint32_t delayMs=10000+millis();
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
#ifdef INFO
		INFO_PRINT(".");
#endif
		if(delayMs<millis()){
			INFO_PRINTF("Could not connect to ssid: %s", ssid);
			WiFi.disconnect();
			break;
		}
	}
#ifdef INFO
	INFO_PRINT(F("WiFi ip is: "));INFO_PRINTLN(WiFi.localIP());
	INFO_PRINTLN(F("#################"));
	WiFi.printDiag(Serial);
	INFO_PRINTLN(F("setup done"));
#endif
}


boolean hasIncomingNotificationRequest(){
	boolean retVal=false;
	if(notifyRequested ){
		notifyRequested=false;
		if(notifyFlowInProgress || pairingFlowInProgress || someHeavyProcessingInProgress){
			retVal=false;
		}else{
			notifyFlowInProgress=true;
			retVal= true;
		}
	}
	if(notifyFlowInProgress){
		uint32_t currentMillis=millis();
		if((currentMillis-lastNotificationTimestamp)>NOTIFY_PROCESSING_TIME){
			DEBUG_PRINTLN(F("Notification response did not come, resetting notification flag"));
			notifyFlowInProgress=false;
			setupNotifyGPIO();
		}

	}
	return retVal;
}

boolean hasIncomingPairingRequest(){
	if(pairingRequested ){
		pairingRequested=false;
		if(notifyFlowInProgress || pairingFlowInProgress || someHeavyProcessingInProgress){
			return false;
		}
		pairingFlowInProgress=true;
		return true;
	}else{
		return false;
	}
}


void processNotify(){
	if(hasIncomingNotificationRequest()){
		DEBUG_PRINTLN(F("Notify requested, sending notify"));
		//if out of home active then immediately play out of home message
		if(properties.get("not_at_home").equals("true")){
			startPlayback(NOT_AT_HOME,REQUEST_SOURCE_NOTIFY);
			notifyFlowInProgress=false;
		}else{
			//startPlayback(PLEASE_WAIT,REQUEST_SOURCE_NOTIFY);
			if(clientManager.getConnectedDeviceCount()>0){
				lastNotificationTimestamp=millis();
				clientManager.notify();
			}else{
				//startPlayback(NOBODY_RESPONDING,REQUEST_SOURCE_NOTIFY);
				notifyFlowInProgress=false;
				setupNotifyGPIO();
			}
		}
	}
}


void processPairing(){
	if(hasIncomingPairingRequest()){
		clientManager.pair();
		pairingFlowInProgress=false;
		DEBUG_PRINTLN(F("Re-enabling pairing gpio"));
		setupPairingGPIO();
	}
}

void loop(void){
	processNotify();
	processPairing();
	if(clientManager.initializeUDPConnection()){
		//this call checks if the device has any data from any client via websocket call
		clientManager.update();
	}
	if(someHeavyProcessingInProgress){
		processTcpIpComm();
		recordMessageFromPhone();
		bufferedPlaybackToSpeaker();
		bufferedRecordFromMic();
		bufferedRestoreFromFlashToSD();
	}
}

uint32_t intrCounter=0;
void gpioNotifyInterrupt() {
	intrCounter++;
	if(intrCounter>3000){
		intrCounter=0;
		detachInterrupt(NOTIFY_GPIO_PIN);
		DEBUG_PRINTLN(F("Notified externally"));
		lastNotificationTimestamp=millis();
		notifyRequested=true;
	}else{
		//DEBUG_PRINTLN(intrCounter);
	}
	//processNotify();
}

void gpioPairingInterrupt() {
	detachInterrupt(PAIR_GPIO_PIN);
	DEBUG_PRINTLN(F("Pairing Initiated"));
	pairingRequested=true;
	//processPairing();
}


void setupDoorControl(){
	INFO_PRINTLN(F("setupDoorControl called"));
	//pinMode(OPEN_DOOR_PIN, OUTPUT);
	//pinMode(CLOSE_DOOR_PIN, OUTPUT);
	//digitalWrite(OPEN_DOOR_PIN, LOW);
	//digitalWrite(CLOSE_DOOR_PIN, LOW);
}
void setupNotifyGPIO(){
	INFO_PRINTLN(F("setupNotifyGPIO called"));
	//pinMode(BUILTIN_LED, OUTPUT);
	pinMode(NOTIFY_GPIO_PIN,INPUT_PULLUP);
	attachInterrupt(NOTIFY_GPIO_PIN, gpioNotifyInterrupt, ONHIGH);
}

void setupPairingGPIO(){
	INFO_PRINTLN(F("setupPairingGPIO called"));
	pinMode(PAIR_GPIO_PIN,INPUT);
	attachInterrupt(PAIR_GPIO_PIN, gpioPairingInterrupt, CHANGE);
}

//Setting up for active HIGH, so while low it would be inactive
void setupNotifyArduino(){
	pinMode(NOTIFY_ARDUINO_PIN, OUTPUT);
	digitalWrite(NOTIFY_ARDUINO_PIN, LOW);
	digitalWrite(BUILTIN_LED, LOW);
}

void notifyArduino(int state){
	digitalWrite(NOTIFY_ARDUINO_PIN, state);
	digitalWrite(BUILTIN_LED, state);
}


void processIncomingWSCommands(CommandData &commandData, WSClientWrapper *wsClient){
	currentWSClient=wsClient;
#ifdef DEBUG
	getFreeHeap();
#endif
	if(strcmp(commands[START_COMM], commandData.getCommand())==0){
		if(!tcpIpCommStarted){
			//preparing response
			makeCommand(START_COMM, responseBuffer);
			if(openAudioChannel()){
#ifdef DEBUG
				__debug__putAudioPreemble();
#endif
				tcpIpCommStarted=true;
				enableTimer1();
				notifyArduino(HIGH);
				someHeavyProcessingInProgress=true;
				heavyProcessResponseClient=currentWSClient;
				strcat(responseBuffer,":ACK");
			}else{
				strcat(responseBuffer,":NACK:TCP/IP connection failed");
			}

		}else{
			strcat(responseBuffer,":NACK:TCP/IP communication already started");
		}
		clientManager.sendWSCommand(responseBuffer, wsClient);
	}else if(strcmp(commands[STOP_COMM], commandData.getCommand())==0){
		makeCommand(STOP_COMM, responseBuffer);
		if(tcpIpCommStarted){
			tcpIpCommStarted=false;
			closeAudioChannel();
			notifyArduino(LOW);
			disableTimer1();
			someHeavyProcessingInProgress=false;
			strcat(responseBuffer,":ACK");
		}else{
			strcat(responseBuffer,"NACK:TCP/IP communication already stopped");
		}
		clientManager.sendWSCommand(responseBuffer, wsClient);
#ifdef DEBUG
		__debug__putAudioPostemble();
#endif
	}else if(strcmp(commands[HELLO], commandData.getCommand())==0){
		testFileWriteSD();
		makeCommand(HELLO, responseBuffer);
		strcat(responseBuffer,":ACK:NexgenLock v0.2");
		clientManager.sendWSCommand(responseBuffer, wsClient);
	}else if(strcmp(commands[RESET], commandData.getCommand())==0){
		makeCommand(RESET, responseBuffer);
		strcat(responseBuffer,":ACK");
		clientManager.sendWSCommand(responseBuffer, wsClient);
		delay(1000);
		ESP.restart();
	}else if(strcmp(commands[OPEN_DOOR], commandData.getCommand())==0){
		digitalWrite(OPEN_DOOR_PIN, HIGH);
		makeCommand(OPEN_DOOR, responseBuffer);
		strcat(responseBuffer,":ACK");
		clientManager.sendWSCommand(responseBuffer, wsClient);
		delay(500);
		digitalWrite(OPEN_DOOR_PIN,LOW);
	}else if(strcmp(commands[CLOSE_DOOR], commandData.getCommand())==0){
		digitalWrite(CLOSE_DOOR_PIN, HIGH);
		makeCommand(CLOSE_DOOR, responseBuffer);
		strcat(responseBuffer,":ACK");
		clientManager.sendWSCommand(responseBuffer, wsClient);
		delay(500);
		digitalWrite(CLOSE_DOOR_PIN, LOW);
	}else if(strcmp(commandData.getCommand(),commands[START_RECORD])==0){
		char * fileName=commandData.getData();
		makeCommand(START_RECORD, responseBuffer);
		if(sdCardPresent){
			if(!recordInProgress &&!someHeavyProcessingInProgress){
				if(openAudioChannel()){
					//recordingFile=initiateFileRecording(fileName);
					trimPath(fileName, fileNameBuffer, true);
					recordingFile=SD.open(fileNameBuffer, FILE_WRITE_NO_APPEND);
#ifdef DEBUG
					if(recordingFile){
						DEBUG_PRINT(F("File opened success: "));DEBUG_PRINTLN(fileNameBuffer);
					}else{
						DEBUG_PRINT(F("File opened failed: "));DEBUG_PRINTLN(fileNameBuffer);
					}
#endif
					if(recordingFile){
						strcat(responseBuffer,":ACK");
						recordInProgress=true;
						someHeavyProcessingInProgress=true;
						heavyProcessResponseClient=currentWSClient;
					}else{
						strcat(responseBuffer,":NACK:File open failed");
					}
				}
			}else{
				strcat(responseBuffer,":NACK:Communication going on");
			}
		}else{
			strcat(responseBuffer,":NACK:SD card not present");
		}
		clientManager.sendWSCommand(responseBuffer, wsClient);
	}else if(strcmp(commandData.getCommand(),commands[STOP_RECORD])==0){
		makeCommand(STOP_RECORD, responseBuffer);
		if(sdCardPresent){
			if(recordInProgress){
				recordInProgress=false;
				someHeavyProcessingInProgress=false;
				closeAudioChannel();
				//stopFileProcessing(recordingFile);
				recordingFile.close();
				recordLength=0;
				strcat(responseBuffer,":ACK");
			}else{
				strcat(responseBuffer,":NACK:TCP/IP recording already stopped");
			}
		}else{
			strcat(responseBuffer,":NACK:SD card not present");
		}
		clientManager.sendWSCommand(responseBuffer, wsClient);
	}else if(strcmp(commandData.getCommand(),commands[START_PLAY])==0){
		const char * fileName=commandData.getData();
		makeCommand(START_PLAY, responseBuffer);
		if(startPlayback(fileName, REQUEST_SOURCE_PHONE)){
			strcat(responseBuffer,":ACK");
		}else{
			strcat(responseBuffer,":NACK:Playback already started");
		}
		clientManager.sendWSCommand(responseBuffer, wsClient);
	}else if(strcmp(commandData.getCommand(),commands[STOP_PLAY])==0){
		makeCommand(STOP_PLAY, responseBuffer);
		if(stopPlayback()){
			strcat(responseBuffer,":ACK");
		}else{
			strcat(responseBuffer,":NACK:playback already stopped");
		}
		clientManager.sendWSCommand(responseBuffer, wsClient);
	}else if(strcmp(commandData.getCommand(),commands[MIC_RECORD_START])==0){
		const char * fileName=commandData.getData();
		makeCommand(MIC_RECORD_START, responseBuffer);
		if(sdCardPresent){
			if(startRecordFromMic(fileName, REQUEST_SOURCE_PHONE)){
				strcat(responseBuffer,":ACK");
			}else{
				strcat(responseBuffer,":NACK:Record from mic already going on");
			}
		}else{
			strcat(responseBuffer,":NACK:SD card not present");

		}
		clientManager.sendWSCommand(responseBuffer, wsClient);
	}else if(strcmp(commandData.getCommand(),commands[MIC_RECORD_STOP])==0){
		makeCommand(MIC_RECORD_STOP, responseBuffer);
		if(sdCardPresent){
			if(stopRecordFromMic()){
				strcat(responseBuffer,":ACK");
			}else{
				strcat(responseBuffer,":NACK:Record from mic already stopped");
			}
		}else{
			strcat(responseBuffer,":NACK:SD card not present");
		}

		clientManager.sendWSCommand(responseBuffer, wsClient);
	}else if(strcmp(commandData.getCommand(),commands[SAVE_CONFIG])==0){
		const char * config=commandData.getData();
		String str(config);
		properties.parsePropertiesAndPut(str);
		properties.print();
		properties.store(CONFIG_FILE);
		makeCommand(SAVE_CONFIG, responseBuffer);
		strcat(responseBuffer,":ACK");
		clientManager.sendWSCommand(responseBuffer, wsClient);
	}else if(strcmp(commandData.getCommand(),commands[LOAD_CONFIG])==0){
		properties.print();
		properties.load(CONFIG_FILE);
		makeCommand(LOAD_CONFIG, responseBuffer);
		strcat(responseBuffer,":ACK:");
		strcat(responseBuffer,properties.serialize().c_str());
		clientManager.sendWSCommand(responseBuffer, wsClient);
	}else if(strcmp(commandData.getCommand(),commands[DELETE_FILE])==0){
		char * fileName=commandData.getData();
		makeCommand(DELETE_FILE, responseBuffer);
		trimPath(fileName, fileNameBuffer, false);
		if(SD.remove(fileNameBuffer)){
			strcat(responseBuffer,":ACK");
		}else{
			strcat(responseBuffer,":NACK:Cant delete file");
		}
		clientManager.sendWSCommand(responseBuffer, wsClient);
	}else if(strcmp(commandData.getCommand(),commands[FORMAT])==0){
		makeCommand(FORMAT, responseBuffer);

		if(SPIFFS.format()){
			strcat(responseBuffer,":ACK");
			delay(1000);
			DEBUG_PRINTLN(F("restarting nodemcu"));
			ESP.restart();
		}else{
			strcat(responseBuffer,":NACK:Cant format");
		}
		clientManager.sendWSCommand(responseBuffer, wsClient);
	}else if(strcmp(commandData.getCommand(),commands[FREE_SPACE])==0){
		makeCommand(FREE_SPACE, responseBuffer);
#ifdef DEBUG
		FSInfo fs_info;
		SPIFFS.info(fs_info);
		DEBUG_PRINT(F("TotalBytes: "));DEBUG_PRINTLN(fs_info.totalBytes);
		DEBUG_PRINT(F("UsedBytes: "));DEBUG_PRINTLN(fs_info.usedBytes);
		DEBUG_PRINT(F("BlockSize: "));DEBUG_PRINTLN(fs_info.blockSize);
		DEBUG_PRINT(F("PageSize: "));DEBUG_PRINTLN(fs_info.pageSize);
		DEBUG_PRINT(F("maxOpenFiles: "));DEBUG_PRINTLN(fs_info.maxOpenFiles);
		DEBUG_PRINT(F("maxPathLength: "));DEBUG_PRINTLN(fs_info.maxPathLength);
#endif
		strcat(responseBuffer,":ACK");
		clientManager.sendWSCommand(responseBuffer, wsClient);
	}else if(strcmp(commandData.getCommand(),commands[GET_MESSAGES])==0){
		char longResponseBuffer[1024];
		makeCommand(GET_MESSAGES, longResponseBuffer);
		msgsDir=SD.open("messages");
		if(msgsDir){
			strcat(longResponseBuffer,":ACK:");
			getMessagesList(msgsDir,longResponseBuffer);
		}else{
			strcat(longResponseBuffer, "NACK:messages folder not found");
		}
		clientManager.sendWSCommand(longResponseBuffer, wsClient);
	}else if(strcmp(commandData.getCommand(),commands[SD_WRITE_TEST])==0){
		makeCommand(SD_WRITE_TEST, responseBuffer);
		generateRandomFileName(fileNameBuffer, NULL);
		//trimFileName(fileNameBuffer, fileNameBuffer);
		recordingFileFromMic=SD.open(fileNameBuffer, FILE_WRITE_NO_APPEND);
		if(recordingFileFromMic){
			DEBUG_PRINTF_P("File Open Success: %s\n",recordingFileFromMic.name());
			recordingFileFromMic.println("testing file ");
			recordingFileFromMic.println(fileNameBuffer);
			recordingFileFromMic.close();
			strcat(responseBuffer,":ACK:File Open Success");
		}else{
			DEBUG_PRINTLN(F("File open failed"));
			strcat(responseBuffer,":NACK:File open failed");
		}
		clientManager.sendWSCommand(responseBuffer, wsClient);
	}else if(strcmp(commandData.getCommand(),commands[NOTIFY])==0){
#ifdef DEBUG
		DEBUG_PRINTLN(F("Notify response processing"));
		DEBUG_PRINT(F("Response: "));DEBUG_PRINTLN(commandData.getResponse());
		DEBUG_PRINT(F("notifyFlowInProgress: "));DEBUG_PRINTLN(notifyFlowInProgress);
#endif
		if(commandData.getResponse() && notifyFlowInProgress){
			clientManager.completeNotificationProcessing(commandData.getPhoneId());
			notifyFlowInProgress=false;
			if(commandData.getError()){
				//rejected
				DEBUG_PRINTLN(F("notify not accepted, play NOBODY_RESPONDING"));
				startPlayback(NOBODY_RESPONDING,REQUEST_SOURCE_NOTIFY);
			}else{
				//accepted
				DEBUG_PRINTLN(F("notify accepted, await tcp/ip communication"));
				//reenable notification
				setupNotifyGPIO();
			}
		}
	}else if(strcmp(commandData.getCommand(),commands[RESTORE])==0){
		char * fileName=commandData.getData();
		makeCommand(RESTORE, responseBuffer);
		if(sdCardPresent){
			if(!someHeavyProcessingInProgress){
				if(startRestoreFromFlashToSD(fileName)){
					strcat(responseBuffer,":ACK:Restore in progress");
				}else{
					strcat(responseBuffer,":NACK:Restore not possible");
				}
			}else{
				strcat(responseBuffer,":NACK:Other tasks going on");
			}
		}else{
			strcat(responseBuffer,":NACK:SD card not present, restore not possible");
		}
		clientManager.sendWSCommand(responseBuffer, wsClient);
	}
}

void getFreeHeap(){
	DEBUG_PRINT(F("Free heap is: "));DEBUG_PRINTLN(ESP.getFreeHeap());
}

//tcp ip tcpIpSocketConnected comm methods



void setupTimer1(){
	timer1_isr_init();
	disableTimer1();
}

void enableTimer1(){
	if(!timer1_enabled()){
		DEBUG_PRINTLN(F("Enabling timer1"));
#ifdef DEBUG
		timer1_attachInterrupt(mockPcmSamplingISR);
#else
		timer1_attachInterrupt(pcmSamplingISR);
#endif
		timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
		uint32_t t1=clockCyclesPerMicrosecond() / 16 * TIMER_DELAY;
		DEBUG_PRINT(F("Counter: "));DEBUG_PRINTLN(t1);
		timer1_write(t1); //32us = 31.250kHz sampling freq
	}else{
		DEBUG_PRINTLN(F("Timer1 already enabled"));
	}
}

void disableTimer1(){
	if(timer1_enabled()){
		DEBUG_PRINTLN(F("Disabling timer1"));
		timer1_disable();
		timer1_detachInterrupt();
	}else{
		DEBUG_PRINTLN(F("Timer1 already disabled"));
	}
}

#pragma GCC push_options
#pragma GCC optimize("O3")

//handles tcp/ip communication
boolean openAudioChannel(){
	if(clientManager.openAudioChannel()){
		tcpIpSocketConnected=true;
	}else{
		tcpIpSocketConnected=false;
	}
	return tcpIpSocketConnected;
}

void closeAudioChannel(){
	clientManager.closeAudioChannel();
	tcpIpSocketConnected=false;
}

void processTcpIpComm(){
	if(tcpIpSocketConnected){
		ESP.wdtDisable();
		processSocketBufferedRead();
		processSocketBufferedWrite();
		ESP.wdtEnable(0);
	}
}


void ICACHE_RAM_ATTR pcmSamplingISR(void){
	//NOTE: In ISR never use yield() or any function that calls yield, this results in resets.
	//thats why client.available is not used in this routine as it has a call to optimistic_yield()
	if(rbufferState[readBufferNextToRead]==1){
		if(readBufferStart[readBufferNextToRead]<readBufferLast[readBufferNextToRead]){
			//last position not reached
			Serial.write(rbuffer[readBufferNextToRead][(readBufferStart[readBufferNextToRead])]);//data at bufPos sent
			readBufferStart[readBufferNextToRead]++;//pointing to next pos
		}else{
			rbufferState[readBufferNextToRead]=0;
			readBufferNextToRead=(readBufferNextToRead==0)?1:0;
		}
	}
	if(wbufferState[writeBufferNextToFill]==0){
		if(writeBufferLast[writeBufferNextToFill]>writeBufferStart[writeBufferNextToFill]){
			sample=Serial.read();
			wbuffer[writeBufferNextToFill][(writeBufferStart[writeBufferNextToFill])]=((sample>=250 || sample<=5)?prevSample:sample);
			writeBufferStart[writeBufferNextToFill]++;
			prevSample=sample;
		}else{
			int temp=writeBufferNextToFill;
			writeBufferNextToFill=(writeBufferNextToFill==0)?1:0;
			wbufferState[temp]=1;
		}
	}

	//buffer writer code
}

//TODO clientManager.getAudioClient().client should be accessed via pointer locally instead of calling via member every time
void processSocketBufferedRead(){
	if(tcpIpSocketConnected && rbufferState[readBufferNextToFill]==0){
		int temp=readBufferNextToFill;
		readBufferLast[readBufferNextToFill]=clientManager.audioSocket.read(rbuffer[readBufferNextToFill],INBUFSIZE);
		readBufferStart[readBufferNextToFill]=0;
		readBufferNextToFill=(readBufferNextToFill==0)?1:0;
		rbufferState[temp]=1;
	}
}

void processSocketBufferedWrite(){//it is the reader of the buffer
	if(tcpIpSocketConnected && wbufferState[writeBufferNextToRead]==1){//ready for read
		clientManager.audioSocket.write((const uint8_t *)wbuffer[writeBufferNextToRead],writeBufferLast[writeBufferNextToRead]);
		writeBufferStart[writeBufferNextToRead]=0;
		wbufferState[writeBufferNextToRead]=0;//set state=0 indicating ready for write
		writeBufferNextToRead=(writeBufferNextToRead==0)?1:0;//switching to next buffer
	}
}

int mockWriteByteCounter=0;
int mockReadByteCounter=0;

#ifdef DEBUG
void ICACHE_RAM_ATTR mockPcmSamplingISR(void){
	if(rbufferState[readBufferNextToRead]==1){
		if(readBufferStart[readBufferNextToRead]<readBufferLast[readBufferNextToRead]){
			//last position not reached
			Serial.write(rbuffer[readBufferNextToRead][(readBufferStart[readBufferNextToRead])]);//data at bufPos sent
			mockWriteByteCounter++;
			if(mockWriteByteCounter>=22050){
				Serial.write("W");
				mockWriteByteCounter=0;
			}
			readBufferStart[readBufferNextToRead]++;//pointing to next pos
		}else{
			rbufferState[readBufferNextToRead]=0;
			readBufferNextToRead=(readBufferNextToRead==0)?1:0;
		}
	}
	if(wbufferState[writeBufferNextToFill]==0){
		if(writeBufferLast[writeBufferNextToFill]>writeBufferStart[writeBufferNextToFill]){
			mockReadByteCounter++;
			if(mockReadByteCounter>=22050){
				Serial.write("R");
				mockReadByteCounter=0;
			}
			//sample=65;//A
			sample=Serial.read();
			//Serial.write(sample);
			wbuffer[writeBufferNextToFill][(writeBufferStart[writeBufferNextToFill])]=((sample>=250 || sample<=5)?prevSample:sample);
			//wbuffer[writeBufferNextToFill][(writeBufferStart[writeBufferNextToFill])]=sample;
			writeBufferStart[writeBufferNextToFill]++;
			prevSample=sample;
		}else{
			int temp=writeBufferNextToFill;
			writeBufferNextToFill=(writeBufferNextToFill==0)?1:0;
			wbufferState[temp]=1;
		}
	}
}
#endif

//record message to file from phone
void recordMessageFromPhone(){
	if(recordInProgress && someHeavyProcessingInProgress){
		if(clientManager.audioSocket.available()){
			int readBytes=clientManager.audioSocket.read(rbuffer[0],INBUFSIZE);
			recordLength+=readBytes;
			DEBUG_PRINT("R-");DEBUG_PRINTLN(recordLength);
			//max file size is MAX_REC_LENGTH
			if(recordLength>maxRecLen){
				recordLength=0;
				recordInProgress=false;
				someHeavyProcessingInProgress=false;
				recordingFile.close();
				//stopFileProcessing(recordingFile);
				makeCommand(STOP_RECORD, responseBuffer);
				strcat(responseBuffer,":MAX_REC_LENGTH-max file size reached");
				clientManager.sendWSCommand(responseBuffer, heavyProcessResponseClient);
				DEBUG_PRINT(F("recordMessageFromPhone stopped, max size reached, bytes: "));DEBUG_PRINTLN(recordLength);
				return;
			}
			size_t size=recordingFile.write(rbuffer[0],readBytes);
			DEBUG_PRINT(F("written bytes: "));DEBUG_PRINTLN(size);
		}
	}
}

//playMessage to speaker from file
void bufferedPlaybackToSpeaker(){
	if(playbackInProgress && rbufferState[readBufferNextToFill]==0){
		DEBUG_PRINT("-P-");
		//if no data available then stop playback
		boolean dataAvailable=false;
		if(playingFromSD){
			dataAvailable=playingFileFromSD.available();
		}else{
			dataAvailable=playingFileFromFlash.available();
		}
		if(!dataAvailable){
			DEBUG_PRINTLN(F("playingFile.available() is false"));
			if(strcmp(playRequestSource,REQUEST_SOURCE_PHONE)==0){
				DEBUG_PRINTLN(F("request source is phone"));
				makeCommand(STOP_PLAY, responseBuffer);
				if(stopPlayback()){
					strcat(responseBuffer,":FILE_END-Playback completed");
				}else{
					strcat(responseBuffer,":FILE_END-File end reached and playback stopped");
				}
				clientManager.sendWSCommand(responseBuffer, heavyProcessResponseClient);
				DEBUG_PRINT(F("sent message is: "));DEBUG_PRINT(responseBuffer);

			}else if(strcmp(playRequestSource,REQUEST_SOURCE_NOTIFY)==0){
				DEBUG_PRINTLN(F("request source is notify"));
				stopPlayback();
				if(sdCardPresent){
					//record message
					generateRandomFileName(fileNameBuffer, "messages");

					if(startRecordFromMic(fileNameBuffer,REQUEST_SOURCE_NOTIFY)){
						//recording started
						DEBUG_PRINTLN(F("recording started"));
					}
				}else{
					//play thank you and close notification process
					DEBUG_PRINT(F("playing THANK_YOU: "));DEBUG_PRINTLN(recordRequestSource);
					startPlayback(THANK_YOU, REQUEST_SOURCE_NOTIFY_STOP);
				}
			}else if(strcmp(playRequestSource,REQUEST_SOURCE_NOTIFY_STOP)==0){
				//do nothing
				DEBUG_PRINTLN(F("request source is notify"));
				stopPlayback();
				notifyFlowInProgress=false;
				setupNotifyGPIO();
			}
			return;
		}
		ESP.wdtDisable();
		int temp=readBufferNextToFill;
		if(playingFromSD){
			readBufferLast[readBufferNextToFill]=playingFileFromSD.read(rbuffer[readBufferNextToFill],INBUFSIZE);
		}else{
			readBufferLast[readBufferNextToFill]=playingFileFromFlash.read(rbuffer[readBufferNextToFill],INBUFSIZE);
		}
		readBufferStart[readBufferNextToFill]=0;
		readBufferNextToFill=(readBufferNextToFill==0)?1:0;
		rbufferState[temp]=1;
		ESP.wdtEnable(0);
	}
}

//Records message in file from mic
void bufferedRecordFromMic(){//it is the reader of the buffer
	if(recordFromMicInProgress && wbufferState[writeBufferNextToRead]==1){//ready for read
		DEBUG_PRINT("-R-");
		ESP.wdtDisable();
		recordLength+=writeBufferLast[writeBufferNextToRead];
		if(recordLength>MAXRECLEN){
			DEBUG_PRINT(F("bufferedRecordFromMic max file size reached, bytes: "));DEBUG_PRINTLN(recordLength);
			recordLength=0;
			stopRecordFromMic();
			DEBUG_PRINT(F("bufferedRecordFromMic recordRequestSource: "));DEBUG_PRINTLN(recordRequestSource);
			if(strcmp(recordRequestSource,REQUEST_SOURCE_NOTIFY)==0){
				DEBUG_PRINT(F("playing THANK_YOU: "));DEBUG_PRINTLN(recordRequestSource);
				startPlayback(THANK_YOU,REQUEST_SOURCE_NOTIFY_STOP);
			}
			return;
		}

		recordingFileFromMic.write((const uint8_t *)wbuffer[writeBufferNextToRead],writeBufferLast[writeBufferNextToRead]);
		writeBufferStart[writeBufferNextToRead]=0;
		wbufferState[writeBufferNextToRead]=0;//set state=0 indicating ready for write
		writeBufferNextToRead=(writeBufferNextToRead==0)?1:0;//switching to next buffer
		ESP.wdtEnable(0);
	}
}

boolean startPlayback(const char * fileName, const char * requestSource){
	boolean retVal=false;
	strcpy(playRequestSource,requestSource);
#ifdef DEBUG
	DEBUG_PRINT(F("startPlayback file: "));DEBUG_PRINTLN(fileName);
	DEBUG_PRINT(F("requestSource: "));DEBUG_PRINTLN(requestSource);
	DEBUG_PRINT(F("playbackInProgress: "));DEBUG_PRINTLN(playbackInProgress);
	DEBUG_PRINT(F("someHeavyProcessingInProgress: "));DEBUG_PRINTLN(someHeavyProcessingInProgress);
	__debug__putAudioPreemble();
#endif
	if(!playbackInProgress && !someHeavyProcessingInProgress){
		//playingFile=initiateFilePlayback(fileName);
		//if sd card is present then try playing from sd card, else from spi flash
		boolean filePresent=false;
		playingFromSD=false;
		if(sdCardPresent){
			trimPath((char *)fileName, fileNameBuffer, false);
			DEBUG_PRINT(F("Opening file: "));DEBUG_PRINTLN(fileNameBuffer);
			playingFileFromSD=SD.open(fileNameBuffer);
			if(playingFileFromSD){
				filePresent=true;
				playingFromSD=true;
			}
		}else{
			char spiFileName[30];
			strcpy(spiFileName, "/");
			strcat(spiFileName, fileName);
			playingFileFromFlash=SPIFFS.open(spiFileName, "r");
			if(playingFileFromFlash){
				filePresent=true;
				playingFromSD=false;
			}
		}
		//if file exists then attempt playback
		if(filePresent){
#ifdef DEBUG
			DEBUG_PRINT(F("startPlayback - playback started "));
			if(sdCardPresent){
				DEBUG_PRINTLN(playingFileFromSD.name());
			}else{
				DEBUG_PRINTLN(playingFileFromFlash.name());
			}
#endif
			enableTimer1();
			notifyArduino(HIGH);
			playbackInProgress=true;
			someHeavyProcessingInProgress=true;
			heavyProcessResponseClient=currentWSClient;
			retVal=true;
		}else{
			DEBUG_PRINTLN(F("startPlayback - file read error"));
			retVal=false;
		}
	}else{
		retVal=false;
	}
	return retVal;
}


boolean stopPlayback(){
	boolean retVal=false;
#ifdef DEBUG
	DEBUG_PRINTLN(F("stopPlaybackFromFlash called"));
	DEBUG_PRINT(F("playbackInProgress: "));DEBUG_PRINTLN(playbackInProgress);
	DEBUG_PRINT(F("someHeavyProcessingInProgress: "));DEBUG_PRINTLN(someHeavyProcessingInProgress);
#endif
	if(playbackInProgress){
		disableTimer1();
		notifyArduino(LOW);
		if(playingFromSD){
			playingFileFromSD.close();
		}else{
			playingFileFromFlash.close();
		}
		playbackInProgress=false;
		someHeavyProcessingInProgress=false;
#ifdef DEBUG
		__debug__putAudioPostemble();
		DEBUG_PRINTLN(F("stopPlaybackFromFlash done success"));
#endif
		retVal=true;
	}else{
		DEBUG_PRINTLN(F("stopPlaybackFromFlash done fault, playbackInProgress is false"));
		retVal=false;
	}
	return retVal;
}


boolean startRecordFromMic(const char * fileName, const char * requestSource){
	boolean retVal=false;
	strcpy(recordRequestSource, requestSource);
#ifdef DEBUG
	DEBUG_PRINT(F("startRecordFromMic file: "));DEBUG_PRINTLN(fileName);
	DEBUG_PRINT(F("requestSource: "));DEBUG_PRINTLN(requestSource);
	DEBUG_PRINT(F("recordFromMicInProgress: "));DEBUG_PRINTLN(recordFromMicInProgress);
	DEBUG_PRINT(F("someHeavyProcessingInProgress: "));DEBUG_PRINTLN(someHeavyProcessingInProgress);
#endif
	if(!recordFromMicInProgress && !someHeavyProcessingInProgress){
		DEBUG_PRINTLN(F("startRecordFromMic in progress: "));
		enableTimer1();
		notifyArduino(HIGH);
		//recordingFile=initiateFileRecording(fileName);
		trimPath((char *)fileName, fileNameBuffer1, true);
		recordingFileFromMic=SD.open(fileNameBuffer1, FILE_WRITE_NO_APPEND);
		recordFromMicInProgress=true;
		someHeavyProcessingInProgress=true;
		heavyProcessResponseClient=currentWSClient;
		recordLength=0;
		retVal=true;
	}else{
		retVal=false;
	}

	return retVal;
}

boolean stopRecordFromMic(){
	boolean retVal=false;
	DEBUG_PRINT(F("stopRecordFromMic called: "));
	if(recordFromMicInProgress){
		DEBUG_PRINT(F("recordFromMicInProgress: "));DEBUG_PRINTLN(recordFromMicInProgress);
		DEBUG_PRINT(F("someHeavyProcessingInProgress: "));DEBUG_PRINTLN(someHeavyProcessingInProgress);
		disableTimer1();
		notifyArduino(LOW);
		recordingFileFromMic.close();
		//stopFileProcessing(recordingFile);
		recordFromMicInProgress=false;
		someHeavyProcessingInProgress=false;
		retVal=true;
	}else{
		retVal=false;
	}
	return retVal;
}

String formatBytes(size_t bytes){
	if (bytes < 1024){
		return String(bytes)+"B";
	} else if(bytes < (1024 * 1024)){
		return String(bytes/1024.0)+"KB";
	} else if(bytes < (1024 * 1024 * 1024)){
		return String(bytes/1024.0/1024.0)+"MB";
	} else {
		return String(bytes/1024.0/1024.0/1024.0)+"GB";
	}
}


void generateRandomFileName(char * shortName, const char * folder){
	if(folder!=NULL){
		strcpy(shortName, folder);
		strcat(shortName,"/");
		strcat(shortName,"R_");
	}else{
		strcpy(shortName,"R_");
	}
	char randBuf[10];
	char * fileNumber=itoa((unsigned short)millis(), randBuf, 10);
	strcat(shortName,fileNumber);
	strcat(shortName,".raw");
}


char *getMessagesList(SDFileWrapper dir, char *fileList){
	if(dir){
		dir.rewindDirectory();
		DEBUG_PRINTLN(dir.name());
		//fileList[0]='\0';
		while(true) {

			SDFileWrapper entry =  dir.openNextFile();
			if (! entry) {
				// no more files
				break;
			}
			if (!entry.isDirectory()) {
				strcat(fileList,entry.name());
				strcat(fileList,",");
			}
			entry.close();
		}
	}

	return fileList;
}


char dateStr[11];
void printDirectory(SDFileWrapper dir, int numTabs) {
	int nextFileCount=0;
	while(true) {

		SDFileWrapper entry =  dir.openNextFile();
		nextFileCount++;
		//PRINT(" next count: ");PRINT(nextFileCount);PRINT("---");
		if (! entry) {
			//PRINTLN("no more files");
			break;
		}
		for (uint8_t i=0; i<numTabs; i++) {
			DEBUG_PRINT('\t');
		}
		//PRINT(entry.name());
		if (entry.isDirectory()) {
			DEBUG_PRINTLN("/");
			printDirectory(entry, numTabs+1);
		} else {
			// files have sizes, directories do not
			DEBUG_PRINT("\t\t");
			DEBUG_PRINTDEC(entry.size());
			DEBUG_PRINT(" ");
			DEBUG_PRINT(entry.getStringDate(dateStr));
			DEBUG_PRINT(" ");
			DEBUG_PRINTLN(entry.getStringTime(dateStr));
		}
		//PRINT("closing entry: ");PRINTLN(entry.name());
		entry.close();
	}
}


void testFileWriteSD(){
	if(sdCardPresent){
		generateRandomFileName(fileNameBuffer, NULL);
		recordingFileFromMic=SD.open(fileNameBuffer, FILE_WRITE_NO_APPEND);
		if(recordingFileFromMic){
			DEBUG_PRINT(F("File Open Success: "));DEBUG_PRINTLN(recordingFileFromMic.name());
			recordingFileFromMic.println("testing file ");
			recordingFileFromMic.println(fileNameBuffer);
			recordingFileFromMic.close();
			SD.remove(fileNameBuffer);
		}else{
			DEBUG_PRINT(F("File open failed: "));DEBUG_PRINTLN(fileNameBuffer);
		}
	}else{
		DEBUG_PRINTLN(F("SD Card not found"));
	}
}


void trimPath(char * sourceStr, char *retstr, bool create){
	char *source=sourceStr;
	const char *s = "\\/";

	char *token;
	char part[20]="";
	/* get the first token */
	token = strtok(source, s);

	/* walk through other tokens */
	int tokenCount=0;
	while( token != NULL )
	{
		part[0]='\0';
		trim(token,part);
		if(tokenCount==0){
			strcpy(retstr, part);
		}else{
			strcat(retstr,"/");
			strcat(retstr, part);

		}
		tokenCount++;
		token = strtok(NULL, s);
	}
	if(create){
		int lastIndex=lastIndexOf(retstr,'/');
		if(lastIndex>0){
			char *dest=(char *)malloc(lastIndex+2);
			substr(retstr,dest,0,lastIndex);
			DEBUG_PRINT(F("dir name: ")); DEBUG_PRINTLN(dest);
			if(!SD.exists(dest)){
				SD.mkdir(dest);
			}
			free(dest);
		}
	}
}

int lastIndexOf(char * source, char character){
	int lastPos=-1;
	for(int i=0;source[i]!='\0';i++){
		if(source[i]==character){
			lastPos=i;
		}
	}
	return lastPos;
}

void substr(char * source, char *dest, int startIndex, int endIndex){
	dest[0]='\0';
	if(startIndex>endIndex)return;
	int j=0;
	for(int i=0;source[i]!='\0';i++){
		if(i>=startIndex && i<endIndex){
			dest[j++]=source[i];
		}
	}
	dest[j]='\0';

}
void trim(char * name, char *sname){
	char *copy=name;
	int dotCount=0;
	int lastDotPos=-1;
	int i=0;
	for(;*copy!='\0';i++){
		if(*copy=='.'){
			dotCount++;
			lastDotPos=i;
		}
		copy++;
	}
	int strLength=i;
	copy=name;
	int namePartLength=0;
	if(lastDotPos>0){
		for(int j=0;j<lastDotPos;j++){
			if(name[j]!='.' && name[j]!=' ' && name[j]!='\t'){
				if(name[j]==',' ||name[j]==';' ||name[j]=='[' || name[j]==']' || name[j]=='+'){
					sname[namePartLength]='_';
				}else{
					sname[namePartLength]=toupper(name[j]);
				}
				if(namePartLength==8){
					sname[6]='~';
					sname[7]='1';
					break;
				}
				namePartLength++;
			}
		}
		if(namePartLength==0){
			//invalid file name
			sname[0]='\0';
			return;
		}
		sname[namePartLength++]='.';
		int extLength=0;
		for(int j=lastDotPos+1;j<strLength;j++){
			if(name[j]!='\t' || name[j]!=' '){
				sname[namePartLength++]=toupper(name[j]);
				extLength++;
				if(extLength>=3){
					break;
				}
			}
		}
		if(extLength==0){
			sname[namePartLength-1]='\0';
		}else{
			sname[namePartLength]='\0';
		}
	}else{
		for(int j=0;j<strLength;j++){
			if(name[j]!='.' && name[j]!=' ' && name[j]!='\t'){
				if(name[j]==',' ||name[j]==';' ||name[j]=='[' || name[j]==']' || name[j]=='+'){
					sname[namePartLength]='_';
				}else{
					sname[namePartLength]=toupper(name[j]);
				}
				if(namePartLength==8){
					sname[6]='~';
					sname[7]='1';
					break;
				}
				namePartLength++;
			}
		}
		if(namePartLength==0){
			//invalid file name
			sname[0]='\0';
			return;
		}else{
			sname[namePartLength]='\0';
		}

	}

}
boolean startRestoreFromFlashToSD(char *fileName){
	boolean retVal=false;
	if(sdCardPresent){
		if(!restoreInProgress && !someHeavyProcessingInProgress){
			//opening source file in flash
			DEBUG_PRINT(F("Restoring file: "));DEBUG_PRINTLN(fileName);
			char spiFileName[30];
			strcpy(spiFileName,"/");
			strcat(spiFileName, (char *)fileName);
			playingFileFromFlash=SPIFFS.open(spiFileName, "r");
			if(!playingFileFromFlash){
				return false;
			}
#ifdef DEBUG
			DEBUG_PRINT(F("open file from flash success: "));DEBUG_PRINT(playingFileFromFlash.name());DEBUG_PRINTLN(playingFileFromFlash.size());
			playingFileFromFlash.close();
#endif
			//opening the destination file

			trimPath((char *)fileName, fileNameBuffer, false);
			playingFileFromSD=SD.open(fileNameBuffer, FILE_WRITE_NO_APPEND);
			if(!playingFileFromSD){
				//dest file could not be opened, so close the source file as well
				playingFileFromFlash.close();
				return false;
			}
			//both files opened
			DEBUG_PRINT(F("open file from sd success: "));DEBUG_PRINT(playingFileFromSD.name());DEBUG_PRINTLN(playingFileFromSD.size());
			restoreInProgress=true;
			someHeavyProcessingInProgress=true;
			heavyProcessResponseClient=currentWSClient;
			retVal = true;
		}
	}
	return retVal;
}

void bufferedRestoreFromFlashToSD(){
	if(restoreInProgress){
		boolean dataAvailable=false;
		dataAvailable=playingFileFromFlash.available();
		if(!dataAvailable){
			DEBUG_PRINTLN(F("playingFileFromFlash.available() is false"));
			makeCommand(RESTORE, responseBuffer);
			strcat(responseBuffer,":FILE_END-Restore completed");
			clientManager.sendWSCommand(responseBuffer, heavyProcessResponseClient);
			DEBUG_PRINT(F("sent message is: "));DEBUG_PRINT(responseBuffer);
			restoreInProgress=false;
			someHeavyProcessingInProgress=false;
			playingFileFromFlash.close();
			playingFileFromSD.close();
		}else{
			ESP.wdtDisable();
			size_t readBytes=playingFileFromFlash.read(rbuffer[0],INBUFSIZE);
			DEBUG_PRINT(F("Bytes read: "));DEBUG_PRINTLN(readBytes);
			playingFileFromSD.write(rbuffer[0],readBytes);
			ESP.wdtEnable(0);
		}
	}
}

/**
 * Used for debug output audio binary data, so that receiver understands that audio is started
 */
void __debug__putAudioPreemble(){
	DEBUG_PRINTLN(F("****************************************************************"));
	Serial.flush();
	delay(100);
}

/**
 * Used for debug output audio binary data, so that receiver understands that audio is ended
 */
void __debug__putAudioPostemble(){
	delay(100);
	DEBUG_PRINTLN(F("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"));
}
