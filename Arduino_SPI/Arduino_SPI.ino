#include <SPI.h>
#include <SimpleDHT.h>

SPISettings spi_settings(100000, MSBFIRST, SPI_MODE0);

int pinDHT11 = A3;
SimpleDHT11 dht11(pinDHT11);

volatile byte temperature;
volatile byte humidity;

volatile byte motorforgas;

void setup(void) {
  //soros port megnyitasa
  Serial.begin(9600);
  Serial.println("Arduino kod elindult");

  //SPI port megnyitasa
  SPCR |= bit(SPE);  //bekapcsolja az SPIt , slave modban
                     //nincs se a MSTR bekapcsolva se az idozites
  //SPI.begin(); //ezt a master teheti csak
  pinMode(MISO, OUTPUT);  //MISOn valaszol az output kell legyen
  SPI.attachInterrupt();  //ha jon az SPIn valami beugrik a fuggvenybe
                          //ISR(SPI_STC_vect) ebbe a fuggvenybe ugrik be

  //DHT11 adatai
  temperature = 1;
  humidity = 1;
  motorforgas = 1;
}

void loop() {
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err=");
    Serial.println(err);
    delay(1000);
    return;
  }

  Serial.print("Sample OK: ");
  Serial.print((int)temperature);
  Serial.print(" *C, ");
  Serial.print((int)humidity);
  Serial.println(" H");

  // DHT11 sampling rate is 1HZ.
  delay(1000);
}

//SPI interrupt routine
//ide akkor jon be ha kapott egy byte-ot
ISR(SPI_STC_vect) {

  //kiolvassuk a kapott karaktert
  char c = SPDR;

  if (c == 't')
    SPDR = temperature;
  else if (c == 'h')
    SPDR = humidity;
  else if (c == '0')
    motorforgas = 0;
  else if (c == 'm')
    motorforgas = 1;
}