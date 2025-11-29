#ifndef WS_TRANSPORT_H
#define WS_TRANSPORT_H

#include "MqttTransport.h"
#include <ESPAsyncWebServer.h>

class WsTransport : public MqttTransport {
private:
    AsyncWebSocketClient* _client;

public:
    WsTransport(AsyncWebSocketClient* client) : _client(client) {
        // En WS, los eventos de conexión/desconexión los gestiona el Listener global,
        // así que aquí no asignamos callbacks internos del _client como en TCP.
    }

    ~WsTransport() {
        // No borramos _client, lo gestiona el AsyncWebSocket internamente.
        // Solo cerramos si sigue vivo.
        if (_client && _client->status() == WS_CONNECTED) {
            _client->close();
        }
    }

    // --- Implementación de MqttTransport ---

    void send(const char* data, size_t len) override {
        // MQTT sobre Websockets DEBE ser binario
        if (_client && _client->status() == WS_CONNECTED) {
            _client->binary((uint8_t*)data, len);
        }
    }

    void close() override {
        if (_client) _client->close();
    }

    bool connected() override {
        return _client && _client->status() == WS_CONNECTED;
    }

    bool canSend() override {
        // queueIsFull() devuelve true si la cola interna del WS está llena
        return _client && _client->status() == WS_CONNECTED && !_client->queueIsFull();
    }

    size_t space() override {
        // Websockets no expone "bytes libres exactos" fácilmente.
        // Devolvemos un valor seguro si la cola no está llena.
        if (canSend()) return 1024; 
        return 0;
    }

    String getIP() override {
        return _client ? _client->remoteIP().toString() : "0.0.0.0";
    }

    // --- Métodos Específicos para el Listener ---

    // El Listener llamará a esto cuando llegue tráfico para este cliente
    void handleIncomingData(uint8_t* data, size_t len) {
        if (_onData) {
            _onData(data, len);
        }
    }

    // El Listener llamará a esto cuando detecte desconexión
    void handleDisconnect() {
        if (_onDisconnect) {
            _onDisconnect();
        }
    }
};

#endif // WS_TRANSPORT_H