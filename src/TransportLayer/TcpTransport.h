#ifndef TCP_TRANSPORT_H
#define TCP_TRANSPORT_H

#include "MqttTransport.h"
#include <AsyncTCP.h>

/**
 * @brief Concrete implementation of MqttTransport for TCP connections.
 * * This class acts as an adapter for the `AsyncTCP` library. It wraps an 
 * `AsyncClient` instance and maps its specific asynchronous events (data, 
 * disconnect, error, timeout) to the generic callbacks defined in the 
 * `MqttTransport` interface. It manages the lifecycle of the underlying 
 * TCP socket.
 * * @note **Callback Strategy:** Unlike WebSocket implementations that typically rely 
 * on a single global event handler for all connections, `TcpTransport` registers 
 * specific callbacks for *each* individual `AsyncClient` instance. This creates 
 * a direct 1:1 binding between the TCP socket events and this transport object.
 */
class TcpTransport : public MqttTransport {
private:
    AsyncClient* _client;

public:
    
    TcpTransport(AsyncClient* client) : _client(client) {
        // Configure AsyncTCP callbacks immediately
        
        // 1. Data received
        _client->onData([this](void* arg, AsyncClient* c, void* data, size_t len) {
            if (_onData) {
                _onData((uint8_t*)data, len);
            }
        });

        // 2. Disconnection
        _client->onDisconnect([this](void* arg, AsyncClient* c) {
            if (_onDisconnect) {
                _onDisconnect();
            }
        });

        // 3. Error / Timeout (Treated as disconnect)
        _client->onError([this](void* arg, AsyncClient* c, int8_t error) {
            if (_onDisconnect) _onDisconnect();
        });
        _client->onTimeout([this](void* arg, AsyncClient* c, uint32_t time) {
            if (_onDisconnect) _onDisconnect();
        });
    }

    ~TcpTransport() {
        if (_client) {
            // Ensure it is closed and deleted to prevent memory leaks
            if(_client->connected()) _client->close(true);
            delete _client; 
            _client = nullptr;
        }
    }

    void send(const char* data, size_t len) override {
        if (_client && _client->connected()) {
            _client->write(data, len);
        }
    }

    void close() override {
        if (_client) _client->close();
    }

    bool connected() override {
        return _client && _client->connected();
    }
    
    bool canSend() override {
        return _client && _client->canSend();
    }

    size_t space() override { 
        return _client ? _client->space() : 0; 
    }
    
    String getIP() override {
        return _client ? _client->remoteIP().toString() : "0.0.0.0";
    }
};

#endif // TCP_TRANSPORT_H