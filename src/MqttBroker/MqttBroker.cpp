#include "MqttBroker.h"
using namespace mqttBrokerName;
MqttBroker::~MqttBroker(){
    
    // delete listenerTask
    newClientListenerTask->stopListen();
    delete newClientListenerTask;

    // delete freeMqttClientTask
    freeMqttClientTask->stop();
    delete freeMqttClientTask;

    // delete all MqttClients
    std::map<int, MqttClient *>::iterator it;
    for (it = clients.begin(); it != clients.end(); it++)
    {
        delete it->second;
    }  

    // delete trie
    delete topicTrie;  
}

MqttBroker::MqttBroker(uint16_t port){
    this->port = port;
    this->maxNumClients = MAXNUMCLIENTS;
    topicTrie = new Trie();
    
    /************* setup queues ***************************/
    deleteMqttClientQueue = xQueueCreate( 1, sizeof(int) );
    if(deleteMqttClientQueue == NULL){
        Serial.println("fail to create queues");
        ESP.restart();
    }

    /************ setup Tasks *****************************/
    this->newClientListenerTask = new NewClientListenerTask(this,port);
    this->newClientListenerTask->setCore(0);
    this->freeMqttClientTask = new FreeMqttClientTask(this,&deleteMqttClientQueue);
    this->freeMqttClientTask->setCore(1);
}


void MqttBroker::addNewMqttClient(WiFiClient tcpClient, ConnectMqttMessage connectMessage){

  MqttClient *mqttClient = new MqttClient(tcpClient, &deleteMqttClientQueue, numClient, connectMessage.getKeepAlive(),this);
  clients.insert(std::make_pair(numClient, mqttClient));
  mqttClient->startTcpListener();
  Serial.println("new client");
  Serial.println(numClient);  
  numClient++;
}

void MqttBroker::deleteMqttClient(int clientId){
    MqttClient * client = clients[clientId];
    clients.erase(clientId);
    delete client;
}

void MqttBroker::startBroker(){
    newClientListenerTask->start();
    freeMqttClientTask->start();
}

void MqttBroker::stopBroker(){
    newClientListenerTask->stopListen();
}


void MqttBroker::publishMessage(PublishMqttMessage * publishMqttMessage){
  std::vector<int>* clientsSubscribedIds = topicTrie->getSubscribedMqttClients(publishMqttMessage->getTopic().getTopic());

  Serial.print("publishing: ");
  Serial.println(publishMqttMessage->getTopic().getTopic());
  
  for(std::size_t it = 0; it != clientsSubscribedIds->size(); it++){
    clients[clientsSubscribedIds->at(it)]->publishMessage(publishMqttMessage);
  }
  delete clientsSubscribedIds; // topicTrie->getSubscirbedMqttClient() don't free std::vector*
                            // the user is responsible to free de memory allocated
}

void MqttBroker::SubscribeClientToTopic(SubscribeMqttMessage * subscribeMqttMessage, MqttClient* client){
  
  
  std::vector<MqttTocpic> topics = subscribeMqttMessage->getTopics();
  Serial.println(topics[0].getTopic());
  NodeTrie *node;
  for(int i = 0; i < topics.size(); i++){
    node = topicTrie->subscribeToTopic(topics[i].getTopic(),client);
    client->addNode(node);
  }
}