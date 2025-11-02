#include "MqttBroker/MqttBroker.h"
using namespace mqttBrokerName;
/***************************** MqttClient class *************************/
MqttClient::~MqttClient(){
    
  for(int i = 0; i < nodesToFree.size(); i++){
    nodesToFree[i]->unSubscribeMqttClient(this);
  }  

  if (reader) {
        delete reader;
        reader = NULL;
    }

  if (tcpConnection) {
      if (tcpConnection->connected()) {
          tcpConnection->close(true); 
      }
      tcpConnection = NULL;
  }
}


MqttClient::MqttClient(AsyncClient *tcpConnection, QueueHandle_t * deleteMqttClientQueue,  int clientId, uint16_t keepAlive,MqttBroker * broker){
  this->clientId = clientId;
  this->keepAlive = keepAlive;
  this->tcpConnection = tcpConnection;
  this->deleteMqttClientQueue = deleteMqttClientQueue;
  this->broker = broker;

  lastAlive = millis();
  this->action = NULL;


  this->reader = new ReaderMqttPacket([this](){
      log_v("Client %i: Mqtt Packet ready to be processed.", this->clientId);
      this->proccessOnMqttPacket();
      
    });

  this->initTCPCallbacks();
}

void MqttClient::initTCPCallbacks(){

  this->tcpConnection->onData([this](void* data, size_t len) {
    log_v("Client %i: Received %u bytes", this->clientId, len);
    if(this->reader) {
        this->reader->addData((uint8_t*)data, len);
    }
  });


  this->tcpConnection->onDisconnect([this](AsyncClient* client){
    log_i("Client %i disconnected (onDisconnect).", this->clientId);
    this->notifyDeleteClient();
  });


  this->tcpConnection->onError([this](AsyncClient* client, int8_t error){
    log_w("Client %i: TCP Error %i", this->clientId, error);
    this->notifyDeleteClient();
  });


  this->tcpConnection->onTimeout([this](AsyncClient* client, uint32_t time){
    log_w("Client %i: TCP Timeout", this->clientId);
    this->notifyDeleteClient();
  });
}

void MqttClient::proccessOnMqttPacket(){
         
  // get new action.
  ActionFactory factory;
  action = factory.getAction(this,*reader);
  action->doAction();

  // free Action allocated memory.
  delete action;  
  lastAlive = millis();
}

void MqttClient::publishMessage(PublishMqttMessage* publishMessage){
  log_v("Topic %s send to %i", publishMessage->getTopic().getTopic().c_str(), this->clientId);
  log_v("\n%s", publishMessage->getTopic().getPayLoad().c_str());
  /*
  for qos > 0
  uint8_t publishFlasgs = 0x6 & topics[i].getQos();
  publishMessage->setFlagsControlType(publishFlasgs);
  */
  if(!tcpConnection.connected()){
    log_w("Client %i not connected. Message not sent.", this->clientId);
    return;
  }
  sendPacketByTcpConnection(publishMessage->buildMqttPacket());
}

void MqttClient::subscribeToTopic(SubscribeMqttMessage * subscribeMqttMessage){
  broker->SubscribeClientToTopic(subscribeMqttMessage, this);
}


void MqttClient::notifyPublishRecived(PublishMqttMessage *publishMessage){
  broker->publishMessage(publishMessage);
}


void MqttClient::sendPacketByTcpConnection(String mqttPacket){
  
  if (tcpConnection == NULL || !tcpConnection->connected()) {
          log_w("Client %i: Not connected. Packet not sent.", clientId);
          return;
      }

  if (tcpConnection->canSend() && tcpConnection->space() >= mqttPacket.length()) {
      tcpConnection->write(mqttPacket.c_str(), mqttPacket.length());
  } else {
      log_w("Client %i: TCP buffer full. Packet dropped.", clientId);
  }
}

void MqttClient::sendPingRes(){
  String resPacket = messagesFactory.getPingResMessage().buildMqttPacket();
  log_v("sending ping response to %i.", this->clientId);
  sendPacketByTcpConnection(resPacket);
}

void MqttClient::disconnect(){
  if(tcpConnection && tcpConnection->connected()){
      tcpConnection->close();
      // This will trigger the onDisconnect callback,
      // which in turn calls notifyDeleteClient().
  }
}