#ifndef MQTTBROKER_H
#define MQTTBROKER_H

#include <WiFi.h> 
#include <map>
#include "WrapperFreeRTOS.h"
#include "MqttMessages/FactoryMqttMessages.h"
#include "MqttMessages/SubscribeMqttMessage.h"
#include "MqttMessages/UnsubscribeMqttMessage.h"
#include "MqttMessages/PublishMqttMessage.h"


namespace mqttBrokerName{

// Depends of your architecture, max num clients is exactly the 
// max num open sockets that your divece can support.
#define MAXNUMCLIENTS 16

// When tcp conection have success, on average, a packet sended over
// this tcp conecction takes at most 500milliseconds to arrive,
// so an mqtt packet takes at most 500milliseconds to arrive to the broker.
// If you mqtt client is connecting and disconnecting from the broker, you can
// try to increasing this value.
#define MAXWAITTOMQTTPACKET 500 

class NewClientListenerTask;
class FreeMqttClientTask;
class MqttClient;
class Trie;
class NodeTrie;
class TCPListenerTask;
class Action;

/**
 * @brief This class listen to new mqttClients, accepting or refusing his
 * connect request, also release allocated memory when a mqttClient disconnects.
 * These actions runs in concurrence.
 * 
 * By default, this broker can listen 16 open sockets at same time, you can set
 * this number according to your hardware support.
 */
class MqttBroker
{
private:
    
    uint16_t port;
    uint16_t maxNumClients;
    // unique id in the scope of this broker.
    int numClient = 0;

    NewClientListenerTask *newClientListenerTask;
    FreeMqttClientTask *freeMqttClientTask;
    Trie *topicTrie;


    /***************************** Queue to sincronize Tasks ****************/
    QueueHandle_t deleteMqttClientQueue;

    /************************* clients structure **************************/
    std::map<int,MqttClient*> clients;
    
public:

    /**
     * @brief Construct a new Mqtt Broker object.
     * 
     * @param port where Mqtt broker listen.
     */
    MqttBroker(uint16_t port = 1883);
    ~MqttBroker();

    /**
     * @brief Start the listen on port, waiting to new clients.
     */
    void startBroker();

    /**
     * @brief Stop the listen on port.
     * 
     */
    void stopBroker();

    /**
     * @brief Create and store a new MqttClient object from a tcp connection and 
     * the data on connect mqtt packet.
     * 
     * @param tcpClient socket where is the connection to this client.
     * @param connectMessage object where are all information to the accepetd connection and his client.
     */
    void addNewMqttClient(WiFiClient tcpClient, ConnectMqttMessage connectMessage);
    
    /**
     * @brief delete and free a MqttClient object.
     * 
     * @param clientId 
     */
    void deleteMqttClient(int clientId);

    /**
     * @brief publish a mqtt message arrived to all mqtt clients interested.
     * 
     * @param publihsMqttMessage mqtt message to publish.
     */
    void publishMessage(PublishMqttMessage * publihsMqttMessage);

    /**
     * @brief Subscribe a MqttClient to a topic.
     * 
     * @param subscribeMqttMessage message where is the information of topic.
     * @param client that want to subscribe to the topic.
     */
    void SubscribeClientToTopic(SubscribeMqttMessage * subscribeMqttMessage, MqttClient* client);    

    /**
     * @brief Set the Max Num Clients that your system can support.
     * 
     * @param numMaxClients.
     */
    void setMaxNumClients(uint16_t numMaxClients){
        this->maxNumClients = numMaxClients;
    }

    /**
     * @brief check if broker has contains maxNumClients connects.
     * 
     * @return true if broker has naxNumClients connects.
     * @return false if not.
     */
    bool isBrokerFullOfClients(){
        return (clients.size() == maxNumClients);
    }
};


/*********************** Tasks **************************/

/**
 * @brief This class listen on port waiting to a new client, if connect mqtt
 * packet is malFormed, the connection will be refused.
 * 
 */
class NewClientListenerTask : public Task{
private:
    WiFiServer *tcpServer;
    MqttBroker *broker;
    FactoryMqttMessages messagesFactory;


    /********************** Tcp communication ***************************/
    void sendAckConnection(WiFiClient tcpClient);
    void sendPacketByTcpConnection(WiFiClient client, String mqttPacket);

public:

    /**
     * @brief Construct a new Client Listener Task object
     * 
     * @param broker where are all information to the MqttClients.
     * @param port where initialize the listen.
     */
    NewClientListenerTask(MqttBroker *broker, uint16_t port);
    ~NewClientListenerTask();
    void run(void *data);

    /**
     * @brief Stop the WiFiServer socket, and the Task.
     * 
     */
    void stopListen();
    
};


/**
 * @brief How it is C++, there isn't a garbage recolector, for this reason
 * it is necessary keep carrefuly with the dinamyc allocated memory, this class
 * free the allocated memory of a MqttClient object.
 */
class FreeMqttClientTask : public Task{
    private:   
        MqttBroker *broker;
        QueueHandle_t *deleteMqttClientQueue;
    
    public:
        /**
         * @brief Construct a new Free Mqtt Client Task object.
         * 
         * @param broker where are all information to the MqttClients objects.
         * @param deleteMqttClientQueue FreeRTOS queue to sincronize Task, this queue
         * sincronize the MqttClient outgoing object with the current Task. 
         */
        FreeMqttClientTask(MqttBroker *broker, QueueHandle_t *deleteMqttClientQueue);
        void run (void * data);
};




/***************************************** MqttClient Class ***********************/


/**
 * @brief When broker reveived a Mqtt packet, hi has to do some action,
 * and this action depends on the kind of mqtt packet arrived. To implement
 * an action that can change dinamycally it is used Strategy GRASP Pattern.
 * There is other problem, at first, it is not known what action it is needed,
 * but it is needed create this action and use it when a mqtt packet is arrived,
 * this problem is resolved with Factory Method GRASP Pattern.
 * 
 */
class Action{
    protected:
        MqttClient* mqttClient;
    public:
        /**
         * @brief Construct a new Action object
         * 
         * @param mqttClient context of Strategy GRASP Pattern.
         */
        Action(MqttClient *mqttClient){
            this->mqttClient = mqttClient;
        }


        virtual ~Action(){}

        /**
         * @brief Action to implement.
         * 
         */
        virtual void doAction();
};


/**
 * @brief Class to abstract a tcp client of this server and his
 * fetaures like:
 *      subscribed topics and qos
 *      will topic
 *      will message
 *      keepAlive
 *      etc...
 * 
 */
class MqttClient
{
private:
    // client id in the scope of this broker.
    int clientId;
    WiFiClient tcpConnection;

    uint16_t keepAlive;
    unsigned long lastAlive;

    FactoryMqttMessages messagesFactory;
    Action *action;
    QueueHandle_t *deleteMqttClientQueue;
    
    TCPListenerTask *tcpListenerTask;

    // vector where are, all nodes where is present the current
    // client, is used to make it easy to release 
    // subscribedMqttClients map, when client desconnets.
    std::vector<NodeTrie*> nodesToFree;

    MqttBroker *broker;
    /**
     * @brief Send a mqtt packet over
     * tcpConnection, the mqtt packet is stored in String class,
     * but tcpConnection can only send bytes buffer, String have
     * a method to get byteBuffers from him self.
     * 
     * @param mqttPacket to send over tcpConnection.
     */
    void sendPacketByTcpConnection(String mqttPacket);
    
public:

    /**
     * @brief Construct a new Mqtt Client object
     * 
     * @param tcpConnection socket that connect the current mqtt client to the broker.
     * @param deleteMqttClientQueue FreeRTOS queue to sincronize the MqttClient class with
     * FreeMqttClientTask class. 
     * @param clientId unique id for this new client in the scope of the broker.
     * @param keepAlive max time that the broker wait to a mqtt client, if mqtt client doesn't send
     * any message to the broker in this time, broker will close the tcp connection.
     */
    MqttClient(WiFiClient tcpConnection, QueueHandle_t * deleteMqttClientQueue, int clientId, uint16_t keepAlive, MqttBroker * broker);

    ~MqttClient();

    /**
     * @brief Get the id of the client.
     * 
     * @return int id of the client.
     */
    int getId(){return clientId;}

    /**
     * @brief Notify to the listeners of this MqttClient the new publish
     * message event ocurred, acording to Delegate Model Event GRASP.
     * 
     * @param publishMessage PublishMqttMessage recived. 
     * @return * void 
     */
    void notifyPublishRecived(PublishMqttMessage *publishMessage);

    /**
     * @brief When the mqtt client disconnects, its should be removed from map structure
     * and its allocated memory should be freed. This method notifies the corresponding task 
     * that this mqtt client should be removed.
     * 
     */
    void notifyDeleteClient(){
        log_v("Notify broker to delete client: %i", clientId);
        xQueueSend((*deleteMqttClientQueue), &clientId, portMAX_DELAY);
    }

    /**
     * @brief Check the conecction state and available data
     * from tcpConnection.
     * 
     * @return uint8_t WiFiClient.connected() to indiciate the
     *         state of tcp connection.
     */
    uint8_t checkConnection();

    /**
     * @brief When broker received a publish request, borker need
     * to forward it to interested clients. This method checks that
     * the current client is interested in the message and forwards to
     * them if is so.
     * 
     * @param publishMessage that check.
     */
    void publishMessage(PublishMqttMessage *publishMessage);

    /**
     * @brief Subscribe to an topic.
     * 
     * @param subscribeMqttMessage 
     */
    void subscribeToTopic(SubscribeMqttMessage * subscribeMqttMessage);

    /**
     * @brief Send a mqtt ping response packet.
     * 
     */
    void sendPingRes();    
    

    /**
     * @brief close tcpConnection.
     * 
     */
    void disconnect(){
        if(tcpConnection.connected()){
            tcpConnection.stop();
        }
    }

    /**
     * @brief Start the task that listen on tcp port, wating for a new mqtt
     * message.
     */
    void startTcpListener();

    void addNode(NodeTrie *node){
        nodesToFree.push_back(node);
    }
};

/**
 * @brief Task that listen on tcp socket, wating for a new mqtt message,
 * when socket is down, this task start the logic to remove this client and 
 * free his allocated memory. 
 */
class TCPListenerTask : public Task{
    private:
        MqttClient * mqttClient;
    
    public:

        /**
         * @brief Construct a new Task TCP Listener object
         * 
         * @param mqttClient that has the tcp socket.
         */
        TCPListenerTask(MqttClient *mqttClient);

        /**
         * @brief Method that the task run indefinitely.
         * 
         * @param data 
         */
        void run(void * data);
};


/**
 * @brief Strategy GRASP: When borker received a mqtt publish request packet,
 * borker has to send this publish packet to all interested clients. This class
 * implements this logic.
 * 
 */
class PublishAction: public Action{
    private:
        PublishMqttMessage *publishMqttMessage;

    public:

        /**
         * @brief Construct a new Publish Action object
         * 
         * @param mqttClient context of the Strategy.
         * @param packetReaded object where are all information to instance a
         * PublishMqttMessage object.
         */
        PublishAction(MqttClient* mqttClient, ReaderMqttPacket packetReaded);
    
        /**
         * @brief Notify to Broker class a publish mqtt request
         * recevided from this client.
         * 
         */
        void doAction() override;
};


/**
 * @brief Strategy GRASP: When a mqttClient wants to subscribe to some topic, this topci
 * need to be store on the topics vector of the current mqttClient. This class
 * implements this logic.
 * 
 */
class SubscribeAction:public Action{
    private:
        SubscribeMqttMessage *subscribeMqttMessage;
    public:

        /**
         * @brief Construct a new Subscribe Action object
         * 
         * @param mqttClient context of the Strategy.
         * @param readedPacket object where are all information to build a SubscribeMqttMessage
         * object.
         */
        SubscribeAction(MqttClient *mqttClient,ReaderMqttPacket readedPacket);
        ~SubscribeAction();

        void doAction() override;
};

/**
 * @brief Strategy GRASP: When an unsubscribe mqtt packet has arrived, broker
 * has to delete the topics in mqttClient that match with
 * topics in unsubscribe mqtt packet. This class implements this logic.
 * 
 */
class UnSubscribeAction:public Action{
    private:
        UnsubscribeMqttMessage *unsubscribeMqttMessage;

    public:

        /**
         * @brief Construct a new Un Subscribe Action object.
         * 
         * @param mqttClient context of Strategy.
         * @param readedPacket object where are all information to build a UnsubscribeMqttMessage
         * object. 
         */
        UnSubscribeAction(MqttClient *mqttClient,ReaderMqttPacket packetReaded);
        ~UnSubscribeAction();

        void doAction() override;
};

/**
 * @brief Strategy GRASP: it is necessary a Strategy that do nothing, to avoid consistency problems, it is applied NullObject
 * GRASP Pattern.
 * 
 */
class NoAction: public Action{
    public:
        NoAction(MqttClient *mqttClient);

        /**
         * @brief Do nothing.
         * 
         */
        void doAction() override;
};
/**
 * @brief Strategy GRASP: Send ping response over tcp connection. This method 
 * implements this logic.
 * 
 */
class PingResAction:public Action{
    public:

        /**
         * @brief Construct a new Ping Res Action object
         * 
         * @param mqttClient context of the Strategy.
         */
        PingResAction(MqttClient * mqttClient);

        /**
         * @brief Send ping response over tcpConnection.
         * 
         */
        void doAction() override;
};

/**
 * @brief Strategy GRASP: When a Disconnect resq packet is arrived, the tcpConnection
 * must be close, and this MqttClient must be deleted. This class implements this logic.
 * 
 */
class DisconnectAction: public Action{
    public:

        /**
         * @brief Construct a new Disconnect Action object
         * 
         * @param mqttClient contex of the Strategy.
         */
        DisconnectAction(MqttClient* mqttClient);

        void doAction() override;
};

/**
 * @brief Class that implements Factory Method to dispatch actions
 * objects.
 * 
 */
class ActionFactory
{
private:
    /* data */
public:
    ActionFactory();
    
    /**
     * @brief Get the Action object
     * 
     * @param mqttClient context to pass to Action object. 
     * @param packetReaded context that has the information to know what kind of
     *               Action object it is needed.
     * @return Action* 
     */
    Action* getAction(MqttClient * mqttClient, ReaderMqttPacket packetReaded);

};


/****************************** NodeTrie Class *****************************/

/**
 * @brief Node for a prefix tree structure.
 * 
 */
class NodeTrie
{
private:
    char character;
    NodeTrie *bro, *son;
    std::map<int, MqttClient*> *subscribedClients;

    /**
     * @brief When some client subscribe to a topic usin "+" wildcard, the tree
     * creates a branch with the character "+". This method check if "+" node
     * is present in this topic level and explore that branch, if is so. If there is a match with
     * the topic, put into clients varable, all mqttClients found.
     * @param clientsIds vector where store the id of mqttClients.
     * @param topic where is looking for in the tree.
     * @param index where start the topic level.
     */
    void matchWithPlusWildCard(std::vector<int>* clientsIds,String topic, int index);
    
    /**
     * @brief When some client subscribe to a topic usin "#" wildcard, the tree
     * creates a branch with the character "#". This method checks if "#" node
     * is present in this topic level, if is so, put into clients varable, all mqttClients found.
     * @param clientsIds vector where store the id of mqttClients.
     * @param topic where is looking for in the tree.
     */
    void matchWithNumberSignWildCard(std::vector<int>* clientsIds,String topic);

public:
    NodeTrie();
    ~NodeTrie();

    /**
     * @brief Search a character in the tree, if the char is found
     * return the son node of node where the character was found . This method guides
     * the proces of searching a topic in the tree.
     * 
     * @param character where that this method searches in the tree.
     * @return NodeTrie* son of current character, NULL in other case.
     */
    NodeTrie *find(char character);

    /**
     * @brief insert a new character in the prefix, the insertion is 
     * in order. If character == '$', the character will insert in the current NodeTrie.
     * 
     * @param character to insert.
     * @param son new node to insert in the prefix.
     */
    void insert(char character, NodeTrie *son);

    /**
     * @brief insert the 
     * new character in the current prefix and create a new son for
     * the new char.
     * 
     * @param character to insert.
     */
    void takeNew(char character);

    /**
     * @brief Add a mqttClient in subscribed clients map. 
     * 
     * @param client to add .
     */
    void addSubscribedMqttClient(MqttClient* client);

    /**
     * @brief Get the Subscribed Mqtt Clients map.
     * 
     * @return std::map<int, MqttClient*>*.
     */
    std::map<int, MqttClient*> * getSubscribedMqttClients(){
        return subscribedClients;
    }

    /**
     * @brief This method insert the mqttClients subscribed to the topic in
     * a map. Here is implemented the search of mqttClients subscribed by wildcards. 
     * 
     * @param clientsIds vector where store all id of mqttClients subscribed to topic.
     * @param topic that clients are subscribed.
     * @param index where start the proccesing of topic.
     */
    void findSubscribedMqttClients(std::vector<int>*clientsIds, String topic, int index);

    void unSubscribeMqttClient(MqttClient * mqttClient){
        subscribedClients->erase(mqttClient->getId());
    }
};

/******************************************* Trie Class ************************************/

/**
 * @brief prefix trie class.
 * 
 */
class Trie
{
private:
    NodeTrie *root;
    int numElem;

public:
    Trie();
    ~Trie();

    /**
     * @brief free all memory of the current trie.
     * 
     */
    void clear(void);

    /**
     * @brief Insert a topic in the tree.
     * 
     * @param topic to insert.
     * @return NodeTrie* where is the end mark of the prefix '$',
     *         this node has the subscribed clients map.
     */
    NodeTrie* insert(String topic);

    /**
     * @brief find a topic in the tree. 
     * 
     * @param topic to find.
     * @return true if topic is in the tree. 
     * @return false in other case.
     */
    bool find(String topic);

    /**
     * @brief Get num topcis in the tree.
     * 
     * @return int 
     */
    int getNumElem(void) { return numElem; }

    /**
     * @brief Subscribe MqttClient* to topic, if topic is not present
     * in the tree, this method also insers it.
     * 
     * @param topic to subscribe.
     * @param client that subscribe.
     * @param NodeTrie* where the client is subscribed.
     */
    NodeTrie* subscribeToTopic(String topic, MqttClient* client);

    /**
     * @brief Get the vector of the id of subscribed mqtt clients, Warning!, this 
     * method allocate dinamically the vector but don't free this, the user is responsible
     * to free the memory allocated to avoid memory holes.
     * 
     * @param topic that mqttClients are subscribed.
     * @return std::vector<int>* vector where are all id of the mqttClients subscribed to this topic.
     */
    std::vector<int>* getSubscribedMqttClients(String topic);

};

#endif //MQTTBROKER_H
}