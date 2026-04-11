#ifndef SECURE_TCP_TRANSPORT_H
#define SECURE_TCP_TRANSPORT_H

#include "MqttTransport.h"
#include "WrapperFreeRTOS.h"
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>

namespace mqttBrokerName {

class SecureTcpTransport;

/**
 * @brief FreeRTOS task handling TLS handshake and I/O for a single MQTTS client.
 * Manages the mbedTLS context and reads incoming encrypted data.
 */
class SecureClientTask : public Task {
private:
    SecureTcpTransport* transport;
    mbedtls_net_context* client_fd;
    const char* server_cert;
    const char* server_key;

    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context pkey;

    volatile bool isRunning;
    bool handshake_done;
    bool isCleanedUp; // flag to avoid double memory release.
    volatile bool taskIsDead; // flag for synchronization with the destructor

    void cleanup();

public:
    SecureClientTask(SecureTcpTransport* transport, mbedtls_net_context* fd, const char* cert, const char* key);
    ~SecureClientTask();

    void run(void* data) override;
    void stopTask();
    void forceCloseSocket();
    
    mbedtls_ssl_context* getSslContext() { return &ssl; }
    bool isHandshakeDone() { return handshake_done; }
};

/**
 * @brief MqttTransport implementation for MQTTS (TLS over TCP).
 * Acts as a bridge between the MQTT broker and the SecureClientTask.
 */
class SecureTcpTransport : public MqttTransport {
private:
    SecureClientTask* clientTask;
    String remoteIP;
    volatile bool _connected;
    bool disconnectNotified; // flag to ensure a single notification

public:
    SecureTcpTransport(mbedtls_net_context* fd, const char* cert, const char* key);
    ~SecureTcpTransport();

    size_t send(const char* data, size_t len) override;
    void close() override;
    bool connected() override;
    bool canSend() override;
    size_t space() override;
    String getIP() override;

    void triggerOnData(uint8_t* data, size_t len);
    void triggerOnDisconnect();
    void setConnected(bool state);
};

} // namespace mqttBrokerName

#endif
