#ifndef MQTTTOPIC_H
#define MQTTTOPIC_H

class MqttTocpic
{
private:
    String topic;
    uint8_t qos;
    String payLoad;

public:
    MqttTocpic(){}
    MqttTocpic(String topic, String payLoad, uint8_t qos = 0){
        this->topic = topic;
        this->payLoad = payLoad;
        this->qos = qos;
    }

    String getTopic(){
        return topic;
    }

    uint16_t getTopicLength(){
        return topic.length();
    }

    String getPayLoad(){
        return payLoad;
    }

    void setTopic(String topic){
        this->topic = topic;
    }

    void setPayLoad(String payLoad){
        this->payLoad = payLoad;
    }

    uint8_t getQos(){
        return qos;
    }
    void setQos(uint8_t qos){
        this->qos = qos;
    }

    bool isTopic(MqttTocpic topic){
        return this->topic.equals(topic.getTopic());
    }

    /**
     * @brief Get the Topic Length, This is a String topic length +
     *        String payload length.
     * 
     * 
     * @return uint16_t 
     */
    uint16_t getTopicAndPayloadLength(){
        uint16_t length = topic.length()+payLoad.length();
        return length;
    }

    String getTopicAndPayLoad(){
        String topicAndPayload;
        topicAndPayload.concat(topic);
        topicAndPayload.concat(payLoad);
        return topicAndPayload;
    }
};



#endif