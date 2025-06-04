#include "PreparingState.h"

void PreparingStateMachine::monitorTask(void *pvParameters) {
   PreparingStateMachine *self = (PreparingStateMachine *)pvParameters;

   while (true) {

      if (controller->isTimeFinished()) {
         controller->skip();
         break;
      }

      float currentTemp = controller->getTemperature();

      float heatingTime = calculateHeatingTime_seconds(
         self->volume_liters,
         self->power_watts,
         currentTemp,
         self->target_temperature_c
      );

      unsigned long remainingTime = controller->remainingTime();

      ESP_LOGI("PreparingState", "Remaining time: %d, target temperature: %.2f, current temperature: %.2f",
         remainingTime, self->target_temperature_c, currentTemp);

      if (remainingTime <= heatingTime) {
         ESP_LOGI("PreparingState", "Heating");
         controller->setTargetTemperature(self->target_temperature_c);
      } else {
         ESP_LOGI("PreparingState", "Waiting");
         controller->setTargetTemperature(0);
      }

      // Give other tasks a chance to run and prevent watchdog timer issues
      vTaskDelay(1000/portTICK_PERIOD_MS);
   }
   
   // Clean up before exiting
   if (self->taskHandle != NULL) {
      self->taskHandle = NULL;
   }
   vTaskDelete(NULL);
}
