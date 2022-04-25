#include "ConnectMqttMessage.h"

ConnectMqttMessage::ConnectMqttMessage(ReaderMqttPacket packetReaded):MqttMessage(packetReaded.getFixedHeader()){
    int index = 0;
    /**
     * index is necesary because it is not know where start the next
     * field in mqtt packet.
     *  It is know that lengt of protocol name is coding in tow bytes
     *      the byte varableHeader[0] and variableHeader[1]. 
     * From here it can be readed name field and it can know where the next fields
     * begings. 
     */

    // 1ª decoding protocolName
    index = packetReaded.decodeTextField(index,&protocolName);

    // 2ª decoding codeVersion
    index = packetReaded.decodeOneByte(index, &version);

    // 3ª decoding conectFlags
    index = packetReaded.decodeOneByte(index,&connectFlags);
 
    // 4ª decoding KeepAlive
    index = packetReaded.decodeTwoBytes(index,&keepAlive); 

    // 5ª decoding clientId
    index = packetReaded.decodeTextField(index,&clientID);

    /*if connectFlags continene username y password{
        decodeUsername();
        decodePassword();
    }
    **/
    
}






