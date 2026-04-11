#include "MqttBroker/MqttBroker.h"
#include <lwip/sockets.h>

using namespace mqttBrokerName;

SecureListenerTask::SecureListenerTask(SecureTcpServerListener* listener, uint16_t port, const char* cert, const char* key)
    : Task("MqttsListener", 4096, TaskPrio_Low), listener(listener), port(port), server_cert(cert), server_key(key), isRunning(false) {}

SecureListenerTask::~SecureListenerTask() { stopTask(); }

void SecureListenerTask::stopTask() {
    isRunning = false;
    if (listen_fd.fd >= 0) {
        close(listen_fd.fd);
        listen_fd.fd = -1;
    }
    stop();
}

void SecureListenerTask::run(void* data) {
    isRunning = true;
    listen_fd.fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    
    int opt = 1;
    setsockopt(listen_fd.fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    bind(listen_fd.fd, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    listen(listen_fd.fd, 5);

    while (isRunning) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_fd.fd, (struct sockaddr *)&source_addr, &addr_len);
        
        if (sock >= 0) {
            mbedtls_net_context* client_fd = new mbedtls_net_context;
            client_fd->fd = sock;
            listener->acceptSecureClient(client_fd); //blocking, pool clients via red interuptions.
        }
    }
    vTaskDelete(NULL);
}
