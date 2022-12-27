#include "MqttBroker/MqttBroker.h"
using namespace mqttBrokerName;
TCPListenerTask::TCPListenerTask(MqttClient *mqttClient) : Task("TCPListener", 1024 * 5, TaskPrio_HMI){
    this->mqttClient = mqttClient;
}

void TCPListenerTask::run(void * data){
    
    while(true){
        if(!mqttClient->checkConnection()){
            mqttClient->notifyDeleteClient();
        }
        vTaskDelay(10);
    }
}
