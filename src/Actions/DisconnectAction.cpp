#include "MqttBroker/MqttBroker.h"
using namespace mqttBrokerName;
DisconnectAction::DisconnectAction(MqttClient* mqttClient):Action(mqttClient){
}

void DisconnectAction::doAction(){
    mqttClient->disconnect();
}