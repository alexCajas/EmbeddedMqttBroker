#include "UnsubscribeMqttMessage.h"

UnsubscribeMqttMessage::UnsubscribeMqttMessage(ReaderMqttPacket packetReaded):MqttMessage(packetReaded.getFixedHeader()){
    int index = 0;
    messageId = 0;

    // 1ª decoding message id
    index = packetReaded.decodeTwoBytes(index,&messageId);

    // 2ª decoding topics.
    index = decodeTopics(index, packetReaded);
}

int UnsubscribeMqttMessage::decodeTopics(int index, ReaderMqttPacket packetReaded){
    
    while (index < packetReaded.getRemainingPacketLength() ){
        MqttTocpic topic;
        index = packetReaded.decodeTopic(index, &topic);
        topics.push_back(topic);       
    }
    return index;
}