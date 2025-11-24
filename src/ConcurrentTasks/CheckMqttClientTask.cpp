#include "MqttBroker/MqttBroker.h" // Necesitamos la definición completa aquí

using namespace mqttBrokerName;

CheckMqttClientTask::CheckMqttClientTask(MqttBroker *broker)
    : Task("CheckMqttClientTask", 1024 * 4, TaskPrio_Low) // 4KB stack, Prioridad Baja
{
    this->broker = broker;
}

void CheckMqttClientTask::run(void *data) {
    // Frecuencia de chequeo de KeepAlive (ej. cada 100ms)
    const TickType_t keepAliveInterval = 100 / portTICK_PERIOD_MS;
    TickType_t lastWakeTime = xTaskGetTickCount();

    while (true) {
        // 1. Procesar la cola de borrado (Prioridad Alta)
        // Delegamos la lógica al broker, que sabe cómo leer su cola.
        // Esto procesará todos los clientes pendientes de borrar en este ciclo.
        broker->processDeletions();

        // 2. Chequear KeepAlives (Prioridad Normal)
        // No hace falta hacerlo en cada tick, usamos un delay controlado.
        broker->processKeepAlives();

        // 3. Ceder CPU (Delay absoluto para mantener ritmo constante)
        vTaskDelayUntil(&lastWakeTime, keepAliveInterval);
    }
}