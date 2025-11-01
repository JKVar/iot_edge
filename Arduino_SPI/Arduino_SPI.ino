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

const uint8_t STEPS_PER_REVOLUTION = 200;
const uint8_t TO_AUTOMATIC_TIMER_S = 10;
const uint8_t NORMAL_BRIGHTNESS_THRESHOLD = 100;
const uint8_t TOO_BRIGHT_THRESHOLD = 1000;

Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

DHT dht(DHTPIN, DHTTYPE);
RTC_DS3231 rtc;
Stepper myStepper(STEPS_PER_REVOLUTION, 8, 6, 5, 4);

uint32_t lastButtonChangeTimestamp = 0;

int brightness = 1;    // <= 0 - too bright, 1 - okay, >= 2 - too dark
uint8_t buttonOldState, buttonState;

bool missingDHT = false;
bool missingRTC = false;
bool missingTSL = false;

SPISettings spi_settings(100000, MSBFIRST, SPI_MODE0);

int pinDHT11 = A3;

// The first four can be stored and sent in a single byte
volatile bool isManual = false;
volatile bool isDay = false;
volatile bool isShutterUp = true;

volatile int engineState = -1;  // <= 0 - off; >=1 - on

/*
  Bits:
    0 - isModeManual
    1 - isItDay
    2 - isShutterUp
    3 - engineState (0 if <= 0, 1 if >= 1)
*/
volatile byte flags = 0b00000000;

// Luminosity needs a more sophisticated way to send over
volatile uint16_t luminosity = 0;
volatile bool highLumByte = false;

volatile byte temperature = 0;
volatile byte humidity = 0;

volatile byte lastCommandByte = 0;
volatile byte lastSentData = 0;

// When true, subsequent requests will read and send data, or perform actions.
// Otherwise they verify the correctness of the data from a previous command.
volatile bool commandMode = true;

// After setting the command mode, the next byte will be processed accordingly.
// This is to avoid clash with certain special characters.
// Its lifetime is one request.
volatile bool commandModeSet = false;

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

void assessBrightness(float light) {
  luminosity = (uint32_t)light;
  if (light) {
    if (light < NORMAL_BRIGHTNESS_THRESHOLD) {
      brightness = 2;
    } else if (light < TOO_BRIGHT_THRESHOLD) {
      brightness = 1;
    } else {
      brightness = 0;
    }
  }
}

void forceShutters() {
  changeShutters(!isShutterUp);
  isManual = true;
}

void assessButtonState(uint32_t unixtime) {
  buttonState = digitalRead(A0);

  if (buttonState != buttonOldState) {
    buttonOldState = buttonState;
    lastButtonChangeTimestamp = unixtime;

    forceShutters();
  }
}

void checkDayTime(uint8_t hour) {
  if (hour >= 8 && hour <= 20) {
    isDay = true;
  } else {
    isDay = false;
  }
}

void canSwitchMode(uint32_t unixtime) {
  int timeDiff = abs(unixtime - lastButtonChangeTimestamp);

  if (timeDiff >= TO_AUTOMATIC_TIMER_S) {
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
    myStepper.step(STEPS_PER_REVOLUTION);
  }
  if (engineState == maxState) {
    engineState = 0;
  }
}

void automaticModeLogic(uint32_t unixtime) {
  if (!isManual) {
    if (isDay) {
      if (brightness <= 0 && isShutterUp) {
        changeShutters(false);
      }
      if (brightness >= 2 && !isShutterUp) {
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

void constructFlagByte() {
  bitWrite(flags, 0, isManual ? 1 : 0);
  bitWrite(flags, 1, isDay ? 1 : 0);
  bitWrite(flags, 2, isShutterUp ? 1 : 0);
  bitWrite(flags, 3, engineState > 0 ? 1 : 0);
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
  uint8_t error = Wire.endTransmission();

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

void printDateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute) {
  Serial.print(year);
  Serial.print("/");
  printWithZero(month);
  Serial.print("/");
  printWithZero(day);
  Serial.print(" - ");
  printWithZero(hour);
  Serial.print(":");
  printWithZero(minute);
  Serial.println();
}

void printBrightness() {
  if (missingTSL) {
    Serial.println(F("No TSL"));
  } else {
    Serial.print(F("Lum: "));
    Serial.print(luminosity);
    Serial.print(F(" ("));
    // Serial.print((highByte(luminosity) << 8) + lowByte(luminosity));
    // Serial.print(F(") ("));
    
    if (brightness <= 0) {
      Serial.print(F("Bright"));
    } else if (brightness == 1) {
      Serial.print(F("Okay"));
    } else {
      Serial.print(F("Dark"));
    }

    Serial.println(F(")"));
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
  Serial.print(F("Shutter: "));
  if (isShutterUp) {
    Serial.println(F("UP"));
  } else {
    Serial.println(F("DOWN"));
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
  if (!missingDHT) {
    Serial.print(F("Humidity: "));
    Serial.print(humidity);
    Serial.println(F("% "));
    Serial.print(F("Temperature: "));
    Serial.print(temperature);
    Serial.println(F("C "));
  } else {
    Serial.println(F("DHT sensor missing"));
  }
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
  bool allGood = checkSensors();
  
  DateTime now = !missingRTC ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);
  delay(50);

  if (!missingTSL) {
    sensors_event_t event;
    tsl.getEvent(&event);
    delay(50);
    if (event.light) {
      assessBrightness(event.light);
    }
  }

  humidity = !missingDHT ? dht.readHumidity() : 0;
  temperature = !missingDHT ? dht.readTemperature() : 20;

  checkDayTime(now.hour());
  automaticModeLogic(now.unixtime());
  assessButtonState(now.unixtime());
  checkEngineState();
  constructFlagByte();

  printDateTime(now.year(), now.month(), now.day(), now.hour(), now.minute());
  printHumidityAndTemperature(humidity, temperature);
  printMode();
  printShutterState();
  printBrightness();
  printEngineState();

  Serial.println();
}

// SPI interrupt routine
ISR(SPI_STC_vect) {
  byte c = SPDR;

  if (!commandModeSet) {
    commandMode = true;   // default
    SPDR = 0xFF;
    
    if (c == '?') {
      commandMode = false;
    }
      
    commandModeSet = true;
  } else {
    if (commandMode) {
      sendData(c);
    } else {
      verifyData(c);
    }

    commandModeSet = false;
  }
}

byte getLuminosityByte(bool highLumByte) {
  return highLumByte ? highByte(luminosity) : lowByte(luminosity);
}

void sendData(byte c) {
  switch (c) {
    case 'b': SPDR = flags; break;
    case 'f': {
      SPDR = getLuminosityByte(highLumByte);
      highLumByte = !highLumByte;
      break;
    }
    case 'h': SPDR = temperature; break;
    case 'e': SPDR = humidity; break;
    case 'm': forceShutters(); break;
    default: return;
  }

  lastSentData = SPDR;
  lastCommandByte = c;
}

void verifyData(byte c) {
  if (c != lastSentData) {
    if (lastCommandByte == 'f') {
      SPDR = getLuminosityByte(!highLumByte);
    } else {
      sendData(lastCommandByte);
    }
  } else {
    SPDR = c;
  }
}