#pragma once

// ─── Board Detection ────────────────────────────────────────
#if defined(BOARD_S3_MINI)
    #include "boards/s3_mini.h"
#elif defined(BOARD_C3_MINI)
    #include "boards/c3_mini.h"
#else
    #error "No board defined. Use -D BOARD_S3_MINI or -D BOARD_C3_MINI"
#endif

// ─── Firmware Info ──────────────────────────────────────────
#ifndef BUILD_GIT_VERSION
    #define BUILD_GIT_VERSION "dev"
#endif
#ifndef BUILD_TIMESTAMP
    #define BUILD_TIMESTAMP "unknown"
#endif
#ifndef FIRMWARE_NAME
    #define FIRMWARE_NAME "inversa"
#endif

// ─── NTC Thermistor Configuration ───────────────────────────
#define NTC_REFERENCE_RESISTANCE  10000   // 10K reference resistor
#define NTC_NOMINAL_RESISTANCE    10000   // 10K NTC at 25°C
#define NTC_NOMINAL_TEMPERATURE   25.0f   // 25°C nominal
#define NTC_B_COEFFICIENT         3950    // Beta coefficient
#define NTC_SUPPLY_VOLTAGE_MV     3300    // 3.3V supply
#define NTC_ADC_RESOLUTION        4095    // 12-bit ADC

// ─── Temperature Limits ─────────────────────────────────────
#define TEMP_MIN                  0.0f
#define TEMP_MAX                  150.0f
#define TEMP_ERROR_VALUE          -999.0f

// ─── PID Defaults ───────────────────────────────────────────
#define PID_KP_DEFAULT            20.0f
#define PID_KI_DEFAULT            0.01f
#define PID_KD_DEFAULT            2000.0f
#define PID_KP_MAX                500.0f
#define PID_KI_MAX                10.0f
#define PID_KD_MAX                50000.0f
#define PID_OUTPUT_MIN            0.0f
#define PID_OUTPUT_MAX            255.0f
#define PID_SAMPLE_TIME_MS        1000

// ─── Control Loop ───────────────────────────────────────────
#define LOOP_INTERVAL_MS          1000
#define TELEMETRY_INTERVAL_MS     1000

// ─── Kalman Filter ──────────────────────────────────────────
#define KALMAN_MEASURE_ERROR      1.0f
#define KALMAN_ESTIMATE_ERROR     1.0f
#define KALMAN_PROCESS_NOISE      0.01f

// ─── BLE Configuration ─────────────────────────────────────
#define BLE_DEVICE_NAME           "Inversa"
#define BLE_SERVICE_UUID          "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // Nordic UART
#define BLE_CHAR_TX_UUID          "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // Notify
#define BLE_CHAR_RX_UUID          "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // Write

// ─── SD Card ────────────────────────────────────────────────
#define SD_RECIPES_DIR            "/recipes"
#define SD_RECOVERY_FILE          "/recovery.bin"

// ─── Power Loss Recovery ────────────────────────────────────
#define RECOVERY_SAVE_INTERVAL_MS 5000
