#include <ESP8266WiFi.h>
#include <SPI.h>

byte temperature;
byte humidity;
byte motorforgas;

SPISettings spi_settings(100000, MSBFIRST, SPI_MODE0);

const char* ssid = "-";
const char* password = "-";

const int SPI_POLL_INTERVAL = 2000;
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

void executeTransaction() {
  SPI.beginTransaction(spi_settings);
  
  SPI.transfer('t');
  temperature = SPI.transfer('h');
  humidity = SPI.transfer(0x00);

  SPI.endTransaction();
}

void mainPageScripts(WiFiClient &client) {
  client.println("<script>");
  client.println("function updateData() {");
  client.println("  fetch('/data')");
  client.println("    .then(response => response.json())");
  client.println("    .then(data => {");
  client.println("        document.getElementById('temp').textContent = data.temperature;");
  client.println("        document.getElementById('hum').textContent = data.humidity;");
  client.println("    })");
  client.println("    .catch(error => {");
  client.println("        console.log(\"Failed to fetch data: \");");
  client.println("        console.log(error);");
  client.println("        console.log();");
  client.println("    });");
  client.println("}");
  client.println("setInterval(updateData, 2000);");
  client.println("updateData();");
  client.println("</script>");
}

void mainPageBody(WiFiClient &client) {
  client.println("<body>");
  client.println("<h1>Temperature & Humidity</h1>");
  client.println("<p>Temperature: <span id='temp'>--</span>°C</p>");
  client.println("<p>Humidity: <span id='hum'>--</span>%</p>");

  mainPageScripts(client);
  client.println("</body>");
}

void mainPageHead(WiFiClient &client) {
  client.println("<head>");
  client.println("<title>Sensor Page</title>");
  client.println("<meta charset=\"UTF-8\">");
  client.println("</head>");
}

void mainPageResponse(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  mainPageHead(client);
  mainPageBody(client);
  client.println("</html>");
  client.flush();
  delay(5);
}

void dataRequestResponse(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println();
  client.print("{\"temperature\":\"");
  client.print(temperature);
  client.print("\",\"humidity\":\"");
  client.print(humidity);
  client.println("\"}");
  client.flush();
  delay(5);
}

void webClientResponse(WiFiClient &client, String request) {
  if (request.indexOf("/data") != -1) {
    dataRequestResponse(client);
    return;
  }

  mainPageResponse(client);
}

void pollSPI() {
  if (millis() - last_SPI_poll > SPI_POLL_INTERVAL) {
    last_SPI_poll = millis();
    executeTransaction();
  }
}

String readClientRequest(WiFiClient &client) {
  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush();
  return request;
}

void handleClient(WiFiClient &client) {
  Serial.println("New client.");

  unsigned long timeout_timer = millis();
  while (!client.available() && millis() - timeout_timer < CLIENT_AVAILABILITY_TIMEOUT) {
    delay(1);
  }
  if (!client.available()) {
    Serial.println("Client timed out. Stopping client...");
    client.stop();
    return;
  }

  String request = readClientRequest(client);
  webClientResponse(client, request);

  Serial.println("Client disconnected.");
  Serial.println();
}

void loop() {
  pollSPI();

  WiFiClient client = server.available();
  if (!client) return;
  
  handleClient(client);
}
