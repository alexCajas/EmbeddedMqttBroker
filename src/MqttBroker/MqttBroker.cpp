#include "MqttBroker.h"
using namespace mqttBrokerName;


MqttBroker::MqttBroker(uint16_t port) {
    this->port = port;
    this->maxNumClients = MAXNUMCLIENTS;
    
    topicTrie = new Trie();
    server = new AsyncServer(port);

    // Queue para borrado diferido (Imprescindible para evitar crash)
    deleteMqttClientQueue = xQueueCreate(10, sizeof(int));
    if (!deleteMqttClientQueue) {
        log_e("Failed to create delete queue");
        ESP.restart();
    }

    brokerEventQueue = xQueueCreate(50, sizeof(BrokerEvent*)); 
    if (!brokerEventQueue) {
        log_e("Failed to create brokerEventQueue");
        ESP.restart();
    }

    clientSetMutex = xSemaphoreCreateMutex();

    // Instanciamos el Worker, pero no lo arrancamos aún
    this->checkMqttClientTask = new CheckMqttClientTask(this);
    // Configuramos el Worker para correr en el Core 0 (dejando el Core 1 para AsyncTCP/Loop)
    this->checkMqttClientTask->setCore(0);
}

MqttBroker::~MqttBroker() {
    
    stopBroker();

    if (checkMqttClientTask) {
        delete checkMqttClientTask; // Esto llama a stop() internamente en WrapperFreeRTOS
    }
    
    if (server) {
        delete server;
    }
    
    if (topicTrie) {
        delete topicTrie;
    }
    
    // Limpiar clientes
    // delete all MqttClients
    std::map<int, MqttClient *>::iterator it;
    for (it = clients.begin(); it != clients.end(); it++)
    {
        delete it->second;
    }
    clients.clear();
    
// Limpiar cola de eventos (ojo con los punteros pendientes)
    BrokerEvent* event;
    while(xQueueReceive(brokerEventQueue, &event, 0) == pdPASS) {
        // Aquí idealmente deberíamos borrar event->message.pubMsg/subMsg también 
        delete event;
    }
    vQueueDelete(brokerEventQueue);

    vSemaphoreDelete(clientSetMutex);
}

void MqttBroker::startBroker() {
    if (!server) return;

    server->onClient([this](void* arg, AsyncClient* client) {
        this->handleNewClient(client);
    }, NULL);

    server->begin();
    checkMqttClientTask->start();
    log_i("Async MqttBroker started on port %u", port);
    
}

void MqttBroker::stopBroker() {
    if (server) {
        server->end();
    }

    if (checkMqttClientTask) {
        checkMqttClientTask->stop();
    }
}

void MqttBroker::handleNewClient(AsyncClient *client) {
    // Lectura rápida de capacidad (asumimos seguro para lectura simple)
    if (isBrokerFullOfClients()) {
        log_w("Broker full. Rejecting %s", client->remoteIP().toString().c_str());
        client->stop();
        return;
    }
    addNewMqttClient(client);
}

void MqttBroker::addNewMqttClient(AsyncClient *client) {
    
    if (xSemaphoreTake(clientSetMutex, portMAX_DELAY) == pdTRUE) {
        numClient++;
        int newId = numClient;

        // Instanciamos MqttClient (él configura sus propios callbacks)
        MqttClient *mqttClient = new MqttClient(client, newId, this);
        
        clients.insert(std::make_pair(newId, mqttClient));
        
        xSemaphoreGive(clientSetMutex);
        
        log_i("New AsyncClient accepted. ID: %i, IP: %s", newId, client->remoteIP().toString().c_str());
    } else {
        log_e("Mutex error. Rejecting client.");
        client->stop();
    }
}

void MqttBroker::queueClientForDeletion(int clientId) {
    // Ponemos el ID en la cola para borrarlo luego en el loop()
    xQueueSend(deleteMqttClientQueue, &clientId, 0);
}


void MqttBroker::deleteMqttClient(int clientId) {
    MqttClient* clientToDelete = nullptr;

    // --- INICIO SECCIÓN CRÍTICA ---
    if (xSemaphoreTake(clientSetMutex, portMAX_DELAY) == pdTRUE) {
        auto it = clients.find(clientId);
        if (it != clients.end()) {
            clientToDelete = it->second; // Guardamos el puntero
            clients.erase(it);           // Lo sacamos del mapa
            log_i("Client %i removed from map.", clientId);
        } else {
            log_w("Client %i not found in map (maybe already deleted).", clientId);
        }
        xSemaphoreGive(clientSetMutex);
    }
    // --- FIN SECCIÓN CRÍTICA ---

    // Borramos el objeto FUERA del Mutex. 
    // Esto previene deadlocks si el destructor tarda mucho o usa otros recursos.
    if (clientToDelete != nullptr) {
        delete clientToDelete; 
        log_v("Client %i object deleted.", clientId);
    }
}


bool MqttBroker::processDeletions() {
    int clientIdToDelete;
    bool workDone = false;
    while (xQueueReceive(deleteMqttClientQueue, &clientIdToDelete, 0) == pdPASS) {
        deleteMqttClient(clientIdToDelete);
        workDone = true;
    }
    return workDone;
}

void MqttBroker::processKeepAlives() {
    unsigned long now = millis();

    // Proteger lectura porque addNewMqttClient (Core 1) puede escribir a la vez
    if (xSemaphoreTake(clientSetMutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
        for (auto const& [id, client] : clients) {
            if (client->getState() == STATE_CONNECTED) {
                client->checkKeepAlive(now);
            }
        }
        xSemaphoreGive(clientSetMutex);
    }
}


void MqttBroker::publishMessage(PublishMqttMessage * msg) {
    // NO procesamos aquí. Creamos un evento.
    BrokerEvent* event = new BrokerEvent;
    event->type = BrokerEventType::EVENT_PUBLISH; // PUBLISH
    event->client = nullptr; // No es relevante para publish global
    event->message.pubMsg = msg;
    // Encolar (No bloqueante o espera mínima)
    if (xQueueSend(brokerEventQueue, &event, 0) != pdPASS) {
        log_w("Broker Queue Full! Dropping publish message.");
        delete event; // Evitar memory leak si la cola está llena
        delete msg;  
    }
}

void MqttBroker::SubscribeClientToTopic(SubscribeMqttMessage * msg, MqttClient* client) {
    std::vector<MqttTocpic> topics = msg->getTopics();
    
    // Creamos un evento por cada tópico (o podrías adaptar el struct para vector)
    for(int i = 0; i < topics.size(); i++){
        BrokerEvent* event = new BrokerEvent;
        event->type = BrokerEventType::EVENT_SUBSCRIBE; // SUBSCRIBE
        event->client = client;
        event->message.subMsg = msg;
        
        if (xQueueSend(brokerEventQueue, &event, 0) != pdPASS) {
            log_w("Broker Queue Full! Dropping subscribe.");
            delete event;
            delete msg;
        }
    }
}



// ------------------------------------------------------------
// 2. MÉTODOS DEL WORKER (Consumidores - Pesados - Corren en Core 0)
// ------------------------------------------------------------

bool MqttBroker::processBrokerEvents() {
    BrokerEvent* event;
    int count = 0;
    bool workDone = false;
    const int MAX_BATCH = 10;

    while (count < MAX_BATCH && xQueueReceive(brokerEventQueue, &event, 0) == pdPASS) {
        if (event->type == EVENT_PUBLISH) {
            _publishMessageImpl(event->message.pubMsg);
        } 
        else if (event->type == EVENT_SUBSCRIBE) {
            _subscribeClientImpl(event->message.subMsg, event->client);
        }
        
        delete event; // Borramos el contenedor
        count++;
        workDone = true;
    }
    return workDone;
}

void MqttBroker::_publishMessageImpl(PublishMqttMessage* msg) {
    String topic = msg->getTopic().getTopic();
    
    // 1. Consultar Trie (Seguro porque solo Worker escribe en Trie)
    std::vector<int>* clientsSubscribedIds = topicTrie->getSubscribedMqttClients(topic);

    log_v("Worker: Publishing topic %s to %i clients", topic.c_str(), clientsSubscribedIds->size());

    // 2. Iterar Clientes (Protegido por Mutex para leer el mapa)
    if (xSemaphoreTake(clientSetMutex, portMAX_DELAY) == pdTRUE) {
        for (int clientId : *clientsSubscribedIds) {
            // Buscar cliente por ID
            auto it = clients.find(clientId);
            if (it != clients.end()) {
                MqttClient* client = it->second;
                // Solo enviar si está conectado
                if (client->getState() == STATE_CONNECTED) {
                    client->publishMessage(msg);
                }
            }
        }
        xSemaphoreGive(clientSetMutex);
    }

    delete clientsSubscribedIds;
    delete msg; // ¡IMPORTANTE! Borramos el mensaje aquí
}

void MqttBroker::_subscribeClientImpl(SubscribeMqttMessage* msg, MqttClient* client) {
    std::vector<MqttTocpic> topics = msg->getTopics();
    NodeTrie *node;
    
    // Acceso seguro al Trie (solo Worker)
    for(int i = 0; i < topics.size(); i++){
        node = topicTrie->subscribeToTopic(topics[i].getTopic(), client);
        
        // OJO: client->addNode toca el vector interno del cliente. 
        // Asumimos que es seguro porque el cliente no se borra mientras estamos aquí
        // (ya que el borrado también ocurre secuencialmente en este Worker).
        if (client) { 
             client->addNode(node);
             log_i("Worker: Client %i subscribed to %s", client->getId(), topics[i].getTopic().c_str());
        }
    }

    delete msg; // ¡IMPORTANTE! Borramos el mensaje aquí
}