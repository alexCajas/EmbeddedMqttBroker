#include "MqttBroker/MqttBroker.h"
using namespace mqttBrokerName;
NoAction::NoAction(MqttClient *mqttClient):Action(mqttClient){

}

void NoAction::doAction(){
    log_i("No action");
}