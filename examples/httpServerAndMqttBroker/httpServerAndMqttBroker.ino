/**
 * @file httpServerAndMqttBroker.ino
 * @author Alex Cajas (alexcajas505@gmail.com)
 * @brief 
 * Hybrid Example: Running a Synchronous HTTP Server and an Asynchronous MQTT Broker simultaneously.
 * * In this example, the HTTP server provides a simple status page showing if the Broker is full.
 * @version 2.0.8
 */

#include <WiFi.h> 
#include "EmbeddedMqttBroker.h"

using namespace mqttBrokerName;

const char *ssid = "SSID";
const char *password = "PASSWORD";

/******************* MQTT Broker ********************/
uint16_t mqttPort = 1883;

// We declare a pointer because the Factory will allocate it dynamically
MqttBroker* broker;

/****************** HTTP Server ********************/
uint16_t httpPort = 80;
// Standard synchronous WiFiServer (runs in loop())
WiFiServer server(httpPort);
WiFiClient httpClient;

void setup(){

  /**
   * @brief To see outputs of broker activity 
   * (message to publish, new client's id etc...), 
   * set your core debug level higher to NONE (I recommend INFO or VERBOSE level).
   * More info: @link https://github.com/alexCajas/EmbeddedMqttBroker @endlink
   */


  Serial.begin(115200);
  
  // --- Connect to WiFi ---
  Serial.println();
  Serial.println("--- Hybrid: HTTP Server + MQTT Broker ---");
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");

  // --- Start HTTP Server ---
  server.begin();
  Serial.println("HTTP server started");  
  Serial.print("HTTP URL: http://");
  Serial.println(WiFi.localIP());

  // --- Start MQTT Broker (Using Factory) ---
  
  // 1. Create the Broker instance using the TCP Factory
  broker = MqttBrokerFactory::createTcpBroker(mqttPort);
  
  broker->setMaxNumClients(1); // Set low for testing "Broker Full" logic
  
  // 2. Start (Runs in background)
  broker->startBroker();
  
  Serial.println("Async MQTT Broker started");
  Serial.print("MQTT Connect: tcp://");
  Serial.print(WiFi.localIP());
  Serial.print(":");
  Serial.println(mqttPort);
}

void loop(){
    // The MQTT Broker runs on background tasks/interrupts.
    // The loop() is free to handle the synchronous HTTP server.

    // Check if a web client has connected
    httpClient = server.available();
    
    if (!httpClient)
    {
        // Yield to let the OS switch tasks if needed (important for single core)
        vTaskDelay(10 / portTICK_PERIOD_MS);
        return;
    }

    // Wait for data (Synchronous blocking wait)
    boolean wait = true;
    int timeout = 0;
    while (!httpClient.available() && wait)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        timeout++;
        if(timeout > 30) wait = false; // Simple timeout ~300ms
    }

    if (!httpClient.available())
    {
        return;
    }

    // Read the first line of the request
    String request = httpClient.readStringUntil('\r');
    Serial.print("HTTP Request: ");
    Serial.println(request);
    
    // Send standard HTTP response header
    httpClient.println("HTTP/1.1 200 OK");
    httpClient.println("Content-Type: text/html");
    httpClient.println("Connection: close");
    httpClient.println();
    
    // --- Interaction between HTTP Logic and MQTT Logic ---
    httpClient.println("<!DOCTYPE HTML>");
    httpClient.println("<html><body>");
    httpClient.print("<h1>ESP32 Status</h1>");
    
    httpClient.print("<p><strong>Broker Status:</strong> ");
    
    // Access the broker methods via the pointer
    if (broker->isBrokerFullOfClients()){
        httpClient.print("<span style='color:red;'>FULL (Max Clients Reached)</span>");
    } else{
        httpClient.print("<span style='color:green;'>Available</span>");
    }
    
    httpClient.println("</p></body></html>");
    
    // Close connection
    httpClient.stop();
}