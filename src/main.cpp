#include <Arduino.h>

#if 1
#include <SPI.h>
#include <PN532_SPI.h>
#include "PN532.h"

PN532_SPI pn532spi(SPI, 3);
PN532 nfc(pn532spi);
#elif 1
#include <PN532/PN532/PN532_HSU.h>
#include <PN532/PN532/PN532.h>

PN532_HSU pn532hsu(Serial1);
PN532 nfc(pn532hsu);
#else
#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#endif

#include <M5Stack.h>

#include <WiFi.h>
#include <PubSubClient.h>
#include <env.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);

TaskHandle_t mqttTaskHandle;
TaskHandle_t displayTaskHandle;

char mqtt_waterniveau = 0;

// Callback function on message receive
void mqttCallback(char* topic, byte* payload, unsigned int length) {
	Serial.print("Message arrived [");
	Serial.print(topic);
	Serial.print("] ");
	for (int i = 0; i < length; i++) {
		Serial.print((char)payload[i]);
	}
	Serial.println();

	mqtt_waterniveau = (char)payload[0];
}

// Core 2 Main loop
void mqttTaskLoop(void* parameter) {
	Serial.print("MQTT running on core ");
	Serial.println(xPortGetCoreID());

	mqttClient.setServer(mqtt_url, mqtt_port);
  	mqttClient.setCallback(mqttCallback);

	while(1)
	{
		if(!mqttClient.connected()) {
			if(mqttClient.connect("M5Stack", mqtt_username, mqtt_pass)) {
				Serial.println("Connected to " + String(mqtt_url) + " as " + String(mqtt_username));
				mqttClient.subscribe("onni/waterniveau");
			}
		}
		mqttClient.loop();
	}
}

// Establish connection and initialize NFC
void setup()
{
	M5.begin();
	M5.Power.begin();

	Serial.begin(115200);
	Serial.println();
	Serial.println("-------Peer to Peer HCE--------");

	WiFi.begin(wifi_ssid, wifi_pass);
	Serial.print("Connecting to ");
	Serial.println(wifi_ssid);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println();
	Serial.println("WiFi connected");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	nfc.begin();

	uint32_t versiondata = nfc.getFirmwareVersion();
	if (!versiondata)
	{
		Serial.print("Didn't find PN53x board");
		while (1); // halt
	}

	// Got ok data, print it out!
	Serial.print("Found chip PN5");
	Serial.println((versiondata >> 24) & 0xFF, HEX);
	Serial.print("Firmware ver. ");
	Serial.print((versiondata >> 16) & 0xFF, DEC);
	Serial.print('.');
	Serial.println((versiondata >> 8) & 0xFF, DEC);

	nfc.SAMConfig();

	xTaskCreate(mqttTaskLoop, "MQTT", 10000, NULL, 0, &mqttTaskHandle);
}

// Main Lock NFC loop
void loop()
{
	bool success;
	uint8_t responseLength = 32;

	Serial.println("Waiting for an ISO14443A card");

	// set shield to inListPassiveTarget
	success = nfc.inListPassiveTarget();

	if (success)
	{
		Serial.println("Found something!");

		uint8_t selectApdu[] = {0x00,									  /* CLA */
								0xA4,									  /* INS */
								0x04,									  /* P1  */
								0x00,									  /* P2  */
								0x07,									  /* Length of AID  */
								0xF0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, /* AID defined on Android App */
								0x00 /* Le  */};

		uint8_t response[32];

		success = nfc.inDataExchange(selectApdu, sizeof(selectApdu), response, &responseLength);

		if (success)
		{
			Serial.print("responseLength: ");
			Serial.println(responseLength);

			nfc.PrintHexChar(response, responseLength);

			do
			{
				//uint8_t apdu[] = "Hello from Arduino";
				//char apdu[32];

				String challenge = String(millis());
				Serial.print("Challenge: ");
				Serial.println(challenge);

				uint8_t back[128];
				uint8_t length = 128;

				success = nfc.inDataExchange((uint8_t*)challenge.c_str(), sizeof(uint8_t) * challenge.length(), back, &length);

				if (success)
				{
					Serial.print("responseLength: ");
					Serial.println(length);

					nfc.PrintHexChar(back, length);
					Serial.println();

					String response = String((char*)back);
					response = response.substring(0, length - 2);

					Serial.println(response);
					if(response == challenge + key) {
						Serial.println("Authenticated!");

						uint8_t apdu[] = {0x90, 0x01};
						uint8_t final_back[2];
						uint8_t final_length = 2;

						success = nfc.inDataExchange(apdu, sizeof(apdu), final_back, &final_length);
						if(success) {
							delay(5000);
						}
					}
				}
				else
				{
					Serial.println("Broken connection?");
				}
			} while (success);
		}
		else
		{
			Serial.println("Failed sending SELECT AID");
		}
	}
	else
	{

		Serial.println("Didn't find anything!");
	}

	delay(1000);
}

void printResponse(uint8_t *response, uint8_t responseLength)
{
	String respBuffer;

	for (int i = 0; i < responseLength; i++)
	{

		if (response[i] < 0x10)
			respBuffer = respBuffer + "0"; // Adds leading zeros if hex value is smaller than 0x10

		respBuffer = respBuffer + String(response[i], HEX) + " ";
	}

	Serial.print("response: ");
	Serial.println(respBuffer);
}