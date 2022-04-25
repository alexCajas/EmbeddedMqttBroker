#ifndef NOTMQTTMESSAGE.H
#define NOTMQTTMESSAGE.H

#include "MqttMessage.h"

/**
 * @brief This class is part of NullObject pattern GRASP.
 * 
 */
class NotMqttMessage: public MqttMessage 
{

public:
    NotMqttMessage(/* args */):MqttMessage(-1,0){

    }
};

#endif //NOTMQTTMESSAGE.H