#include "FactoryMqttMessages.h"


FactoryMqttMessages::FactoryMqttMessages(){

}

MqttMessage FactoryMqttMessages::decodeMqttPacket(WiFiClient client){
    ReaderMqttPacket reader;
    reader.readMqttPacket(client);
    uint8_t type;
    uint8_t flagsControlType;
    type = reader.getFixedHeader() >> 4;
    switch (type)
    {
    case CONNECT:
        return ConnectMqttMessage(reader);
        break;
        
    case PINGREQ:
        return PingReqMqttMessage();
    
    case PUBLISH:
        return PublishMqttMessage(reader);
    default:
        break;
    }

    return NotMqttMessage();
}

ConnectMqttMessage FactoryMqttMessages::getConnectMqttMessage(WiFiClient client){
    ReaderMqttPacket reader;
    reader.readMqttPacket(client);
    return ConnectMqttMessage(reader);    
}




AckConnectMqttMessage FactoryMqttMessages::getAceptedAckConnectMessage(){
    return AckConnectMqttMessage(NOTSESIONPRESENT,CONNECTACCEPTED);
}

PingResMqttMessage FactoryMqttMessages::getPingResMessage(){
    return PingResMqttMessage();
}

PublishMqttMessage FactoryMqttMessages::getPublishMqttMessage(uint8_t publishFlags){
    return PublishMqttMessage(publishFlags);
}
