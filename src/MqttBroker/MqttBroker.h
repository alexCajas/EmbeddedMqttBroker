#ifndef MQTTBROKER_H
#define MQTTBROKER_H

#include <WiFi.h> 
#include <map>
#include <AsyncTCP.h>
#include "WrapperFreeRTOS.h"
#include "MqttMessages/FactoryMqttMessages.h"
#include "MqttMessages/SubscribeMqttMessage.h"
#include "MqttMessages/UnsubscribeMqttMessage.h"
#include "MqttMessages/PublishMqttMessage.h"
#include "TransportLayer/MqttTransport.h"
#include "TransportLayer/TcpTransport.h"
#include "TransportLayer/WsTransport.h"

namespace mqttBrokerName{

// Depends of your architecture, max num clients is exactly the 
// max num open sockets that your divece can support.
#define MAXNUMCLIENTS 16

// When tcp conection have success, on average, a packet sended over
// this tcp conecction takes at most 500milliseconds to arrive,
// so an mqtt packet takes at most 500milliseconds to arrive to the broker.
// If you mqtt client is connecting and disconnecting from the broker, you can
// try to increasing this value.
#define MAXWAITTOMQTTPACKET 500 

class CheckMqttClientTask;
class NewClientListenerTask;
class FreeMqttClientTask;
class MqttClient;
class Trie;
class NodeTrie;
class TCPListenerTask;
class Action;
class ServerListener;
class TcpServerListener;
class WsServerListener;

/**
 * @brief Defines the types of asynchronous events handled by the CheckMqttClientTask Task.
 */
enum BrokerEventType {
    /**
     * @brief A Publish packet received from a client that needs to be distributed.
     */
    EVENT_PUBLISH,

    /**
     * @brief A Subscribe packet received from a client that needs to be processed against the Trie.
     */
    EVENT_SUBSCRIBE
};

/**
 * @brief Data structure used to pass tasks from the Network Thread. 
 * to the CheckMqttClientTask Thread.
 * * This struct is designed to be lightweight. It uses a **union** to save memory,
 * assuming that a single event can only be of one type at a time.
 */
struct BrokerEvent {
    /**
     * @brief The type of event (PUBLISH or SUBSCRIBE).
     * This determines which member of the 'message' union is valid.
     */
    BrokerEventType type;

    /**
     * @brief Pointer to the client associated with this event.
     * - For SUBSCRIBE: It is the client requesting the subscription.
     * - For PUBLISH: It is usually nullptr (as broadcast doesn't depend on source), 
     * or the source client if access control is needed.
     */
    MqttClient* client; 
    
    /**
     * @brief Polymorphic container for the message object.
     * * @note **Memory Management Rule:** The objects pointed to by this union 
     * MUST be allocated on the Heap (using `new`). The CheckMqttClientTask Task assumes 
     * ownership of these pointers and is responsible for `delete`-ing them 
     * after processing.
     */
    union {
        PublishMqttMessage* pubMsg;
        SubscribeMqttMessage* subMsg;
    } message;
};

/**
 * @brief This class listen to new mqttClients, accepting or refusing his
 * connect request, also release allocated memory when a mqttClient disconnects.
 * These actions runs in concurrence.
 * 
 * By default, this broker can listen 16 open sockets at same time, you can set
 * this number according to your hardware support.
 */
class MqttBroker
{
private:
    
    /**
     * @brief Strategy Interface for network listening.
     * Polymorphic pointer that abstracts the underlying server implementation 
     */
    ServerListener* listener;

    /** @brief Port number the broker is listening on (e.g., 1883 or 8080). */
    uint16_t port;

    /** @brief Maximum number of concurrent connections allowed. */
    uint16_t maxNumClients;

    /** * @brief Rolling counter for Client ID generation.
     * Used to assign a unique internal integer ID to every new connection.
     */
    int numClient = 0;

    /**
     * @brief The Background CheckMqttClientTask.
     * A FreeRTOS task running to processes the `brokerEventQueue` 
     * and handles client cleanups to keep the Network Thread non-blocking.
     */
    CheckMqttClientTask *checkMqttClientTask;

    /**
     * @brief Data structure for efficient Topic matching.
     * Stores subscriptions and allows fast retrieval of clients interested in a specific topic.
     */
    Trie *topicTrie;


    /***************************** Synchronization Primitives ****************/

    /**
     * @brief Queue for deferred client deletion.
     * Stores pointers to `MqttTransport` objects that need to be destroyed. 
     * Used to safely delete clients outside of the network callback context.
     */
    QueueHandle_t deleteMqttClientQueue;

    /**
     * @brief Mutex for thread safety.
     * Protects shared resources (specifically the `clients` map) from concurrent 
     * access between the Network Thread (AsyncTCP/WS callbacks) and the Worker Thread.
     */
    SemaphoreHandle_t clientSetMutex;

    /**
     * @brief Queue for high-level Broker Events.
     * Buffers `BrokerEvent` structures (Publish/Subscribe requests) so they can 
     * be processed sequentially by the CheckMqttClientTask task without blocking the network.
     */
    QueueHandle_t brokerEventQueue;

    /************************* Client Registry **************************/

    /**
     * @brief Main container of active clients.
     * * **Key:** `MqttTransport*` - The pointer to the transport interface is used as the unique key.
     * * **Value:** `MqttClient*` - The instance of the client logic.
     */
    std::map<MqttTransport*, MqttClient*> clients;

public:

    /**
     * @brief Construct a new Mqtt Broker object.
     * 
     * @param port where Mqtt broker listen.
     */
    MqttBroker(ServerListener* listener);
    ~MqttBroker();

    /**
     * @brief Accepts a new physical connection from the ServerListener.
     * * This is the "Entry Point" for new clients. It is called by the `ServerListener`
     *  when a TCP handshake or WebSocket upgrade completes.
     * * @note <b>Thread Safety:</b> This method acquires the `clientSetMutex` to safely 
     * add the new client to the `clients` map.
     * * @param transport A pointer to the abstract `MqttTransport` wrapper.
     * The Broker takes ownership of this pointer.
     */
    void acceptClient(MqttTransport* transport);

    /**
     * @brief Schedules a client for safe deletion.
     * * This method is called by `MqttClient` when a disconnection occurs.  
     * It pushes the transport key into the `deleteMqttClientQueue`.
     * * The actual deletion happens later in the CheckMqttClientTask thread.
     * * @param transportKey The pointer to the transport, used as the unique key to identify the client.
     */
    void queueClientForDeletion(MqttTransport* transportKey);

    /**
     * @brief Processes pending Broker events (Publish/Subscribe).
     * * This method is called repeatedly by the `CheckMqttClientTask`.
     * It consumes the `brokerEventQueue`, unpacking the `BrokerEvent` structures 
     * and dispatching them to the internal implementation methods (`_impl`).
     * * @return true If at least one event was processed (keeps the CheckMqttClientTask busy).
     * @return false If the queue was empty (allows the CheckMqttClientTask to sleep).
     */
    bool processBrokerEvents();

    /**
     * @brief Internal implementation of the Publish logic.
     * * It queries the `Trie` to find subscribers for the 
     * given topic and iterates through the `clients` map to send the message.
     * * @note <b>Memory Management:</b> This method assumes ownership of the 
     * `PublishMqttMessage*` and is responsible for `delete`-ing it after processing.
     * * @param msg Pointer to the message object created on the Heap.
     */
    void _publishMessageImpl(PublishMqttMessage* msg);

    /**
     * @brief Internal implementation of the Subscribe logic.
     * * Executed by the CheckMqttClientTask. It interacts with the `Trie` data structure to 
     * register the client's interest in specific topics.
     * * @param msg Pointer to the subscribe message object (will be deleted after use).
     * @param client Pointer to the client requesting the subscription.
     */
    void _subscribeClientImpl(SubscribeMqttMessage* msg, MqttClient* client);

    /**
     * @brief Start the listen on port, waiting to new clients.
     */
    void startBroker();

    /**
     * @brief Stop the listen on port.
     * 
     */
    void stopBroker();

    /**
     * @brief Create and store a new MqttClient object from a tcp connection and 
     * the data on connect mqtt packet.
     * 
     * @param tcpClient socket where is the connection to this client.
     */
    void addNewMqttClient(AsyncClient *client);
    
    /**
     * @brief delete and free a MqttClient object.
     * 
     * @param clientId 
     */
    void deleteMqttClient(MqttTransport* transportKey);

    /**
     * @brief publish a mqtt message arrived to all mqtt clients interested.
     * 
     * @param publihsMqttMessage mqtt message to publish.
     */
    void publishMessage(PublishMqttMessage * publihsMqttMessage);

    /**
     * @brief Subscribe a MqttClient to a topic.
     * 
     * @param subscribeMqttMessage message where is the information of topic.
     * @param client that want to subscribe to the topic.
     */
    void SubscribeClientToTopic(SubscribeMqttMessage * subscribeMqttMessage, MqttClient* client);    

    /**
     * @brief Set the Max Num Clients that your system can support.
     * 
     * @param numMaxClients.
     */
    void setMaxNumClients(uint16_t numMaxClients){
        this->maxNumClients = numMaxClients;
    }

    /**
     * @brief check if broker has contains maxNumClients connects.
     * 
     * @return true if broker has naxNumClients connects.
     * @return false if not.
     */
    bool isBrokerFullOfClients(){
        return (clients.size() == maxNumClients);
    }

/**
     * @brief Processes the deferred client deletion queue.
     * * This method acts as the **Garbage Collector** of the Broker. It is executed 
     * by the CheckMqttClientTask Task. It consumes the `deleteMqttClientQueue`, which 
     * contains references to clients that have disconnected on the Network Thread.
     * * **Why is this needed?** Deleting an object directly inside its own callback causes 
     * "use-after-free" crashes. By deferring deletion to this separate thread/loop, 
     * we ensure the object is safely destroyed only when it is no longer in use.
     * * @return true If at least one client was deleted (signals the CheckMqttClientTask to keep running).
     * @return false If the queue was empty.
     */
    bool processDeletions();

    /**
     * @brief Periodically checks for client inactivity (Keep-Alive).
     * * This method iterates through the entire registry of active clients. For each 
     * connected client, it calculates the time elapsed since the last received packet.
     * If this time exceeds 1.5x the negotiated Keep-Alive interval (as per MQTT Spec), 
     * the client is forcibly disconnected.
     * * @note <b>Thread Safety:</b> Since this method reads the `clients` map (which is written 
     * to by the Network Thread on new connections), it acquires the `clientSetMutex` 
     * to prevent iterator invalidation crashes.
     */
    void processKeepAlives();
};

/**
 * @brief Abstract interface for Network Server Listeners.
 * * This class applies the **Strategy Pattern** to decouple the connection acceptance logic
 * from the main Broker logic. Concrete implementations (like TcpServerListener or 
 * WsServerListener) inherit from this class to handle protocol-specific details 
 * of starting a server and accepting incoming connections.
 */
class ServerListener {
protected:
    /**
     * @brief Pointer to the main MqttBroker.
     * The listener uses this to notify the broker when a new client connects
     * by calling `broker->acceptClient()`.
     */
    MqttBroker* broker = nullptr;

public:
    virtual ~ServerListener() {}

    /**
     * @brief Injects the MqttBroker dependency (Observer/Callback pattern).
     * * @param b Pointer to the MqttBroker instance that owns this listener.
     */
    void setBroker(MqttBroker* b) { broker = b; }

    // --- Lifecycle Methods ---

    /**
     * @brief Starts the underlying network server.
     * Implementation should initialize the specific server (e.g., AsyncTCP, WebServer)
     * and begin listening on the configured port.
     */
    virtual void begin() = 0;

    /**
     * @brief Stops the underlying network server.
     * Implementation should stop listening and release network resources.
     */
    virtual void stop() = 0;
};

/**
 * @brief Concrete implementation of ServerListener for TCP connections.
 * * This class wraps the `AsyncServer` from the AsyncTCP library. It is responsible
 * for listening on a specific TCP port and handling raw incoming TCP connections.
 * When a connection is established, it wraps the `AsyncClient` in a `TcpTransport`
 * adapter and passes it to the `MqttBroker`.
 */
class TcpServerListener : public ServerListener {
private:
    /**
     * @brief The port number to listen on.
     */
    uint16_t port;

    /**
     * @brief Pointer to the underlying AsyncTCP server instance.
     */
    AsyncServer* server;

public:
    /**
     * @brief Construct a new Tcp Server Listener.
     * * @param port The TCP port to listen on (default MQTT is 1883).
     */
    TcpServerListener(uint16_t port);

    /**
     * @brief Destroy the Tcp Server Listener.
     * Stops the server and releases allocated memory.
     */
    ~TcpServerListener();

    /**
     * @brief Starts the TCP server.
     * * This method initializes the `AsyncServer`, sets up the `onClient` callback
     * to handle new connections, and begins listening on the specified port.
     * When a client connects, it creates a `TcpTransport` and notifies the Broker.
     */
    void begin() override;

    /**
     * @brief Stops the TCP server.
     * Stops listening for new connections.
     */
    void stop() override;
};

/**
 * @brief Concrete implementation of ServerListener for WebSocket connections.
 * * This class acts as a bridge between the `ESPAsyncWebServer` library and the 
 * `MqttBroker`. It listens on a specific TCP port (usually 8080 or 80) for 
 * HTTP/WebSocket upgrade requests on the "/mqtt" path.
 * * @note **Routing Strategy:** Unlike `AsyncTCP` which provides per-client callbacks, 
 * `AsyncWebSocket` triggers a single **Global Event** for all connected clients. 
 * Therefore, this class maintains an internal map (`activeTransports`) to route 
 * global events (Data, Disconnect) to the specific `WsTransport` instance 
 * associated with a client ID.
 */
class WsServerListener : public ServerListener {
private:
    /**
     * @brief The port to listen on.
     */
    uint16_t port;

    /**
     * @brief Pointer to the underlying Async Web Server.
     */
    AsyncWebServer* webServer;

    /**
     * @brief Pointer to the WebSocket handler (manages the /mqtt endpoint).
     */
    AsyncWebSocket* ws;

    /**
     * @brief Routing Map: WebSocket Client ID -> Transport Instance.
     * Used to dispatch global events to the correct specific transport.
     */
    std::map<uint32_t, WsTransport*> activeTransports;

public:
    /**
     * @brief Construct a new Ws Server Listener.
     * @param port The port to listen on (e.g., 8080).
     */
    WsServerListener(uint16_t port);

    /**
     * @brief Destroy the Ws Server Listener.
     * Stops the server and releases memory.
     */
    ~WsServerListener();

    /**
     * @brief Starts the WebSocket server.
     * Initializes the WebServer, attaches the WebSocket handler to "/mqtt",
     * binds the global event callback, and begins listening.
     */
    void begin() override;

    /**
     * @brief Stops the WebSocket server.
     * Ends the server instance and clears the internal routing map.
     */
    void stop() override;

private:
    /**
     * @brief Internal handler for Global WebSocket Events.
     * This method acts as a dispatcher/router. It receives events for ALL clients
     * and forwards them to the correct `WsTransport` based on the client ID.
     * * @param server The WebSocket handler instance.
     * @param client The specific client that triggered the event.
     * @param type The type of event (CONNECT, DISCONNECT, DATA).
     * @param arg Event specific argument (e.g., frame info).
     * @param data Pointer to the data buffer.
     * @param len Length of the data.
     */
    void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
};


/*********************** Tasks **************************/

/**
 * @brief The Background Worker Task (The Engine).
 * * This class implements the **Worker Thread Pattern**. It runs on a separate FreeRTOS 
 * task (typically pinned to **Core 0**), isolating heavy processing logic from the 
 * Network I/O events (which occur on **Core 1** via AsyncTCP).
 * * Its main responsibilities are:
 * 1. **Event Processing:** Consuming the `brokerEventQueue` to handle PUBLISH routing 
 * and SUBSCRIBE trie insertions sequentially.
 * 2. **Garbage Collection:** Consuming the `deleteMqttClientQueue` to safely destroy 
 * disconnected clients without race conditions.
 * 3. **Maintenance:** Periodically checking Keep-Alive timeouts.
 */
class CheckMqttClientTask : public Task {
private:
    /** @brief Reference to the Broker instance to access queues and implementation methods. */
    MqttBroker *broker;

public:
    /**
     * @brief Construct a new Worker Task.
     * * Initializes the task configuration (stack size, priority). It does not start 
     * the task immediately; `start()` must be called explicitly by the Broker.
     * * @param broker Pointer to the main MqttBroker instance.
     */
    CheckMqttClientTask(MqttBroker *broker);
    
    /**
     * @brief The Main Event Loop.
     * * This method contains the infinite loop that drives the Broker's logic.
     * It continuously polls the Broker's queues (Events and Deletions) and 
     * performs periodic maintenance.
     * * It uses an adaptive sleep strategy (`taskYIELD` under load, `vTaskDelay` when idle) 
     * to balance throughput and power consumption.
     * * @param data Unused parameter required by FreeRTOS task signature.
     */
    void run(void *data) override;
};

/***************************************** MqttClient Class ***********************/


/**
 * @brief When broker reveived a Mqtt packet, hi has to do some action,
 * and this action depends on the kind of mqtt packet arrived. To implement
 * an action that can change dinamycally it is used Strategy GRASP Pattern.
 * There is other problem, at first, it is not known what action it is needed,
 * but it is needed create this action and use it when a mqtt packet is arrived,
 * this problem is resolved with Factory Method GRASP Pattern.
 * 
 */
class Action{
    protected:
        MqttClient* mqttClient;
    public:
        /**
         * @brief Construct a new Action object
         * 
         * @param mqttClient context of Strategy GRASP Pattern.
         */
        Action(MqttClient *mqttClient){
            this->mqttClient = mqttClient;
        }


        virtual ~Action(){}

        /**
         * @brief Action to implement.
         * 
         */
        virtual void doAction();
};


/**
 * @brief Defines the internal lifecycle states of an MQTT client connection.
 * * This enum is used to implement the **State Pattern** within the `MqttClient`. 
 * It ensures that the protocol strictness is maintained: a client cannot perform 
 * any actions (Publish, Subscribe) until it has successfully completed the 
 * MQTT Handshake (CONNECT packet).
 */
enum MqttClientState {
    /**
     * @brief Initial state waiting for the MQTT Handshake.
     * * The client enters this state immediately after the TCP connection is accepted.
     * In this state, the Broker expects the **first** received packet to be a valid 
     * `CONNECT` message. If any other packet type is received, or if the `CONNECT` 
     * packet is malformed, the client will be disconnected immediately.
     */
    STATE_PENDING,

    /**
     * @brief Authenticated and operational state.
     * * The client transitions to this state after a valid `CONNECT` packet is processed 
     * and a `CONNACK` has been sent. In this state, the client is fully authorized 
     * to send `PUBLISH`, `SUBSCRIBE`, `PINGREQ`, and other MQTT control packets. 
     * The Keep-Alive timer is active in this state.
     */
    STATE_CONNECTED
};

/**
 * @brief Represents a single connected MQTT Client.
 * * This class acts as the **Session Manager** for an MQTT connection. It is responsible for:
 * 1. Managing the lifecycle of the connection (Handshake vs. Operational state).
 * 2. Parsing incoming byte streams using `ReaderMqttPacket`.
 * 3. Maintaining client state (Subscriptions, KeepAlive, Client ID).
 * 4. acting as the bridge between the abstract Network Layer (`MqttTransport`) 
 * and the logic layer (`MqttBroker`).
 * * It is agnostic to the underlying protocol (TCP or WebSocket) thanks to the 
 * `MqttTransport` abstraction.
 */
class MqttClient
{
private:
    /** @brief Unique Client ID assigned by the Broker. */
    int clientId;

    /** * @brief Abstract interface for network communication. 
     * Can be an instance of `TcpTransport` or `WsTransport`.
     */
    MqttTransport* transport;

    /** @brief State machine parser for incoming MQTT packets. */
    ReaderMqttPacket *reader;

    /** @brief Current internal state (STATE_PENDING or STATE_CONNECTED). */
    MqttClientState _state;

    /** * @brief Pointer to the deletion queue. 
     * @note In the latest architecture, the client calls `broker->queueClientForDeletion`, 
     * so this member might be legacy depending on implementation details.
     */
    QueueHandle_t *deleteMqttClientQueue;

    /** @brief Maximum inactivity time (in seconds) allowed before disconnection. */
    uint16_t keepAlive;

    /** @brief Timestamp (millis) of the last packet received from this client. */
    unsigned long lastAlive;

    /** @brief Factory to create response messages (CONNACK, PINGRESP, etc.). */
    FactoryMqttMessages messagesFactory;

    /** @brief Current action being processed (Strategy Pattern for packet handling). */
    Action *action;
    
    /**
     * @brief Registry of Trie Nodes this client is subscribed to.
     * Used to efficiently unsubscribe the client from the Topic Trie 
     * upon disconnection without searching the entire tree.
     */
    std::vector<NodeTrie*> nodesToFree;

    /** @brief Pointer to the main Broker instance (The Owner). */
    MqttBroker *broker;

    /**
     * @brief Sends a serialized MQTT packet over the network.
     * * It uses the `transport` abstraction to send data. 
     * @note **QoS 0 Implementation:** Currently, if the transport buffer is full,
     * the packet is dropped to prevent blocking the asynchronous loop. 
     * Future improvements for QoS 1/2 should implement an Outbox queue here.
     * * @param mqttPacket The raw string/bytes of the MQTT packet to send.
     */
    void sendPacketByTcpConnection(String mqttPacket);

    /**
     * @brief Operational Callback: Processes standard MQTT packets.
     * * This method is the callback for the `ReaderMqttPacket` when the client 
     * is in `STATE_CONNECTED`. It uses `ActionFactory` to execute logic 
     * for PUBLISH, SUBSCRIBE, PINGREQ, etc.
     */
    void proccessOnMqttPacket();

    /**
     * @brief Handshake Callback: Processes the initial CONNECT packet.
     * * This method is the callback for the `ReaderMqttPacket` when the client 
     * is in `STATE_PENDING`. It validates that the first packet is strictly 
     * a CONNECT packet. If valid, it transitions the state to CONNECTED; 
     * otherwise, it disconnects the client.
     */
    void processOnConnectMqttPacket();
    

public:

    /**
     * @brief Construct a new Mqtt Client object.
     * * Initializes the client, sets up the `ReaderMqttPacket`, and configures
     * the callbacks on the `MqttTransport` to bind network events to this object.
     * * @param transport The abstract network wrapper (TCP or WS) created by the Listener.
     * @param clientId The unique ID assigned by the Broker.
     * @param broker Pointer to the managing Broker instance.
     */
    MqttClient(MqttTransport* transport, int clientId, MqttBroker * broker);

    /**
     * @brief Destroy the Mqtt Client object.
     * * Cleans up resources, unsubscribes from all topics in the Trie, 
     * and closes/deletes the underlying transport.
     */
    ~MqttClient();

    /**
     * @brief Get the unique Client ID.
     * @return int The client ID.
     */
    int getId(){return clientId;}

    /**
     * @brief Notifies the Broker that a PUBLISH message has been received.
     * * This delegates the routing logic to the Broker, which will find 
     * matching subscribers.
     * * @param publishMessage The parsed Publish message object.
     */
    void notifyPublishRecived(PublishMqttMessage *publishMessage);

    /**
     * @brief Sends a PUBLISH message TO this client.
     * * Called by the Broker/Worker when this client is identified as a subscriber
     * for a topic. It serializes the message and sends it via the transport.
     * * @param publishMessage The message object to send.
     */
    void publishMessage(PublishMqttMessage *publishMessage);

    /**
     * @brief Processes a SUBSCRIBE request from this client.
     * * Delegates the subscription logic (updating the Trie) to the Broker.
     * * @param subscribeMqttMessage The parsed Subscribe packet.
     */
    void subscribeToTopic(SubscribeMqttMessage * subscribeMqttMessage);

    /**
     * @brief Sends a PINGRESP packet to the client.
     * Response to a PINGREQ to keep the connection alive.
     */
    void sendPingRes();    
    
    /**
     * @brief Forcibly closes the network connection.
     * This will trigger the `onDisconnect` callback in the transport layer,
     * eventually leading to the cleanup of this object.
     */
    void disconnect();

    /**
     * @brief Registers a Trie Node to this client.
     * * When the client subscribes to a topic, the Trie node is added here.
     * This allows for fast unsubscription (cleanup) when the client disconnects.
     * * @param node Pointer to the NodeTrie.
     */
    void addNode(NodeTrie *node){
        nodesToFree.push_back(node);
    }

    /**
     * @brief Sets the Keep Alive interval.
     * Usually called after parsing the CONNECT packet.
     * @param keepAlive Time in seconds.
     */
    void setKeepAlive(uint16_t keepAlive){
        this->keepAlive = keepAlive;
    }

    /**
     * @brief Checks if the client has timed out.
     * * Called periodically by the Broker's Worker. It compares the current time
     * against `lastAlive`. If the limit (1.5x KeepAlive) is exceeded, 
     * it triggers disconnection.
     * * @param currentMillis The current system time.
     * @return true if the client is still alive, false if disconnected.
     */
    bool checkKeepAlive(unsigned long currentMillis);
    
    /**
     * @brief Gets the current lifecycle state.
     * @return MqttClientState (PENDING or CONNECTED).
     */
    MqttClientState getState() { return _state; }

};


/**
 * @brief Strategy GRASP: When borker received a mqtt publish request packet,
 * borker has to send this publish packet to all interested clients. This class
 * implements this logic.
 * 
 */
class PublishAction: public Action{
    private:
        PublishMqttMessage *publishMqttMessage;

    public:

        /**
         * @brief Construct a new Publish Action object
         * 
         * @param mqttClient context of the Strategy.
         * @param packetReaded object where are all information to instance a
         * PublishMqttMessage object.
         */
        PublishAction(MqttClient* mqttClient, ReaderMqttPacket &packetReaded);
        ~PublishAction();
        /**
         * @brief Notify to Broker class a publish mqtt request
         * recevided from this client.
         * 
         */
        void doAction() override;
};


/**
 * @brief Strategy GRASP: When a mqttClient wants to subscribe to some topic, this topci
 * need to be store on the topics vector of the current mqttClient. This class
 * implements this logic.
 * 
 */
class SubscribeAction:public Action{
    private:
        SubscribeMqttMessage *subscribeMqttMessage;
    public:

        /**
         * @brief Construct a new Subscribe Action object
         * 
         * @param mqttClient context of the Strategy.
         * @param readedPacket object where are all information to build a SubscribeMqttMessage
         * object.
         */
        SubscribeAction(MqttClient *mqttClient,ReaderMqttPacket &readedPacket);
        ~SubscribeAction();

        void doAction() override;
};

/**
 * @brief Strategy GRASP: When an unsubscribe mqtt packet has arrived, broker
 * has to delete the topics in mqttClient that match with
 * topics in unsubscribe mqtt packet. This class implements this logic.
 * 
 */
class UnSubscribeAction:public Action{
    private:
        UnsubscribeMqttMessage *unsubscribeMqttMessage;

    public:

        /**
         * @brief Construct a new Un Subscribe Action object.
         * 
         * @param mqttClient context of Strategy.
         * @param readedPacket object where are all information to build a UnsubscribeMqttMessage
         * object. 
         */
        UnSubscribeAction(MqttClient *mqttClient,ReaderMqttPacket &packetReaded);
        ~UnSubscribeAction();

        void doAction() override;
};

/**
 * @brief Strategy GRASP: it is necessary a Strategy that do nothing, to avoid consistency problems, it is applied NullObject
 * GRASP Pattern.
 * 
 */
class NoAction: public Action{
    public:
        NoAction(MqttClient *mqttClient);

        /**
         * @brief Do nothing.
         * 
         */
        void doAction() override;
};
/**
 * @brief Strategy GRASP: Send ping response over tcp connection. This method 
 * implements this logic.
 * 
 */
class PingResAction:public Action{
    public:

        /**
         * @brief Construct a new Ping Res Action object
         * 
         * @param mqttClient context of the Strategy.
         */
        PingResAction(MqttClient * mqttClient);

        /**
         * @brief Send ping response over tcpConnection.
         * 
         */
        void doAction() override;
};

/**
 * @brief Strategy GRASP: When a Disconnect resq packet is arrived, the tcpConnection
 * must be close, and this MqttClient must be deleted. This class implements this logic.
 * 
 */
class DisconnectAction: public Action{
    public:

        /**
         * @brief Construct a new Disconnect Action object
         * 
         * @param mqttClient contex of the Strategy.
         */
        DisconnectAction(MqttClient* mqttClient);

        void doAction() override;
};

/**
 * @brief Class that implements Factory Method to dispatch actions
 * objects.
 * 
 */
class ActionFactory
{
private:
    /* data */
public:
    ActionFactory();
    
    /**
     * @brief Get the Action object
     * 
     * @param mqttClient context to pass to Action object. 
     * @param packetReaded context that has the information to know what kind of
     *               Action object it is needed.
     * @return Action* 
     */
    Action* getAction(MqttClient * mqttClient, ReaderMqttPacket &packetReaded);

};


/****************************** NodeTrie Class *****************************/

/**
 * @brief Node for a prefix tree structure.
 * 
 */
class NodeTrie
{
private:
    char character;
    NodeTrie *bro, *son;
    std::map<int, MqttClient*> *subscribedClients;

    /**
     * @brief When some client subscribe to a topic usin "+" wildcard, the tree
     * creates a branch with the character "+". This method check if "+" node
     * is present in this topic level and explore that branch, if is so. If there is a match with
     * the topic, put into clients varable, all mqttClients found.
     * @param clientsIds vector where store the id of mqttClients.
     * @param topic where is looking for in the tree.
     * @param index where start the topic level.
     */
    void matchWithPlusWildCard(std::vector<MqttClient*>* clients,String topic, int index);
    
    /**
     * @brief When some client subscribe to a topic usin "#" wildcard, the tree
     * creates a branch with the character "#". This method checks if "#" node
     * is present in this topic level, if is so, put into clients varable, all mqttClients found.
     * @param clientsIds vector where store the id of mqttClients.
     * @param topic where is looking for in the tree.
     */
    void matchWithNumberSignWildCard(std::vector<MqttClient*>* clients,String topic);

public:
    NodeTrie();
    ~NodeTrie();

    /**
     * @brief Search a character in the tree, if the char is found
     * return the son node of node where the character was found . This method guides
     * the proces of searching a topic in the tree.
     * 
     * @param character where that this method searches in the tree.
     * @return NodeTrie* son of current character, NULL in other case.
     */
    NodeTrie *find(char character);

    /**
     * @brief insert a new character in the prefix, the insertion is 
     * in order. If character == '$', the character will insert in the current NodeTrie.
     * 
     * @param character to insert.
     * @param son new node to insert in the prefix.
     */
    void insert(char character, NodeTrie *son);

    /**
     * @brief insert the 
     * new character in the current prefix and create a new son for
     * the new char.
     * 
     * @param character to insert.
     */
    void takeNew(char character);

    /**
     * @brief Add a mqttClient in subscribed clients map. 
     * 
     * @param client to add .
     */
    void addSubscribedMqttClient(MqttClient* client);

    /**
     * @brief Get the Subscribed Mqtt Clients map.
     * 
     * @return std::map<int, MqttClient*>*.
     */
    std::map<int, MqttClient*> * getSubscribedMqttClients(){
        return subscribedClients;
    }

    /**
     * @brief This method insert the mqttClients subscribed to the topic in
     * a map. Here is implemented the search of mqttClients subscribed by wildcards. 
     * 
     * @param clientsIds vector where store all id of mqttClients subscribed to topic.
     * @param topic that clients are subscribed.
     * @param index where start the proccesing of topic.
     */
    void findSubscribedMqttClients(std::vector<MqttClient*>* clients, String topic, int index);

    void unSubscribeMqttClient(MqttClient * mqttClient){
        subscribedClients->erase(mqttClient->getId());
    }

    int getNumSubscribedClients(){
        return subscribedClients->size();
    }
};

/******************************************* Trie Class ************************************/

/**
 * @brief prefix trie class.
 * 
 */
class Trie
{
private:
    NodeTrie *root;
    int numElem;

public:
    Trie();
    ~Trie();

    /**
     * @brief free all memory of the current trie.
     * 
     */
    void clear(void);

    /**
     * @brief Insert a topic in the tree.
     * 
     * @param topic to insert.
     * @return NodeTrie* where is the end mark of the prefix '$',
     *         this node has the subscribed clients map.
     */
    NodeTrie* insert(String topic);

    /**
     * @brief find a topic in the tree. 
     * 
     * @param topic to find.
     * @return true if topic is in the tree. 
     * @return false in other case.
     */
    bool find(String topic);

    /**
     * @brief Get num topcis in the tree.
     * 
     * @return int 
     */
    int getNumElem(void) { return numElem; }

    /**
     * @brief Subscribe MqttClient* to topic, if topic is not present
     * in the tree, this method also insers it.
     * 
     * @param topic to subscribe.
     * @param client that subscribe.
     * @param NodeTrie* where the client is subscribed.
     */
    NodeTrie* subscribeToTopic(String topic, MqttClient* client);

    /**
     * @brief Get the vector of the id of subscribed mqtt clients, Warning!, this 
     * method allocate dinamically the vector but don't free this, the user is responsible
     * to free the memory allocated to avoid memory holes.
     * 
     * @param topic that mqttClients are subscribed.
     * @return std::vector<int>* vector where are all id of the mqttClients subscribed to this topic.
     */
    std::vector<MqttClient*>* getSubscribedMqttClients(String topic);

};

#endif //MQTTBROKER_H
}