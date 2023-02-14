# Notes of develop procces

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
