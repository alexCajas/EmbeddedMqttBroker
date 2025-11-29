# Notes of develop procces

## v1.0.7-qos0

### Memory lacks:

* there was a problem with arudino core 1.0.6 for esp32 and WiFiClient objetc, the core have a memory lack when create a several WiFiCients objects:
  * This issue was fixed, current core 3.2.0 don't have this problem.
* If you have a stack canary watch dog in core 1, FreeMqttClientTask need more heap, actualy it has 3*1024. 

### Todo

* review Trie and nodeTrie allocated memory
* keep alive is not implement yet for async version
* MqttClient::sendMqttPacket doesn't support qos1/qos2

### doing

* Migrate MqttBroker to asyncTCP:
  * Adapter pattern --> done
  * issues with test_tow_client_conextion.sh:
    * There are some packets lost, probably is needed an outBox vector:
      * try with no logs: there less lost, but it doesn't solve:
        * Here the explanation: Task + WiFiClient is autoblocking, so it adapts to wireless velocity, but AsyncTCP doesn't wait, so it only push message and continue even if wireless card are full, but is normal in an async connection.
        * The solution is outbox. 

### done

#### 29/11/2025

* _publishMessageImpl:
  * This O(N*M) loop is a temporary inefficiency due to mapping IDs vs Pointers Future optimization: Store MqttClient* in the Trie directly.


* WebSocket MqttBroker:
  * try to improve search client in wsListener--> it is no need, map find in O(log n) time.

#### 26/11/2025


* There aren't workers for hard task:
  * worker for check keep alive y delete operations ok
  * worker for publish and subscribe mqttpacket:
    * now try to use struct and events, need to pass PublishMqttMessage in queue.
  * worker for readerMqttPacket.

#### 24/11/2025

* MqttClient doesn't has notifiDeleteClient implementation:
  * allmost implemented, there is issues with memory --> done it wasn't free AsyncTCP
* MqttClient doesn't has keepAlive implementation

#### 01/11/2025

* migrating mqtt client to asynctcp version
* Migrating MqttMessages to use new ReaderMqttPacket

#### 06/10/2025

* improve memory manage:
  * improve sockets manage:


* To looking for memory lack:
  * replace concurrent tasks to normal task
  * simulate case with messages.

* some notes:
  * It was found some reserved memory with not free, but there wasn't the main problem.
  * see test_publis_mqtt_client_connection_lack:
    * it only use publish mqtt client and a wificlient and you can see lack memory here.
  * Found lack using WifiClient wiht 1.0.6 core version, in last core verison (3.2.0) is fixed:
    * The lack happend when a client connect and disconnect fast, like defect behavior of mosquitto mqtt client. 
  * Test with 3.2.0 and current state of broker look stable and without memory lack. But remain test with all mqtt packets actives.
  * Some crash detected, freeMqttTask need some memory, now 3*1024 is enought

* test:
  * basic mqtt client free memory ok!
  * one client connecting and disconnecting ok!
  * several clients connecting and disconnecting ok!
  * send message simulation:
    * create an ack mqtt message -- done  
    * a mqtt publish message. 
    * use this mqtt message when a client try to connect/send message.

---

## v1.0.5-qoss0

### doing


### done 

### 14/2/2022
* Added info of how to see outputs of broker activity in examples, version level up to 1.0.5, to arduino manager.

---

## v1.0.4 - qos 0

### doing

### done

#### 13/2/2022

* Adding Steeve suggest esp32 logger library:
  * It can only to use with a precompile core debug level:
    * Arduino IDE: set using, tools/Core Debug Level
    * VSCODE: set using, arduino.json and adding ":
  
  ~~~c++
  "buildPreferences": [
        ["build.extra_flags", "-DCORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_VERBOSE"]
    ]
  ~~~

  * Platformio: set using:
  
  ~~~yaml
  [env:esp32]
  platform = espressif32
  framework = arduino
  board = esp32dev
  lib_deps = 
          alexcajas/WrapperFreeRTOS @ ^1.0.1
          alexcajas/EmbeddedMqttBroker @ 1.0.4-qos0
  build_flags = -DCORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_INFO
  ~~~

  * I'm triying to set core debug level in code, but arduino framework use compile time configuration of debug level, for otherwise, I'm triyed:
  
  ~~~c++
  #ifdef CORE_DEBUG_LEVEL
  #undef CORE_DEBUG_LEVEL
  #endif
  #define CORE_DEBUG_LEVEL 3

  #ifdef LOG_LOCAL_LEVEL
  #undef LOG_LOCAL_LEVEL
  #endif
  #define LOG_LOCAL_LEVEL 3
  .
  .
  .
  setup(){
      esp_log_level_set("*",ESP_LOG_INFO); # this line don't work.
  }
  ~~~

  * But is not posible set core debug level in runtime.

#### 12/2/2022

Issue 4 closed:

* Adding: wait to mqtt packet from client in NewClientListenerTask:
  * On average RTT at most 500 millisecs
  * vTaskdelay(100) worst the efficiency of the broker
  * vTaskDelay(10) just add a delay in the connections.

* Add test_pub and test_sub

#### --/--/--

* Issue 5 closed.

---

## v1.0.3 - qos 0

### doing

* Add test_pub and test_sub (moved to v1.0.4 - qos0 )

### Done

#### update 27/12/2022

* Added name space in project 


---

## v1.0.2

* This version implements only qos 0

### To do

---

### Doing

### Done

* uploaded MqttBroker::publishMessage():
  * Changed std::map clientSubscribed to std::vector clientSubscribedIds to improve memory use.
  * Changed all methods that colaborate in the search of mqttClients subscribeds.
