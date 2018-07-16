#include "general.h"
#include "measurements.h"
#include <rom/rtc.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_FeatherOLED.h>
#include <Statistic.h>

Adafruit_FeatherOLED oled = Adafruit_FeatherOLED();
Measurements m = Measurements();
Statistic readings;

bool runTest = false;
unsigned long battery_last_run = 0;
unsigned long TIMEOUT_US = 500 * 1000; // 500 ms 
int LIGHT_DELTA_THRESHOLD_HIGH;
int LIGHT_DELTA_THRESHOLD_LOW;
double light_bias;

#define BUTTON_A_PIN    A8
#define BUTTON_B_PIN    A7
#define BUTTON_C_PIN    A6
#define LED_PIN         A9

void IRAM_ATTR isr() {
    runTest = true;
}

void bootMsg() {
    uint64_t chipid = ESP.getEfuseMac();
    DEBUG_MSG_P(PSTR("\n\n"));
    DEBUG_MSG_P(PSTR("[INIT] %s %s\n"), (char *) APP_NAME, (char *) APP_VERSION);
    DEBUG_MSG_P(PSTR("[INIT] CPU chip ID: %04X%08X REV:%d\n"), (uint16_t)(chipid>>32), (uint32_t)chipid, ESP.getChipRevision());
    DEBUG_MSG_P(PSTR("[INIT] CPU frequency: %d MHz\n"), ESP.getCpuFreqMHz());
    DEBUG_MSG_P(PSTR("[INIT] SDK version: %s\n"), ESP.getSdkVersion());
    DEBUG_MSG_P(PSTR("\n"));

    FlashMode_t mode = ESP.getFlashChipMode();
    DEBUG_MSG_P(PSTR("[INIT] Flash speed: %u Hz\n"), ESP.getFlashChipSpeed());
    DEBUG_MSG_P(PSTR("[INIT] Flash mode: %s\n"), mode == FM_QIO ? "QIO" : mode == FM_QOUT ? "QOUT" : mode == FM_DIO ? "DIO" : mode == FM_DOUT ? "DOUT" : "UNKNOWN");
    DEBUG_MSG_P(PSTR("[INIT] Flash size: %8u bytes\n"), ESP.getFlashChipSize());
    DEBUG_MSG_P(PSTR("\n"));

    DEBUG_MSG_P(PSTR("[INIT] Last reset reason CPU0: %d CPU1: %d\n"), rtc_get_reset_reason(0), rtc_get_reset_reason(1));
    DEBUG_MSG_P(PSTR("[INIT] Free heap: %u bytes\n"), ESP.getFreeHeap());
    DEBUG_MSG_P(PSTR("\n"));

    #ifdef STAT_USE_STDEV
    DEBUG_MSG_P(PSTR("[INIT] Statistic with STDEV"));
    #else
    DEBUG_MSG_P(PSTR("[INIT] Statistic without STDEV"));
    #endif
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Serial.begin(115200);
    bootMsg();

    oled.init();
    oled.setBatteryVisible(true);
    oled.setCursor(0, 8);
    oled.println("READY");
    oled.println("Press C to test");
    oled.display();

    pinMode(BUTTON_C_PIN, INPUT_PULLUP);
    attachInterrupt(BUTTON_C_PIN, isr, FALLING);
    readings.clear();
}

int lightDelta() {
    return m.readLightSensor() - light_bias;
}

unsigned long measuredLEDOnUs() {
    unsigned long startTime = 0;
    unsigned long elapsedTime = 0;
  
    startTime = micros();
    while (true) {
        // Determine if we timed out
        elapsedTime = micros() - startTime; 
        if (elapsedTime > TIMEOUT_US) {
            Serial.println("Wait led on timeout");
            elapsedTime = -1;
            break;
        }

        // Determine if we got a valid measurement
        elapsedTime = micros() - startTime;
        if (lightDelta() > LIGHT_DELTA_THRESHOLD_HIGH) {
            break;
        }
    }

    DEBUG_MSG_P(PSTR("[LOOP] Delta LED On: %d\n"),  lightDelta());
    return elapsedTime;
}

unsigned long waitLEDMeasuredOffUs() {
    unsigned long elapsedTime = 0;
    unsigned long startTime = micros();

    while(true) {
        elapsedTime = micros() - startTime;
        if (elapsedTime > TIMEOUT_US) {
            Serial.println("Wait led off timeout");
            elapsedTime = -1;
            break;
        }

        elapsedTime = micros() - startTime;
        if (lightDelta() < LIGHT_DELTA_THRESHOLD_LOW) {
            break;
        }
    }

    return elapsedTime;
}

long getCurrLatencyUs() {
    unsigned long elapsedTimeOn = 0;
    unsigned long elapsedTimeOff = 0;

    digitalWrite(LED_PIN, HIGH);
    elapsedTimeOn = measuredLEDOnUs();
    digitalWrite(LED_PIN, LOW);
    elapsedTimeOff = waitLEDMeasuredOffUs();

    DEBUG_MSG_P(PSTR("[LOOP] Delta LED Off: %d Elapsed time on: %lu Elapsed time off: %lu\n"),  lightDelta(), elapsedTimeOn, elapsedTimeOff);

    if ((elapsedTimeOn != -1) && (elapsedTimeOff != -1))
        return (elapsedTimeOn + elapsedTimeOff) / 2;
    else
        return -1;
}

void calibrateThresholds() {
    light_bias = m.readLightSensor();
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    int onLux = lightDelta();
    digitalWrite(LED_PIN, LOW);
    delay(500);
    int offLux = lightDelta();
    LIGHT_DELTA_THRESHOLD_HIGH = onLux * 80 / 100;
    LIGHT_DELTA_THRESHOLD_LOW = onLux * 20 / 100;
    DEBUG_MSG_P(PSTR("[LOOP] Delta LED On: %d Delta LED Off: %d\n"), onLux, offLux);
    DEBUG_MSG_P(PSTR("[LOOP] High Threshold: %d Low Threshold: %d\n"), LIGHT_DELTA_THRESHOLD_HIGH, LIGHT_DELTA_THRESHOLD_LOW);
}

void loop() {
    // Update to battery display every 60s
    if ((millis() - battery_last_run > 60000) || (battery_last_run == 0)) {
        battery_last_run = millis();

        // get the current voltage of the battery from
        // one of the platform specific functions below
        float battery = m.getBatteryVoltage();

        // update the battery icon
        oled.fillRect(77, 0, 51, 7, BLACK);
        oled.setBattery(battery);
        oled.renderBattery();

        // update the display with the new count
        oled.display();
    }

    if (runTest) {
        long currentLatency = 0;

        oled.fillRect(0, 24, 128, 8, BLACK);
        oled.clearMsgArea();
        oled.println("Calibrating...");
        oled.display();
        calibrateThresholds();

        oled.clearMsgArea();
        oled.print("Testing");
        oled.display();
        for (int i = 0; i < 150; i++) {
            currentLatency = getCurrLatencyUs();
            DEBUG_MSG_P(PSTR("[LOOP] Current latency %ld ms\n"), currentLatency / 1000);
            if (currentLatency > 0) {
                readings.add(currentLatency);
                oled.print(".");
                oled.display();
            } else {
                oled.print("x");
                oled.display();
            }
        }
        if (readings.count() > 0)  {
            DEBUG_MSG_P(PSTR("[LOOP] Average latency %f us\n"), readings.average());
            DEBUG_MSG_P(PSTR("[LOOP] Maximum latency %f us\n"), readings.maximum());
            DEBUG_MSG_P(PSTR("[LOOP] Minimum latency %f us\n"), readings.minimum());
            DEBUG_MSG_P(PSTR("[LOOP] Variance latency %f us\n"), readings.variance());
            DEBUG_MSG_P(PSTR("[LOOP] Population standard deviation latency %f us\n"), readings.pop_stdev());
            DEBUG_MSG_P(PSTR("[LOOP] Unbiased standard deviation latency %f us\n"), readings.unbiased_stdev());
            oled.fillRect(0, 24, 128, 8, BLACK);
            oled.clearMsgArea();
            oled.print("Avg:");
            oled.print(readings.average() / 1000);
            oled.print("ms from ");
            oled.println(readings.count());
            oled.print("Std Dev: ");
            oled.println(readings.pop_stdev() / 1000, 8);
            oled.print("Max:");
            oled.print(readings.maximum() / 1000);
            oled.print(" Min:");
            oled.println(readings.minimum() / 1000);
            oled.display();
        } else {
            oled.clearMsgArea();
            oled.println("Error: ");
            oled.println("Latency too low");
            oled.display();
        }
        readings.clear();
        runTest = false;
    }
}
