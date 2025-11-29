#include "MqttBroker/MqttBroker.h"
#include "MqttMessages/ConnectMqttMessage.h"

using namespace mqttBrokerName;

// --- DESTRUCTOR ---
MqttClient::~MqttClient(){
    log_v("Client %i destructor called.", this->clientId);

    // 1. Unsubscribe from all topics in the Trie to prevent dangling pointers.
    for(int i = 0; i < nodesToFree.size(); i++){
        nodesToFree[i]->unSubscribeMqttClient(this);
    }  
    nodesToFree.clear();

    // 2. Free the Packet Reader.
    if (reader) {
        delete reader;
        reader = NULL;
    }

    // 3. Free the Transport Interface.
    // This action closes the underlying socket (TCP or WS) and releases its memory.
    if (transport) {
        transport->close();
        delete transport; 
        transport = NULL;
    }

    // Free the Outbox Mutex
    if (_outboxMutex) {
        vSemaphoreDelete(_outboxMutex);
    }
}

// --- CONSTRUCTOR ---
MqttClient::MqttClient(MqttTransport* transport, int clientId, MqttBroker * broker){
    this->transport = transport;
    this->clientId = clientId;
    this->broker = broker;
    this->_state = STATE_PENDING; // Start in Handshake mode

    this->keepAlive = 60; // Default value, will be updated by CONNECT packet
    this->lastAlive = millis();
    this->action = NULL;

    // Critical Failure Check:
    _outboxMutex = xSemaphoreCreateMutex();
    if (!_outboxMutex) {
        log_e("Failed to create outboxMutex"); ESP.restart();
    }

    // 1. Configure Reader Callback (State Machine Entry Point)
    // The reader accumulates bytes and calls this lambda when a full packet is ready.
    this->reader = new ReaderMqttPacket([this](){
        log_v("Client %i: Mqtt Packet ready.", this->clientId);
        
        // Dispatch based on current lifecycle state
        if (this->_state == STATE_PENDING) {
             this->processOnConnectMqttPacket(); // Expecting CONNECT only
        } else {
             this->proccessOnMqttPacket(); // Expecting standard MQTT packets
        }
    });

    // 2. Configure Transport Callbacks (Network Layer Binding)
    
    // On Data Received: Feed the raw bytes into the Reader
    this->transport->setOnData([this](uint8_t* data, size_t len) {
        log_v("Client %i: Received %u bytes", this->clientId, len);
        if(this->reader) {
            this->reader->addData(data, len);
        }
    });

    // On Ready to Send: Attempt to drain the outbox
    this->transport->setOnReadyToSend([this]() {
            this->_drainOutbox();
        });

    // On Disconnect: Notify Broker to schedule cleanup
    this->transport->setOnDisconnect([this]() {
        log_i("Client %i disconnected (Transport closed).", this->clientId);
        // We use the transport pointer as the unique key for deletion
        this->broker->queueClientForDeletion(this->transport);
    });
}

// --- HANDSHAKE LOGIC ---

void MqttClient::processOnConnectMqttPacket(){
    uint8_t type = reader->getFixedHeader() >> 4;

    if (type == CONNECT) {
        ConnectMqttMessage connectMessage(*reader); 
        
        if (!connectMessage.malFormedPacket()) {
            log_i("Client %i (%s): Handshake OK.", 
                  this->clientId, 
                  transport->getIP().c_str());

            // Transition to Operational State
            this->_state = STATE_CONNECTED;
            this->setKeepAlive(connectMessage.getKeepAlive());
            this->lastAlive = millis();

            // Send CONNACK to confirm connection
            String ack = messagesFactory.getAceptedAckConnectMessage().buildMqttPacket();
            sendPacketByTcpConnection(ack);

        } else {
            log_w("Client %i: Malformed CONNECT.", this->clientId);
            disconnect();
        }
    } else {
        log_w("Client %i: First packet was not CONNECT.", this->clientId);
        disconnect();
    }
}

// --- OPERATIONAL LOGIC ---

void MqttClient::proccessOnMqttPacket(){
    // Reset Keep-Alive timer on any valid packet received
    lastAlive = millis(); 

    // Use Factory to create the specific Action (Publish, Subscribe, etc.)
    ActionFactory factory;
    action = factory.getAction(this, *reader);
    
    if (action) {
        action->doAction();
        delete action;  
        action = NULL;
    }
}


void MqttClient::sendSubAck(SubscribeMqttMessage * subscribeMqttMessage) {
    uint16_t packetId = subscribeMqttMessage->getMessageId();
    AckSubscriptionMqttMessage subAck = messagesFactory.getSubAckMessage(packetId);
    String packet = subAck.buildMqttPacket();
    sendPacketByTcpConnection(packet);
    
    log_v("Client %i: Sent SUBACK for PacketID %u", clientId, packetId);
}

void MqttClient::publishMessage(PublishMqttMessage* publishMessage){
    // Serializes the message object into bytes and sends it
    sendPacketByTcpConnection(publishMessage->buildMqttPacket());
}

void MqttClient::subscribeToTopic(SubscribeMqttMessage * subscribeMqttMessage){
    // Delegates subscription logic to the Broker (Trie update)
    broker->SubscribeClientToTopic(subscribeMqttMessage, this);
}

void MqttClient::notifyPublishRecived(PublishMqttMessage *publishMessage){
    // Delegates routing logic to the Broker
    broker->publishMessage(publishMessage);
}

// --- NETWORK I/O ---

void MqttClient::sendPacketByTcpConnection(String mqttPacket){
    // 1. Sanity Check: If disconnected, clear outbox to free RAM.
    if (!transport || !transport->connected()) {
        if (xSemaphoreTake(_outboxMutex, portMAX_DELAY)) {
            _outbox.clear();
            xSemaphoreGive(_outboxMutex);
        }
        return;
    }

    // --- CRITICAL SECTION (Producer) ---
    // Protect access to the std::deque
    if (xSemaphoreTake(_outboxMutex, portMAX_DELAY) == pdTRUE) {
        
        size_t len = mqttPacket.length();
        // Check if the network stack is ready right now
        bool transportReady = transport->canSend() && transport->space() >= len;

        // 2. FIFO Logic: If queue has items OR network is busy -> Queue it.
        // We cannot bypass existing items in the queue.
        if (!_outbox.empty() || !transportReady) {
            
            // Queue Protection: Cap size to prevent OOM
            if (_outbox.size() < 100) {
                _outbox.push_back(mqttPacket);
            } else {
                log_e("Client %i: Outbox full! Dropping packet.", clientId);
            }
            
            xSemaphoreGive(_outboxMutex); // Release lock before calling draining logic
            
            // Try to drain immediately in case space just freed up
            if (transportReady) _drainOutbox();
            return;
        }
        
        // 3. Fast Path (Optimization): Queue is empty AND Network is ready.
        // Release mutex first to avoid holding it during the network call.
        xSemaphoreGive(_outboxMutex); 
        
        // Send directly without queuing (Zero-Copy efficiency)
        transport->send(mqttPacket.c_str(), len);
    }
}

void MqttClient::_drainOutbox() {
    if (!transport || !transport->connected()) return;

    // --- CRITICAL SECTION (Consumer) ---
    // Try to acquire lock with a short timeout. If Worker is writing, we retry later 
    // rather than blocking the Network Thread for too long.
    if (xSemaphoreTake(_outboxMutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
        
        while (!_outbox.empty()) {
            String& nextPacket = _outbox.front();
            size_t len = nextPacket.length();

            // Check network availability
            if (transport->canSend() && transport->space() >= len) {
                // Attempt actual write
                size_t written = transport->send(nextPacket.c_str(), len);
                
                if (written == len) {
                    _outbox.pop_front(); // Success: Remove from queue
                } else {
                    break; // Partial write/Failure: Stop and retry later
                }
            } else {
                break; // Buffer full: Stop pumping
            }
        }
        xSemaphoreGive(_outboxMutex);
    }
}

void MqttClient::sendPingRes(){
    String resPacket = messagesFactory.getPingResMessage().buildMqttPacket();
    sendPacketByTcpConnection(resPacket);
}

void MqttClient::disconnect(){
    if(transport && transport->connected()){
        transport->close(); // This will trigger onDisconnect callback
    }
}

// --- MAINTENANCE ---

bool MqttClient::checkKeepAlive(unsigned long currentMillis){
    if (this->keepAlive == 0) return true; // KeepAlive disabled

    // MQTT Spec allows 1.5x the keep alive interval
    unsigned long timeoutMs = (unsigned long)this->keepAlive * 1500;

    if ((currentMillis - this->lastAlive) > timeoutMs) {
        log_w("Client %i: KeepAlive Timeout. Disconnecting.", this->clientId);
        disconnect(); 
        return false;
    }
    return true; 
}