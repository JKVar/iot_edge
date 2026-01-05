#include "BluetoothSerial.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

BluetoothSerial SerialBT;
LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

#define BT_DISCOVER_TIME 10000

static bool btScanAsync = true;
static bool btScanSync = true;
uint8_t address[] = {0x2c, 0xbc, 0xbb, 0x0b, 0xe0, 0xd2};

void btAdvertisedDeviceFound(BTAdvertisedDevice *pDevice) {
  Serial.printf("Found a device asynchronously: %s\n", pDevice->toString().c_str());
}

void configureLCD() {
  lcd.init();
  delay(500);

  lcd.backlight();
  lcd.home();
}

void flushScreen() {
  lcd.flush();
  lcd.clear();
  lcd.setCursor(0,0);
  delay(50);
}

void printToLCD(String data) {
  lcd.print(data);
}

void connectToBT() {
  flushScreen();
  Serial.println("Connecting...");
  printToLCD("Connecting...");

  while (!SerialBT.connect(address)) {
    Serial.println("Retrying...");
    flushScreen();
    printToLCD("Retrying...");
    delay(2000);
  }

  flushScreen();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  Wire.setClock(100000);
  configureLCD();

  SerialBT.begin("ESP32_RECEIVER", true);
  delay(2000);

  connectToBT();
  Serial.println("Connected to server!");
}

void loop() {
  if (SerialBT.available()) {
    String data = SerialBT.readStringUntil('\n');
    String lcdString, dataText;
    if (data[0] == 'U') {
      delay(3000);
      flushScreen();
    }

    if (data[0] == 'T') {
      Serial.print("Temperature: ");
      lcd.setCursor(0, 0);
      dataText = "Temp: ";
    }

    if (data[0] == 'H') {
      Serial.print("Humidity: ");
      lcd.setCursor(0, 1);
      dataText = "Humi: ";
    }

    if (data[0] == 'L') {
      Serial.print("Luminosity: ");
      lcd.setCursor(0, 2);
      dataText = "Lumi: ";
    }

    if (data[0] == 'D') {
      Serial.print("Date: ");
      lcd.setCursor(0, 3);
      dataText = "";
    }

    if (data[0] == 'E') {
      lcd.setCursor(0, 0);
      dataText = "";
      if (data[1] == 'T') { 
        lcd.setCursor(0,2); 
      }
    }

    printToLCD(dataText + data.substring(1));
    Serial.println(data.substring(1));
  } else {
    flushScreen();
    printToLCD("Disconnected");
    delay(1000);
    connectToBT();
  }

}
