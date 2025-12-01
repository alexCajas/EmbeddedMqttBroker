#ifndef FACTORYMQTTMESSAGES_H
#define FACTORYMQTTMESSAGES_H

#include "MqttMessage.h"
#include "ConnectMqttMessage.h"
#include "AckConnectMqttMessage.h"
#include "AckSubscriptionMqttMessage.h"
#include "PingResMqttMessage.h"
#include "PingReqMqttMessage.h"
#include "ReaderMqttPacket.h"
#include "PublishMqttMessage.h"
#include "NotMqttMessage.h"
#include "ReaderMqttPacket.h"

class FactoryMqttMessages {
    private:

    
    public:
        FactoryMqttMessages();
        MqttMessage decodeMqttPacket(ReaderMqttPacket &reader);
        AckConnectMqttMessage getAceptedAckConnectMessage();
        PingResMqttMessage getPingResMessage();
        PublishMqttMessage getPublishMqttMessage(uint8_t publishFlags);
        ConnectMqttMessage getConnectMqttMessage(ReaderMqttPacket &reader);
        AckSubscriptionMqttMessage getSubAckMessage(uint16_t packetId);
};

#endif