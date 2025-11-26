#ifndef MQTT_TRANSPORT_H
#define MQTT_TRANSPORT_H

#include <Arduino.h>
#include <functional>

class MqttTransport {
protected:
    // Callbacks que el MqttClient nos configurará
    std::function<void(uint8_t*, size_t)> _onData;
    std::function<void()> _onDisconnect;

public:
    virtual ~MqttTransport() {}

    // Métodos puros que deben implementar TCP (y luego WS)
    virtual void send(const char* data, size_t len) = 0;
    virtual void close() = 0;
    virtual bool connected() = 0;
    virtual bool canSend() = 0;
    virtual String getIP() = 0;

    // Configuración de hooks (Llamados por MqttClient)
    void setOnData(std::function<void(uint8_t*, size_t)> cb) { _onData = cb; }
    void setOnDisconnect(std::function<void()> cb) { _onDisconnect = cb; }
};

#endif // MQTT_TRANSPORT_H