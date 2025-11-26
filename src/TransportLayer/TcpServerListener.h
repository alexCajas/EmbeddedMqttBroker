#ifndef TCP_SERVER_LISTENER_H
#define TCP_SERVER_LISTENER_H

#include "TcpTransport.h"
#include "MqttBroker/MqttBroker.h" // Necesario para llamar a broker->acceptClient

class TcpServerListener : public ServerListener {
private:
    uint16_t port;
    AsyncServer* server;

public:
    TcpServerListener(uint16_t port) : port(port), server(nullptr) {}

    ~TcpServerListener() {
        stop();
        if (server) delete server;
    }

    void begin() override {
        if (!server) server = new AsyncServer(port);

        // Callback cuando alguien se conecta por TCP
        server->onClient([this](void* arg, AsyncClient* client) {
            if (this->broker) {
                // 1. Convertimos AsyncClient -> TcpTransport
                MqttTransport* transport = new TcpTransport(client);
                
                // 2. Se lo pasamos al Broker
                this->broker->acceptClient(transport);
            } else {
                // Si no hay broker asignado, rechazamos
                client->close();
                delete client;
            }
        }, nullptr);

        server->begin();
        log_i("TCP Listener iniciado en puerto %u", port);
    }

    void stop() override {
        if (server) server->end();
    }
};

#endif // TCP_SERVER_LISTENER_H