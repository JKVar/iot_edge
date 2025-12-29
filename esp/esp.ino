#include "BluetoothSerial.h"
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

#define DHTPIN 2
#define DHTTYPE DHT11

Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
BluetoothSerial SerialBT;
DHT dht(DHTPIN, DHTTYPE);

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

void setup() {
  Serial.begin(9600);
  SerialBT.begin("ESP32_Server");
  // SerialBT.deleteAllBondedDevices();

  Serial.flush();
  Serial.println("\nBluetooth started");
  Serial.println(SerialBT.getBtAddressString());

  configureTSL();
  dht.begin();
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

  delay(3000);
}
