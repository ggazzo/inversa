#ifndef NTC_SENSOR_TEMPERATURE_H
#define NTC_SENSOR_TEMPERATURE_H

#include "Temperature.h"

#include <NTC_Thermistor_CustomFormula_ESP32.h>

class NTC_Sensor_Temperature : public TemperatureSensor {

    public:
        NTC_Sensor_Temperature(Thermistor *thermistor, on_change_callback_t on_change);
        ~NTC_Sensor_Temperature();

        float getTemperature() override;
        virtual void setup() override;
        virtual void loop() override;

    private:
        Thermistor* thermistor;
        xTaskHandle taskHandle;
        static void monitorTask(void *pvParameters);
  
};


#endif
