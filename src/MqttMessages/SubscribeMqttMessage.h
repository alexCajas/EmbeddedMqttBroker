#ifndef SUBSCRIBEMQTTMESSAGE_H
#define SUBSCRIBEMQTTMESSAGE_H

#include "MqttMessage.h"
#include "MqttTocpic.h"
#include "ReaderMqttPacket.h"
#include <vector>

/**
 * @brief Class that abstracts a subscribe mqtt packet, this class
 * decodes the subscribe mqtt packet.
 * 
 */
class SubscribeMqttMessage:public MqttMessage
{
private:
    /**
     * @brief Subscribed topics.
     * 
     */
    std::vector<MqttTocpic> topics;
    uint16_t messageId;

    /**
     * @brief Get topics from mqtt packet.
     * 
     * @param index where start the lengt of the first mqtt topic field.
     * @param packetReaded object where all variableHeader of mqtt packet, and provide
     *        utils to get data from the bytes of variable header.    
     * @return int the index where the topics fields ends and start the next
     *         mqtt field.
     */
    int decodeTopics(int index, ReaderMqttPacket packetReaded);
 

public:

    /**
     * @brief Construct a new Subscribe Mqtt Message object.
     * 
     * @param packetReaded object where is all mqtt packet raw data.
     */
    SubscribeMqttMessage(ReaderMqttPacket packetReaded);

    std::vector<MqttTocpic> getTopics(){
        return topics;
    }
};

#endif //SUBSCRIBEMQTTMESSAGE_H