#ifndef CONNECTMQTTMESSAGE_H
#define CONNECTMQTTMESSAGE_H

#include "MqttMessage.h"
#include "ReaderMqttPacket.h"

/**
 * @brief Represent a Mqtt Connect command, borker receives this packet
 * from some mqtt client.
 * protocolName, version, connect flags,
 * keep alive and clientId fields, are always presents. 
 * User name and password are not required.
 */
class ConnectMqttMessage: public MqttMessage{
    private:
        
        String protocolName;
        uint8_t version;
        uint8_t connectFlags;
        uint16_t keepAlive;
        String clientID;

        /**
         * @brief To implement in next versions.
         * 
         */
        String userName;
        String pass;

    public:

    /**
     * @brief Construct a new Connect Mqtt Message object.
     * 
     * @param packetReaded Object where are all raw data readed
     *        from tcpConnection, this datas are in bytes.
     */
    ConnectMqttMessage(ReaderMqttPacket packetReaded);
    
    uint16_t getKeepAlive(){
        return keepAlive;
    }

    String getClientId(){
        return clientID;
    }

    /**
     * @brief Check if packet is good formed, check type of packet
     * and flags, in connect packet that is 000010000.
     * 
     * @return true if packet is malFormed.
     * @return false if packet is goodFormed.
     */
    bool malFormedPacket(){
        return !((this->getTypeAndFlags() & 0x10) == 0x10); 
    }
};

#endif