/**
 * @file httpServerAndMqttBroker.ino
 * @author Alex Cajas (alexcajas505@gmail.com)
 * @brief 
 * Example of using a http web server and a mqtt broker 
 * at the same time. This example shows a small web where
 * you can see if the broker is full of mqtt clients and how 
 * to configure the maximum number of mqtt clients connections acepted
 * by the broker.
 * @version 1.0.0
 */

#include <WiFi.h> 
#include "EmbeddedMqttBroker.h"
using namespace mqttBrokerName;
const char *ssid = "...";
const char *password = "***";
const char *AP_NameChar = "WebServerAndBroker";

/******************* mqtt broker ********************/
uint16_t mqttPort = 1883;
MqttBroker broker(mqttPort);

/****************** http server ********************/
uint16_t httpPort = 80;
WiFiServer server(httpPort);
WiFiClient httpClient;

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
  Serial.println("http server and mqtt broker");
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  //WiFi.mode(WIFI_AP_STA);
  //MDNS.begin(AP_NameChar,WiFi.localIP());

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    WiFi.begin(ssid, password);
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");

  // Start http server
  server.begin();
  Serial.println("http server started");  
  // Print the IP address
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("");

  // Start the mqtt broker
  broker.setMaxNumClients(1); // set according to your system.
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

    // Check if a client has connected
    httpClient = server.available();
    if (!httpClient)
    {
        vTaskDelay(10);
        return;
    }

    boolean wait = true;
    while (!httpClient.available() and wait)
    {
        vTaskDelay(300);
        wait = false;
    }

    if (!httpClient.available())
    {
        return;
    }

    String request = httpClient.readStringUntil('\r');
    Serial.println(request);
    
    httpClient.print("is the broker full?: ");
    if (broker.isBrokerFullOfClients()){
        httpClient.print("True");
    } else{
        httpClient.print("False");
    }
    
}


