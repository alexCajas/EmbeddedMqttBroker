#ifndef UNSUBSCRIBEMQTTMESSAGE_H
#define UNSUBSCRIBEMQTTMESSAGE_H

#include "MqttMessage.h"
#include <vector>
#include "MqttTocpic.h"
#include "ReaderMqttPacket.h"

/**
 * @brief Class that abstracts an unsubscribe mqtt packet, this class
 * decodes the  mqtt packet readed from tcpConection..
 * 
 */
class UnsubscribeMqttMessage:public MqttMessage
{
private:
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
    int decodeTopics(int index, ReaderMqttPacket reader);


public:

    /**
     * @brief Construct a new Unsubscribe Mqtt Message object.
     * 
     * @param packetReaded object where is all mqtt packet raw data.
     */    
    UnsubscribeMqttMessage(ReaderMqttPacket packetReaded);
};


#endif
