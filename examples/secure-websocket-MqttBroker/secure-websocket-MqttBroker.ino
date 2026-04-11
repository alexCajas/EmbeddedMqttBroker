/**
 * @file secure-websocket-MqttBroker.ino
 * @author Alex Cajas (alexcajas505@gmail.com)
 * @brief 
 * Example of using this library to create a Secure WebSocket MQTT Broker (WSS).
 * This example uses the Factory pattern to instantiate a WSS listener on port 443.
 * @version 2.1.12
 */

#include <WiFi.h> 
#include "EmbeddedMqttBroker.h"

using namespace mqttBrokerName;

const char *ssid = "SSID";
const char *password = "PASSWORD";

/******************* mqtt broker ********************/
// Secure WebSockets usually run on port 443
uint16_t wssPort = 443;

MqttBroker* broker;

// Replace with your actual server private cert in PEM format
const char* SERVER_CERT = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDKjCCAhICFGW/JaC47eW3lmLeuN/IbW53muZZMA0GCSqGSIb3DQEBCwUAME8x\n" \
//..................................................................
"uo1qC9OV6n/IzycKD7h4f4O2WB8LYrQjNwK4kV7FS85E009F6cYOHFjf3yzF1A==\n" \
"-----END CERTIFICATE-----\n" \
;

// Replace with your actual server private key in PEM format
const char* SERVER_KEY = \
"-----BEGIN PRIVATE KEY-----\n" \
"MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDURY7X/YycGd8y\n" \
//..................................................................
"I7OEI3KzUC+B0ig15wNonRPt\n" \
"-----END PRIVATE KEY-----\n" \
;

void setup(){

  /**
   * @brief To see outputs of broker activity 
   * (message to publish, new client's id etc...), 
   * set your core debug level higher to NONE (I recommend INFO or VERBOSE level).
   * More info: @link https://github.com/alexCajas/EmbeddedMqttBroker @endlink
   */
  
  Serial.begin(115200);
  
  // Connect to WiFi network
  Serial.println();
  Serial.println("--- Secure WebSocket MQTT Broker (WSS) ---");
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");

  // Create the Secure Broker using the Factory Method for WebSockets over TLS
  // Default instantiation (ws endpoint: "/mqtt")
  broker = MqttBrokerFactory::createSecureWsBroker(SERVER_CERT, SERVER_KEY, wssPort);
  
  broker->startBroker();
  
  Serial.println("Secure Broker started successfully!");

  // Print connection info for Browser Clients
  Serial.println("Connection details for wss mqtt clients:");
  Serial.print("Host: "); Serial.println(WiFi.localIP());
  Serial.print("Port: "); Serial.println(wssPort);
  Serial.println("Path: /mqtt");
  Serial.print("URL:  wss://");
  Serial.print(WiFi.localIP());
  Serial.print(":");
  Serial.print(wssPort);
  Serial.println("/mqtt");
  Serial.println("Note: Browsers may reject self-signed certificates for WSS connections. Add your cert to the browser's trusted certificate store.");
}

void loop(){
  // The broker runs asynchronously.
  vTaskDelete(NULL); 
}
