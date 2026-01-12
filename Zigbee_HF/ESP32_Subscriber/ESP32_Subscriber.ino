#include <WiFi.h>
#include <PubSubClient.h>

constexpr int MAX_TOKENS = 4;
constexpr int MAX_INPUT_SIZE = 32;

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

char message[MAX_INPUT_SIZE + 1];

// Function prototype definitions
void callback(char* topic, byte* payload, unsigned int length);

int split(char* input, char* tokens[]);

bool isKeyword(const char* input);
bool isNumber(const char* input);

// Core program
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
  client.setCallback(callback);
  while (!client.connected()) {
    if (client.connect("ESP32_Subscriber")) {
      Serial.println(F("Connected to MQTT broker."));
    } else {
      Serial.println(F("MQTT connection failed. Retrying in 1s."));
      delay(1000);
    }
  }

  short topicsLen = sizeof(topics) / sizeof(topics[0]);
  for (short i = 0; i < topicsLen; ++i) {
    client.subscribe(topics[i]);
  }

  Serial.println(F("Initialization complete. Starting..."));
}

void loop() {
  client.loop();
}

void callback(char* topic, byte* payload, unsigned int length) {
  if (length >= MAX_INPUT_SIZE) return;

  memcpy(message, payload, length);
  message[length] = '\0';

  char* tokens[MAX_TOKENS];
  if (split(message, tokens) != 3 
      || !isKeyword(tokens[0]) 
      || !isNumber(tokens[1]) 
      || !isNumber(tokens[2])) return;

  float x = atof(tokens[1]), y = atof(tokens[2]);

  Serial.print(F("Received request to "));
  if (strcmp(tokens[0], "add") == 0) {
    Serial.print(F("add "));
    Serial.print(y);
    Serial.print(F(" to "));
    Serial.print(x);
    Serial.print(F(". Result: "));
    Serial.println(x + y);

  } else if (strcmp(tokens[0], "subtract") == 0) {
    Serial.print(F("subtract "));
    Serial.print(y);
    Serial.print(F(" from "));
    Serial.print(x);
    Serial.print(F(". Result: "));
    Serial.println(x - y);

  } else if (strcmp(tokens[0], "multiply") == 0) {
    Serial.print(F("multiply "));
    Serial.print(x);
    Serial.print(F(" by "));
    Serial.print(y);
    Serial.print(F(". Result: "));
    Serial.println(x * y);

  } else if (strcmp(tokens[0], "divide") == 0) {
    Serial.print(F("divide "));
    Serial.print(x);
    Serial.print(F(" by "));
    Serial.print(y);
    Serial.print(F(". Result: "));
    
    if (fabs(y) < 1e-6) {
      Serial.print(F("undefined"));
    } else {
      Serial.println(x / y);
    }
  }
}

int split(char* input, char* tokens[]) {
  int count = 0;
  char* t = strtok(input, " ");

  while (t && count < MAX_TOKENS) {
    tokens[count] = t;
    t = strtok(nullptr, " ");
    count++;
  }

  return count;
}

bool isKeyword(const char* input) {
  if (strcmp(input, "add") == 0) return true;
  if (strcmp(input, "subtract") == 0) return true;
  if (strcmp(input, "multiply") == 0) return true;
  if (strcmp(input, "divide") == 0) return true;
  return false;
}

bool isNumber(const char* input) {
  if (*input == '+' || *input == '-') input++;
  if (*input == '\0') return false;

  bool hasDecimal = false;
  bool hasDigit = false;

  for (const char* c = input; *c != '\0'; c++) {
    if (*c == '.') {
      if (hasDecimal) return false;
      if (!hasDigit) return false;
      hasDecimal = true;
    } else {
      if (!isdigit(*c)) return false;
      hasDigit = true;
    }
  }

  return hasDigit;
}
