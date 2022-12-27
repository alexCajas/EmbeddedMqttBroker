#include "MqttBroker/MqttBroker.h"

/****************************** NodeTrie Class *****************************************/
using namespace mqttBrokerName;
NodeTrie::NodeTrie()
{
    character = '\0';
    bro = NULL;
    son = NULL;
    subscribedClients = NULL;
}
NodeTrie::~NodeTrie()
{   
    if(subscribedClients != NULL){
        delete subscribedClients;
    }
    
    delete bro;
    if (son != this)
        delete son;
}

NodeTrie *NodeTrie::find(char character)
{
    if (character == '$' || this->character == character){
        return this->son;
    }

    // search the character in this branch level.
    NodeTrie *tmp = this->bro;
    while ((tmp != NULL) && (tmp->character < character))
    {
        tmp = tmp->bro;
    }

    // if the character was found, return the next level.
    if ((tmp != NULL) && (tmp->character == character))
    {
        return tmp->son;
    }else{
        return NULL;
    }
        
}

void NodeTrie::insert(char character, NodeTrie *son)
{   
    // if character is end mark, create a new map for subscribed clients.
    if (character == '$')
    {
        this->character = '$';
        this->subscribedClients = new std::map<int,MqttClient*>;
        this->son = son;

    } else { // if not, insert in order in this level.

        // find the correct position for this char.
        NodeTrie *tmp = this;
        while ((tmp->bro != NULL) && (tmp->bro->character < character)){
            tmp = tmp->bro;
        }

        // if the char is present, add the son.   
        if ((tmp->bro != NULL) && (tmp->bro->character == character))
        {
            tmp->bro->son = son;

        }else{ // if not present, insert the char and his son between
               // the tmp nodeTrie and his brother.
            NodeTrie *aux = new NodeTrie();
            aux->character = character;
            aux->son = son;
            aux->bro = tmp->bro;
            tmp->bro = aux;
        }
    }
}

void NodeTrie::takeNew(char character)
{
    NodeTrie *son = new NodeTrie();
    insert(character, son);
}


void NodeTrie::addSubscribedMqttClient(MqttClient* client){
    subscribedClients->insert(std::make_pair(client->getId(),client));
}


void NodeTrie::findSubscribedMqttClients(std::vector<int>*clientsIds, String topic, int index){
   
    NodeTrie *tmp = this;
    unsigned int i = index;

    // when start to explorer a topic level, we need to now
    // if some one subcribed to this level usin "+" or "#" wildcards.
    tmp->matchWithPlusWildCard(clientsIds,topic,i);
    tmp->matchWithNumberSignWildCard(clientsIds,topic);
    
    // explore main branch
    while (topic[i] != '$')
    {   
        // if character is not present, means that this topic is
        // not present in the tree, no body subcribe to this topic.
        if (tmp->find(topic[i]) == NULL){
            return ;
        }
            
        tmp = tmp->find(topic[i]);
        // begin of topic level detected.
        if(topic[i] == '/'){
            // explore "+" and/or "#" sub-branchs if there are present in this level.
            tmp->findSubscribedMqttClients(clientsIds,topic,i); 
        }
        i++; // next character.
    }    

    // if numer of branch level explored is == topic.length() - 1
    // and there is end mark in this branch, there is a match with topic in the main branch. 
   if ( (i == topic.length() - 1) &&  (tmp->find('$')) ){
       // insert the mqttClients subscribed to this topic into clients map.
       
       for(std::map<int,MqttClient*>::iterator it = tmp->getSubscribedMqttClients()->begin(); it != tmp->getSubscribedMqttClients()->end(); ++it) {
            clientsIds->push_back(it->first);
            }
   }
        
}


void NodeTrie::matchWithPlusWildCard(std::vector<int>* clients,String topic, int index){

    NodeTrie *plusWildCard = this->find('+');
    if(plusWildCard == NULL){
        return; // there is not "+" wildcard in this topic level.
    }

// ***** explorer ("/prefix/"->"+"->"/"->"some suffix$"), branch. ****************
    int indexAux = index;
    indexAux++;
    indexAux = topic.indexOf('/',indexAux);
    if(indexAux != -1){
        plusWildCard->findSubscribedMqttClients(clients,topic,indexAux);
    }
    
// ***** explorer ("/prefix/"->"+"->"$"), branch. *******************
    plusWildCard = plusWildCard->find('$');
    if(plusWildCard == NULL){
        return; // no body subscribed at /prefix/+ topic in this topic level.
    }

    indexAux = index;
    indexAux++;
    if( (topic.indexOf('/',indexAux) == -1) ){

        indexAux = topic.length() - 1;
        plusWildCard->findSubscribedMqttClients(clients,topic,indexAux);
    }
}

void NodeTrie::matchWithNumberSignWildCard(std::vector<int>* clients,String topic){

    NodeTrie * numberSingWildCard = this->find('#');
    if(numberSingWildCard == NULL){
        return;
    }

    int index = topic.length() - 1;
    numberSingWildCard->findSubscribedMqttClients(clients,topic,index);
}