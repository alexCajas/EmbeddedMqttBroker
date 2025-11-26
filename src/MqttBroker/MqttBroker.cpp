#include "MqttBroker.h"


using namespace mqttBrokerName;

// --- CONSTRUCTOR ---
MqttBroker::MqttBroker(ServerListener* listener) {
    this->maxNumClients = MAXNUMCLIENTS;
    this->listener = listener;
    
    // Configuramos el listener para que nos avise (Inyección de dependencia)
    if (this->listener) {
        this->listener->setBroker(this);
    }
    
    topicTrie = new Trie();

    // 1. Crear Colas
    // Aumentamos tamaño para evitar cuellos de botella en ráfagas
    deleteMqttClientQueue = xQueueCreate(20, sizeof(MqttTransport*)); // OJO: Ahora guardamos puntero al transporte como clave
    if (!deleteMqttClientQueue) {
        log_e("Failed to create delete queue"); ESP.restart();
    }

    brokerEventQueue = xQueueCreate(50, sizeof(BrokerEvent*)); 
    if (!brokerEventQueue) {
        log_e("Failed to create brokerEventQueue"); ESP.restart();
    }

    // 2. Crear Mutex
    clientSetMutex = xSemaphoreCreateMutex();
    if (!clientSetMutex) {
        log_e("Failed to create mutex"); ESP.restart();
    }

    // 3. Instanciar Worker (Core 0)
    this->checkMqttClientTask = new CheckMqttClientTask(this);
    this->checkMqttClientTask->setCore(0);
}

// --- DESTRUCTOR ---
MqttBroker::~MqttBroker() {
    stopBroker();

    if (checkMqttClientTask) {
        delete checkMqttClientTask; 
    }
    
    // El Broker es dueño del Listener, así que lo borramos
    if (listener) {
        delete listener;
    }
    
    if (topicTrie) {
        delete topicTrie;
    }
    
    // Limpiar clientes
    for (auto const& [transport, client] : clients) {
        delete client; // Esto borrará también el transporte
    }
    clients.clear();
    
    // Limpiar eventos pendientes
    BrokerEvent* event;
    while(xQueueReceive(brokerEventQueue, &event, 0) == pdPASS) {
        // Nota: Deberíamos borrar el mensaje interno si es complejo
        delete event;
    }
    vQueueDelete(brokerEventQueue);
    vQueueDelete(deleteMqttClientQueue);

    vSemaphoreDelete(clientSetMutex);
}

// --- CONTROL ---

void MqttBroker::startBroker() {
    // Arrancamos el Listener (Polimorfismo: TCP o WS)
    if (listener) {
        listener->begin();
        log_i("MqttBroker Listener Started");
    }

    // Arrancamos el Worker
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

// --- GESTIÓN DE CLIENTES (Entrada) ---

/**
 * @brief Método llamado por el Listener cuando hay una nueva conexión.
 * Recibe un transporte abstracto (TCP o WS).
 */
void MqttBroker::acceptClient(MqttTransport *transport) {
    
    // 1. Validación de capacidad
    if (isBrokerFullOfClients()) {
        log_w("Broker full. Rejecting client IP: %s", transport->getIP().c_str());
        transport->close();
        delete transport; // Importante: borrar el wrapper si no lo aceptamos
        return;
    }
    
    // 2. Sección Crítica: Añadir al mapa
    if (xSemaphoreTake(clientSetMutex, portMAX_DELAY) == pdTRUE) {
        numClient++; // Generador simple de IDs (podría mejorar)
        int newId = numClient;

        // Instanciamos MqttClient pasándole la abstracción de transporte
        MqttClient *mqttClient = new MqttClient(transport, newId, this);
        
        // Usamos el puntero de transporte como clave única del mapa
        clients[transport] = mqttClient;
        
        xSemaphoreGive(clientSetMutex);
        
        log_i("Client Accepted. ID: %i, IP: %s", newId, transport->getIP().c_str());
    } else {
        log_e("Mutex Error. Rejecting.");
        transport->close();
        delete transport;
    }
}

// --- BORRADO DE CLIENTES (Salida) ---

void MqttBroker::queueClientForDeletion(MqttTransport* transportKey) {
    // Encolamos el puntero del transporte para identificar al cliente
    xQueueSend(deleteMqttClientQueue, &transportKey, 0);
}

void MqttBroker::deleteMqttClient(MqttTransport* transportKey) {
    MqttClient* clientToDelete = nullptr;

    if (xSemaphoreTake(clientSetMutex, portMAX_DELAY) == pdTRUE) {
        // Buscamos por la clave de transporte
        auto it = clients.find(transportKey);
        
        if (it != clients.end()) {
            clientToDelete = it->second;
            clients.erase(it); // Sacamos del mapa
            log_i("Client removed from map.");
        } else {
            log_w("Client not found in map for deletion.");
        }
        xSemaphoreGive(clientSetMutex);
    }

    // Borrado fuera del Mutex para evitar deadlocks
    if (clientToDelete != nullptr) {
        delete clientToDelete; // El destructor de MqttClient limpiará el transporte
        log_v("Client object memory freed.");
    }
}

// --- WORKER LOGIC (Core 0) ---

bool MqttBroker::processDeletions() {
    MqttTransport* transportKey;
    bool workDone = false;
    
    // Procesamos toda la cola de borrado pendiente
    while (xQueueReceive(deleteMqttClientQueue, &transportKey, 0) == pdPASS) {
        deleteMqttClient(transportKey);
        workDone = true;
    }
    return workDone;
}

void MqttBroker::processKeepAlives() {
    unsigned long now = millis();

    if (xSemaphoreTake(clientSetMutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
        for (auto const& [transport, client] : clients) {
            if (client->getState() == STATE_CONNECTED) {
                client->checkKeepAlive(now);
            }
        }
        xSemaphoreGive(clientSetMutex);
    }
}

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
        
        delete event; // Borramos el contenedor del evento
        count++;
        workDone = true;
    }
    return workDone;
}

// --- IMPLEMENTACIONES INTERNAS (Lógica de Negocio) ---

void MqttBroker::_publishMessageImpl(PublishMqttMessage* msg) {
    if (msg == nullptr) return;

    String topic = msg->getTopic().getTopic();
    
    // 1. Consultar Trie (Seguro en Worker)
    // Nota: El Trie devuelve un vector de punteros MqttClient* o IDs?
    // Asumimos que devuelve punteros a MqttClient* para eficiencia
    // Si devuelve IDs, habría que buscar en el mapa clients.
    // Vamos a asumir que devuelve IDs (ints) como en tu código original.
    std::vector<int>* clientsSubscribedIds = topicTrie->getSubscribedMqttClients(topic);

    if (clientsSubscribedIds) {
        log_v("Worker: Publishing topic %s to %i clients", topic.c_str(), clientsSubscribedIds->size());

        // 2. Iterar Clientes (Protegido por Mutex para leer el mapa)
        if (xSemaphoreTake(clientSetMutex, portMAX_DELAY) == pdTRUE) {
            
            // Esta parte es ineficiente si tenemos que buscar por ID en un mapa indexado por Transport*
            // OPTIMIZACIÓN FUTURA: El Trie debería guardar MqttClient* directamente.
            // Por ahora, hacemos un escaneo lineal o asumimos que podemos buscar.
            
            // PARCHE TEMPORAL: Iteramos todos los clientes para encontrar los IDs
            // (Esto es lento O(N*M), deberíamos cambiar el mapa a <int, MqttClient*>
            // o hacer que el Trie guarde punteros).
            
            for (int targetId : *clientsSubscribedIds) {
                for (auto const& [transport, client] : clients) {
                    if (client->getId() == targetId) {
                        if (client->getState() == STATE_CONNECTED) {
                            client->publishMessage(msg);
                        }
                        break; // ID encontrado
                    }
                }
            }
            xSemaphoreGive(clientSetMutex);
        }
        delete clientsSubscribedIds;
    }
    
    delete msg; // ¡IMPORTANTE! Borramos el mensaje aquí
}

void MqttBroker::_subscribeClientImpl(SubscribeMqttMessage* msg, MqttClient* client) {
    if (msg == nullptr || client == nullptr) return;

    std::vector<MqttTocpic> topics = msg->getTopics();
    NodeTrie *node;
    
    // Acceso seguro al Trie (solo Worker)
    for(int i = 0; i < topics.size(); i++){
        node = topicTrie->subscribeToTopic(topics[i].getTopic(), client);
        
        if (node) { 
             client->addNode(node);
             log_i("Worker: Client %i subscribed to %s", client->getId(), topics[i].getTopic().c_str());
        }
    }

    delete msg; // ¡IMPORTANTE! Borramos el mensaje aquí
}

// --- MÉTODOS PÚBLICOS DE ENCOLADO (Productores) ---

void MqttBroker::publishMessage(PublishMqttMessage * msg) {
    BrokerEvent* event = new BrokerEvent;
    event->type = BrokerEventType::EVENT_PUBLISH;
    event->client = nullptr;
    event->message.pubMsg = msg;

    if (xQueueSend(brokerEventQueue, &event, 0) != pdPASS) {
        log_w("Broker Queue Full! Dropping publish.");
        delete event;
        delete msg;  
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