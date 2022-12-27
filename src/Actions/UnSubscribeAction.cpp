#include "MqttBroker/MqttBroker.h"
using namespace mqttBrokerName;
UnSubscribeAction::UnSubscribeAction(MqttClient *mqttClient,ReaderMqttPacket packetReaded):Action(mqttClient){
    unsubscribeMqttMessage = new UnsubscribeMqttMessage(packetReaded);
}

UnSubscribeAction::~UnSubscribeAction(){
    delete unsubscribeMqttMessage;
}

void UnSubscribeAction::doAction(){
    // to do
}