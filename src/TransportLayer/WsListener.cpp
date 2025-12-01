#include "MqttBroker/MqttBroker.h"

using namespace mqttBrokerName;

WsServerListener::WsServerListener(uint16_t port, const char* wsEndpoint) : port(port), wsEndpoint(wsEndpoint), webServer(nullptr), ws(nullptr) {
}

WsServerListener::~WsServerListener() {
    stop();
    if (webServer) {
        delete webServer;
        webServer = nullptr;
    }
    if (ws) {
        delete ws;
        ws = nullptr;
    }
}

void WsServerListener::begin() {
    webServer = new AsyncWebServer(port);
    ws = new AsyncWebSocket(wsEndpoint); // Standard path for MQTT over WebSockets

    // Bind the Global WebSocket Event Callback using a lambda
    ws->onEvent([this](AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
        this->onWsEvent(server, client, type, arg, data, len);
    });

    webServer->addHandler(ws);
    webServer->begin();
    log_i("WS Listener started on port %u. Path: /mqtt", port);
}

void WsServerListener::stop() {
    if (webServer) {
        webServer->end();
    }
    // Clear the routing map. 
    // Note: The WsTransport objects themselves are managed/deleted by the MqttBroker/MqttClient lifecycle.
    activeTransports.clear();
}

void WsServerListener::onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
    
    // 1. NEW CONNECTION
    if (type == WS_EVT_CONNECT) {
        // Create the Transport Adapter
        WsTransport* transport = new WsTransport(client);
        
        // Register in our routing map (WS ID -> Transport)
        activeTransports[client->id()] = transport;

        // Hand over to the Broker (which will create the MqttClient)
        if (broker) {
            broker->acceptClient(transport);
        } else {
            // No broker to handle it, close and clean up
            client->close();
            delete transport;
        }
    } 
    
    // 2. DISCONNECTION
    else if (type == WS_EVT_DISCONNECT) {
        // Find the associated transport
        auto it = activeTransports.find(client->id());
        if (it != activeTransports.end()) {
            WsTransport* transport = it->second;
            
            // Notify the transport (which notifies MqttClient -> Broker -> Delete Queue)
            transport->handleDisconnect();
            
            // Remove from OUR local routing map.
            // (The WsTransport object memory will be freed by MqttClient destructor in the Broker)
            activeTransports.erase(it);
        }
    } 
    
    // 3. INCOMING DATA
    else if (type == WS_EVT_DATA) {
        AwsFrameInfo * info = (AwsFrameInfo*)arg;
        
        // MQTT always uses binary frames. We process only if it's a final frame and binary opcode.
        // (Note: For very large fragmented MQTT messages, reassembly logic would be needed here, 
        // but this covers 99% of use cases).
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_BINARY) {
            
            auto it = activeTransports.find(client->id());
            if (it != activeTransports.end()) {
                // Inject data into the specific transport
                it->second->handleIncomingData(data, len);
            }
        }
    }
}