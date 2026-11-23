#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <RTClib.h>
#include <cmath>
#include "Bluetooth.h"

//#include "ICC316WiFi.h"
#include "esp32_wifi.hpp"
#include "ICC316LoRa.h"
#include "config.h"

// Hardware Pin Definitions
#define ONE_WIRE_BUS  7
#define SD_CS         2
#define SD_MOSI       34
#define SD_MISO       19
#define SD_SCK        5
#define I2C_SDA       41
#define I2C_SCL       42
#define TURBIDITY_PIN 1  // ADC1 Pin for Turbidity Sensor
#define PH_PIN        4  // ADC1 Pin for pH Sensor (Po pin)

// Calibration constants for pH-4502C
const float PH_NEUTRAL_VOLTAGE = 2.50; // Voltage at pH 7.0
const float PH_SLOPE = -0.18;          // Volts per pH unit

// Object Instances
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
SPIClass sdSPI(HSPI);
RTC_DS3231 rtc;

// Configuration & State Parameters
const unsigned long SAMPLE_INTERVAL_MS = 30000;
const char* CSV_PATH = "/data.csv";
const char* TMP_CSV_PATH = "/data_tmp.csv";

String transmissionType;
bool hasPendingData = false;

/*
 * Helper function to read and calculate pH
 */
float readPH() {
  const int numSamples = 20;
  unsigned long adcSum = 0;

  for (int i = 0; i < numSamples; i++) {
    adcSum += analogRead(PH_PIN);
    delay(10);
  }

  float avgAdc = (float)adcSum / numSamples;
  float voltage = (avgAdc / 4095.0) * 3.3;

  float phValue = 7.0 + ((PH_NEUTRAL_VOLTAGE - voltage) / 0.18);

  if (phValue < 0.0) phValue = 0.0;
  if (phValue > 14.0) phValue = 14.0;

  return phValue;
}

/*
 * Helper function to read and calculate Turbidity (NTU)
 */
float readTurbidity() {
  const int numSamples = 20;
  unsigned long adcSum = 0;

  for (int i = 0; i < numSamples; i++) {
    adcSum += analogRead(TURBIDITY_PIN);
    delay(10);
  }

  float avgAdc = (float)adcSum / numSamples;
  float voltage = (avgAdc / 4095.0) * 3.3;

  float ntu = -1120.4 * (voltage * voltage) + 5742.3 * voltage - 4353.8;

  if (ntu < 0.0) ntu = 0.0;
  if (ntu > 3000.0) ntu = 3000.0;

  return ntu;
}

/*
 * Helper function to initialize the DS3231 RTC module
 */
void setupRTC() {
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!rtc.begin(&Wire)) {
    Serial.println("Error: Could not find DS3231 RTC module!");
    return;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power! Setting time to compile time...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  Serial.println("RTC DS3231 initialized successfully.");
}

/*
 * Generate ISO 8601 formatted timestamp string from RTC (YYYY-MM-DDTHH:MM:SS)
 */
String getISOTimestamp() {
  DateTime now = rtc.now();
  char buf[25];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  return String(buf);
}

/*
 * Scans /data.csv at boot to check if any undelivered rows already exist
 */
bool scanForPendingRecords() {
  File csvFile = SD.open(CSV_PATH, FILE_READ);
  if (!csvFile) return false;

  // Skip header
  if (csvFile.available()) {
    csvFile.readStringUntil('\n');
  }

  while (csvFile.available()) {
    String line = csvFile.readStringUntil('\n');
    line.trim();
    if (line.endsWith(",false")) {
      csvFile.close();
      return true;
    }
  }

  csvFile.close();
  return false;
}

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

  if (!SD.exists(CSV_PATH)) {
    File headerFile = SD.open(CSV_PATH, FILE_WRITE);
    if (headerFile) {
      headerFile.println("Timestamp,Temperature_C,pH,Turbidity_NTU,delivered");
      headerFile.close();
      Serial.println("Created /data.csv with header.");
    }
  } else {
    hasPendingData = scanForPendingRecords();
    if (hasPendingData) {
      Serial.println("Found pending undelivered readings on SD card.");
    }
  }
}

/*
 * Helper function to log sensor data locally to SD Card with delivery status
 */
bool logToSD(const String& timestamp, float tempC, float ph, float turbidity, bool delivered) {
  File csvFile = SD.open(CSV_PATH, FILE_APPEND);
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

  csvFile.print(",");
  csvFile.println(delivered ? "true" : "false");
  csvFile.close();

  if (!delivered) {
    hasPendingData = true;
  }

  Serial.printf("CSV Logged locally to SD Card [delivered=%s].\n", delivered ? "true" : "false");
  return true;
}

bool transmitData(String timestamp, float temp, float ph, float turbidity) {
#if (ACTIVE_COMM_MODE == COMM_MODE_BLE)
  return sendMeasurementBle(timestamp, temp, ph, turbidity); // BLE logic[cite: 4]
#elif (ACTIVE_COMM_MODE == COMM_MODE_WIFI)
  return sendMeasurement(timestamp, temp, ph, turbidity);    // Wi-Fi logic[cite: 4]
#elif (ACTIVE_COMM_MODE == COMM_MODE_LORA)
  return sendMeasurementLora(timestamp, temp, ph, turbidity);  // LoRa logic[cite: 4]
#else
  return false;
#endif
}

/*
 * Replays all rows with delivered=false in quick succession and updates /data.csv
 */
void syncPendingReadings() {
  Serial.println("\n--- Starting Sync of Pending Readings ---");

  File srcFile = SD.open(CSV_PATH, FILE_READ);
  if (!srcFile) {
    Serial.println("Sync Error: Could not open /data.csv for reading.");
    return;
  }

  if (SD.exists(TMP_CSV_PATH)) {
    SD.remove(TMP_CSV_PATH);
  }

  File dstFile = SD.open(TMP_CSV_PATH, FILE_WRITE);
  if (!dstFile) {
    Serial.println("Sync Error: Could not create temporary CSV file.");
    srcFile.close();
    return;
  }

  // Copy header line
  if (srcFile.available()) {
    String header = srcFile.readStringUntil('\n');
    header.trim();
    dstFile.println(header);
  }

  bool stillHasPending = false;
  bool connectionLostMidSync = false;
  int syncedCount = 0;

  while (srcFile.available()) {
    String line = srcFile.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    // Parse CSV columns: Timestamp,Temperature_C,pH,Turbidity_NTU,delivered
    int c1 = line.indexOf(',');
    int c2 = line.indexOf(',', c1 + 1);
    int c3 = line.indexOf(',', c2 + 1);
    int c4 = line.indexOf(',', c3 + 1);

    if (c1 == -1 || c2 == -1 || c3 == -1 || c4 == -1) {
      // Preserve any malformed line without crashing
      dstFile.println(line);
      continue;
    }

    String tsStr = line.substring(0, c1);
    String tempStr = line.substring(c1 + 1, c2);
    String phStr = line.substring(c2 + 1, c3);
    String turbStr = line.substring(c3 + 1, c4);
    String deliveredStr = line.substring(c4 + 1);
    deliveredStr.trim();

    if (deliveredStr == "false") {
      if (connectionLostMidSync) {
        // Keep remaining rows marked false if gateway disconnected during sync
        dstFile.println(line);
        stillHasPending = true;
        continue;
      }

      float tempVal = tempStr.toFloat();
      float phVal = (phStr == "NA") ? NAN : phStr.toFloat();
      float turbVal = (turbStr == "NA") ? NAN : turbStr.toFloat();

      Serial.printf("Replaying pending record [%s]... ", tsStr.c_str());
      delay(300); // Brief pause to let gateway/BLE radio settle between packets

      bool ok = transmitData(tsStr, tempVal, phVal, turbVal);
      if (ok) {
        Serial.println("DELIVERED");
        dstFile.printf("%s,%s,%s,%s,true\n", tsStr.c_str(), tempStr.c_str(), phStr.c_str(), turbStr.c_str());
        syncedCount++;
      } else {
        Serial.println("FAILED (aborting remaining sync until next connection)");
        dstFile.println(line);
        stillHasPending = true;
        connectionLostMidSync = true;
      }
    } else {
      // Already delivered
      dstFile.println(line);
    }
  }

  srcFile.close();
  dstFile.close();

  // Atomically replace old CSV with updated CSV
  SD.remove(CSV_PATH);
  SD.rename(TMP_CSV_PATH, CSV_PATH);

  hasPendingData = stillHasPending;
  Serial.printf("--- Sync Complete: %d record(s) delivered. Pending remaining: %s ---\n\n",
                syncedCount, hasPendingData ? "YES" : "NO");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing Environmental Sensing Node...");

  // Configure ADC resolution for ESP32
  analogReadResolution(12);
  pinMode(TURBIDITY_PIN, INPUT);
  pinMode(PH_PIN, INPUT);

  // Start Hardware Modules
  sensors.begin();
  setupSDCard();
  setupRTC();

  if (ACTIVE_COMM_MODE == COMM_MODE_BLE) {
    transmissionType = "Bluetooth";
  }
  if (ACTIVE_COMM_MODE == COMM_MODE_WIFI) {
    transmissionType = "WiFi";
  }
  if (ACTIVE_COMM_MODE == COMM_MODE_LORA) {
    transmissionType = "Lora";
  }

  Serial.println("Setup Complete. Entering main loop...");
}

void loop() {
  // 1. Read All Active Sensors
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  float ph = readPH();
  float turbidity = readTurbidity();

  if (tempC != DEVICE_DISCONNECTED_C) {
    String timestampStr = getISOTimestamp();

    Serial.printf("\n--- New Reading [%s] ---\n", timestampStr.c_str());
    Serial.printf("Temperature: %.2f °C\n", tempC);
    Serial.printf("pH Level   : %.2f\n", ph);
    Serial.printf("Turbidity  : %.2f NTU\n", turbidity);

    // 2. Attempt Wireless Transmission First
    Serial.printf("Attempting transmission to Gateway via: %s\n", transmissionType.c_str());
    bool success = transmitData(timestampStr, tempC, ph, turbidity);

    if (success) {
      Serial.println("Transmission: SUCCESS (ACK received)");
    } else {
      Serial.println("Transmission: FAILED or TIMED OUT");
    }

    // 3. Log Current Reading to SD Card AFTER Transmission Attempt
    logToSD(timestampStr, tempC, ph, turbidity, success);

    // 4. If Gateway is Reachable and Backlog Exists, Flush All Pending Readings
    if (success && hasPendingData) {
      syncPendingReadings();
    }

  } else {
    Serial.println("Error: DS18B20 disconnected. Check 4.7k pull-up resistor & wiring.");
  }

  // 5. Wait for next measurement cycle
  delay(SAMPLE_INTERVAL_MS);
}