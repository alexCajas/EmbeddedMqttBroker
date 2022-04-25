#ifndef PINGREQMQTTMESSAGE.H
#define PINGREQMQTTMESSAGE.H

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

#endif //PINGREQMQTTMESSAGE.H