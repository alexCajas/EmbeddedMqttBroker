#ifndef PINGREQMQTTMESSAGE_H
#define PINGREQMQTTMESSAGE_H

#include "MqttMessage.h"

/**
 * @brief Class to abstract a pingReqMqttMessage.
 * 
 */
class PingReqMqttMessage: public MqttMessage
{
private:
    
public:
    PingReqMqttMessage():MqttMessage(PINGREQ,RESERVERTO0){

    }
};

#endif //PINGREQMQTTMESSAGE_H