#include "MqttBroker/MqttBroker.h" // Necesitamos la definición completa aquí

using namespace mqttBrokerName;

CheckMqttClientTask::CheckMqttClientTask(MqttBroker *broker, QueueHandle_t *brokerEventQueue, QueueHandle_t *deleteMqttClientQueue)
    : Task("CheckMqttClientTask", 1024 * 4, TaskPrio_Low) // 4KB stack, Prioridad Baja
{
    this->broker = broker;
    this->brokerEventQueue = brokerEventQueue;
    this->deleteMqttClientQueue = deleteMqttClientQueue;
}

void CheckMqttClientTask::run (void * data){
  
  // Variables locales para no re-declarar en el bucle
  int clientIdToDelete;
  BrokerEvent* event;
  TickType_t lastKeepAliveCheck = 0;
  const TickType_t KEEP_ALIVE_INTERVAL = 100 / portTICK_PERIOD_MS; // Cada 100ms

  while(true){
    bool workDone = false; // Para saber si podemos dormir o no

    // -------------------------------------------------
    // 1. PRIORIDAD MÁXIMA: Procesar Eventos (Pub/Sub)
    // -------------------------------------------------
    // Procesamos hasta 10 eventos por ciclo para no monopolizar la CPU
    int eventsProcessed = 0;
    while (eventsProcessed < 10 && xQueueReceive((*brokerEventQueue), &event, 0) == pdPASS) {
        
        if (event->type == EVENT_PUBLISH) {
            broker->_publishMessageImpl(event->topic, event->payload);
        } 
        else if (event->type == EVENT_SUBSCRIBE) {
            broker->_subscribeClientImpl(event->topic, event->client);
        }
        
        delete event; // Liberar memoria del evento
        eventsProcessed++;
        workDone = true;
    }

    // -------------------------------------------------
    // 2. PRIORIDAD MEDIA: Limpieza de Muertos
    // -------------------------------------------------
    while (xQueueReceive((*deleteMqttClientQueue), &clientIdToDelete, 0) == pdPASS) {
        broker->deleteMqttClient(clientIdToDelete);
        workDone = true;
    }

    // -------------------------------------------------
    // 3. PRIORIDAD BAJA: Keep Alives (Periódico)
    // -------------------------------------------------
    if ((xTaskGetTickCount() - lastKeepAliveCheck) > KEEP_ALIVE_INTERVAL) {
        broker->processKeepAlives();
        lastKeepAliveCheck = xTaskGetTickCount();
        // No marcamos workDone aquí para permitir dormir si solo fue un chequeo rápido
    }

    // -------------------------------------------------
    // GESTIÓN DE ENERGÍA
    // -------------------------------------------------
    if (workDone) {
        // Si hubo trabajo, solo cedemos el turno por si hay algo urgente
        taskYIELD(); 
    } else {
        // Si no hubo nada que hacer, dormimos un poco para ahorrar CPU
        vTaskDelay(10 / portTICK_PERIOD_MS); 
    }
  }
}