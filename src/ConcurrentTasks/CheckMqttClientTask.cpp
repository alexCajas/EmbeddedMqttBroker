#include "MqttBroker/MqttBroker.h" 

using namespace mqttBrokerName;


CheckMqttClientTask::CheckMqttClientTask(MqttBroker *broker)
    : Task("MqttWorker", 1024 * 4, TaskPrio_Low) 
{
    this->broker = broker;
    // Note: The stack size (4KB) is chosen to handle the overhead of 
    // processing MQTT packets and Trie traversals without overflow.
    // Priority is Low to ensure it doesn't starve the Network Task (LwIP).
}


void CheckMqttClientTask::run (void * data){
  
  TickType_t lastKeepAliveCheck = 0;
  
  // Define the interval for maintenance checks (100ms).
  // This prevents checking timeouts in every single CPU cycle.
  const TickType_t KEEP_ALIVE_INTERVAL = 100 / portTICK_PERIOD_MS; 

  while(true){
    bool workDone = false; // Flag to determine the CPU yielding strategy

    // 1. HIGH PRIORITY: Process Pub/Sub Events
    // We delegate to the broker to process a batch of pending events.
    // If true, it means we handled data (throughput is needed).
    if (broker->processBrokerEvents()) {
        workDone = true;
    }

    // 2. MEDIUM PRIORITY: Garbage Collection
    // Process the deletion queue to free memory of disconnected clients.
    if (broker->processDeletions()) {
        workDone = true;
    }

    // 3. LOW PRIORITY: Periodic Maintenance (Keep Alive)
    // Runs only if the time interval has passed.
    if ((xTaskGetTickCount() - lastKeepAliveCheck) > KEEP_ALIVE_INTERVAL) {
        broker->processKeepAlives();
        lastKeepAliveCheck = xTaskGetTickCount();
    }

    // --- ADAPTIVE SCHEDULING ---
    if (workDone) {
        // High Load Mode: If we did work, there might be more coming immediately.
        // We yield the current timeslice but remain ready to run ASAP.
        taskYIELD(); 
    } else {
        // Idle Mode: If queues are empty, sleep for 10ms to save power 
        // and free up the CPU for other system tasks (like WiFi).
        vTaskDelay(10 / portTICK_PERIOD_MS); 
    }
  }
}