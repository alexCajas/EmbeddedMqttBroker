#include "MqttMessage.h"

uint32_t MqttMessage::codeSize(size_t size){
    uint8_t encodedByte;
    uint32_t encodedBytes = 0;

    do
    {
        encodedByte = size % 128;
        size = size / 128;

        // if there are more data to encode, set the top bit of this byte
        if (size > 0)
        {
            encodedByte = encodedByte | 128;
        }
        
        encodedBytes = (encodedBytes << 8) | encodedByte;
        
    } while (size > 0);

    return encodedBytes;
}