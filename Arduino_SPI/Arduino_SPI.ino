#include <SPI.h>
// #include <SimpleDHT.h>
#include "DHT.h"
#include "RTClib.h"
#include <Stepper.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#define DHTPIN A3  // Digital pin connected to the DHT sensor

#define DHTTYPE DHT11  // DHT 11

#if defined(ARDUINO) && ARDUINO >= 100
#define printByte(args) write(args);
#else
#define printByte(args) print(args, BYTE);
#endif

const int stepsPerRevolution = 200;

Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

DHT dht(DHTPIN, DHTTYPE);
RTC_DS3231 rtc;
Stepper myStepper(stepsPerRevolution, 8, 6, 5, 4);

const int automaticManualChangeDelay = 10;
const int normalLightThreshold = 100;
const int tooBrightThreshold = 1000;

uint32_t lastButtonChangeTimestamp = 0;
int buttonOldState, buttonState;
int luminosity = 1;    // <= 0 - too bright, 1 - okay, >= 2 - too dark
int engineState = -1;  // <= 0 - off; >=1 - on

bool isManual = false;
bool isItDay;
bool isShutterUp = true;

bool missingDHT = false;
bool missingRTC = false;
bool missingTSL = false;

SPISettings spi_settings(100000, MSBFIRST, SPI_MODE0);

int pinDHT11 = A3;
// SimpleDHT11 dht11(pinDHT11);

volatile byte temperature;
volatile byte humidity;
volatile byte motorforgas;


bool checkSensors() {
  float test = dht.readTemperature();
  if (isnan(test)) {
    missingDHT = true;
    Serial.println(F("Missing DHT sensor"));
  } else {
    missingDHT = false;
  }

  bool allGood = !(missingDHT || missingTSL);
  return allGood;
}

void assessLuminosity(float light) {
  if (light) {
    if (light < normalLightThreshold) {
      luminosity = 2;
    } else if (light < tooBrightThreshold) {
      luminosity = 1;
    } else {
      luminosity = 0;
    }
  }
}

void assessButtonState(uint32_t unixtime) {
  buttonState = digitalRead(A0);

  if (buttonState != buttonOldState) {
    buttonOldState = buttonState;
    changeShutters(!isShutterUp);

    lastButtonChangeTimestamp = unixtime;
    isManual = true;
  }
}

void checkDayTime(uint8_t hour) {
  if (hour >= 8 && hour <= 20) {
    isItDay = true;
  } else {
    isItDay = false;
  }
}

void canSwitchMode(uint32_t unixtime) {
  int timeDiff = abs(unixtime - lastButtonChangeTimestamp);

  if (timeDiff >= automaticManualChangeDelay) {
    isManual = false;
  }
}

void changeShutters(bool state) {
  isShutterUp = state;
  engineState = 1;
}

void checkEngineState() {
  const int maxState = 5;
  if (engineState > 0 && engineState < maxState) {
    engineState++;
    myStepper.step(stepsPerRevolution);
  }
  if (engineState == maxState) {
    engineState = 0;
  }
}

void automaticModeLogic(uint32_t unixtime) {
  if (!isManual) {
    if (isItDay) {
      if (luminosity <= 0 && isShutterUp) {
        changeShutters(false);
      }
      if (luminosity >= 2 && !isShutterUp) {
        changeShutters(true);
      }
    } else {
      if (isShutterUp) {
        changeShutters(false);
      }
    }
  } else {
    canSwitchMode(unixtime);
  }
}

void configureTSL() {
  if (tsl.begin()) {
    tsl.enableAutoRange(true);
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);
  } else {
    missingTSL = true;
    Serial.println(F("Failed to start TSL."));
  }
}

void configureDHT() {
  dht.begin();
  delay(1000);

  float test = dht.readTemperature();
  if (isnan(test)) {
    missingDHT = true;
    Serial.println(F("Failed to start DHT."));
  }
}

void configureRTC() {
  Wire.beginTransmission(0x68);
  byte error = Wire.endTransmission();
  if (error == 0) {
    if (rtc.begin()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  } else {
    missingRTC = true;
    Serial.println(F("Failed to start RTC."));
  }
}

void setup(void) {
  //soros port megnyitasa
  Serial.begin(9600);
  Serial.println("Arduino started.");

  //SPI port megnyitasa
  SPCR |= bit(SPE);  //bekapcsolja az SPIt , slave modban
                     //nincs se a MSTR bekapcsolva se az idozites
  //SPI.begin(); //ezt a master teheti csak
  pinMode(MISO, OUTPUT);  //MISOn valaszol az output kell legyen
  SPI.attachInterrupt();  //ha jon az SPIn valami beugrik a fuggvenybe
                          //ISR(SPI_STC_vect) ebbe a fuggvenybe ugrik be

  Serial.println("Slave data initialized.");

  //DHT11 adatai
  temperature = 1;
  humidity = 1;
  motorforgas = 1;

  pinMode(A0, INPUT_PULLUP);
  buttonOldState = digitalRead(A0);
  buttonState = buttonOldState;

  Wire.begin();
  
  configureDHT();
  Serial.println("DHT initialized.");
  configureRTC();
  Serial.println("RTC initialized.");
  configureTSL();
  Serial.println("TSL initialized.");
  myStepper.setSpeed(60);

  Serial.println("Setup ready.");
  Serial.println();
}

void printWithZero(uint8_t number) {
  if (number < 10) Serial.print("0");
  Serial.print(number);
}

void printDate(uint16_t year, uint8_t month, uint8_t day) {
  Serial.print(year);
  Serial.print("/");
  printWithZero(month);
  Serial.print("/");
  printWithZero(day);
  Serial.println(" ");
}

void printDayOrNight(uint8_t hour, uint8_t minute) {
  printWithZero(hour);
  Serial.print(":");
  printWithZero(minute);
  Serial.print(" ");
}

void printLuminosity() {
  Serial.print(F(" "));

  if (missingTSL) {
    Serial.println(F("No TSL"));
  } else {
    if (luminosity <= 0) {
      Serial.println(F("Bright"));
    } else if (luminosity == 1) {
      Serial.println(F("Okay"));
    } else {
      Serial.println(F("Dark"));
    }
  }
}

void printMode() {
  Serial.print(F("Mode: "));

  if (isManual) {
    Serial.println(F("Manual"));
  } else {
    Serial.println(F("Automatic"));
  }
}

void printShutterState() {
  if (isShutterUp) {
    Serial.println(F("UP"));
  } else {
    Serial.println(F("UP"));
  }
}

void printEngineState() {
  Serial.print(F("Engine: "));

  if (engineState > 0) {
    Serial.println(F("On"));
  } else {
    Serial.println(F("Off"));
  }
}

void printHumidityAndTemperature(float humidity, float temperature) {
  Serial.print(humidity);
  Serial.println(F("% "));
  Serial.print(temperature);
  Serial.println(F("C "));
}

void printSensorProblems() {
  Serial.print(F("ERROR"));
  int row = 1;

  if (missingDHT) {
    Serial.print(F("Missing DHT sensor"));
    row++;
  }

  if (missingTSL) {
    Serial.print(F("Missing TSL sensor"));
    row++;
  }
}


void loop() {
  // // DHT11 sampling rate is 1HZ.
  delay(1000);
  DateTime now = !missingRTC ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);
  delay(50);

  if (!missingTSL) {
    sensors_event_t event;
    tsl.getEvent(&event);
    delay(50);
    if (event.light) {
      assessLuminosity(event.light);
    }
  }

  bool allGood = checkSensors();

  humidity = !missingDHT ? dht.readHumidity() : 0;
  temperature = !missingDHT ? dht.readTemperature() : 20;

  // checkDayTime(now.hour());
  automaticModeLogic(now.unixtime());
  assessButtonState(now.unixtime());

  printDate(now.year(), now.month(), now.day());
  printDayOrNight(now.hour(), now.minute());

  if (missingDHT) {
    Serial.println(F("DHT sensor missing"));
  } else {
    printHumidityAndTemperature(humidity, temperature);
  }

  printMode();
  printShutterState();

  printLuminosity();
  printEngineState();
  checkEngineState();
  Serial.println();
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