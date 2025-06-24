#ifndef RTC_H
#define RTC_H

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266mDNS.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <ESPmDNS.h>
#endif

#include <RTClib.h>
#include <NTPClient.h>

#include "Components/RTC.h"

WiFiUDP ntpUDP;

const long utcOffsetInSeconds = - 3 * 60 * 60;
class RTC : public IRTC {
    public:

        RTC():timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds) {}


        void setup() override {
            if (rtc.begin()) {
                ESP_LOGI("LocalController", "RTC Begin");
                if (!rtc.isrunning()) {
                        ESP_LOGI("LocalController", "Failed to set time from NTP, setting time from compile date");
                        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
                }
            }
    /**
     * Setup API and start task to update time from NTP every hour
     */
            WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
                vTaskDelete(timerTask);
                xTaskCreate(monitorTask, "LocalController::monitor", configMINIMAL_STACK_SIZE * 4, this, 1, &timerTask);
            }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);

        }

        void loop() override {
            // RTC loop implementation - currently empty as RTC operations are handled in the monitor task
        }   

        uint32_t now() override {
            return rtc.now().secondstime();
        }
        void setTime(char* isoDate) override{
            rtc.adjust(DateTime(isoDate));
        }

    private:
        RTC_DS1307 rtc;
        NTPClient timeClient;
        TaskHandle_t timerTask;

        static void monitorTask(void *pvParameters) {
            RTC *rtc = (RTC *)pvParameters;
            while (true) {

                if (!rtc->rtc.begin()) {
                    continue;
                }

                rtc->timeClient.begin();
                ESP_LOGI("RTC", "NTP Client Begin");
                if(rtc->timeClient.update()){
                    ESP_LOGI("RTC", "NTP Client Update");
                    rtc->rtc.adjust(DateTime(rtc->timeClient.getEpochTime()));
                }
                vTaskDelay(1000 * 60 * 60 / portTICK_PERIOD_MS); // every hour
            }
        }
};

#endif