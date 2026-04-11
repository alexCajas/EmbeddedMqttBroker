#ifndef MQTT_BROKER_FACTORY_H
#define MQTT_BROKER_FACTORY_H

#include "MqttBroker.h"

namespace mqttBrokerName {

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
     * * @param wsEndpont The WebSocket endpoint. Default is "/mqtt".
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

    /**
     * @brief Creates a Secure MQTT Broker over TCP (MQTTS).
     * Recomendation: If you are having ram inestability for TLS consumption, set max num clients to 4.
     * @param server_cert The server certificate (PEM format).
     * @param server_key The server private key (PEM format).
     * @param port The TCP port to listen on. Default is 8883 (IANA standard for MQTTS).
     * @return MqttBroker* Pointer to the new Broker instance.
     */
    static MqttBroker* createSecureTcpBroker(const char* server_cert, const char* server_key, uint16_t port = 8883) {
        // Create the concrete strategy for Secure TCP
        ServerListener* listener = new SecureTcpServerListener(port, server_cert, server_key);
        
        // Inject dependency and return the configured Context (Broker)
        MqttBroker* broker = new MqttBroker(listener);
        
        // Limit for TLS RAM consumption
        //broker->setMaxNumClients(4); 
        
        return broker;
    }

    /**
     * @brief Creates a Secure MQTT Broker over WebSockets (WSS).
     * Recomendation: If you are having ram inestability for TLS consumption, set max num clients to 4.
     * @param server_cert The server certificate (PEM format).
     * @param server_key The server private key (PEM format).
     * @param port The TCP port to listen on. Default is 443.
     * @param wsEndpoint The WebSocket endpoint. Default is "/mqtt".
     * @return MqttBroker* Pointer to the new Broker instance.
     */
    static MqttBroker* createSecureWsBroker(const char* server_cert, const char* server_key, uint16_t port = 443, const char* wsEndpoint = "/mqtt") {
        // Create the concrete strategy for Secure WebSockets
        ServerListener* listener = new SecureWsServerListener(port, wsEndpoint, server_cert, server_key);
        
        // Inject dependency and return the configured Context (Broker)
        MqttBroker* broker = new MqttBroker(listener);
        
        // Forces a strict limit to ensure system stability (RAM)
        //broker->setMaxNumClients(4); 
        
        return broker;
    }
};

} // namespace mqttBrokerName

#endif // MQTT_BROKER_FACTORY_H
