#include "MqttBroker/MqttBroker.h"
using namespace mqttBrokerName;
ActionFactory::ActionFactory(){}

Action* ActionFactory::getAction(MqttClient * mqttClient, ReaderMqttPacket packetReaded){
    
    uint8_t type = packetReaded.getFixedHeader() >> 4;
    type = packetReaded.getFixedHeader() >> 4;
    
    switch (type)
    {
    case PINGREQ:
        return new PingResAction(mqttClient);
        break;
    
    case PUBLISH:
        return new PublishAction(mqttClient,packetReaded);
    
    case SUBSCRIBE:
        return new SubscribeAction(mqttClient,packetReaded);
    
    case DISCONNECT:
        return new DisconnectAction(mqttClient);

    case UNSUBSCRIBE:
        return new UnSubscribeAction(mqttClient, packetReaded);
    
    default:
        break;
    }

    return new NoAction(mqttClient);
}