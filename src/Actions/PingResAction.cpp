#include "MqttBroker/MqttBroker.h"

PingResAction::PingResAction(MqttClient * mqttClient):Action(mqttClient){

}

void PingResAction::doAction(){
    mqttClient->sendPingRes();
}