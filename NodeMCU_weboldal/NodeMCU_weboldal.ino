#include <ESP8266WiFi.h>
#include <SPI.h>

const uint8_t COMMAND_QUEUE_MAX_SIZE = 128;
byte commandQueue[128];
uint8_t currentQueueSize = 0;

bool isManual = false;
bool isDay = false;
bool isShutterUp = false;
bool isEngineOn = false;

uint16_t luminosity = 0;
byte lumLowerByte = 0, lumHigherByte = 0;

byte flags = 0;
byte temperature = 0;
byte humidity = 0;

byte resentData = 0;
bool isDataError = false;

SPISettings spi_settings(100000, MSBFIRST, SPI_MODE0);

const char* ssid = "-";
const char* password = "-";

const byte MAX_SPI_RETRIES = 3;
const int SPI_POLL_INTERVAL = 1500;
const int CLIENT_AVAILABILITY_TIMEOUT = 2000;
unsigned long last_SPI_poll = 0;

int SS_PIN = D8;  // GPIO13---D7 of NodeMCU
WiFiServer server(80);

void setup() {
  Serial.begin(9600);
  SPI.begin();  //  SPI control regiszterben SPE MSTR + sebesseg

  delay(10);

  pinMode(SS_PIN, OUTPUT);
  digitalWrite(SS_PIN, LOW);  //lekapcsolja a LEDet LOW==0

  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
}

void resyncPoll() {
  SPI.transfer('!');
}

bool attemptPolling(byte command, byte *data) {
  byte sync = 0;
  // Tell the slave to enter data sending mode.
  SPI.transfer('!');

  // Send the command.
  sync = SPI.transfer(command);
  if (sync != 0xFF) return false;

  // Read the received data and tell the slave to enter data verifying mode.
  byte receivedData = SPI.transfer('?');

  // Re-send the received data to verify it.
  sync = SPI.transfer(receivedData);
  if (sync != 0xFF) return false;

  byte resentData = SPI.transfer('!');
  if (resentData != receivedData) return false;

  // Reset slave to command setting mode.
  sync = SPI.transfer(0x00);
  if (sync != 0xFF) return false;

  *data = receivedData;
  return true;
}

void poll(byte command, byte *data) {
  for (byte attempt = 0; attempt < MAX_SPI_RETRIES; ++attempt) {
    if (attemptPolling(command, data)) {
      return;
    }

    Serial.print(F("Retrying SPI command "));
    Serial.print(command);
    Serial.print(F(" (attempt "));
    Serial.print(attempt);
    Serial.println(F(")"));

    delay(10);
    resyncPoll();
    delay(5);
  }

  Serial.print(F("SPI command failed after "));
  Serial.print(MAX_SPI_RETRIES);
  Serial.print(F(" attempts."));

  *data = 0xFF;
}

void setLuminosity() {
  luminosity = ((uint16_t)lumHigherByte << 8) | lumLowerByte;
}

void setBooleans() {
  isManual = bitRead(flags, 0);
  isDay = bitRead(flags, 1);
  isShutterUp = bitRead(flags, 2);
  isEngineOn = bitRead(flags, 3);
}

void executePolling() {
  isDataError = false;
  SPI.beginTransaction(spi_settings);
  
  poll('b', &flags);
  poll('h', &temperature);
  poll('e', &humidity);
  poll('f', &lumLowerByte);
  poll('f', &lumHigherByte);

  SPI.endTransaction();

  if (!isDataError) {
    setLuminosity();
    setBooleans();
  }
}

void executeQueuedCommands() {

}

void mainPageScripts(WiFiClient &client) {
  client.println(F("<script>"));
  client.println(F("function updateData() {"));
  client.println(F("  fetch('/data')"));
  client.println(F("    .then(response => response.json())"));
  client.println(F("    .then(data => {"));
  client.println(F("        document.getElementById('temp').textContent = data.temperature;"));
  client.println(F("        document.getElementById('hum').textContent = data.humidity;"));
  client.println(F("        document.getElementById('lum').textContent = data.luminosity"));
  client.println(F("        document.getElementById('daynight').textContent = data.day ? \"DAY\" : \"NIGHT\";"));
  client.println(F("        document.getElementById('sysmode').textContent = data.manual ? \"MANUAL\" : \"AUTOMATIC\";"));
  client.println(F("        document.getElementById('shutterstate').textContent = data.shutter ? \"UP\" : \"DOWN\";"));
  client.println(F("        document.getElementById('enginestate').textContent = data.engine ? \"ON\" : \"OFF\";"));
  client.println(F("    })"));
  client.println(F("    .catch(error => {"));
  client.println(F("        console.log(\"Failed to fetch data: \", error);"));
  client.println(F("    })"));
  client.println(F("    .finally(() => {"));
  client.println(F("        setTimeout(updateData, 2000);"));
  client.println(F("    });"));
  client.println(F("}"));
  client.println(F("updateData();"));
  client.println(F("</script>"));
}

void mainPageBody(WiFiClient &client) {
  client.println(F("<body>"));
  client.println(F("<h3>Sensor data:</h3>"));
  client.println(F("<p>Temperature: <span id='temp'>--</span>°C</p>"));
  client.println(F("<p>Humidity: <span id='hum'>--</span>%</p>"));
  client.println(F("<p>Luminosity: <span id='lum'>--</span> lux</p>"));
  client.println(F("<p>-- Day/Night: <span id='daynight'>--</span></p>"));
  client.println(F("<p>-- System mode: <span id='sysmode'>--</span></p>"));
  client.println(F("<p>-- Shutter: <span id='shutterstate'>--</span></p>"));
  client.println(F("<p>-- Engine: <span id='enginestate'>--</span></p>"));

  mainPageScripts(client);

  client.println(F("</body>"));
}

void mainPageHead(WiFiClient &client) {
  client.println(F("<head>"));
  client.println(F("<title>Sensor Page</title>"));
  client.println(F("<meta charset=\"UTF-8\">"));
  client.println(F("</head>"));
}

void mainPageResponse(WiFiClient &client) {
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Connection: close"));
  client.println(F("Access-Control-Allow-Origin: *"));
  client.println();
  client.println(F("<!DOCTYPE HTML>"));
  client.println(F("<html>"));

  mainPageHead(client);
  mainPageBody(client);
  
  client.println(F("</html>"));
  client.flush();
}

String createDataJson() {
  String json = "{\"temperature\":";
  json += temperature;
  json += ",\"humidity\":";
  json += humidity;
  json += ",\"luminosity\":";
  json += luminosity;
  json += ",\"manual\":";
  json += isManual;
  json += ",\"day\":";
  json += isDay;
  json += ",\"shutter\":";
  json += isShutterUp;
  json += ",\"engine\":";
  json += isEngineOn;
  json += "}";

  return json;
}

void dataRequestResponse(WiFiClient &client) {
  String json = createDataJson();
  Serial.println(json);

  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.println(F("Connection: close"));
  client.print(F("Content-Length: "));
  client.println(json.length());
  client.println(F("Access-Control-Allow-Origin: *"));
  client.println();
  client.println(json);
  client.flush();
}

void webClientResponse(WiFiClient &client, String request) {
  if (request.indexOf("/data") != -1) {
    dataRequestResponse(client);
    return;
  }

  mainPageResponse(client);
}

String readClientRequest(WiFiClient &client, int timeout = 2000) {
  unsigned long start = millis();
  String request = "";

  while(client.connected() && millis() - start < timeout) {
    while(client.available()) {
      char c = client.read();
      request += c;

      if (request.endsWith("\r\n\r\n")) return request;
    }
  }
  return request;
}

void pollSPI() {
  if (millis() - last_SPI_poll > SPI_POLL_INTERVAL) {
    last_SPI_poll = millis();
    executePolling();
  }
  
  executeQueuedCommands();
}

void handleClient(WiFiClient &client) {
  if (client) {
    Serial.println(F("New client."));

    unsigned long currentTime = millis();
    unsigned long timeoutTimer = currentTime;

    while (client.connected() && currentTime - timeoutTimer < CLIENT_AVAILABILITY_TIMEOUT) {
      currentTime = millis();
      if (client.available()) {
        String request = readClientRequest(client);
        if (request.length()) {
          webClientResponse(client, request);
          break;
        }
      }
    }
    if (currentTime - timeoutTimer >= CLIENT_AVAILABILITY_TIMEOUT) {
      Serial.println(F("Client timed out."));
    }

    client.stop();
    Serial.println(F("Client disconnected."));
    Serial.println();
  }
}

void loop() {
  pollSPI();

  WiFiClient client = server.available();
  handleClient(client);
}
