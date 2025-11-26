#include "MqttBroker/MqttBroker.h"
#include "MqttMessages/ConnectMqttMessage.h"

using namespace mqttBrokerName;

/***************************** MqttClient class *************************/

MqttClient::~MqttClient(){
    log_v("Client %i destructor called.", this->clientId);

    // 1. Desuscribir de tópicos
    for(int i = 0; i < nodesToFree.size(); i++){
        nodesToFree[i]->unSubscribeMqttClient(this);
    }  
    nodesToFree.clear();

    // 2. Liberar Reader
    if (reader) {
        delete reader;
        reader = NULL;
    }

    // 3. Liberar Transporte (Esto cerrará el socket subyacente)
    if (transport) {
        transport->close();
        delete transport; // Borramos la interfaz, que borrará la implementación
        transport = NULL;
    }
}

// Constructor actualizado para recibir la Interfaz de Transporte y el ID
MqttClient::MqttClient(MqttTransport* transport, int clientId, MqttBroker * broker){
    this->transport = transport;
    this->clientId = clientId;
    this->broker = broker;
    this->_state = STATE_PENDING;

    this->keepAlive = 60; // Default, se actualiza con el CONNECT
    this->lastAlive = millis();
    this->action = NULL;

    // 1. Configurar Reader (Máquina de estados)
    this->reader = new ReaderMqttPacket([this](){
        log_v("Client %i: Mqtt Packet ready.", this->clientId);
        // Dependiendo del estado, procesamos handshake o normal
        if (this->_state == STATE_PENDING) {
             this->processOnConnectMqttPacket();
        } else {
             this->proccessOnMqttPacket();
        }
    });

    // 2. Configurar Callbacks del Transporte (Agnóstico del protocolo)
    
    // Callback de Datos
    this->transport->setOnData([this](uint8_t* data, size_t len) {
        log_v("Client %i: Received %u bytes", this->clientId, len);
        if(this->reader) {
            this->reader->addData(data, len);
        }
    });

    // Callback de Desconexión
    this->transport->setOnDisconnect([this]() {
        log_i("Client %i disconnected (Transport closed).", this->clientId);
        this->broker->queueClientForDeletion(this->transport);
    });
}

void MqttClient::processOnConnectMqttPacket(){
    uint8_t type = reader->getFixedHeader() >> 4;

    if (type == CONNECT) {
        ConnectMqttMessage connectMessage(*reader); 
        
        if (!connectMessage.malFormedPacket()) {
            log_i("Client %i (%s): Handshake OK.", 
                  this->clientId, 
                  transport->getIP().c_str()); // Usamos getIP() abstracto

            this->_state = STATE_CONNECTED;
            this->setKeepAlive(connectMessage.getKeepAlive());
            this->lastAlive = millis();

            String ack = messagesFactory.getAceptedAckConnectMessage().buildMqttPacket();
            sendPacketByTcpConnection(ack);
            
            // Nota: Ya no necesitamos cambiar el callback del reader porque 
            // lo gestionamos con el 'if' en la lambda del constructor.

        } else {
            log_w("Client %i: Malformed CONNECT.", this->clientId);
            disconnect();
        }
    } else {
        log_w("Client %i: First packet was not CONNECT.", this->clientId);
        disconnect();
    }
}

void MqttClient::proccessOnMqttPacket(){
    // Actualizamos vida en cualquier paquete válido
    lastAlive = millis(); 

    ActionFactory factory;
    action = factory.getAction(this, *reader);
    
    if (action) {
        action->doAction();
        delete action;  
        action = NULL;
    }
}

void MqttClient::publishMessage(PublishMqttMessage* publishMessage){
    // Nota: No chequeamos 'connected' aquí porque el broker ya filtra por estado.
    sendPacketByTcpConnection(publishMessage->buildMqttPacket());
}

void MqttClient::subscribeToTopic(SubscribeMqttMessage * subscribeMqttMessage){
    broker->SubscribeClientToTopic(subscribeMqttMessage, this);
}

void MqttClient::notifyPublishRecived(PublishMqttMessage *publishMessage){
    broker->publishMessage(publishMessage);
}

void MqttClient::sendPacketByTcpConnection(String mqttPacket){
    
    if (transport && transport->connected()) {
        size_t len = mqttPacket.length();

        // 1. Comprobamos espacio ANTES de enviar, igual que en tu versión original
        if (transport->canSend() && transport->space() >= len) {
            transport->send(mqttPacket.c_str(), len);
        } 
        else {
            // 2. Ahora sí detectamos si está lleno
            log_w("Client %i: Transport buffer full (%u bytes free). Packet dropped.", 
                  clientId, transport->space());
        }
    } else {
        log_w("Client %i: Transport not connected.", clientId);
    }
}

void MqttClient::sendPingRes(){
    String resPacket = messagesFactory.getPingResMessage().buildMqttPacket();
    sendPacketByTcpConnection(resPacket);
}

void MqttClient::disconnect(){
    if(transport && transport->connected()){
        transport->close();
    }
}

bool MqttClient::checkKeepAlive(unsigned long currentMillis){
    if (this->keepAlive == 0) return true; 

    unsigned long timeoutMs = (unsigned long)this->keepAlive * 1500;

    if ((currentMillis - this->lastAlive) > timeoutMs) {
        log_w("Client %i: KeepAlive Timeout. Disconnecting.", this->clientId);
        disconnect(); 
        return false;
    }
    return true; 
}