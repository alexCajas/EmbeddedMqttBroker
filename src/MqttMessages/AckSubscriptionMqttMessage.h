#ifndef ACKSUBSCRIPTIONMQTTMESSAGE_H
#define ACKSUBSCRIPTIONMQTTMESSAGE_H

#include "MqttMessage.h"
#include "MqttMessagesSerealizable.h"
#include "ControlPacketType.h" // Asumo que aquí tienes definidos los tipos como SUBACK

/**
 * @brief Class to build a AckSubscriptionMqttMessage (SUBACK).
 * MQTT SUBACK packet structure has three parts:
 * 1. Fixed header (2 bytes):
 * -> Control Packet Type (SUBACK = 9)
 * -> Flags (Reserved, must be 0)
 * -> Remaining Length (Variable, usually 3 bytes for a single topic success)
 * * 2. Variable header (2 bytes):
 * -> Packet Identifier: The same Message ID from the original SUBSCRIBE packet.
 * * 3. Payload:
 * -> Return Code: One byte per topic filter.
 * 0x00 = Success - Maximum QoS 0
 * 0x01 = Success - Maximum QoS 1
 * 0x02 = Success - Maximum QoS 2
 * 0x80 = Failure
 */
class AckSubscriptionMqttMessage: public MqttMessage, public MqttMessageSerealizable
{
private:
    uint16_t packetId;
    uint8_t returnCode;
  
public:

    /**
     * @brief Construct a new Ack Subscription Mqtt Message object.
     * * @param packetId The Message ID from the original SUBSCRIBE packet to acknowledge.
     * @param returnCode The result code (default 0x00 for Success QoS 0).
     */
    AckSubscriptionMqttMessage(uint16_t packetId, uint8_t returnCode = 0x00);
    
    /**
     * @brief Build the string representation of the SUBACK packet.
     * @return String containing the raw bytes to be sent over TCP/WS.
     */
    String buildMqttPacket() override;
};

#endif // ACKSUBSCRIPTIONMQTTMESSAGE_H