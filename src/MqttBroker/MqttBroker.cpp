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
}

MqttBroker::~MqttBroker() {
    stopBroker();
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
    log_i("Async MqttBroker started on port %u", port);
}

void MqttBroker::stopBroker() {
    if (server) server->end();
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

void MqttBroker::loop() {
    int clientIdToDelete;
    // Procesamos la cola de borrado de forma NO bloqueante
    while (xQueueReceive(deleteMqttClientQueue, &clientIdToDelete, 0) == pdPASS) {
        deleteMqttClient(clientIdToDelete);
    }
}

void MqttBroker::deleteMqttClient(int clientId) {
    if (xSemaphoreTake(clientSetMutex, portMAX_DELAY) == pdTRUE) {
        auto it = clients.find(clientId);
        if (it != clients.end()) {
            MqttClient* client = it->second;
            clients.erase(it);
            delete client; // Llama a ~MqttClient -> clean subscriptions -> free reader
            log_i("Client %i deleted safely.", clientId);
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