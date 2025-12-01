#ifndef WS_TRANSPORT_H
#define WS_TRANSPORT_H

#include "MqttTransport.h"
#include <ESPAsyncWebServer.h>

/**
 * @brief Concrete implementation of MqttTransport for WebSocket connections.
 * * This class adapts the `AsyncWebSocketClient` from the `ESPAsyncWebServer` library 
 * to the generic `MqttTransport` interface. It handles binary data transmission 
 * required for MQTT over WebSockets.
 * * @note **Callback Strategy:** Unlike TCP connections where callbacks are assigned 
 * per client, the underlying `AsyncWebSocket` uses a single **Global Event Listener** * for all connected clients. Therefore, this class provides helper methods 
 * (`handleIncomingData`, `handleDisconnect`) that must be called by the `WsServerListener` 
 * when it routes global events to this specific transport instance.
 */
class WsTransport : public MqttTransport {
private:
    AsyncWebSocketClient* _client;

public:
    
    /**
     * @brief Construct a new WsTransport object.
     * @param client Pointer to the underlying WebSocket client.
     */
    WsTransport(AsyncWebSocketClient* client) : _client(client) {
        // In WS, connection/disconnection events are managed by the global Listener,
        // so we do not assign internal callbacks from _client here as we do in TCP.
    }

    ~WsTransport() {
        // We do not delete _client; it is managed internally by AsyncWebSocket.
        // We only ensure it is closed if it is still alive.
        if (_client && _client->status() == WS_CONNECTED) {
            _client->close();
        }
    }

    // --- MqttTransport Interface Implementation ---

    size_t send(const char* data, size_t len) override {
        // MQTT over WebSockets MUST be binary.
        if (_client && _client->status() == WS_CONNECTED) {
            _client->binary((uint8_t*)data, len);
            return len;
        }

        return 0;
    }

    void close() override {
        if (_client) _client->close();
    }

    bool connected() override {
        return _client && _client->status() == WS_CONNECTED;
    }

    bool canSend() override {
        // queueIsFull() returns true if the internal WS queue is full.
        return _client && _client->status() == WS_CONNECTED && !_client->queueIsFull();
    }

    size_t space() override {
        // WebSockets library does not expose "exact free bytes" easily.
        // We return a safe arbitrary value if the queue is not full to allow sending.
        if (canSend()) return 1024; 
        return 0;
    }

    String getIP() override {
        return _client ? _client->remoteIP().toString() : "0.0.0.0";
    }

    // --- Listener Specific Methods ---

    /**
     * @brief Injects incoming data into the transport processing pipeline.
     * This method is called by the WsServerListener when data arrives for this specific client ID.
     * @param data Pointer to the received data buffer.
     * @param len Length of the received data.
     */
    void handleIncomingData(uint8_t* data, size_t len) {
        if (_onData) {
            _onData(data, len);
        }
    }

    /**
     * @brief Triggers the disconnection logic.
     * This method is called by the WsServerListener when it detects this client has disconnected.
     */
    void handleDisconnect() {
        if (_onDisconnect) {
            _onDisconnect();
        }
    }
};

#endif // WS_TRANSPORT_H