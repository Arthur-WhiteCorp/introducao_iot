#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include <string.h>
#include "esp32_wifi.hpp"

// =============================================================
// Ensure Wi-Fi connection
// =============================================================

bool ensureWiFiConnection()
{
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    Serial.println("[WiFi] Connecting...");

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const unsigned long startTime = millis();

    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime >= WIFI_TIMEOUT_MS) {
            Serial.println("[WiFi] Connection timeout.");
            return false;
        }

        delay(100);
    }

    Serial.print("[WiFi] Connected. IP: ");
    Serial.println(WiFi.localIP());

    return true;
}


// =============================================================
// Send measurement to Raspberry Pi gateway
// =============================================================

bool sendMeasurement(
    const String& timestamp,
    float temperature,
    float ph,
    float turbidity
)
{
    // 1. Ensure Wi-Fi connection
    if (!ensureWiFiConnection()) {
        Serial.println("[HTTP] Wi-Fi unavailable.");
        return false;
    }

    // 2. Build gateway URL
    const String url =
        String("http://") +
        GATEWAY_IP + ":" +
        String(GATEWAY_PORT) +
        GATEWAY_PATH;

    // 3. Build JSON payload (ArduinoJson 7)
    JsonDocument doc;

    doc["node_id"] = NODE_ID;
    doc["timestamp"] = timestamp;

    if (isfinite(temperature)) {
        doc["temperature"] = temperature;
    } else {
        doc["temperature"] = nullptr;
    }

    if (isfinite(ph)) {
        doc["ph"] = ph;
    } else {
        doc["ph"] = nullptr;
    }

    if (isfinite(turbidity)) {
        doc["turbidity"] = turbidity;
    } else {
        doc["turbidity"] = nullptr;
    }

    // 4. Serialize JSON
    String payload;
    serializeJson(doc, payload);

    Serial.println("[HTTP] Sending measurement:");
    Serial.println(payload);

    // 5. Initialize HTTP client
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);

    if (!http.begin(url)) {
        Serial.println("[HTTP] Failed to initialize connection.");
        return false;
    }

    http.addHeader("Content-Type", "application/json");

    // 6. Send HTTP POST request
    const int httpCode = http.POST(payload);

    bool success = false;

    // 7. Validate gateway response
    if (httpCode == HTTP_CODE_OK) {
        const String response = http.getString();

        Serial.print("[HTTP] Gateway response: ");
        Serial.println(response);

        JsonDocument responseDoc;

        const DeserializationError error =
            deserializeJson(responseDoc, response);

        if (!error) {
            const char* status = responseDoc["status"] | "";

            if (strcmp(status, "ok") == 0) {
                success = true;
                Serial.println("[HTTP] Measurement acknowledged.");
            } else {
                Serial.println("[HTTP] Gateway did not confirm measurement.");
            }
        } else {
            Serial.println("[HTTP] Invalid JSON response.");
        }
    } else if (httpCode > 0) {
        Serial.print("[HTTP] Unexpected status code: ");
        Serial.println(httpCode);
    } else {
        Serial.print("[HTTP] Request failed: ");
        Serial.println(http.errorToString(httpCode));
    }

    // 8. Release HTTP resources
    http.end();

    return success;
}
