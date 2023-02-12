#include "ReaderMqttPacket.h"

ReaderMqttPacket::ReaderMqttPacket(){}

void ReaderMqttPacket::readMqttPacket(WiFiClient client){
  // 1º reading fixed header.
  client.readBytes(fixedHeader,1);    

  // 2º reading and decoding remainingLengt of this mqtt packet from fixedHeader
  remainingLengt = readRemainLengtSize(client);

  // 3ª reading remaining lengt bytes packet.
  remainingPacket = (uint8_t*) malloc(remainingLengt);
  client.readBytes(remainingPacket,remainingLengt);  
}

size_t ReaderMqttPacket::readRemainLengtSize(WiFiClient client){
  int multiplier = 1;
  size_t value = 0;
  uint8_t encodedByte = 0;

  do {
    encodedByte = client.read();
    value += (encodedByte & 127) * multiplier;
    multiplier *= 128;

     if (multiplier > 128 * 128 * 128)
     {
       // throw Error(Malformed Remaining Length)
       log_e("Malformed remaining length.");
     }

  }while ((encodedByte & 128) != 0);

  return value;
}



/********************** public utils *******************************/

int ReaderMqttPacket::decodeTextField(int index, String* textField){
    
    uint8_t msByte = remainingPacket[index];
    index++; // advance to lsByte

    uint16_t textFieldLengt = concatenateTwoBytes(msByte,remainingPacket[index]);
    index++;// now we are in the first byte of text field.
    return bytesToString(index,textFieldLengt,textField);
}


int ReaderMqttPacket::decodeTopic(int index, MqttTocpic *topic){
    String topicAux;
    index = decodeTextField(index,&topicAux);
    topic->setTopic(topicAux); 
    return index;
}

int ReaderMqttPacket::decodePayLoad(int index, MqttTocpic *topic){
    size_t length = remainingLengt - index;
    String payLoad;
    if(length > 0){
        index = bytesToString(index,length,&payLoad);
        topic->setPayLoad(payLoad);
    }
    
    return index;
}

int ReaderMqttPacket::decodeQosTopic(int index, MqttTocpic * topic){
    topic->setQos(remainingPacket[index]);
    index++;
    return index;
}

int ReaderMqttPacket::decodeTwoBytes(int index, uint16_t* variable){
    uint8_t msByte = remainingPacket[index];
    index++;
    (*variable) = concatenateTwoBytes(msByte,remainingPacket[index]);
    
    // andvance to the newt field,
    // index increment in one unit.
    index++;
    return index;
}

int ReaderMqttPacket::decodeOneByte(int index, uint8_t *variable){

    (*variable) = remainingPacket[index];
    index++;
    return index; // return index++ don't send back index+=1, return index++
                  //  send back index.
}

/********************** private utils **************************/


uint16_t ReaderMqttPacket::concatenateTwoBytes(uint8_t msByte, uint8_t lsByte){
    return ((uint16_t)msByte << 8) + lsByte;
}


int ReaderMqttPacket::bytesToString(int index, size_t textFieldLengt,String*textField){
    // reserving memory to copy the mqtt text field.
    char* aux = (char*)malloc(textFieldLengt+1);
    
    // memcpy is necesary because reader object will be deleted.
    memcpy(aux,&remainingPacket[index],textFieldLengt);
    aux[textFieldLengt] = '\0';
    //textField = new String(aux); --> memory is in the context of
    // this method, the allocated memory is in the stack of this method
    // and it is losed when the method ends..
    
    textField->concat(aux);
    free(aux);
    return index+textFieldLengt;
}
