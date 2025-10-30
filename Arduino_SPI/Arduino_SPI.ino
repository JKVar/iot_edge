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

byte nightChar[] = {
  B00011,
  B10001,
  B11001,
  B10101,
  B10011,
  B10001,
  B11000,
  B00000,
};

byte dayChar[] = {
  B11100,
  B11010,
  B11001,
  B11001,
  B11001,
  B11001,
  B11010,
  B11100,
};

byte upChar[] = {
  B00000,
  B00100,
  B01110,
  B10101,
  B00100,
  B00100,
  B00100,
  B00000,
};

byte downChar[] = {
  B00000,
  B00100,
  B00100,
  B00100,
  B10101,
  B01110,
  B00100,
  B00000,
};

byte gradeChar[] = {
  B00111,
  B00101,
  B00111,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
};

byte automaticChar[] = {
  B00000,
  B01110,
  B10001,
  B10001,
  B11111,
  B10001,
  B10001,
  B00000,
};

byte manualChar[] = {
  B00000,
  B11011,
  B10101,
  B10101,
  B10001,
  B10001,
  B00000,
  B00000,
};

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

  // if (tsl.begin()) {
  //   tsl.enableAutoRange(true);
  //   tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);
  //   missingTSL = false;
  // } else {
  //   missingTSL = true;
  //   Serial.print(F("Missing TSL sensor"));
  // }

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

// void configureLCD() {
//   lcd.init();
//   delay(500);

//   lcd.backlight();
//   lcd.home();
// }

void configureRTC() {
  if (rtc.begin()) {
    // Comment this out after first run.
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  } else {
    missingRTC = true;
    Serial.println(F("Failed to start RTC."));
  }
}

// void configureCustomChars() {
//   lcd.createChar(0, nightChar);
//   lcd.createChar(1, dayChar);
//   lcd.createChar(2, upChar);
//   lcd.createChar(3, downChar);
//   lcd.createChar(4, gradeChar);
//   lcd.createChar(5, automaticChar);
//   lcd.createChar(6, manualChar);
// }


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

  pinMode(A0, INPUT_PULLUP);
  buttonOldState = digitalRead(A0);
  buttonState = buttonOldState;
  Wire.begin();

  configureDHT();
  // configureLCD();
  configureRTC();
  configureTSL();
  myStepper.setSpeed(60);

  // configureCustomChars();
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

  // if (isItDay) {
  //   lcd.printByte(1);  // nappal karaktere
  // } else {
  //   lcd.printByte(0);  // ejszaka karaketre
  // }
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
    // Serial.printByte(6);
  } else {
    Serial.println(F("Automatic"));
    // lcd.printByte(5);
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
  // flushScreen();
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
  // int err = SimpleDHTErrSuccess;
  // if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
  //   Serial.print("Read DHT11 failed, err=");
  //   Serial.println(err);
  //   delay(1000);
  //   return;
  // }

  // Serial.print("Sample OK: ");
  // Serial.print((int)temperature);
  // Serial.print(" *C, ");
  // Serial.print((int)humidity);
  // Serial.println(" H");

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
  //
  //  if (!allGood) {
  //    printSensorProblems();
  //    return -1;
  //  }

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