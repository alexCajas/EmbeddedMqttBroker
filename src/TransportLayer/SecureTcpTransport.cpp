#include "SecureTcpTransport.h"
#include <lwip/sockets.h>

namespace mqttBrokerName {

SecureClientTask::SecureClientTask(SecureTcpTransport* transport, mbedtls_net_context* fd, const char* cert, const char* key)
    : Task("MqttsClient", 8192, TaskPrio_Low), transport(transport), client_fd(fd), server_cert(cert), server_key(key), isRunning(false), handshake_done(false), isCleanedUp(false), taskIsDead(false) {
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&srvcert);
    mbedtls_pk_init(&pkey);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
}

SecureClientTask::~SecureClientTask() { 
    isRunning = false;
    
    // Synchronization: Wait for the FreeRTOS thread to finish its execution
    // before destroying the object and freeing RAM.
    // Safety timeout of 5 seconds to prevent infinite deadlocks.
    int timeout = 500; 
    while (!taskIsDead && timeout > 0) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        timeout--;
    }
    
    if (!taskIsDead) {
        log_w("Timeout waiting for TLS thread to die. Forcing cleanup.");
    }

    cleanup(); 
}

void SecureClientTask::cleanup() {
    if (isCleanedUp) return;
    isCleanedUp = true;

    if (client_fd && client_fd->fd >= 0) {
        // Copy and mark as -1 before closing to avoid double-close due to race conditions
        int temp_fd = client_fd->fd;
        client_fd->fd = -1;
        ::close(temp_fd);
    }

    mbedtls_net_free(client_fd);
    delete client_fd;
    mbedtls_x509_crt_free(&srvcert);
    mbedtls_pk_free(&pkey);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

void SecureClientTask::stopTask() {
    isRunning = false;
    stop();
}

void SecureClientTask::forceCloseSocket() {
    isRunning = false;
    if (client_fd && client_fd->fd >= 0) {
        // Copy and mark as -1 before closing to prevent double-close
        int temp_fd = client_fd->fd;
        client_fd->fd = -1;
        ::shutdown(temp_fd, SHUT_RDWR);
        ::close(temp_fd);
    }
}

void SecureClientTask::run(void* data) {
    isRunning = true;
    int ret;
    const char *pers = "mqtts_server";
    
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers, strlen(pers));
    
    // Validate certificate parsing
    ret = mbedtls_x509_crt_parse(&srvcert, (const unsigned char *)server_cert, strlen(server_cert) + 1);
    if (ret != 0) {
        log_e("Error parsing certificate: -0x%x", -ret);
        goto exit_task;
    }
    
    // Validate private key parsing
    ret = mbedtls_pk_parse_key(&pkey, (const unsigned char *)server_key, strlen(server_key) + 1, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        log_e("Error parsing private key: -0x%x", -ret);
        goto exit_task;
    }

    mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey);
    mbedtls_ssl_setup(&ssl, &conf);
    mbedtls_ssl_set_bio(&ssl, client_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            log_e("TLS handshake failed: -0x%x", -ret);
            goto exit_task;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    
    handshake_done = true;
    transport->setConnected(true);

    unsigned char buf[1024];
    while (isRunning) {
        ret = mbedtls_ssl_read(&ssl, buf, sizeof(buf));
        if (ret > 0) {
            transport->triggerOnData(buf, ret);
        } else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        } else {
            // Error or connection closed by the client
            break;
        }
    }

exit_task:
    transport->setConnected(false);
    isRunning = false;
    
    // 1. Clean up resources BEFORE notifying the broker.
    cleanup();
    
    // 2. Store the pointer locally because 'this' might cease to exist 
    SecureTcpTransport* t = transport;
    t->triggerOnDisconnect();
    
    // 3. Mark the task as dead so the destructor can release memory safely
    taskIsDead = true;
    
    // 4. Suspend the task indefinitely. The base class 'Task' destructor 
    // will handle the safe removal from the OS using vTaskDelete.
    vTaskSuspend(NULL);
}

SecureTcpTransport::SecureTcpTransport(mbedtls_net_context* fd, const char* cert, const char* key) : _connected(false), disconnectNotified(false) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    getpeername(fd->fd, (struct sockaddr*)&addr, &addr_len);
    remoteIP = inet_ntoa(addr.sin_addr);

    clientTask = new SecureClientTask(this, fd, cert, key);
    clientTask->start();
}

SecureTcpTransport::~SecureTcpTransport() {
    if (clientTask) {
        clientTask->stopTask();
        delete clientTask;
    }
}

size_t SecureTcpTransport::send(const char* data, size_t len) {
    if (!_connected || !clientTask || !clientTask->isHandshakeDone()) return 0;
    int ret = mbedtls_ssl_write(clientTask->getSslContext(), (const unsigned char*)data, len);
    return ret > 0 ? ret : 0;
}

void SecureTcpTransport::close() {
    _connected = false;
    
    if (clientTask) {
        // Instead of killing the task, we close the socket so mbedtls_ssl_read unblocks
        clientTask->forceCloseSocket();
    }
    
    // Force immediate disconnection notification to avoid KeepAlive loops
    triggerOnDisconnect();
}

bool SecureTcpTransport::connected() { return _connected; }
bool SecureTcpTransport::canSend() { return _connected && clientTask && clientTask->isHandshakeDone(); }
size_t SecureTcpTransport::space() { return canSend() ? 2048 : 0; }
String SecureTcpTransport::getIP() { return remoteIP; }
void SecureTcpTransport::triggerOnData(uint8_t* data, size_t len) { if (_onData) _onData(data, len); }

void SecureTcpTransport::triggerOnDisconnect() { 
    if (!disconnectNotified) {
        disconnectNotified = true;
        if (_onDisconnect) _onDisconnect(); 
    }
}

void SecureTcpTransport::setConnected(bool state) { _connected = state; }

} // namespace mqttBrokerName
