#include "MqttBroker/MqttBroker.h"

NoAction::NoAction(MqttClient *mqttClient):Action(mqttClient){

}

void NoAction::doAction(){
    Serial.println("no action");
}