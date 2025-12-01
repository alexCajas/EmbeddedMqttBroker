/**
 * @file simple-tcp-MqttBroker.ino
 * @author Alex Cajas (alexcajas505@gmail.com)
 * @brief 
 * Simple example of using this library to create a standard TCP MQTT Broker.
 * This example uses the Factory pattern to instantiate a TCP listener.
 * @version 2.0.8
 */

#include <WiFi.h> 
#include "EmbeddedMqttBroker.h" 

using namespace mqttBrokerName;

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

/******************* mqtt broker ********************/
uint16_t mqttPort = 1883;

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
  Serial.println("--- Simple TCP MQTT Broker ---");
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

  // Create the Broker using the Factory Method for TCP
  broker = MqttBrokerFactory::createTcpBroker(mqttPort);

  // Optional: Configure buffer size for high-traffic bursts
  // broker->setOutBoxMaxSize(200); // Default is 100

  // Start the broker (Listeners and Workers)
  broker->startBroker();
  
  Serial.println("Broker started successfully!");

  // Print connection info
  Serial.print("Connect using: mqtt://");
  Serial.print(WiFi.localIP());
  Serial.print(":");
  Serial.println(mqttPort);
}

void loop(){
  // The broker runs asynchronously in the background (Core 0 & Core 1).
  // No need to call any loop method here.
  vTaskDelete(NULL); // Optional: Delete the Arduino loop task to save RAM
}