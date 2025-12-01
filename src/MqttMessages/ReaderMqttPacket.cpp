#include "ReaderMqttPacket.h"

ReaderMqttPacket::ReaderMqttPacket(std::function<void(void)> onPacketReadyCallback)
    : _onPacketReadyCallback(onPacketReadyCallback) {
    remainingPacket = NULL;
    reset(); // Initialize all state variables
}

ReaderMqttPacket::~ReaderMqttPacket(){
    if (remainingPacket != NULL) {
        free(remainingPacket);
    }
}

void ReaderMqttPacket::reset() {
    if (remainingPacket != NULL) {
        free(remainingPacket);
        remainingPacket = NULL;
    }

    remainingLengt = 0;
    fixedHeader[0] = 0;

    _state = WAITING_FIXED_HEADER;
    _bytesReadSoFar = 0;
    _resetRemLenParser();
}

void ReaderMqttPacket::_resetRemLenParser() {
    _multiplier = 1;
    _remLenComplete = false;
    // 'remainingLengt' (public) is used as the value accumulator
    // and is reset to 0 in the main reset() function.
}

bool ReaderMqttPacket::_parseRemainingLength(uint8_t byte) {
    remainingLengt += (byte & 127) * _multiplier;
    _multiplier *= 128;

    if (_multiplier > 128 * 128 * 128) {
        log_e("Malformed remaining length.");
        // Connection should be dropped by the caller,
        // we just reset to stop processing this packet.
        reset();
        return false;
    }

    if ((byte & 128) == 0) {
        _remLenComplete = true; // Length parsing is complete
        return true;
    }

    return false; // More bytes are needed
}

void ReaderMqttPacket::addData(uint8_t* data, size_t len) {
    size_t dataIdx = 0;

    // Process all bytes in the incoming buffer
    while (dataIdx < len) {

        switch (_state) {

            case WAITING_FIXED_HEADER:
                fixedHeader[0] = data[dataIdx];
                dataIdx++;
                _state = WAITING_REMAINING_LENGTH;
                _resetRemLenParser(); // Prepare for length parsing
                break;

            case WAITING_REMAINING_LENGTH:
                // _parseRemainingLength returns true when the full
                // length field has been processed.
                if (_parseRemainingLength(data[dataIdx])) {
                    // Length successfully decoded
                    if (remainingLengt == 0) {
                        // No 'remaining packet' (e.g., PINGREQ)
                        _state = PACKET_READY;
                    } else {
                        // Allocate memory for the 'remaining packet'
                        remainingPacket = (uint8_t*) malloc(remainingLengt);
                        if (remainingPacket == NULL) {
                            log_e("Failed to allocate memory for MQTT packet!");
                            // Critical error, reset and stop processing.
                            reset();
                            return; // Exit addData immediately
                        }
                        _bytesReadSoFar = 0;
                        _state = WAITING_REMAINING_PACKET;
                    }
                }
                dataIdx++; // Consume the length byte
                break;

            case WAITING_REMAINING_PACKET:
            {
                // We need to fill the 'remainingPacket' buffer.
                // Calculate how many bytes we still need
                size_t bytesNeeded = remainingLengt - _bytesReadSoFar;
                // Calculate how many bytes are available in the current data chunk
                size_t bytesAvailable = len - dataIdx;

                // Copy the minimum of what's needed vs. what's available
                size_t bytesToCopy = min(bytesNeeded, bytesAvailable);

                memcpy(&remainingPacket[_bytesReadSoFar], &data[dataIdx], bytesToCopy);

                //Update counters
                _bytesReadSoFar += bytesToCopy;
                dataIdx += bytesToCopy;

                // Check if we have read the entire remaining packet
                if (_bytesReadSoFar == remainingLengt) {
                    _state = PACKET_READY;
                }
            }
                break;

            case PACKET_READY:
                // This state is handled by the 'if' block below.
                // If we enter here, it's a safety break.
                break;
        } // end switch

        // --- Packet Chaining Handler ---
        // If the state machine has moved to PACKET_READY...
        if (_state == PACKET_READY) {
            
            // ...trigger the callback so the packet can be processed.
            if (_onPacketReadyCallback) {
                _onPacketReadyCallback();
            }

            // ...and reset the parser for the *next* packet.
            reset();

            // The 'while' loop will continue from dataIdx if there are
            // more bytes left in the buffer (packet chaining).
        }

    } // end while
}


/*******************************************************************/
/********************** public decode utils ************************/
/* */
/* (These methods remain unchanged as they operate on the      */
/* 'remainingPacket' buffer *after* it is filled)             */
/* */
/*******************************************************************/

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

    // andvance to the next field,
    // index increment in one unit.
    index++;
    return index;
}

int ReaderMqttPacket::decodeOneByte(int index, uint8_t *variable){

    (*variable) = remainingPacket[index];
    index++;
    return index;
}

/*******************************************************************/
/********************** private decode utils ***********************/
/*******************************************************************/


uint16_t ReaderMqttPacket::concatenateTwoBytes(uint8_t msByte, uint8_t lsByte){
    return ((uint16_t)msByte << 8) + lsByte;
}


int ReaderMqttPacket::bytesToString(int index, size_t textFieldLengt,String*textField){

    for (size_t i = 0; i < textFieldLengt; i++) {
        textField->concat((char)remainingPacket[index + i]);
    }

    return index + textFieldLengt;
}