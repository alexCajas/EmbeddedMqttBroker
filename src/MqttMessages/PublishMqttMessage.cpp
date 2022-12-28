#include "PublishMqttMessage.h"

PublishMqttMessage::PublishMqttMessage(uint8_t publishFlags):MqttMessage(PUBLISH,publishFlags){

}

String PublishMqttMessage::buildMqttPacket(){
    
    /**
     * there is not message Id field in qos = 0, add this field in next versions.
     * 
     */
    String mqttPacket;

    //concat fixed header
    //mqttPacket.concat((char)getTypeAndFlags()); // for versions with qos > 0 support
    mqttPacket.concat((char)48); // this line must be 
    // replaced for the above line, for versions with qos > 0

    // process to concat remainingLength
        // 1ª calculate remainingLengt value
    size_t remainingLength = topic.getTopicAndPayloadLength()+2;
        
        // 2ª Encode remaininLengt value according mqtt format.
    uint32_t encodedSize = codeSize(remainingLength);  
        
        // 3ª Concatenate encoded remaining length field to mqttPacket.
    mqttPacket = concatEncodedSize(encodedSize,mqttPacket);

    // concat topic lengt field, the field have two bytes allways
    uint8_t MSB = topic.getTopicLength() >> 8;
    uint8_t LSB = topic.getTopicLength();
    mqttPacket.concat((char)MSB);
    mqttPacket.concat((char)LSB);

    // concat topic and payload.
    mqttPacket.concat(topic.getTopicAndPayLoad());

    return mqttPacket;
}

PublishMqttMessage::PublishMqttMessage(ReaderMqttPacket packetReaded):MqttMessage(packetReaded.getFixedHeader()){
    int index = 0;
    messageId = 0;

    index = packetReaded.decodeTopic(index, &topic);

    // it is for qos > 0, not implement yet!!    
    if( (this->getFlagsControlType() & 0x6) > 0){
        index = packetReaded.decodeTwoBytes(index,&messageId);
    }

    index = packetReaded.decodePayLoad(index,&topic);
}



String PublishMqttMessage::concatEncodedSize(uint32_t encodedSize, String mqttPacket){
   
    /**
     * remaining lengt field has between 1 to 4 bytes,
     * and this bytes encode the size of remaining packet.
     * The encode is: MSBit is a flag and indicate that are
     * one byte more to endcode the size.
     * 
     * Example to an encodeSize:
     *   32-26      25-17       16-9       8-1
     * [00000000] [00000000] [10001010] [00000001]
     * 
     * In this example the byte [16-9] indicate that are one more
     * byte. The others bytes indicate don't have the flag active.
     * 
     * Other example  
     *   32-26      25-17       16-9       8-1
     * [00000000] [00000000] [00000000] [00000001]
     * 
     * There are only one byte to concat.
     * 
     * Other example
     *   32-26      25-17       16-9       8-1
     * [00000000] [10100000] [10001010] [00000001] 
     * 
     * There are three bytes to conncat.
     */
    unsigned int shift = 24;
    uint8_t byteToConcat;
    for(int i = 0; i < 3 ; i++){

        // when it is use uint_8 variable = uint_32 otherVariable, 
        // otherVariable is truncated to 8 LSBites and copy into
        // variable.
        byteToConcat = (encodedSize >> shift); 

        // byteToConcat & 0x80 = byteToConcat & 100000000
        // if in byteToConcat has his MSB to 1 this operation
        // is 1xxxxxxxx & 100000000 = 10000000
        if((byteToConcat & 0x80) == 0x80 ){
            // concat remaining lengt bytes
            mqttPacket.concat((char)byteToConcat);
        }
        shift = shift - 8;
    }

    // concat last byte, is always present.
    byteToConcat = encodedSize;
    mqttPacket.concat((char)byteToConcat);

    return mqttPacket;

}
