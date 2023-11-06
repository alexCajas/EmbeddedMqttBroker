#include "MqttBroker/MqttBroker.h"
using namespace mqttBrokerName;
PublishAction::PublishAction(MqttClient* mqttClient, ReaderMqttPacket packetReaded):Action(mqttClient){
    publishMqttMessage = new PublishMqttMessage(packetReaded);
}

PublishAction::~PublishAction(){
    delete publishMqttMessage;
}


void PublishAction::doAction(){
    mqttClient->notifyPublishRecived(publishMqttMessage);
}