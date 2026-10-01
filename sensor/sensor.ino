#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <SD.h>
#include <cmath>
#include "Bluetooth.h"
#include "config.h"

// Hardware Pin Definitions
#define ONE_WIRE_BUS 7
#define SD_CS        2
#define SD_MOSI      34
#define SD_MISO      19
#define SD_SCK       5

// Instance Initialization
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
SPIClass sdSPI(HSPI);

// Configuration parameters
const unsigned long SAMPLE_INTERVAL_MS = 900000; // 15 minutes between reads

/*
 * Helper function to ensure SD Card CSV header exists
 */
void setupSDCard() {
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  
  int attempts = 0;
  while (!SD.begin(SD_CS, sdSPI) && attempts < 5) {
    Serial.println("SD Card Mount Failed! Retrying...");
    delay(500);
    attempts++;
  }

  if (attempts >= 5) {
    Serial.println("Warning: SD Card failed to initialize. System will continue without logging.");
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached.");
    return;
  }

  Serial.println("SD Card Mounted Successfully!");

  // Create header if CSV does not exist
  if (!SD.exists("/data.csv")) {
    File headerFile = SD.open("/data.csv", FILE_WRITE);
    if (headerFile) {
      headerFile.println("Timestamp_ms,Temperature_C,pH,Turbidity");
      headerFile.close();
      Serial.println("Created /data.csv with header.");
    }
  }
}

/*
 * Helper function to log sensor data locally to SD Card
 */
bool logToSD(const String& timestamp, float tempC, float ph, float turbidity) {
  File csvFile = SD.open("/data.csv", FILE_APPEND);
  if (!csvFile) {
    Serial.println("Error: Could not open /data.csv for writing.");
    return false;
  }

  csvFile.print(timestamp);
  csvFile.print(",");
  csvFile.print(tempC, 2);
  csvFile.print(",");
  
  if (isnan(ph)) csvFile.print("NA");
  else csvFile.print(ph, 2);
  
  csvFile.print(",");
  if (isnan(turbidity)) csvFile.print("NA");
  else csvFile.print(turbidity, 2);

  csvFile.println();
  csvFile.close();

  Serial.println("CSV Logged locally to SD Card.");
  return true;
}

void transmitData(String timestamp, float temp, float ph, float turbidity) {
#if (ACTIVE_COMM_MODE == COMM_MODE_BLE)
  sendMeasurement(timestamp, temp, ph, turbidity); // BLE logic
#elif (ACTIVE_COMM_MODE == COMM_MODE_WIFI)
  sendWiFiHTTP(timestamp, temp, ph, turbidity);    // Wi-Fi logic
#elif (ACTIVE_COMM_MODE == COMM_MODE_LORA)
  sendLoRaPacket(timestamp, temp, ph, turbidity);  // LoRa logic
#endif
}

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing Environmental Sensing Node...");

  // Start Temperature Sensor
  sensors.begin();

  // Initialize SD Card
  setupSDCard();

  Serial.println("Setup Complete. Entering main loop...");
}

void loop() {
  // 1. Read Sensors
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  // Set pH and Turbidity to NAN (Not A Number) if sensors are not yet connected
  float ph = NAN; 
  float turbidity = NAN;

  if (tempC != DEVICE_DISCONNECTED_C) {
    // Generate timestamp string from uptime in milliseconds
    String timestampStr = String(millis());

    Serial.printf("\n--- New Reading [%s ms] ---\n", timestampStr.c_str());
    Serial.printf("Temperature: %.2f °C\n", tempC);

    // 2. Persistent Local Storage
    logToSD(timestampStr, tempC, ph, turbidity);

    // 3. Wireless Transmission over BLE
    Serial.println("Attempting BLE transmission to Raspberry Pi Gateway...");
    bool bleSuccess = transmitData(timestampStr, tempC, ph, turbidity);

    if (bleSuccess) {
      Serial.println("BLE Transmission: SUCCESS (ACK received)");
    } else {
      Serial.println("BLE Transmission: FAILED or TIMED OUT");
    }

  } else {
    Serial.println("Error: DS18B20 disconnected. Check 4.7k pull-up resistor & wiring.");
  }

  // 4. Wait for next measurement cycle
  delay(SAMPLE_INTERVAL_MS);
}
