#include "FactoryMqttMessages.h"


FactoryMqttMessages::FactoryMqttMessages(){

}

MqttMessage FactoryMqttMessages::decodeMqttPacket(ReaderMqttPacket &reader){
    
    if(!reader.isPacketReady()){
        log_e("No mqtt packet ready to decode.");
        return NotMqttMessage();
    }

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

ConnectMqttMessage FactoryMqttMessages::getConnectMqttMessage(ReaderMqttPacket &reader){
    
    return ConnectMqttMessage(reader);    
}




AckConnectMqttMessage FactoryMqttMessages::getAceptedAckConnectMessage(){
    return AckConnectMqttMessage(NOTSESIONPRESENT,CONNECTACCEPTED);
}

AckSubscriptionMqttMessage FactoryMqttMessages::getSubAckMessage(uint16_t packetId){
    return AckSubscriptionMqttMessage(packetId, 0x00); // 0x00 = Success QoS 0
}

PingResMqttMessage FactoryMqttMessages::getPingResMessage(){
    return PingResMqttMessage();
}

PublishMqttMessage FactoryMqttMessages::getPublishMqttMessage(uint8_t publishFlags){
    return PublishMqttMessage(publishFlags);
}

