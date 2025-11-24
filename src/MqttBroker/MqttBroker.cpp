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


void MqttBroker::publishMessage(PublishMqttMessage * publishMqttMessage){
  
  std::vector<int>* clientsSubscribedIds = topicTrie->getSubscribedMqttClients(publishMqttMessage->getTopic().getTopic());

  log_v("Publishing topic: %s", publishMqttMessage->getTopic().getTopic().c_str());
  log_d("Publishing to %i client(s).", clientsSubscribedIds->size());
  
  for(std::size_t it = 0; it != clientsSubscribedIds->size(); it++){
    clients[clientsSubscribedIds->at(it)]->publishMessage(publishMqttMessage);
  }

  delete clientsSubscribedIds; // topicTrie->getSubscirbedMqttClient() don't free std::vector*
                            // the user is responsible to free de memory allocated

}                  

void MqttBroker::SubscribeClientToTopic(SubscribeMqttMessage * subscribeMqttMessage, MqttClient* client){
  
  std::vector<MqttTocpic> topics = subscribeMqttMessage->getTopics();
  NodeTrie *node;
  for(int i = 0; i < topics.size(); i++){
    node = topicTrie->subscribeToTopic(topics[i].getTopic(),client);
    client->addNode(node);
    log_i("Client %i subscribed to %s.", client->getId(), topics[i].getTopic().c_str());
  }
  
}