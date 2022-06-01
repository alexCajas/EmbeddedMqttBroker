#ifndef PINGRESMQTTMESSAGE_H
#define PINGRESMQTTMESSAGE_H

#include "MqttMessage.h"
#include "MqttMessagesSerealizable.h"


/**
 * @brief Class that implements a ping response in  mqtt protocol.
 * 
 */
class PingResMqttMessage:public MqttMessage, public MqttMessageSerealizable
{
public:
    PingResMqttMessage():MqttMessage(PINGRESP,RESERVERTO0){

    }
    String buildMqttPacket(){
        String pingRespPacket;
        pingRespPacket.concat((char)getTypeAndFlags());
        pingRespPacket.concat((char)0);
        return pingRespPacket;        
    }
};



#endif //PINGRESMQTTMESSAGE_H