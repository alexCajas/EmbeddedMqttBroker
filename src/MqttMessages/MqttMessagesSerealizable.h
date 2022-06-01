#ifndef MQTTMESSAGESEREALIZABLE_H
#define MQTTMESSAGESEREALIZABLE_H

/**
 * @brief Interfaz to MqttMessages that can be send by broker mqtt.
 * 
 */
class MqttMessageSerealizable{
    public:
        MqttMessageSerealizable(){

        }

        /**
         * @brief Get the String that is a representation of a mqtt packet, it is
         * to use String.getBytes() to get a bytesByffer to send by tcp connection to
         * mqtt client.
         * 
         * @return uint8_t* 
         */
        virtual String buildMqttPacket() = 0;     


};

#endif //MQTTMESSAGESEREALIZABLE_H