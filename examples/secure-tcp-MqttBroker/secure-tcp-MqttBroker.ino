/**
 * @file secure-tcp-MqttBroker.ino
 * @author Alex Cajas (alexcajas505@gmail.com)
 * @brief 
 * Example of using this library to create a Secure TCP MQTT Broker (MQTTS).
 * This example uses the Factory pattern to instantiate a TLS listener on port 8883.
 * @version 2.1.12
 */

#include <WiFi.h> 
#include "EmbeddedMqttBroker.h" 

using namespace mqttBrokerName;

const char *ssid = "SSID";
const char *password = "PASSWORD";

IPAddress ip(192,168,1,131);    
IPAddress gateway(192,168,1,1);   
IPAddress subnet(255,255,255,0);
IPAddress dns1(212,231,6,7);
IPAddress dns2(46,6 ,113,34); 


/******************* mqtt broker ********************/
uint16_t mqttsPort = 8883;

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
  Serial.println("--- Secure TCP MQTT Broker (MQTTS) ---");
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.config(ip, gateway, subnet,dns1,dns2);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println();

  // Create the Secure Broker using the Factory Method for TCP over TLS
  broker = MqttBrokerFactory::createSecureTcpBroker(SERVER_CERT, SERVER_KEY, mqttsPort);

  // Start the broker (Listeners and Workers)
  broker->startBroker();
  
  Serial.println("Secure Broker started successfully!");

  // Print connection info
  Serial.print("Connect using: mqtts://");
  Serial.print(WiFi.localIP());
  Serial.print(":");
  Serial.println(mqttsPort);
  Serial.println("Note: Ensure your MQTT client accepts the provided certificate.");
}

void loop(){
  // The broker runs asynchronously in the background (Core 0 & Core 1).
  // No need to call any loop method here.
  vTaskDelete(NULL); // Optional: Delete the Arduino loop task to save RAM
}
