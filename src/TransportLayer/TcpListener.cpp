#include "TcpTransport.h"
#include "MqttBroker/MqttBroker.h"

using namespace mqttBrokerName;

TcpServerListener::TcpServerListener(uint16_t port) : port(port), server(nullptr) {
}

TcpServerListener::~TcpServerListener() {
    stop();
    if (server) {
        delete server;
        server = nullptr;
    }
}

void TcpServerListener::begin() {
    if (!server) {
        server = new AsyncServer(port);
    }

    // Configure callback for new TCP connections
    server->onClient([this](void* arg, AsyncClient* client) {
        if (this->broker) {
            // 1. Wrap the raw AsyncClient into our Transport Adapter
            MqttTransport* transport = new TcpTransport(client);
            
            // 2. Inject the transport into the Broker logic
            this->broker->acceptClient(transport);
        } else {
            // No broker assigned to handle this, reject connection
            client->close();
            delete client; // Important: AsyncTCP expects us to manage this if not used
        }
    }, nullptr);

    server->begin();
    log_i("TCP Listener started on port %u", port);
}

void TcpServerListener::stop() {
    if (server) {
        server->end();
    }
}