#include "MqttBroker/MqttBroker.h"

DisconnectAction::DisconnectAction(MqttClient* mqttClient):Action(mqttClient){
}

void DisconnectAction::doAction(){
    mqttClient->disconnect();
}