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
    this->checkMqttClientTask = new CheckMqttClientTask(this, &brokerEventQueue, &deleteMqttClientQueue);
    // Configuramos el Worker para correr en el Core 0 (dejando el Core 1 para AsyncTCP/Loop)
    this->checkMqttClientTask->setCore(0);
}

MqttBroker::~MqttBroker() {
    
    stopBroker();

    if (checkMqttClientTask) {
        delete checkMqttClientTask; // Esto llama a stop() internamente en WrapperFreeRTOS
    }
    
    if (server) delete server;
    if (topicTrie) delete topicTrie;
    
    // Limpiar clientes
    // delete all MqttClients
    std::map<int, MqttClient *>::iterator it;
    for (it = clients.begin(); it != clients.end(); it++)
    {
        delete it->second;
    }
    clients.clear();
    
    vQueueDelete(deleteMqttClientQueue);
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
    if (server) server->end();
    if (checkMqttClientTask) checkMqttClientTask->stop();
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


void MqttBroker::processDeletions() {
    int clientIdToDelete;
    // Usamos '0' ticks de espera. Si hay algo, lo sacamos. Si no, seguimos.
    while (xQueueReceive(deleteMqttClientQueue, &clientIdToDelete, 0) == pdPASS) {
        deleteMqttClient(clientIdToDelete);
    }
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
    event->topic = msg->getTopic().getTopic();
    event->payload = msg->getTopic().getPayLoad();
    event->client = nullptr; // No es relevante para publish global

    // Encolar (No bloqueante o espera mínima)
    if (xQueueSend(brokerEventQueue, &event, 0) != pdPASS) {
        log_w("Broker Queue Full! Dropping publish message.");
        delete event; // Evitar memory leak si la cola está llena
    }
}

void MqttBroker::SubscribeClientToTopic(SubscribeMqttMessage * msg, MqttClient* client) {
    std::vector<MqttTocpic> topics = msg->getTopics();
    
    // Creamos un evento por cada tópico (o podrías adaptar el struct para vector)
    for(int i = 0; i < topics.size(); i++){
        BrokerEvent* event = new BrokerEvent;
        event->type = BrokerEventType::EVENT_SUBSCRIBE; // SUBSCRIBE
        event->topic = topics[i].getTopic();
        event->client = client;
        
        if (xQueueSend(brokerEventQueue, &event, 0) != pdPASS) {
            log_w("Broker Queue Full! Dropping subscribe.");
            delete event;
        }
    }
}



// ------------------------------------------------------------
// 2. MÉTODOS DEL WORKER (Consumidores - Pesados - Corren en Core 0)
// ------------------------------------------------------------

void MqttBroker::processBrokerEvents() {
    BrokerEvent* event;
    // Procesamos hasta vaciar la cola (o un límite por ciclo)
    while (xQueueReceive(brokerEventQueue, &event, 0) == pdPASS) {
        
        if (event->type == 0) { // PUBLISH
            _publishMessageImpl(event->topic, event->payload);
        } 
        else if (event->type == 1) { // SUBSCRIBE
            _subscribeClientImpl(event->topic, event->client);
        }

        // Importante: Borrar el evento de memoria dinámica una vez procesado
        delete event; 
    }
}

// Aquí movemos la lógica pesada que tenías antes
void MqttBroker::_publishMessageImpl(String topic, String payload) {
    
    // NOTA: Como estamos en Core 0, y addNewMqttClient está en Core 1,
    // necesitamos proteger la lectura del mapa 'clients' con Mutex si vamos a acceder a él.
    
    // 1. Trie Lookup (Lectura)
    // Si el Trie no está protegido por su propio mutex interno, 
    // debemos asegurarnos de que nadie escriba en él (Subscribe) a la vez.
    // Como hemos movido Subscribe a este mismo hilo (Core 0), ¡NO NECESITAMOS MUTEX PARA EL TRIE!
    // Esta es la gran ventaja del Worker Pattern: serializamos el acceso.
    
    std::vector<int>* clientsSubscribedIds = topicTrie->getSubscribedMqttClients(topic);

    log_v("Publishing topic: %s", topic.c_str());
    log_d("Publishing to %i client(s).", clientsSubscribedIds->size());
    // 2. Envío a clientes
    // Aquí sí necesitamos el clientSetMutex porque estamos leyendo el mapa 'clients'
    // y 'addNewMqttClient' (Core 1) podría estar escribiendo en él.
    if (xSemaphoreTake(clientSetMutex, portMAX_DELAY) == pdTRUE) {
        for(size_t it = 0; it != clientsSubscribedIds->size(); it++){
            clients[clientsSubscribedIds->at(it)]->publishMessage(publishMqttMessage);
        }
        xSemaphoreGive(clientSetMutex);
    }

    delete clientsSubscribedIds;
}

void MqttBroker::_subscribeClientImpl(String topic, MqttClient* client) {
    // Ya no necesitamos Mutex para el Trie porque solo este hilo (Worker) lo toca.
    // (A menos que uses el Trie en otro lugar fuera del Worker).
    
    NodeTrie *node = topicTrie->subscribeToTopic(topic, client);
    client->addNode(node); // OJO: client->addNode debe ser thread-safe o protegido
    
    log_i("Worker: Client %i subscribed to %s", client->getId(), topic.c_str());
}