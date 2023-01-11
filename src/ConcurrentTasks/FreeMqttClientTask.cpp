#include "MqttBroker/MqttBroker.h"
using namespace mqttBrokerName;
/********************** FreeMqttClientTask *************************/

FreeMqttClientTask::FreeMqttClientTask(MqttBroker *broker,QueueHandle_t *deleteMqttClientQueue):Task("FreeMqttClientTask",1024*2,TaskPrio_Low){
    this->broker = broker;
    this->deleteMqttClientQueue = deleteMqttClientQueue;
}

void FreeMqttClientTask::run (void * data){

  int clientId;
  while(true){
    
    xQueueReceive((*deleteMqttClientQueue), &clientId, portMAX_DELAY);
    log_i("Deleting client: %i", clientId);
    broker->deleteMqttClient(clientId);
    
  }

}