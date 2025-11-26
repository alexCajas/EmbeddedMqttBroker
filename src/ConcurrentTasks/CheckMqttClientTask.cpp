#include "MqttBroker/MqttBroker.h" 

using namespace mqttBrokerName;

CheckMqttClientTask::CheckMqttClientTask(MqttBroker *broker)
    : Task("MqttWorker", 1024 * 4, TaskPrio_Low) 
{
    this->broker = broker;
}

void CheckMqttClientTask::run (void * data){
  
  TickType_t lastKeepAliveCheck = 0;
  // Intervalo para chequear KeepAlives (ej. cada 100ms)
  const TickType_t KEEP_ALIVE_INTERVAL = 100 / portTICK_PERIOD_MS; 

  while(true){
    bool workDone = false;

    // 1. Procesar Eventos de Pub/Sub (Prioridad Alta)
    // Delegamos al broker. Si retorna true, es que procesó algo.
    if (broker->processBrokerEvents()) {
        workDone = true;
    }

    // 2. Procesar Limpieza de Clientes (Prioridad Media)
    if (broker->processDeletions()) {
        workDone = true;
    }

    // 3. Procesar Keep Alives (Periódico - Prioridad Baja)
    if ((xTaskGetTickCount() - lastKeepAliveCheck) > KEEP_ALIVE_INTERVAL) {
        broker->processKeepAlives();
        lastKeepAliveCheck = xTaskGetTickCount();
    }

    // Gestión de energía y ceder CPU
    if (workDone) {
        // Si hubo trabajo, cedemos el turno por si hay algo urgente
        taskYIELD(); 
    } else {
        // Si no hubo nada, dormimos un poco para no saturar el Core 0
        vTaskDelay(10 / portTICK_PERIOD_MS); 
    }
  }
}