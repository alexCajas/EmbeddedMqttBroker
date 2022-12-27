#include "MqttBroker/MqttBroker.h"
using namespace mqttBrokerName;
/****************************************** Trie Class *********************************************/
Trie::Trie()
{
    numElem = 0;
    root = new NodeTrie;
}
Trie::~Trie()
{
    delete root;
}
void Trie::clear()
{
    numElem = 0;
    delete root;
    root = new NodeTrie();
}

NodeTrie* Trie::insert(String topic)
{
    NodeTrie *tmp = root;
    unsigned int i = 0;
    topic += '$';// add end mark to the topic.

    // start iterate insertion.
    while (topic[i] != '$')
    {   
        // if the char is not present, insert the char in the trie.
        if (tmp->find(topic[i]) == NULL){
            tmp->takeNew(topic[i]);
        }

        // down to the next level of this branch.    
        tmp = tmp->find(topic[i]);
        i++; // next char to insert.
    }

    if (tmp->find('$') == NULL)
    {   
        // insert end mark in the current node.
        tmp->insert('$', tmp);
        numElem++;
    }
    return tmp;
}

bool Trie::find(String topic)
{
    NodeTrie *tmp = root;
    unsigned int i = 0;   
    // add end mark to the topic.
    topic += '$';

    while (topic[i] != '$')
    {
        if (tmp->find(topic[i]) == NULL){
            return false;
        }

        tmp = tmp->find(topic[i]); // down to the next level.
        i++;
    }
    
    return ( (i == topic.length() - 1) && (tmp->find('$')) ); 
}

NodeTrie* Trie::subscribeToTopic(String topic, MqttClient* client){
    NodeTrie* aux = insert(topic);
    aux->addSubscribedMqttClient(client);
    return aux;
}

std::vector<int>* Trie::getSubscribedMqttClients(String topic){
    
    std::vector<int>* clientsIds = new std::vector<int>();    
    NodeTrie *tmp = root;
    unsigned int i = 0;
    topic += '$';
    tmp->findSubscribedMqttClients(clientsIds,topic,i);
    return clientsIds;
}