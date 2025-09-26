#include "MqttBroker/MqttBroker.h"
using namespace mqttBrokerName;
/********************** ListenerNewClientTask ******************/

NewClientListenerTask::NewClientListenerTask(MqttBroker *broker, uint16_t port) : Task("ListenerNewClientTask", 1024*3, TaskPrio_Mid)
{
    this->broker = broker;
    this->tcpServer = new WiFiServer(port);
    this->ackPacket = messagesFactory.getAceptedAckConnectMessage().buildMqttPacket();
}

NewClientListenerTask::~NewClientListenerTask(){
  this->stop();  
  if (tcpServer) {
        tcpServer->close();
        delete tcpServer;
    }
}

void NewClientListenerTask::run(void *data){
  
  tcpServer->begin();
  while(true){
    // Check for a new mqtt client connected
    WiFiClient client = tcpServer->available();
    if (!client)
    {
      vTaskDelay(10/portTICK_PERIOD_MS);   // yield cpu, This line is necessary for meet freeRTOS time constrains
      continue;         // next iteration
    }

    // Waiting the mqtt packet sended by the mqtt client
    for (size_t i = 0; i < MAXWAITTOMQTTPACKET; i += 100)
    {
      if (client.available())
        break;
      vTaskDelay(10/portTICK_PERIOD_MS);
    }

    /** if client don't send mqttpacket**/
    if (!client.available())
    {
      log_w("Client from %s:%u rejected.", client.remoteIP().toString().c_str(), client.remotePort());
      client.stop();
      continue; // next iteration.
    } 

    /*reading bytes from client, in this point Broker only recive and
     acept connect mqtt packets**/
      
     // test if lack comes to messagesFactory.
     //ConnectMqttMessage connectMessage = messagesFactory.getConnectMqttMessage(client);

    //if(!connectMessage.malFormedPacket() && !broker->isBrokerFullOfClients()){
      sendAckConnection(client);
      broker->addNewMqttClient(client/*, connectMessage*/);
    //}
  }
}

void NewClientListenerTask::stopListen(){
    this->stop();
    if (tcpServer) {
        tcpServer->close();
    }
}

void NewClientListenerTask::sendAckConnection(WiFiClient &tcpClient){
  //String ackPacket = messagesFactory.getAceptedAckConnectMessage().buildMqttPacket();
  sendPacketByTcpConnection(tcpClient, this->ackPacket);
}

void NewClientListenerTask::sendPacketByTcpConnection(WiFiClient &client, String mqttPacket){
  log_i("before send packet by tcp connection, free heap: %u", ESP.getFreeHeap());
  uint8_t *buff = new uint8_t[mqttPacket.length()];
  mqttPacket.getBytes(buff, mqttPacket.length());
  client.write(buff, mqttPacket.length());
  delete[] buff;
  log_i("after send packet by tcp connection, free heap: %u", ESP.getFreeHeap());
}
