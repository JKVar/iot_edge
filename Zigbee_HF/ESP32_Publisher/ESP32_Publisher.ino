#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "Mateinfo";
const char* password = "computer";

const char* mqtt_server = "192.168.39.54";
const char* topics[] = {
  "esp32/math/add",
  "esp32/math/subtract",
  "esp32/math/multiply",
  "esp32/math/divide"
};

WiFiClient wifi;
PubSubClient client(wifi);

String serialBuffer = "";

void processSerialInput(const String& input);
short getTopicIndex(const String& input);

void setup() {
  Serial.begin(115200);
  Serial.print(F("Initializing..."));

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(F("\nWiFi connected."));

  client.setServer(mqtt_server, 1883);
  while (!client.connected()) {
    if (client.connect("ESP32_Publisher")) {
      Serial.println(F("Connected to MQTT broker."));
    } else {
      Serial.println(F("MQTT connection failed. Retrying in 1s."));
      delay(1000);
    }
  }

  Serial.println(F("Initialization complete. Starting..."));
}

void loop() {
  client.loop();

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      serialBuffer.trim();
      if (serialBuffer.length() > 0) {
        processSerialInput(serialBuffer);
      }
      serialBuffer = "";
    } else {
      serialBuffer += c;
    }
  }
}

void processSerialInput(const String& input) {
  short index = getTopicIndex(input);
  if (index >= 0 && index < (sizeof(topics) / sizeof(topics[0]))) {
    bool isPublished = client.publish(topics[index], input.c_str());

    if (isPublished) {
      Serial.print(F("Published to [\""));
      Serial.print(topics[index]);
      Serial.print(F("\"]:"));
      Serial.println(input);
    } else {
      Serial.print(client.connected());
      Serial.print("Input not published to[\"");
      Serial.print(topics[index]);
      Serial.print(F("\"]:"));
      Serial.println(input);
    }

  } else {
    Serial.print(F("Invalid topic for input: "));
    Serial.println(input);
  }
}

short getTopicIndex(const String& input) {
  // Example input "add 2 3" -> index = 0
  if (input.startsWith("add")) return 0;
  if (input.startsWith("subtract")) return 1;
  if (input.startsWith("multiply")) return 2;
  if (input.startsWith("divide")) return 3;
  return -1;
}
