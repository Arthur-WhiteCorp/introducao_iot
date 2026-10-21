#ifndef GATEWAY_SENDER_HPP
#define GATEWAY_SENDER_HPP

#include <Arduino.h>

// =============================================================
// Configuration Definitions
// =============================================================

#ifndef WIFI_SSID
#define WIFI_SSID       "labmaker-03"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD   "maker123"
#endif

#ifndef GATEWAY_IP
#define GATEWAY_IP      "10.203.219.125"
#endif

#ifndef GATEWAY_PORT
#define GATEWAY_PORT    5000
#endif

#ifndef GATEWAY_PATH
#define GATEWAY_PATH    "/dados"
#endif

#ifndef NODE_ID
#define NODE_ID         "grupo_01"
#endif

#ifndef WIFI_TIMEOUT_MS
#define WIFI_TIMEOUT_MS 10000
#endif

#ifndef HTTP_TIMEOUT_MS
#define HTTP_TIMEOUT_MS 5000
#endif

// =============================================================
// Function Prototypes
// =============================================================

/**
 * Connects to Wi-Fi if not currently connected.
 * @return true if connected, false on timeout/failure.
 */
bool ensureWiFiConnection();

/**
 * Sends measurement data to the Raspberry Pi gateway via HTTP POST.
 * 
 * @param timestamp   ISO timestamp or formatted time string
 * @param temperature Water temperature in Celsius (use NAN if unavailable)
 * @param ph          pH value (use NAN if unavailable)
 * @param turbidity   Turbidity value (use NAN if unavailable)
 * @return true if gateway returns {"status": "ok"}, false otherwise.
 */
bool sendMeasurement(
    const String& timestamp,
    float temperature,
    float ph,
    float turbidity
);

#endif // GATEWAY_SENDER_HPP
