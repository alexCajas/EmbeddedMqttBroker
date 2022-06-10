#include "MqttBroker/MqttBroker.h"

PublishAction::PublishAction(MqttClient* mqttClient, ReaderMqttPacket packetReaded):Action(mqttClient){
    publishMqttMessage = new PublishMqttMessage(packetReaded);
}

void PublishAction::doAction(){
    mqttClient->notifyPublishRecived(publishMqttMessage);
}