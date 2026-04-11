#include "MqttBroker/MqttBroker.h"
#include "SecureTcpTransport.h"
#include <lwip/sockets.h>

using namespace mqttBrokerName;


SecureTcpServerListener::SecureTcpServerListener(uint16_t port, const char* cert, const char* key)
    : port(port), server_cert(cert), server_key(key), listenerTask(nullptr) {}

SecureTcpServerListener::~SecureTcpServerListener() { stop(); }

void SecureTcpServerListener::begin() {
    if (!listenerTask) {
        listenerTask = new SecureListenerTask(this, port, server_cert, server_key);
        listenerTask->start();
    }
}

void SecureTcpServerListener::stop() {
    if (listenerTask) {
        listenerTask->stopTask();
        delete listenerTask;
        listenerTask = nullptr;
    }
}

void SecureTcpServerListener::acceptSecureClient(mbedtls_net_context* client_fd) {
    if (broker) {
        MqttTransport* transport = new SecureTcpTransport(client_fd, server_cert, server_key);
        broker->acceptClient(transport);
    } else {
        close(client_fd->fd);
        delete client_fd;
    }
}
