/**
 * @file simple-websocket-MqttBroker.ino
 * @author Alex Cajas (alexcajas505@gmail.com)
 * @brief 
 * Example of using this library to create an MQTT Broker over WebSockets.
 * Ideal for connecting web dashboards directly to the ESP32.
 * You can find a dashboard with web mqtt clients in mqtt-websocket-tester.html example.
 * @version 2.0.11
 */

#include <WiFi.h> 
#include "EmbeddedMqttBroker.h"

using namespace mqttBrokerName;

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

/******************* mqtt broker ********************/
// WebSockets usually run on port 80 or 8080
uint16_t wsPort = 8080;

MqttBroker* broker;

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
  Serial.println("--- WebSocket MQTT Broker ---");
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

  // Create the Broker using the Factory Method for WebSockets
  
  // Default instantiation (ws endpoint: "/mqtt")
  broker = MqttBrokerFactory::createWsBroker(wsPort);
  
  // Custom Endpoint Example:
  // broker = MqttBrokerFactory::createWsBroker(wsPort, "/my-custom-endpoint"); 

  broker->startBroker();
  
  Serial.println("Broker started successfully!");

  // Print connection info for Browser Clients
  Serial.println("Connection details for ws mqtt clients:");
  Serial.print("Host: "); Serial.println(WiFi.localIP());
  Serial.print("Port: "); Serial.println(wsPort);
  Serial.println("Path: /mqtt");
  Serial.print("URL:  ws://");
  Serial.print(WiFi.localIP());
  Serial.print(":");
  Serial.print(wsPort);
  Serial.println("/mqtt");
}

void loop(){
  // The broker runs asynchronously.
  vTaskDelete(NULL); 
}