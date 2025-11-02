#ifndef READERMQTTPACKET_H
#define READERMQTTPACKET_H

#include <Arduino.h>
#include <functional>     // Required for std::function (our callback)
#include "MqttTocpic.h"   // Keep for decode utils

/**
 * @brief MQTT Packet State Machine Parser.
 *
 * This class is designed to parse an MQTT packet from an asynchronous stream
 * of bytes (e.g., from AsyncTCP's onData event). It builds a complete
 * MQTT packet in an internal buffer and triggers a callback when one
 * full packet is ready to be processed.
 */
class ReaderMqttPacket {

private:
    /**
     * @brief Internal states for the packet parsing state machine.
     */
    enum MqttPacketState {
        WAITING_FIXED_HEADER,
        WAITING_REMAINING_LENGTH,
        WAITING_REMAINING_PACKET,
        PACKET_READY
    };

    /**
     * @brief fixedHeader byte.
     */
    uint8_t fixedHeader[1];

    /**
     * @brief remaining lengt value, decoded from the stream.
     */
    size_t remainingLengt;

    /**
     * @brief remaining packet buffer (variable header and payload).
     * This buffer is allocated once the remainingLengt is known.
     */
    uint8_t * remainingPacket;

    /**
     * @brief Current state of the parser state machine.
     */
    MqttPacketState _state;

    /**
     * @brief Callback function to be executed when a full packet is ready.
     */
    std::function<void(void)> _onPacketReadyCallback;

    // --- State machine variables for parsing remaining length ---

    /**
     * @brief Multiplier for decoding the variable-length 'remainingLength' field.
     */
    int _multiplier;

    /**
     * @brief Flag to indicate if the 'remainingLength' field is complete.
     */
    bool _remLenComplete;

    // --- State machine variables for filling the remainingPacket buffer ---

    /**
     * @brief Counter for how many bytes have been copied into 'remainingPacket' so far.
     */
    size_t _bytesReadSoFar;


    /****************** Internal Parser Utils *************************/

    /**
     * @brief Resets the 'remainingLength' parser variables to their initial state.
     */
    void _resetRemLenParser();

    /**
     * @brief Processes a single byte for the variable-length 'remainingLength' field.
     *
     * @param byte The byte to process.
     * @return true if the field is complete, false if more bytes are needed.
     */
    bool _parseRemainingLength(uint8_t byte);

    /****************** Decode Utils (from original) *****************/

    /**
     * @brief Auxilar method that concatenate two bytes in one uint16_t variable.
     *
     * @param msByte Most significant byte.
     * @param lsByte Lest significant byte.
     * @return uint16_t result to concatenate msByte and lsByte.
     */
    uint16_t concatenateTwoBytes(uint8_t msByte, uint8_t lsByte);

    /**
     * @brief Store a mqtt text field in a String buff.
     *
     * @param index where start the mqtt text field.
     * @param textFieldLengt length of mqtt text field.
     * @param textField String where the text field will be stored.
     * @return int the index after the text field.
     */
    int bytesToString(int index, size_t textFieldLengt,String*textField);


public:

    /**
     * @brief Construct a new Reader Mqtt Packet object.
     *
     * @param onPacketReadyCallback The function to call when one
     * complete packet has been parsed and is ready.
     */
    ReaderMqttPacket(std::function<void(void)> onPacketReadyCallback);
    ~ReaderMqttPacket();

    /**
     * @brief Feeds new data from the asynchronous stream (onData) into
     * the state machine.
     *
     * This method processes the incoming data, builds the packet,
     * and will trigger the callback (potentially multiple times)
     * if one or more complete packets are found in the stream.
     *
     * @param data Pointer to the new data buffer.
     * @param len Length of the data in the buffer.
     */
    void addData(uint8_t* data, size_t len);

    /**
     * @brief Resets the parser state machine to wait for a new packet.
     * Frees the internal 'remainingPacket' buffer.
     */
    void reset();

    /**
     * @brief Get the Fixed Header byte.
     * Call this *after* the onPacketReadyCallback has fired.
     *
     * @return uint8_t that represent fixed header.
     */
    uint8_t getFixedHeader (){
        return fixedHeader[0];
    }

    /**
     * @brief Get the Remaining Packet buffer (variable header + payload).
     * Call this *after* the onPacketReadyCallback has fired.
     *
     * @return uint8_t* pointer to the buffer.
     */
    uint8_t* getRemainingPacket(){
        return remainingPacket;
    }

    /**
     * @brief Get the Remaining Packet Length.
     * Call this *after* the onPacketReadyCallback has fired.
     *
     * @return size_t The length of the 'remainingPacket' buffer.
     */
    size_t getRemainingPacketLength(){
        return remainingLengt;
    }

    /************************* Decode Utils ******************/
    // These methods are called by the MqttClient *after* the
    // onPacketReadyCallback has fired, operating on the
    // 'remainingPacket' buffer.

    /**
     * @brief Extract a variable mqtt text field (like client-id, topic).
     *
     * @param index where is the first byte of text field length.
     * @param textField String where to copy the text field.
     * @return int, index after the extracted field.
     */
    int decodeTextField(int index, String* textField);

    /**
     * @brief Get topic from mqtt packet.
     *
     * @param index where start the lengt of mqtt topic field.
     * @param topic MqttTopic where store the char* topic readed.
     * @return int the index after the topic field.
     */
    int decodeTopic(int index,MqttTocpic *topic);

    /**
     * @brief Extrat payLoad from the 'remainingPacket' buffer.
     *
     * @param index where start the payload mqtt field.
     * @param topic MqttTopic where store payload.
     * @return int the index after the payload field.
     */
    int decodePayLoad(int index, MqttTocpic *topic);

    /**
     * @brief Extract qos level for a subscribe topic.
     *
     * @param index where start the qos mqtt field.
     * @param topic pointer to MqttTopic where store qos level.
     * @return int the index after the qos field.
     */
    int decodeQosTopic(int index,MqttTocpic *topic);


    /**
     * @brief Get one byte from remaining buffer and put into variable.
     *
     * @param index where byte is.
     * @param variable where save the byte.
     * @return int the index after the byte.
     */
    int decodeOneByte(int index, uint8_t* variable);

    /**
     * @brief Get and concatenate two bytes from remaining packet.
     *
     * @param index where is the first byte.
     * @param variable where save the bytes concatenated.
     * @return int the index after the two bytes.
     */
    int decodeTwoBytes(int index, uint16_t *variable);

    bool isPacketReady(){
        return _state == PACKET_READY;
    }
};

#endif //READERMQTTPACKET_H