#include "AckSubscriptionMqttMessage.h"

AckSubscriptionMqttMessage::AckSubscriptionMqttMessage(uint16_t packetId, uint8_t returnCode)
    : MqttMessage(SUBACK, RESERVERTO0) 
{
    this->packetId = packetId;
    this->returnCode = returnCode;
}

String AckSubscriptionMqttMessage::buildMqttPacket(){
    String ackPacket;
    
    // 1. Fixed Header
    // Byte 1: Type (SUBACK) + Flags (RESERVERTO0)
    ackPacket.concat((char)getTypeAndFlags());
    
    // Byte 2: Remaining Length
    // Length = 2 bytes (PacketID) + 1 byte (Return Code) = 3 bytes
    // Note: Assuming simple ack for 1 topic.
    ackPacket.concat((char)3); 
    
    // 2. Variable Header (Packet Identifier)
    // MSB (Most Significant Byte)
    ackPacket.concat((char)(packetId >> 8));
    // LSB (Least Significant Byte)
    ackPacket.concat((char)(packetId & 0xFF));
    
    // 3. Payload (Return Code)
    ackPacket.concat((char)returnCode);
    
    return ackPacket;
}