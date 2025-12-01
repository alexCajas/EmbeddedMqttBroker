#ifndef MQTT_BROKER_FACTORY_H
#define MQTT_BROKER_FACTORY_H

#include "MqttBroker.h"

using namespace mqttBrokerName;

/**
 * @brief Factory class for creating configured MqttBroker instances.
 * * This class implements the **Factory Method Pattern** to abstract the complexity 
 * of network listener instantiation. It allows the user to create a fully configured 
 * Broker without needing to manually.
 */
class MqttBrokerFactory {
public:
    /**
     * @brief Creates a standard MQTT Broker over TCP..
     * * @param port The TCP port to listen on. Default is 1883 (IANA standard).
     * @return MqttBroker* Pointer to the new Broker instance.
     * @note **Ownership:** The caller is responsible for managing the lifetime 
     * of the returned pointer (e.g., calling `delete` if the broker is stopped/destroyed).
     */
    static MqttBroker* createTcpBroker(uint16_t port = 1883) {
        // Create the concrete strategy for TCP
        ServerListener* listener = new TcpServerListener(port);
        
        // Inject dependency and return the configured Context (Broker)
        return new MqttBroker(listener);
    }

    /**
     * @brief Creates an MQTT Broker over WebSockets.
     * * @param port The HTTP port to listen on. Default is 8080.
     * @return MqttBroker* Pointer to the new Broker instance.
     * @note **Ownership:** The caller is responsible for managing the lifetime 
     * of the returned pointer.
     */
    static MqttBroker* createWsBroker(uint16_t port = 8080, const char* wsEndpoint = "/mqtt") {
        // Create the concrete strategy for WebSockets
        ServerListener* listener = new WsServerListener(port, wsEndpoint);
        
        // Inject dependency and return the configured Context (Broker)
        return new MqttBroker(listener);
    }
};

#endif // MQTT_BROKER_FACTORY_H