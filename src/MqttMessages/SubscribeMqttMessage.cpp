#include "SubscribeMqttMessage.h"

SubscribeMqttMessage::SubscribeMqttMessage(ReaderMqttPacket packetReaded):MqttMessage(SUBSCRIBE,RESERVERTO2){
    int index = 0;
    messageId = 0;

    //decoding Message Id
    index = packetReaded.decodeTwoBytes(index, &messageId);
    index = decodeTopics(index,packetReaded);
}

int SubscribeMqttMessage::decodeTopics(int index, ReaderMqttPacket packetReaded){
    
    while (index < packetReaded.getRemainingPacketLength() ){
        MqttTocpic topic;
        index = packetReaded.decodeTopic(index, &topic);
        index = packetReaded.decodeQosTopic(index, &topic);
        topics.push_back(topic);       
    }

    return index;
}
