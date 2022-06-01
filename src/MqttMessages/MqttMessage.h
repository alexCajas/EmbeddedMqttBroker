#ifndef MQTTMESSAGE_H
#define MQTTMESSAGE_H

#include "WiFi.h"
#include "ControlPacketType.h"
/**
 * @brief This class is an interfaz for the diferents mqtt packets. 
 * When broker receives a mqtt packet, doesn't now what kind of packet is,
 * Factory GRASP pattern solves this problem. 
 * 
 * Mqtt packets has three parts: 
 *     fixed header.
 *     Variable header.
 *     PayLoad.
 *  
 * Variable header and payload is not present in all mqtt packets, but
 * fixed header is.
 */

class MqttMessage {
    private:
        /**
         * @brief MQTT Control Packet type
         * 
         */
        uint8_t type;

        /**
         * @brief Flags specific to each MQTT Control Packet type.
         * 
         */
        uint8_t flagsControlType;



        /**
         * @brief Encode MqttMessage class to bytes buffer, needed
         * to send a message over tcp connection.
         * 
         * @param message 
         * @return uint8_t* 
         */
        uint8_t * codeMqttMessage(MqttMessage message);    

    protected:

        /**
         * @brief Code the size of mqtt packet. Note that the 
         * size in byte coding, can be coding beetwen 1 and 4 bytes.
         * 
         * @param size value in decimal to be code.
         * @return uint32_t size in byte coding.
         */
        uint32_t codeSize(size_t size); 


        public:

            /**
             * @brief Construct a new Mqtt Message object.
             * 
             * @param fixedHeader byte to encode type and flagsControlType 
             *         of a Mqtt packet.                   
             */
            MqttMessage(uint8_t fixedHeader){
                type = fixedHeader >> 4;
                flagsControlType = fixedHeader & 0x0F;
            }

            /**
             * @brief Construct a new Mqtt Message object.
             * 
             * @param type Type of mqtt packet @see ControlPacketType.h
             * @param flagsControlType of mqtt packet @see ControlPacketType.h
             */
            MqttMessage(uint8_t type, uint8_t flagsControlType){
                this->type = type;
                this->flagsControlType = flagsControlType;
            }

            uint8_t getTypeAndFlags(){
                uint8_t typeAndFlags = ((type << 4) + flagsControlType); 
                return typeAndFlags;
            }


            uint8_t getType(){
                return type;
            }

            void setFlagsControlType(uint8_t flagsControlType){
                this->flagsControlType = flagsControlType;
            }
            uint8_t getFlagsControlType(){
                return flagsControlType;
            }
};

#endif //MQTTMESSAGE_H