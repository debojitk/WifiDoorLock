How to use arduinoota to upload using ota feature
-------------------------------------------------

Ans:espota.py is to be executed with the following parameters, it is available inside arduinoPlugin/packages/esp8266/hardware/esp8266/2.3.0/tools location. If the command fails try disabling windows firewall.

command line arguments:
-----------------------

-i=hostname, make sure to use the ip, hostname does not work
-p=port default is 8266
--auth=FALSE if password not set else set the password within double quotes
-f=location to the compiled bin file. Its better to compile the file first using eclipse. get the location and then use the location, again in double quotes.

Sample command:
---------------
python.exe "E:/debojit/electronics project/2016/ide/eclipseArduino/arduinoPlugin/packages/esp8266/hardware/esp8266/2.3.0/tools/espota.py" -i "192.168.0.106" -p 8266 --auth="123456" -f "E:\debojit\electronics project\2016\ide\eclipseArduino\workspace\ArduinoOTAV1/Release/ArduinoOTAV1.bin"


Basic code to include ota feature:
----------------------------------
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

const char* ssid = "debojit-dlink";
const char* password = "India@123";

void setup() {
	Serial.begin(115200);
	Serial.println("Booting");Serial.println(ESP.getChipId());
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	while (WiFi.waitForConnectResult() != WL_CONNECTED) {
		Serial.println("Connection Failed! Rebooting...");
		delay(5000);
		ESP.restart();
	}

	// Port defaults to 8266
	// ArduinoOTA.setPort(8266);

	// Hostname defaults to esp8266-[ChipID]
	//ArduinoOTA.setHostname("myesp8266");

	// No authentication by default
	ArduinoOTA.setPassword((const char *)"123456");

	ArduinoOTA.onStart([]() {
		Serial.println("Start");
	});
	ArduinoOTA.onEnd([]() {
		Serial.println("\nEnd");
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\r\n", (progress / (total / 100)));
	});
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
		else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
		else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
		else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
		else if (error == OTA_END_ERROR) Serial.println("End Failed");
	});
	ArduinoOTA.begin();
	Serial.println("Ready");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());
	Serial.println(ArduinoOTA.getHostname());
	pinMode(BUILTIN_LED, OUTPUT);
}

void loop() {
	ArduinoOTA.handle();
	digitalWrite(BUILTIN_LED, LOW);
	delay(1000);
	digitalWrite(BUILTIN_LED, HIGH);
	delay(1000);

}
