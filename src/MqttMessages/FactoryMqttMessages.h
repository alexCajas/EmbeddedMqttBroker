#ifndef FACTORYMQTTMESSAGES_H
#define FACTORYMQTTMESSAGES_H

#include "MqttMessage.h"
#include "ConnectMqttMessage.h"
#include "AckConnectMqttMessage.h"
#include "PingResMqttMessage.h"
#include "PingReqMqttMessage.h"
#include "ReaderMqttPacket.h"
#include "PublishMqttMessage.h"
#include "NotMqttMessage.h"
#include "ReaderMqttPacket.h"

class FactoryMqttMessages {
    private:
    ReaderMqttPacket reader;
    
    public:
        FactoryMqttMessages();
        MqttMessage decodeMqttPacket(ReaderMqttPacket &reader);
        AckConnectMqttMessage getAceptedAckConnectMessage();
        PingResMqttMessage getPingResMessage();
        PublishMqttMessage getPublishMqttMessage(uint8_t publishFlags);
        ConnectMqttMessage getConnectMqttMessage(ReaderMqttPacket &reader);
};

#endif