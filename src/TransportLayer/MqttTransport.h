#ifndef MQTT_TRANSPORT_H
#define MQTT_TRANSPORT_H

#include <Arduino.h>
#include <functional>

/**
 * @brief Abstract interface for MQTT Transport layers.
 * * This class defines the contract that any network transport (TCP, WebSocket, etc.)
 * must implement to be used by the MqttClient. It decouples the MQTT protocol logic
 * from the underlying network technology, allowing the broker to support multiple
 * protocols using the Strategy Pattern.
 */
class MqttTransport {
protected:
    /**
     * @brief Callback function triggered when new data arrives.
     * Arguments: (uint8_t* data, size_t length)
     */
    std::function<void(uint8_t*, size_t)> _onData;

    /**
     * @brief Callback function triggered when the connection is lost.
     */
    std::function<void()> _onDisconnect;

public:
    virtual ~MqttTransport() {}

    /**
     * @brief Sends raw bytes over the specific transport medium.
     * * @param data Pointer to the buffer containing the data to send.
     * @param len The size of the data in bytes.
     */
    virtual void send(const char* data, size_t len) = 0;

    /**
     * @brief Closes the underlying network connection.
     */
    virtual void close() = 0;

    /**
     * @brief Checks if the connection is currently active.
     * * @return true If the socket/connection is open.
     * @return false If disconnected.
     */
    virtual bool connected() = 0;

    /**
     * @brief Checks if the transport is ready to accept write operations.
     * This is used to prevent blocking or overflowing the network stack.
     * * @return true If data can be sent.
     * @return false If the transport is busy or congested.
     */
    virtual bool canSend() = 0;

    /**
     * @brief Gets the available space in the write buffer.
     * Critical for Backpressure handling: if space is low, the MqttClient
     * should buffer the data in the Outbox instead of sending it.
     * * @return size_t Number of bytes available for writing.
     */
    virtual size_t space() = 0;

    /**
     * @brief Gets the IP address of the connected client.
     * * @return String Representation of the IP address (e.g., "192.168.1.50").
     */
    virtual String getIP() = 0;

    /**
     * @brief Registers the callback to handle incoming data.
     * Called by MqttClient to link its logic with the network events.
     * * @param cb The function to call when data is received.
     */
    void setOnData(std::function<void(uint8_t*, size_t)> cb) { _onData = cb; }

    /**
     * @brief Registers the callback to handle disconnection events.
     * Called by MqttClient to link its cleanup logic with the network events.
     * * @param cb The function to call when the connection closes.
     */
    void setOnDisconnect(std::function<void()> cb) { _onDisconnect = cb; }
};

#endif // MQTT_TRANSPORT_H