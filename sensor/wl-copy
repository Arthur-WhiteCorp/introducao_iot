#include "ICC316LoRa.h"

#include <RadioLib.h>
#include <ArduinoJson.h>
#include "config.h"

namespace {
SX1262 radio = new Module(
    LORA_CS_PIN, LORA_DIO1_PIN, LORA_RST_PIN, LORA_BUSY_PIN
);
bool initialized = false;

bool initializeRadio() {
    if (initialized) return true;

    Serial.println(F("[LoRa] Initializing SX1262 radio..."));

    int16_t state = radio.begin(
        LORA_FREQUENCY_MHZ,
        LORA_BANDWIDTH_KHZ,
        LORA_SPREADING_FACTOR,
        LORA_CODING_RATE,
        LORA_SYNC_WORD,
        LORA_OUTPUT_POWER_DBM,
        LORA_PREAMBLE_LENGTH,
        LORA_TCXO_VOLTAGE
    );

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("[LoRa Error] Initialization failed with error code: "));
        Serial.println(state);
        return false;
    }

    initialized = true;
    Serial.println(F("[LoRa] Radio initialized successfully."));
    return true;
}
}

bool sendMeasurementLora(
    const String& timestamp,
    float temperature,
    float ph,
    float turbidity
) {
    if (!initializeRadio()) {
        Serial.println(F("[LoRa Error] Aborting send: Radio failed to initialize."));
        return false;
    }

    JsonDocument doc;
    doc["node_id"] = NODE_ID;
    doc["timestamp"] = timestamp;
    doc["temperature"] = temperature;

    if (isnan(ph)) doc["ph"] = nullptr;
    else doc["ph"] = ph;

    if (isnan(turbidity)) doc["turbidity"] = nullptr;
    else doc["turbidity"] = turbidity;

    String payload;
    serializeJson(doc, payload);

    Serial.print(F("[LoRa Tx] Transmitting packet: "));
    Serial.println(payload);

    int16_t state = radio.transmit(payload);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("[LoRa Error] Transmission failed with error code: "));
        Serial.println(state);
        return false;
    }
    Serial.println(F("[LoRa Tx] Packet sent successfully. Waiting for ACK..."));

    String ack;
    state = radio.receive(ack, LORA_ACK_TIMEOUT_MS);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("[LoRa Error] ACK reception failed or timed out. Error code: "));
        Serial.println(state);
        return false;
    }

    Serial.print(F("[LoRa Rx] Received ACK string: '"));
    Serial.print(ack);
    Serial.println(F("'"));

    if (ack != LORA_ACK_OK) {
        Serial.print(F("[LoRa Error] ACK mismatch. Expected: '"));
        Serial.print(LORA_ACK_OK);
        Serial.println(F("'"));
        return false;
    }

    Serial.println(F("[LoRa] Transmission and ACK verification complete."));
    return true;
}