#include "MqttBroker.h"

using namespace mqttBrokerName;

MqttBroker::MqttBroker(ServerListener* listener) {
    this->maxNumClients = MAXNUMCLIENTS;
    this->listener = listener;
    
    // Inject dependencies: The listener needs a reference back to the broker
    // to notify when new clients connect.
    if (this->listener) {
        this->listener->setBroker(this);
    }
    
    topicTrie = new Trie();

    // 1. Create Queues
    // deleteMqttClientQueue stores pointers to MqttTransport objects that need cleanup.
    deleteMqttClientQueue = xQueueCreate(20, sizeof(MqttTransport*)); 
    if (!deleteMqttClientQueue) {
        log_e("Failed to create delete queue"); ESP.restart();
    }

    // brokerEventQueue stores pointers to BrokerEvent structs for the Worker.
    brokerEventQueue = xQueueCreate(50, sizeof(BrokerEvent*)); 
    if (!brokerEventQueue) {
        log_e("Failed to create brokerEventQueue"); ESP.restart();
    }

    // 2. Create Mutex
    // Required to protect the 'clients' map from concurrent access by Core 1 (Network) and Core 0 (Worker).
    clientSetMutex = xSemaphoreCreateMutex();
    if (!clientSetMutex) {
        log_e("Failed to create mutex"); ESP.restart();
    }

    // 3. Instantiate Worker (Core 0)
    // The worker task handles heavy processing to keep the network loop non-blocking.
    this->checkMqttClientTask = new CheckMqttClientTask(this);
    this->checkMqttClientTask->setCore(0);
}


MqttBroker::~MqttBroker() {
    stopBroker();

    if (checkMqttClientTask) {
        delete checkMqttClientTask; 
    }
    
    // The Broker owns the Listener, so we are responsible for deleting it.
    if (listener) {
        delete listener;
    }
    
    if (topicTrie) {
        delete topicTrie;
    }
    
    // Clean up all active clients.
    // Iterating and deleting here will close their transports and free memory.
    for (auto const& [transport, client] : clients) {
        delete client; 
    }
    clients.clear();
    
    // Drain and clean up pending events in the queue to prevent leaks.
    BrokerEvent* event;
    while(xQueueReceive(brokerEventQueue, &event, 0) == pdPASS) {
        // Ideally, we should also delete the inner message objects (pubMsg/subMsg) here.
        delete event;
    }
    vQueueDelete(brokerEventQueue);
    vQueueDelete(deleteMqttClientQueue);

    vSemaphoreDelete(clientSetMutex);
}

// --- CONTROL ---

void MqttBroker::startBroker() {
    // Start the specific network listener (TCP or WebSocket)
    if (listener) {
        listener->begin();
        log_i("MqttBroker Listener Started");
    }

    // Start the background worker task
    checkMqttClientTask->start();
}

void MqttBroker::stopBroker() {
    if (listener) {
        listener->stop();
    }
    if (checkMqttClientTask) {
        checkMqttClientTask->stop();
    }
}

// --- CLIENT MANAGEMENT (Incoming Connections) ---

void MqttBroker::acceptClient(MqttTransport *transport) {
    
    // 1. Capacity check
    if (isBrokerFullOfClients()) {
        log_w("Broker full. Rejecting client IP: %s", transport->getIP().c_str());
        transport->close();
        delete transport; // Must delete the wrapper since we won't store it
        return;
    }
    
    // 2. Critical Section: Add to the map
    if (xSemaphoreTake(clientSetMutex, portMAX_DELAY) == pdTRUE) {
        numClient++; 
        int newId = numClient;

        // Instantiate MqttClient, injecting the abstract transport.
        // The MqttClient constructor will configure the transport callbacks.
        MqttClient *mqttClient = new MqttClient(transport, newId, this);
        
        // Store in the map using the transport pointer as the unique key.
        clients[transport] = mqttClient;
        
        xSemaphoreGive(clientSetMutex);
        
        log_i("Client Accepted. ID: %i, IP: %s", newId, transport->getIP().c_str());
    } else {
        log_e("Mutex Error. Rejecting.");
        transport->close();
        delete transport;
    }
}

// --- CLIENT DELETION (Cleanup) ---

void MqttBroker::queueClientForDeletion(MqttTransport* transportKey) {
    // Push the transport pointer to the queue. The Worker will process this later.
    xQueueSend(deleteMqttClientQueue, &transportKey, 0);
}

void MqttBroker::deleteMqttClient(MqttTransport* transportKey) {
    MqttClient* clientToDelete = nullptr;

    // Critical Section: Remove from map
    if (xSemaphoreTake(clientSetMutex, portMAX_DELAY) == pdTRUE) {
        auto it = clients.find(transportKey);
        
        if (it != clients.end()) {
            clientToDelete = it->second;
            clients.erase(it); // Remove the entry from the map
            log_i("Client removed from map.");
        } else {
            log_w("Client not found in map for deletion.");
        }
        xSemaphoreGive(clientSetMutex);
    }

    // Actual deletion happens OUTSIDE the mutex to prevent deadlocks
    // (e.g., if the destructor needs to access other locked resources).
    if (clientToDelete != nullptr) {
        delete clientToDelete; // MqttClient destructor handles cleanup of Transport and Reader
        log_v("Client object memory freed.");
    }
}

// --- WORKER LOGIC ---

bool MqttBroker::processDeletions() {
    MqttTransport* transportKey;
    bool workDone = false;
    
    // Process all pending deletion requests
    while (xQueueReceive(deleteMqttClientQueue, &transportKey, 0) == pdPASS) {
        deleteMqttClient(transportKey);
        workDone = true;
    }
    return workDone;
}

void MqttBroker::processKeepAlives() {
    unsigned long now = millis();

    // Protect map iteration
    if (xSemaphoreTake(clientSetMutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
        for (auto const& [transport, client] : clients) {
            // Only check KeepAlive for fully connected clients
            if (client->getState() == STATE_CONNECTED) {
                client->checkKeepAlive(now);

                // 2. Active Flow Control / Outbox Pumping
                client->processOutbox();
            }
        }
        xSemaphoreGive(clientSetMutex);
    }
}

bool MqttBroker::processBrokerEvents() {
    BrokerEvent* event;
    int count = 0;
    bool workDone = false;
    const int MAX_BATCH = 10; // Limit processing per loop to yield CPU

    while (count < MAX_BATCH && xQueueReceive(brokerEventQueue, &event, 0) == pdPASS) {
        if (event->type == EVENT_PUBLISH) {
            _publishMessageImpl(event->message.pubMsg);
        } 
        else if (event->type == EVENT_SUBSCRIBE) {
            _subscribeClientImpl(event->message.subMsg, event->client);
        }
        
        delete event; // Clean up the event container
        count++;
        workDone = true;
    }
    return workDone;
}

// --- INTERNAL LOGIC IMPLEMENTATIONS ---

void MqttBroker::_publishMessageImpl(PublishMqttMessage* msg) {
    if (msg == nullptr) return;

    String topic = msg->getTopic().getTopic();
    
    // 1. Query the Trie to find interested subscribers
    std::vector<MqttClient*>* subscribers = topicTrie->getSubscribedMqttClients(topic);

    if (subscribers && !subscribers->empty()) {
        log_v("Worker: Publishing topic %s to %i clients", topic.c_str(), subscribers->size());

        // 2. Iterate clients (Protected Read)
        if (xSemaphoreTake(clientSetMutex, portMAX_DELAY) == pdTRUE) {
            
            // 3. Publish to each subscriber
            for (MqttClient* client : *subscribers) {
                if (client && client->getState() == STATE_CONNECTED) {
                    client->publishMessage(msg);
                }
            }
            xSemaphoreGive(clientSetMutex);
        }
        delete subscribers;
    }
    
    // Important: Delete the message object here, as the broker took ownership.
    delete msg; 
}

void MqttBroker::_subscribeClientImpl(SubscribeMqttMessage* msg, MqttClient* client) {
    if (msg == nullptr || client == nullptr) return;

    std::vector<MqttTocpic> topics = msg->getTopics();
    NodeTrie *node;
    
    // Access the Trie safely (serialized by the Worker thread)
    for(int i = 0; i < topics.size(); i++){
        node = topicTrie->subscribeToTopic(topics[i].getTopic(), client);
        
        if (node) { 
             client->addNode(node);
             log_i("Worker: Client %i subscribed to %s", client->getId(), topics[i].getTopic().c_str());
        }
    }


    // Send SUBACK to the client
    // if client still connected, send SUBACK
    if (client->getState() == STATE_CONNECTED) {
        client->sendSubAck(msg); 
    }

    delete msg; 
}

// --- PUBLIC QUEUING METHODS (Producers) ---

void MqttBroker::publishMessage(PublishMqttMessage * msg) {
    BrokerEvent* event = new BrokerEvent;
    event->type = BrokerEventType::EVENT_PUBLISH;
    event->client = nullptr;
    event->message.pubMsg = msg;

    // Send to queue
    if (xQueueSend(brokerEventQueue, &event, 0) != pdPASS) {
        log_w("Broker Queue Full! Dropping publish.");
        delete event;
        delete msg; // Prevent memory leak
    }
}

void MqttBroker::SubscribeClientToTopic(SubscribeMqttMessage * msg, MqttClient* client) {
    BrokerEvent* event = new BrokerEvent;
    event->type = BrokerEventType::EVENT_SUBSCRIBE;
    event->client = client;
    event->message.subMsg = msg;
    
    if (xQueueSend(brokerEventQueue, &event, 0) != pdPASS) {
        log_w("Broker Queue Full! Dropping subscribe.");
        delete event;
        delete msg;
    }
}