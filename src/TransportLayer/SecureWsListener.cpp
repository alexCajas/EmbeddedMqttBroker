#include "MqttBroker/MqttBroker.h"
#include "SecureWsTransport.h"
#include <esp_https_server.h>
#include <lwip/sockets.h>

using namespace mqttBrokerName;

// Static global pointer so the C-style handler can access the C++ instance
static SecureWsServerListener* globalWsInstance = nullptr;

SecureWsServerListener::SecureWsServerListener(uint16_t port, const char* wsEndpoint, const char* cert, const char* key)
    : port(port), wsEndpoint(wsEndpoint), server_cert(cert), server_key(key), server(nullptr) {
    globalWsInstance = this;
}

SecureWsServerListener::~SecureWsServerListener() {
    stop();
    if (globalWsInstance == this) {
        globalWsInstance = nullptr;
    }
}

void SecureWsServerListener::begin() {
    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    conf.port_secure = port;
    conf.servercert = (const uint8_t*)server_cert;
    conf.servercert_len = strlen(server_cert) + 1;
    conf.prvtkey_pem = (const uint8_t*)server_key;
    conf.prvtkey_len = strlen(server_key) + 1;

    httpd_handle_t _server = nullptr;
    esp_err_t ret = httpd_ssl_start(&_server, &conf);
    if (ret != ESP_OK) {
        log_e("Error starting WSS server: %d", ret);
        return;
    }
    server = _server; // Store in the void* pointer

    httpd_uri_t ws_uri = {
        .uri        = wsEndpoint,
        .method     = HTTP_GET,
        .handler    = (esp_err_t (*)(httpd_req_t*))ws_handler,
        .user_ctx   = NULL,
        .is_websocket = true
    };

    httpd_register_uri_handler(_server, &ws_uri);
    log_i("WSS Listener started on port %u. Path: %s", port, wsEndpoint);
}

void SecureWsServerListener::stop() {
    if (server) {
        httpd_ssl_stop((httpd_handle_t)server);
        server = nullptr;
    }
    activeTransports.clear();
}

esp_err_t SecureWsServerListener::ws_handler(struct httpd_req *req) {
    if (globalWsInstance) {
        return globalWsInstance->handleWsEvent(req);
    }
    return ESP_FAIL;
}

esp_err_t SecureWsServerListener::handleWsEvent(struct httpd_req *req) {
    httpd_req_t* request = (httpd_req_t*)req;
    int fd = httpd_req_to_sockfd(request);

    // 1. Initial Handshake (New Connection)
    if (request->method == HTTP_GET) {
        SecureWsTransport* transport = new SecureWsTransport((httpd_handle_t)server, fd);
        activeTransports[fd] = transport;
        
        if (broker) {
            broker->acceptClient(transport);
        } else {
            delete transport;
        }
        return ESP_OK;
    }

    // 2. Frame Reception
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    // Get frame length
    esp_err_t ret = httpd_ws_recv_frame(request, &ws_pkt, 0);
    if (ret != ESP_OK) {
        // If there is a read error (e.g., abrupt closure), force disconnection
        auto it = activeTransports.find(fd);
        if (it != activeTransports.end()) {
            it->second->handleDisconnect();
            activeTransports.erase(it);
        }
        return ret; // Return error so the HTTP server closes the socket
    }

    auto it = activeTransports.find(fd);
    if (it == activeTransports.end()) return ESP_FAIL;
    SecureWsTransport* transport = it->second;

    // Read payload if it exists
    if (ws_pkt.len > 0) {
        uint8_t* buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
        if (buf) {
            ws_pkt.payload = buf;
            ret = httpd_ws_recv_frame(request, &ws_pkt, ws_pkt.len);
            if (ret == ESP_OK) {
                // MQTT uses binary frames, though text is also handled
                if (ws_pkt.type == HTTPD_WS_TYPE_BINARY || ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
                    transport->handleIncomingData(ws_pkt.payload, ws_pkt.len);
                }
            } else {
                // Error reading the payload
                transport->handleDisconnect();
                activeTransports.erase(it);
                free(buf);
                return ret;
            }
            free(buf);
        }
    }

    // 3. Clean Disconnection
    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        transport->handleDisconnect();
        activeTransports.erase(it);
    }
    
    return ESP_OK;
}