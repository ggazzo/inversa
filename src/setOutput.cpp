
#include "Arduino.h"
#include "setOutput.h"

#ifdef I2C

PCA9685 gpio = (ADDRESS);

#endif

void configureOutputs() { 

    #ifdef I2C
     Wire.begin();

        gpio.resetDevices();       // Resets all PCA9685 devices on i2c line

        gpio.init();               // Initializes module using default totem-pole driver mode, and default disabled phase balancer

        gpio.setPWMFrequency(60); // Set PWM freq to 100Hz (default is 200Hz, supports 24Hz to 1526Hz)
    #else
    #ifdef ESP32
        #define PWM_CHANNEL 0       // LEDC channel (0-15)
        #define PWM_RESOLUTION 7    // PWM resolution in bits (e.g., 8 bits = 0-255 duty cycle)
        #define PWM_FREQUENCY 100   // PWM frequency in Hz
        ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);  // Configure channel
        ledcAttachPin(HEATER_PIN, PWM_CHANNEL);  // Attach the channel to the GPIO to be controlled
        // analogWriteFrequency(PWM_FREQUENCY);
    #else
    analogWriteFreq(100);
    #endif
    pinMode(HEATER_PIN, OUTPUT);
    #endif
}

void setOutput(int outputVal){
    #ifdef I2C
       gpio.setChannelPWM(HEATER_PIN, outputVal); // Set PWM to 128/255, shifted into 4096-land
    #else
        // #ifdef ESP32
        // ledcWrite(HEATER_PIN, outputVal);
        // #else
        analogWrite(HEATER_PIN, outputVal); 
        // #endif
    #endif
}