#ifdef __IN_ECLIPSE__
//This is a automatic generated file
//Please do not modify this file
//If you touch this file your change will be overwritten during the next build
//This file has been generated on 2017-06-16 01:29:25

#include "Arduino.h"
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
void setup();
void setupSDCard();
void readConfigFileAndConfigureWifi();
void setupEspRadioAsSoftAP(const char *ssid);
void setupEspRadioAsStation(const char *softAPSsid, const char *ssid, const char * password);
boolean hasIncomingNotificationRequest();
boolean hasIncomingPairingRequest();
void processNotify();
void processPairing();
void loop(void);
void gpioNotifyInterrupt() ;
void gpioPairingInterrupt() ;
void setupDoorControl();
void setupNotifyGPIO();
void setupPairingGPIO();
void setupNotifyArduino();
void notifyArduino(int state);
void processIncomingUDPCommands(CommandData &commandData, WSClientWrapper *wsClient);
void getFreeHeap();
void setupTimer1();
void enableTimer1();
void disableTimer1();
boolean openAudioChannel();
void closeAudioChannel();
void processTcpIpComm();
void ICACHE_RAM_ATTR pcmSamplingISR(void);
void processSocketBufferedRead();
void processSocketBufferedWrite();
void ICACHE_RAM_ATTR mockPcmSamplingISR(void);
void recordMessageFromPhone();
void bufferedPlaybackToSpeaker();
void bufferedRecordFromMic();
boolean startPlayback(const char * fileName, const char * requestSource);
boolean stopPlayback();
boolean startRecordFromMic(const char * fileName, const char * requestSource);
boolean stopRecordFromMic();
String formatBytes(size_t bytes);
void generateRandomFileName(char * shortName, const char * folder);
char *getMessagesList(SDFileWrapper dir, char *fileList);
void printDirectory(SDFileWrapper dir, int numTabs) ;
void testFileWriteSD();
void trimPath(char * sourceStr, char *retstr, bool create);
int lastIndexOf(char * source, char character);
void substr(char * source, char *dest, int startIndex, int endIndex);
void trim(char * name, char *sname);
boolean startRestoreFromFlashToSD(char *fileName);
void bufferedRestoreFromFlashToSD();


#include "TestESPTransceiverSoftAPV5.ino"

#endif
