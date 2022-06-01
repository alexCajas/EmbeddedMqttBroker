#ifndef ACKCONNECTMQTTMESSAGE_H
#define ACKCONNECTMQTTMESSAGE_H

#include "MqttMessage.h"
#include "MqttMessagesSerealizable.h"
/**
 * @brief Class to build a AckConnectMqttMessage.
 * mqtt ack connect packet has to parts:
 *   fixed header with two bytes:
 *      -> controlPacketType:
 *          -> MSB encoding mqtt type
 *          -> MLB is reserved and must be 0
 *      
 *      -> Remaining lengt field
 *          -> One byte, and must set to two because variable 
 *              header has only two bytes in this kind of packet.
 * 
 *   variable header with two bytes:
 *      -> ack flags:
 *          If the Server accepts a connection with CleanSession set to 1, the Server MUST set Session Present to 0 in the CONNACK packet in addition to setting a zero return code in the CONNACK packet [MQTT-3.2.2-1].
            If the Server accepts a connection with CleanSession set to 0, the value set in Session Present depends on whether the Server already has stored Session state for the supplied client ID. 
            If the Server has stored Session state, it MUST set Session Present to 1 in the CONNACK packet [MQTT-3.2.2-2]. 
            If the Server does not have stored Session state, it MUST set Session Present to 0 in the CONNACK packet. This is in addition to setting a zero return code in the CONNACK packet [MQTT-3.2.2-3].
 *       
         -> This Server, doesn't store down connections, for this, flag is always 0
 *      
 *       -> return code of acceptance:
 *          -> one byte, here it is indicated if the connection is accepted or not and why not.
 * 
 */
class AckConnectMqttMessage: public MqttMessage, public MqttMessageSerealizable
{
private:
    uint8_t ackFlags;
    uint8_t connectReturnCode;
  
public:

    /**
     * @brief Construct a new Ack Connect Mqtt Message object.
     * 
     * @param ackFlags flags of ack packet.
     * @param connectReturnCode of ackPacket see @see ControlPacketType.h .
     */
    AckConnectMqttMessage(uint8_t ackFlags, uint8_t connectReturnCode);
    
    String buildMqttPacket();
};



#endif //ACKCONNECTMQTTMESSAGE_H