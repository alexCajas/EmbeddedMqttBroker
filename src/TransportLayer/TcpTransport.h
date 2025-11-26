#ifndef TCP_TRANSPORT_H
#define TCP_TRANSPORT_H

#include "MqttTransport.h"
#include <AsyncTCP.h>

class TcpTransport : public MqttTransport {
private:
    AsyncClient* _client;

public:
    // El constructor toma posesión del socket
    TcpTransport(AsyncClient* client) : _client(client) {
        // Configurar los callbacks de AsyncTCP inmediatamente
        
        // 1. Datos
        _client->onData([this](void* arg, AsyncClient* c, void* data, size_t len) {
            if (_onData) {
                _onData((uint8_t*)data, len);
            }
        });

        // 2. Desconexión
        _client->onDisconnect([this](void* arg, AsyncClient* c) {
            if (_onDisconnect) {
                _onDisconnect();
            }
        });

        // 3. Error / Timeout (Lo tratamos como desconexión)
        _client->onError([this](void* arg, AsyncClient* c, int8_t error) {
            if (_onDisconnect) _onDisconnect();
        });
        _client->onTimeout([this](void* arg, AsyncClient* c, uint32_t time) {
            if (_onDisconnect) _onDisconnect();
        });
    }

    ~TcpTransport() {
        if (_client) {
            // Nos aseguramos de que esté cerrado y borrado
            if(_client->connected()) _client->close(true);
            delete _client; 
            _client = nullptr;
        }
    }

    // --- Implementación de la Interfaz ---

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

    String getIP() override {
        return _client ? _client->remoteIP().toString() : "0.0.0.0";
    }
};

#endif // TCP_TRANSPORT_H