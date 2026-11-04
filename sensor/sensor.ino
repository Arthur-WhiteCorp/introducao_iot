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
#define ONE_WIRE_BUS 7
#define SD_CS        2
#define SD_MOSI      34
#define SD_MISO      19
#define SD_SCK       5
#define I2C_SDA      41
#define I2C_SCL      42

// Instance Initialization
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
SPIClass sdSPI(HSPI);
RTC_DS3231 rtc;

// Configuration parameters
const unsigned long SAMPLE_INTERVAL_MS = 30000;
String transmissionType;

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
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  return String(buf);
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

  // Create header if CSV does not exist
  if (!SD.exists("/data.csv")) {
    File headerFile = SD.open("/data.csv", FILE_WRITE);
    if (headerFile) {
      headerFile.println("Timestamp,Temperature_C,pH,Turbidity");
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

bool transmitData(String timestamp, float temp, float ph, float turbidity) {
#if (ACTIVE_COMM_MODE == COMM_MODE_BLE)
  return sendMeasurementBle(timestamp, temp, ph, turbidity); // BLE logic
#elif (ACTIVE_COMM_MODE == COMM_MODE_WIFI)
  return sendMeasurement(timestamp, temp, ph, turbidity);    // Wi-Fi logic
#elif (ACTIVE_COMM_MODE == COMM_MODE_LORA)
  return sendMeasurementLora(timestamp, temp, ph, turbidity);  // LoRa logic
#else
  return false;
#endif
}

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing Environmental Sensing Node...");

  // Start Temperature Sensor
  sensors.begin();

  // Initialize SD Card
  setupSDCard();

  // Initialize DS3231 RTC
  setupRTC();

  if (ACTIVE_COMM_MODE == COMM_MODE_BLE)
  {
    transmissionType = "Bluetooth";
  }
  if (ACTIVE_COMM_MODE == COMM_MODE_WIFI)
  {
    transmissionType = "WiFi";
  }
  if (ACTIVE_COMM_MODE == COMM_MODE_LORA)
  {
    transmissionType = "Lora";
  }

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
    // Generate ISO 8601 timestamp string from DS3231 RTC
    String timestampStr = getISOTimestamp();

    Serial.printf("\n--- New Reading [%s] ---\n", timestampStr.c_str());
    Serial.printf("Temperature: %.2f °C\n", tempC);

    // 2. Persistent Local Storage
    logToSD(timestampStr, tempC, ph, turbidity);

    // 3. Wireless Transmission
    Serial.printf("Attempting transmission to Gateway via: %s\n", transmissionType.c_str());
    bool success = transmitData(timestampStr, tempC, ph, turbidity);

    if (success) {
      Serial.println("Transmission: SUCCESS (ACK received)");
    } else {
      Serial.println("Transmission: FAILED or TIMED OUT");
    }

  } else {
    Serial.println("Error: DS18B20 disconnected. Check 4.7k pull-up resistor & wiring.");
  }

  // 4. Wait for next measurement cycle
  delay(SAMPLE_INTERVAL_MS);
}