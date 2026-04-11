#include "SecureWsTransport.h"
#include <lwip/sockets.h>

namespace mqttBrokerName {

SecureWsTransport::SecureWsTransport(httpd_handle_t server, int fd)
    : server(server), fd(fd), _connected(true), disconnectNotified(false) {
    
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    getpeername(fd, (struct sockaddr*)&addr, &addr_len);
    remoteIP = inet_ntoa(addr.sin_addr);
}

SecureWsTransport::~SecureWsTransport() {
    close();
}

size_t SecureWsTransport::send(const char* data, size_t len) {
    if (!_connected || !server) return 0;

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = len;
    ws_pkt.type = HTTPD_WS_TYPE_BINARY; // MQTT requires binary

    // We use the asynchronous version because send() is called from Core 0 (CheckMqttClientTask)
    // and the HTTP server runs in its own dedicated thread.
    esp_err_t ret = httpd_ws_send_frame_async(server, fd, &ws_pkt);
    
    return (ret == ESP_OK) ? len : 0;
}

void SecureWsTransport::close() {
    if (_connected && server) {
        _connected = false;
        // Request the HTTP server to close the session for this specific socket
        httpd_sess_trigger_close(server, fd);
    }
    
    // Force immediate disconnection notification to prevent KeepAlive loops
    handleDisconnect();
}

bool SecureWsTransport::connected() {
    return _connected;
}

bool SecureWsTransport::canSend() {
    return _connected;
}

size_t SecureWsTransport::space() {
    return _connected ? 2048 : 0;
}

String SecureWsTransport::getIP() {
    return remoteIP;
}

void SecureWsTransport::handleIncomingData(uint8_t* data, size_t len) {
    if (_onData) _onData(data, len);
}

void SecureWsTransport::handleDisconnect() {
    _connected = false;
    if (!disconnectNotified) {
        disconnectNotified = true;
        if (_onDisconnect) _onDisconnect();
    }
}

} // namespace mqttBrokerName
