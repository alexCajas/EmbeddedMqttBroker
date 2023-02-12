# Notes of develop procces

## v1.0.4 - qos 0

### doing



### done

Issue 4 closed:

* Adding: wait to mqtt packet from client in NewClientListenerTask:
  * On average RTT at most 500 millisecs
  * vTaskdelay(100) worst the efficiency of the broker
  * vTaskDelay(10) just add a delay in the connections.

* Add test_pub and test_sub

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
