#include"AckConnectMqttMessage.h"

AckConnectMqttMessage::AckConnectMqttMessage(uint8_t ackFlags, uint8_t connectReturnCode):MqttMessage(CONNECTACK,RESERVERTO0){
    this->ackFlags = ackFlags;
    this->connectReturnCode = connectReturnCode;
}

String AckConnectMqttMessage::buildMqttPacket(){
    String ackPacket;
    
    ackPacket.concat((char)getTypeAndFlags());
    ackPacket.concat((char)2);
    ackPacket.concat((char)ackFlags);
    ackPacket.concat((char)connectReturnCode);
    
    return ackPacket;
}