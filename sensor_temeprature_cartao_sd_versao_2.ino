// Include the libraries we need
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <SD.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 7


#define SD_CS   2
#define SD_MOSI 34
#define SD_MISO 19
#define SD_SCK  5

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
SPIClass sdSPI(HSPI);

void rawSdProbe() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdSPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));

  for (int i = 0; i < 10; i++) sdSPI.transfer(0xFF);  // 80 clocks, CS high

  digitalWrite(SD_CS, LOW);
  uint8_t cmd0[] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95};
  for (uint8_t b : cmd0) sdSPI.transfer(b);

  uint8_t r = 0xFF;
  for (int i = 0; i < 10 && r == 0xFF; i++) r = sdSPI.transfer(0xFF);

  digitalWrite(SD_CS, HIGH);
  sdSPI.endTransaction();
  Serial.printf("CMD0 response: 0x%02X\n", r);
}

/*
 * The setup function. We only start the sensors here
 */
void setup(void)
{
  // start serial port
  Serial.begin(115200);
  Serial.println("Dallas Temperature IC Control Library Demo");
/*
  for (int i = 0; i < 30; i++) {
  rawSdProbe();
  delay(300);
}
  */
  // Start up the library
  sensors.begin();
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  while (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("SD Card Mount Failed!");
    delay(200);
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  
  Serial.println("SD Card Mounted Successfully!");
  if (!SD.exists("/data.csv")) {
  File headerFile = SD.open("/data.csv", FILE_WRITE);
  if (headerFile) {
    headerFile.println("Timestamp_ms,Temperature_C");
    headerFile.close();
  }
}
}

void loop(void) {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  if (tempC != DEVICE_DISCONNECTED_C) {
    // Open CSV file in append mode
    File csvFile = SD.open("/data.csv", FILE_APPEND);

    if (csvFile) {
      unsigned long currentMillis = millis();

      // Write row: Milliseconds, Temperature
      csvFile.print(currentMillis);
      csvFile.print(",");
      csvFile.println(tempC, 2); // Limit precision to 2 decimal places
      csvFile.close();           // Ensure data flushes to hardware

      Serial.print("CSV Logged -> ");
      Serial.print(currentMillis);
      Serial.print(" ms | ");
      Serial.print(tempC);
      Serial.println(" °C");
    } else {
      while(!csvFile) {
        csvFile = SD.open("/data.csv", FILE_APPEND);
        Serial.println("Error: Could not open /data.csv for writing.");
        delay(200);
      }
    }
  } else {
    Serial.println("Error: DS18B20 disconnected. Check 4.7k resistor & wiring.");
  }

  delay(5000); // Adjust sample interval as needed
}
