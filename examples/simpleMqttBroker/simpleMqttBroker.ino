/**
 * @file simpleMqttBroker.ino
 * @author Alex Cajas (alexcajas505@gmail.com)
 * @brief 
 * Simple example of using this Mqtt Broker
 * @version 1.0.0
 */

#include <WiFi.h> 
#include "EmbeddedMqttBroker.h"
using namespace mqttBrokerName;
const char *ssid = "...";
const char *password = "***";

/******************* mqtt broker ********************/
uint16_t mqttPort = 1883;
MqttBroker broker(mqttPort);

void setup(){
  
  /**
   * @brief To see outputs of broker activity 
   * (message to publish, new client's id etc...), 
   * set your core debug level higher to NONE (I recommend INFO level).
   * More info: @link https://github.com/alexCajas/EmbeddedMqttBroker @endlink
   */

  Serial.begin(115200);
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.println("simple mqtt broker");
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    WiFi.begin(ssid, password);
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");

  // Start the mqtt broker
  broker.setMaxNumClients(9); // set according to your system.
  broker.startBroker();
  Serial.println("broker started");

  // Print the IP address
  Serial.print("Use this ip: ");
  Serial.println(WiFi.localIP());
  Serial.print("and this port: ");
  Serial.println(mqttPort);
  Serial.println("To connect to mqtt broker");

}

void loop(){

}


