#ifndef SECURE_WS_TRANSPORT_H
#define SECURE_WS_TRANSPORT_H

#include "MqttTransport.h"
#include <esp_https_server.h>

namespace mqttBrokerName {

/**
 * @brief MqttTransport implementation for WebSocket Secure (WSS).
 * Wraps the esp_https_server socket to send and receive MQTT packets over WSS.
 */
class SecureWsTransport : public MqttTransport {
private:
    httpd_handle_t server;
    int fd;
    bool _connected;
    String remoteIP;
    bool disconnectNotified; // flag to ensure unique notification

public:
    SecureWsTransport(httpd_handle_t server, int fd);
    ~SecureWsTransport();

    size_t send(const char* data, size_t len) override;
    void close() override;
    bool connected() override;
    bool canSend() override;
    size_t space() override;
    String getIP() override;

    // Listener call functions
    void handleIncomingData(uint8_t* data, size_t len);
    void handleDisconnect();
};

} // namespace mqttBrokerName

#endif // SECURE_WS_TRANSPORT_H
