#include "BluetoothSerial.h"
#include "DHT.h"
#include "RTClib.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

#define DHTPIN 2
#define DHTTYPE DHT11

Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
BluetoothSerial SerialBT;
DHT dht(DHTPIN, DHTTYPE);
RTC_DS3231 rtc;

bool missingDHT = false;
bool missingRTC = false;
bool missingTSL = false;

void configureTSL() {
  if (tsl.begin()) {
    tsl.enableAutoRange(true);
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);
  } else {
    missingTSL = true;
    Serial.print(F("Failed to start TSL."));
  }
}

void configureRTC() {
  if (rtc.begin()) {
    // Comment this out after first run.
     rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  } else {
    missingRTC = true;
    Serial.print(F("Failed to start RTC."));
  }
}

void setup() {
  Serial.begin(9600);
  SerialBT.begin("ESP32_Server");
  // SerialBT.deleteAllBondedDevices();

  Serial.flush();
  Serial.println("\nBluetooth started");
  Serial.println(SerialBT.getBtAddressString());

  configureTSL();
  configureRTC();
  dht.begin();
}

void printWithZero(uint8_t number) {
  if (number < 10) Serial.print("0");
  Serial.print(number);
}

void printWithZeroBT(uint8_t number) {
  if (number < 10) SerialBT.print("0");
  SerialBT.print(number);
}

void printDate(uint16_t year, uint8_t month, uint8_t day) {
  Serial.print(year);
  Serial.print("/");
  printWithZero(month);
  Serial.print("/");
  printWithZero(day);
  Serial.print(" ");
}

void printDayOrNight(uint8_t hour, uint8_t minute) {
  printWithZero(hour);
  Serial.print(":");
  printWithZero(minute);
  Serial.println(" ");
}

void sendDateTime(DateTime now) {
  SerialBT.print("D");
  SerialBT.print(now.year());
  SerialBT.print("/");
  printWithZeroBT(now.month());
  SerialBT.print("/");
  printWithZeroBT(now.day());
  SerialBT.print(" ");
  printWithZeroBT(now.hour());
  SerialBT.print(":");
  printWithZeroBT(now.minute());
  SerialBT.println();
}

void loop() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  Serial.flush();
  if (!isnan(temperature) && !isnan(humidity)) {
    SerialBT.print("T");
    SerialBT.print(temperature);
    SerialBT.println();

    SerialBT.print("H");
    SerialBT.print(humidity);
    SerialBT.println();

    Serial.print("\nTemperature: ");
    Serial.print(temperature);
    Serial.println(" °C");
  } else {
    SerialBT.println("EDHT error");
    Serial.print("DHT error");
  }

  if (!missingTSL) {
    sensors_event_t event;
    tsl.getEvent(&event);
    delay(50);
    // if (event.light) {
    //   assessLuminosity(event.light);
    // }
    SerialBT.print("L");
    SerialBT.print(event.light);
    SerialBT.println();

    Serial.print("Luminosity: ");
    Serial.print(event.light);
    Serial.println();
  } else {
    SerialBT.println("ETSL2561 error");
  }

  DateTime now = !missingRTC ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);
  delay(50);
  printDate(now.year(), now.month(), now.day());
  printDayOrNight(now.hour(), now.minute());
  sendDateTime(now);

  delay(3000);
}
