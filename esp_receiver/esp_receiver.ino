#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

BluetoothSerial SerialBT;

#define BT_DISCOVER_TIME 10000

static bool btScanAsync = true;
static bool btScanSync = true;
uint8_t address[] = {0x2c, 0xbc, 0xbb, 0x0b, 0xe0, 0xd2};

void btAdvertisedDeviceFound(BTAdvertisedDevice *pDevice) {
  Serial.printf("Found a device asynchronously: %s\n", pDevice->toString().c_str());
}

void setup() {
  Serial.begin(115200);

  SerialBT.begin("ESP32_RECEIVER", true);
  delay(2000);

  Serial.println("Connecting...");
  while (!SerialBT.connect(address)) {
    Serial.println("Retrying...");
    delay(2000);
  }

  Serial.println("Connected to server!");
}

void loop() {
  if (SerialBT.available()) {
    String data = SerialBT.readStringUntil('\n');
    if (data[0] == 'T') {
      Serial.print("Temperature: ");
    }

    if (data[0] == 'H') {
      Serial.print("Humidity: ");
    }

    if (data[0] == 'L') {
      Serial.print("Luminosity: ");
    }
    Serial.print(data.substring(1));
    Serial.println();
  }

  delay(900);
}
