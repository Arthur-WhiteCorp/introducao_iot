#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// COMMUNICATION MODE SELECTOR
// ============================================================================
#define COMM_MODE_BLE   0
#define COMM_MODE_WIFI  1
#define COMM_MODE_LORA  2

// Select active mode (COMM_MODE_BLE, COMM_MODE_WIFI, or COMM_MODE_LORA)
#define ACTIVE_COMM_MODE COMM_MODE_LORA

// ============================================================================
// GENERAL NODE CONFIGURATION
// ============================================================================
#define NODE_ID "ICC316-G-NODE1"  // Must match prefix expected by Pi Gateway

// ============================================================================
// BLUETOOTH LOW ENERGY (BLE) CONFIGURATION
// ============================================================================
#define BLUETOOTH_DEVICE_NAME             "ICC316-G-NODE1"
#define BLUETOOTH_SERVICE_UUID            "7d8f0001-3160-4b12-9a10-000000000001"
#define BLUETOOTH_MEASUREMENT_UUID        "7d8f0002-3160-4b12-9a10-000000000002"
#define BLUETOOTH_ACK_UUID                "7d8f0003-3160-4b12-9a10-000000000003"
#define ICC316_BT_ACK                     "OK"

#define BLUETOOTH_CONNECTION_TIMEOUT_MS   10000  // 10 seconds
#define BLUETOOTH_ACK_TIMEOUT_MS          5000   // 5 seconds

// ============================================================================
// WI-FI CONFIGURATION (HTTP / API Endpoint)
// ============================================================================
#define WIFI_SSID                        "AndroidAP6D68"
#define WIFI_PASSWORD                    "hpqis4103"
#define WIFI_TIMEOUT_MS           15000  // 15 seconds
#define GATEWAY_PORT                      5000
#define GATEWAY_IP                        "10.203.219.125"
#define HTTP_TIMEOUT_MS              5000   // 5 seconds
// ============================================================================
// LORA CONFIGURATION (Heltec WiFi LoRa 32 V3 - SX1262 Radio)
// ============================================================================
  // RF Parameters

#define NODE_ID "grupo_01"

#define LORA_CS_PIN 8
#define LORA_DIO1_PIN 14
#define LORA_RST_PIN 12
#define LORA_BUSY_PIN 13

#define LORA_FREQUENCY_MHZ 915.0
#define LORA_BANDWIDTH_KHZ 125.0
#define LORA_SPREADING_FACTOR 7

// RadioLib usa 5 para representar coding rate 4/5.
#define LORA_CODING_RATE 5

#define LORA_SYNC_WORD 0x12
#define LORA_OUTPUT_POWER_DBM 14
#define LORA_PREAMBLE_LENGTH 8

// TCXO da Heltec V3.
#define LORA_TCXO_VOLTAGE 1.8

#define LORA_ACK_TIMEOUT_MS 60000
#define LORA_ACK_OK "OK"

#endif // CONFIG_H
