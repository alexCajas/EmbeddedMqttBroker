#ifndef READERMQTTPACKET_H
#define READERMQTTPACKET_H

#include "WiFi.h"
#include "MqttMessage.h"
#include "MqttTocpic.h"
/**
 * @brief Class to Read mqtt packet in byte coding, from tcp connection.
 * tcp connection provides a stream of bytes that we can left in byte buffers.
 * This class also provide utils methods to help in the decode of the mqtt packet
 * readed.
 */
class ReaderMqttPacket {

    private:

        /**
         * @brief fixedHeader byte.
         * 
         */
        uint8_t fixedHeader[1];

        /**
         * @brief remaining lengt value to decode/encode. 
         * 
         */
        size_t remainingLengt;        

        
        /**
         * @brief remaining packet, here are variable header and payload
         * if it is needed in byte coding.
         */
        uint8_t * remainingPacket;

        /**
         * @brief read remainLengt field and decode the size of mqtt packet.
         * @param client who is sendig the packet.
         * @return size_t remaining size of mqtt packet to read.
         */
        size_t readRemainLengtSize(WiFiClient client);


        /****************** decode ultils*************************/

        /**
         * @brief Auxilar method that concatenate two bytes in one uint16_t variable.
         * 
         * @param msByte Most significant byte. 
         * @param lsByte Lest significant byte.
         * @return uint16_t result to concatenate msByte and lsByte.
         */
        uint16_t concatenateTwoBytes(uint8_t msByte, uint8_t lsByte);      

        /**
         * @brief Store a mqtt text field in a String buff, used to save 
         *        the information throught staks's or heap's scope changes.
         * 
         * @param index where start the mqtt text field.
         * @param textFieldLengt length of mqtt text field.
         * @param textField String where the text field will store.
         * @return int the index where finally the actual field and start the next
         *         mqtt field. 
         */
        int bytesToString(int index, size_t textFieldLengt,String*textField);



    public: 

        ReaderMqttPacket();
        
        /**
         * @brief Read bytes stream that represent a mqtt message packet.
         * 
         * @param client how is sending the mqtt packet. 
         */
        void readMqttPacket(WiFiClient client);

        /**
         * @brief Get the Fixed Header like uint8_t.
         * 
         * @return uint8_t that represent fixed header.
         */
        uint8_t getFixedHeader (){
            return fixedHeader[0];
        }

        /**
         * @brief Get the Remaining Packet.
         * 
         * @return uint8_t* 
         */
        uint8_t* getRemainingPacket(){
            return remainingPacket;
        }

        /**
         * @brief Get the Remaining Packet Length
         * 
         * @return size_t 
         */
        size_t getRemainingPacketLength(){
            return remainingLengt;
        }
        /**
         * @brief Decode Mqtt packet coded in byte buffer.
         * 
         * @param client 
         * @return MqttMessage 
         */
        MqttMessage decodeMqttPacket(WiFiClient client);
        

    /*************************decode utils******************/

       /**
         * @brief Extrat a variable mqtt text field, like client-id, password, userName
         *        topic, the first two bytes of this fields containts the length of text to extract.
         * 
         * @param index where is the first byte of text field length.
         * @param textField String where to copy the text field. 
         * @return int, index where the current field ends and start the next
         *         mqtt field. 
         */
        int decodeTextField(int index, String* textField);

        /**
         * @brief Get topic from mqtt packet.
         * 
         * @param index where start the lengt of mqtt topic field.
         * @param topic MqttTopic where store the char* topic readed from tcpConnection.
         * @return int the index where the current field ends and start the next
             *         mqtt field.
         */
        int decodeTopic(int index,MqttTocpic *topic);
        
        /**
         * @brief Extrat payLoad from bytes buffer that is encoding all mqtt variable header.
         * 
         * @param index where start the payload mqtt field. Note that the size of payLoad is calculated,
         *        it is know the size of  variableHeader buff (remainingLengt), and using the above methos, it is
         *        now know many bytes are readed, payLoad length is variableHeaderLength - index.     
         * @param topic MqttTopic where store payload.
         * @return int the index where the current field ends and start the next
         *         mqtt field.  
         */
        int decodePayLoad(int index, MqttTocpic *topic);

        /**
         * @brief Extract qos level for a subscribe topic from raw mqtt packet in bytes buffer.
         * 
         * @param index where start the qos mqtt field.
         * @param topic pointer to MqttTopic where store qos level.
         * @return int the index where the current field ends and start the next
         *         mqtt field. 
         */
        int decodeQosTopic(int index,MqttTocpic *topic);   

        
        /**
         * @brief Get one byte from remaining buff and put into
         * variable.
         * 
         * @param index where byte is. 
         * @param variable where save the byte.
         * @return int the index where the current field ends and start the next
         *         mqtt field. 
         */
        int decodeOneByte(int index, uint8_t* variable);                             

        /**
         * @brief Get and concatenate two bytes from remaining packet,
         * the first byte is in index and the second is in index++;
         * 
         * @param index where is the first byte.
         * @param variable where save the bytes concatenated.
         * @return int the index where the current field ends and start the next
         *         mqtt field.
         */
        int decodeTwoBytes(int index, uint16_t *variable);
};

#endif //READERMQTTPACKET_H