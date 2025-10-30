#include <ESP8266WiFi.h>
#include <SPI.h>

byte temperature;
byte humidity;

SPISettings spi_settings(100000, MSBFIRST, SPI_MODE0);

const char* ssid = "wifi";
const char* password = "jelszo";

int ledPin = 13;  // GPIO13---D7 of NodeMCU
WiFiServer server(80);

void setup() {
  Serial.begin(9600);
  SPI.begin();  //  SPI control regiszterben SPE MSTR + sebesseg

  delay(10);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);  //lekapcsolja a LEDet LOW==0

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

void loop() {
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Wait until the client sends some data
  Serial.println("new client");
  while (!client.available()) {
    delay(1);
  }

  // Read the first line of the request
  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush();

  // Match the request

  int value = LOW;
  if (request.indexOf("/LED=ON") != -1) {
    digitalWrite(ledPin, HIGH);
    value = HIGH;
  }
  if (request.indexOf("/LED=OFF") != -1) {
    digitalWrite(ledPin, LOW);
    value = LOW;
  }


  //SPI kommunikacio ez gyors
  SPI.beginTransaction(spi_settings);

  //trash =
  SPI.transfer('t');
  temperature = SPI.transfer('h');
  humidity = SPI.transfer('.');

  SPI.endTransaction();
  delay(1000);

  //soros kommunikacio ez lassu
  Serial.print("Temperature = ");
  Serial.println(temperature, DEC);
  Serial.print("Humidity = ");
  Serial.println(humidity, DEC);



  // Set ledPin according to the request
  //digitalWrite(ledPin, value);

  // Return the response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("");  //  do not forget this one
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");

  client.print("Temperature: ");
  client.print(temperature);
  client.println("<br><br>");

  client.print("Humidity: ");
  client.print(humidity);
  client.println("<br><br>");


  // client.print("Led is now: ");

  // if(value == HIGH) {
  //   client.print("On");
  // } else {
  //   client.print("Off");
  // }
  // client.println("<br><br>");
  // client.println("<a href=\"/LED=ON\"\"><button>On </button></a>");
  // client.println("<a href=\"/LED=OFF\"\"><button>Off </button></a><br />");

  client.println("</html>");
  delay(500);

  Serial.println("Client disonnected");
  Serial.println("");
}