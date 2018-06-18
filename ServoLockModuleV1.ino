/**
 * This version adds a Nokia 5110 LCD
 * and a Servo MG995
 * enables light sleep mode, see setup method, that works only in station mode
 * wifi_set_sleep_type(LIGHT_SLEEP_T);
 * additional code removed
 *
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
#include "SPI.h"
#include "ClientManager.h"
#include "Servo.h"

//LCD Library
#include "ESP8266_Nokia5110.h"

#define DEFAULT_SOFT_AP_SSID "ESPAP"
#define CONFIG_FILE "/config.txt"


#define LCD_PIN_SCE   5  //D1
#define LCD_PIN_RESET 4  //D2
#define LCD_PIN_DC    0  //D3
#define LCD_PIN_SDIN  2  //D4
#define LCD_PIN_SCLK  14 //D5

#define PROG_VER "Servo/LCD/Lock Module V 1.0"
#define LCD_LINE_0 "NexGenLockV1"
#define LCD_LINE_1_CONNECTED " CONNECTED  "
#define LCD_LINE_1_DISCONNECTED "DISCONNECTED"
#define LCD_LINE_1_INITIALIZING "INITIALIZING"
#define LCD_LINE_1_INITIALIZING_DOTS "------------"
#define LCD_LINE_1_NO_WIFI "  NO WIFI   "
#define LCD_LINE_1_SEARCHING " SEARCHING  "
#define LCD_LINE_1_PAIRING "  PAIRING   "

#define PAIR_GPIO_PIN D6

#define SERVO_ENABLE_PIN D8
#define SERVO_PWM_PIN D7
#define SERVO_SHAFT_LENGTH 2

//servo displacement to angle derivation
#define angle(x) (180-2*acos((1.0*x)/(2.0*SERVO_SHAFT_LENGTH)))


char responseBuffer[256];       // a string to send back


boolean pairingRequested=false;
boolean pairingFlowInProgress=false;

IPAddress softAPIP(192, 168, 4, 1);//way to set gateway ip manually


Properties properties(10);

uint32_t currentTimeStamp=0, previousTimeStamp=0;
ClientManager clientManager;
WSClientWrapper *currentWSClient;
ESP8266_Nokia5110 lcd = ESP8266_Nokia5110(LCD_PIN_SCLK,LCD_PIN_SDIN,LCD_PIN_DC,LCD_PIN_SCE,LCD_PIN_RESET);//reset is 3.3v,
Servo servo;



boolean tcpIpSocketConnected=false;
boolean tcpIpCommStarted=false;



void setup(){

	Serial.begin(921600);//20usec/byte
	//Serial.begin(1000000);
	INFO_PRINT(F("\nStarting "));INFO_PRINTLN(F(PROG_VER));
	INFO_PRINTLN(F("Configuring SPI flash"));
	initLCD();
	SPIFFS.begin();
	readConfigFileAndConfigureWifi();
	//starting up clientManager
	clientManager.setup(processIncomingWSCommands);
	setupPairingGPIO();
	setupDoorControl();
	currentTimeStamp=previousTimeStamp=millis();
	wifi_set_sleep_type(LIGHT_SLEEP_T);
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
		properties.put("servo_length","3");//2 cm

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

boolean wifiStationConnected=false;
void setupEspRadioAsStation(const char *softAPSsid, const char *ssid, const char * password){
	INFO_PRINTLN(F("Configuring ESP radio access point as STA mode..."));

	//INFO_PRINTF("Connecting to %s-%s\nConfiguring access point...", ssid, password);
	/* You can remove the password parameter if you want the AP to be open. */
	//WiFi.softAP(softAPSsid);
	//IPAddress myIP = WiFi.softAPIP();
	//INFO_PRINT(F("AP IP address: "));DEBUG_PRINTLN(myIP);

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	uint32_t delayMs=10000+millis();
	while (WiFi.status() != WL_CONNECTED) {
		writeToLCD(0,1, LCD_LINE_1_INITIALIZING);
		delay(500);
		writeToLCD(0,1, LCD_LINE_1_INITIALIZING_DOTS);
#ifdef INFO
		INFO_PRINT(".");
#endif
		if(delayMs<millis()){
			wifiStationConnected=false;
			INFO_PRINTF("Could not connect to ssid: %s", ssid);
			WiFi.disconnect();
			writeToLCD(0,1, LCD_LINE_1_NO_WIFI);
			break;
		}
		wifiStationConnected=true;
	}
#ifdef INFO
	INFO_PRINT(F("WiFi ip is: "));INFO_PRINTLN(WiFi.localIP());
	INFO_PRINTLN(F("#################"));
	WiFi.printDiag(Serial);
	INFO_PRINTLN(F("setup done"));
#endif
	if(wifiStationConnected){
		writeToLCD(0,1, LCD_LINE_1_SEARCHING);
	}
	writeToLCD(0,3,ssid);
	writeToLCD(0,4, WiFi.localIP().toString());

}


boolean hasIncomingPairingRequest(){
	if(pairingRequested ){
		pairingRequested=false;
		if(pairingFlowInProgress){
			return false;
		}
		pairingFlowInProgress=true;
		return true;
	}else{
		return false;
	}
}




void processPairing(){
	if(hasIncomingPairingRequest()){
		updateStatus(LCD_LINE_1_PAIRING);
		clientManager.pair();
		pairingFlowInProgress=false;
		DEBUG_PRINTLN(F("Re-enabling pairing gpio"));
		setupPairingGPIO();
	}
}

boolean blinkLcdStatus=false;

void loop(void){
	processPairing();
	if(clientManager.initializeUDPConnection()){
		//this call checks if the device has any data from any client via websocket call
		clientManager.update();
		updateStatus(LCD_LINE_1_CONNECTED);
	}else{
		updateStatus(LCD_LINE_1_SEARCHING);
	}
	delay(2000);
}

uint32_t intrCounter=0;

void gpioPairingInterrupt() {
	intrCounter++;
	DEBUG_PRINT(intrCounter);
	if(intrCounter>3000){
		intrCounter=0;
		detachInterrupt(PAIR_GPIO_PIN);
		DEBUG_PRINTLN(F("Pairing Initiated"));
		pairingRequested=true;
	}
	//processPairing();
}

//Make sure to use external pulldown unless activated digitally
void setupDoorControl(){
	INFO_PRINTLN(F("setupDoorControl called"));
	pinMode(SERVO_ENABLE_PIN, OUTPUT);
	digitalWrite(SERVO_ENABLE_PIN, LOW);
	activateAndRunServo(0);
}

void activateAndRunServo(long angle){
	Serial.println("servo activating");
	digitalWrite(SERVO_ENABLE_PIN, HIGH);//activate servo, transistor base turned on
	servo.attach(SERVO_PWM_PIN);
	servo.write(angle);
	delay(1500);//wait until servo goes to its position
	servo.detach();
	digitalWrite(SERVO_ENABLE_PIN,LOW);//deactivate servo, no power
	Serial.println("servo deactivated");
}

void setupPairingGPIO(){
	INFO_PRINTLN(F("setupPairingGPIO called"));
	pinMode(PAIR_GPIO_PIN,INPUT_PULLUP);
	attachInterrupt(PAIR_GPIO_PIN, gpioPairingInterrupt, ONLOW);
}


void processIncomingWSCommands(CommandData &commandData, WSClientWrapper *wsClient){
	currentWSClient=wsClient;
#ifdef DEBUG
	getFreeHeap();
#endif
	if(strcmp(commands[HELLO], commandData.getCommand())==0){
		makeCommand(HELLO, responseBuffer);
		strcat(responseBuffer,":ACK:");
		strcat(responseBuffer,PROG_VER);
		clientManager.sendWSCommand(responseBuffer, wsClient);
	}else if(strcmp(commands[RESET], commandData.getCommand())==0){
		makeCommand(RESET, responseBuffer);
		strcat(responseBuffer,":ACK");
		clientManager.sendWSCommand(responseBuffer, wsClient);
		delay(1000);
		ESP.restart();
	}else if(strcmp(commands[OPEN_DOOR], commandData.getCommand())==0){
		makeCommand(OPEN_DOOR, responseBuffer);
		strcat(responseBuffer,":ACK");
		wifi_set_sleep_type(NONE_SLEEP_T);
		activateAndRunServo(100);
		delay(1000);
		activateAndRunServo(0);
		wifi_set_sleep_type(LIGHT_SLEEP_T);

		clientManager.sendWSCommand(responseBuffer, wsClient);
		delay(500);
	}

	else if(strcmp(commandData.getCommand(),commands[SAVE_CONFIG])==0){
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
	}
}

void getFreeHeap(){
	DEBUG_PRINT(F("Free heap is: "));DEBUG_PRINTLN(ESP.getFreeHeap());
}

//tcp ip tcpIpSocketConnected comm methods





#pragma GCC push_options
#pragma GCC optimize("O3")

//handles tcp/ip communication


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

void initLCD(){
	DEBUG_PRINTLN(F("Initializing LCD Module"));
	lcd.begin();
	lcd.clear();
	lcd.setContrast(0x25);//best contrast
	writeToLCD(0,0, LCD_LINE_0);
}

void writeToLCD(int x, int y, String text){
	lcd.setCursor(x,y);
	lcd.print(text);
}

char lastStatus[13];
void updateStatus(const char * status){
	if(strcmp(lastStatus, status)!=0){
		//status changed, update display
		strcpy(lastStatus,status);
		writeToLCD(0,1,status);
	}
}
