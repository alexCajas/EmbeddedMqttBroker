#include "MqttBroker/MqttBroker.h"
using namespace mqttBrokerName;
PingResAction::PingResAction(MqttClient * mqttClient):Action(mqttClient){

}

void PingResAction::doAction(){
    mqttClient->sendPingRes();
}