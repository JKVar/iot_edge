#include "DHT.h"
#include "RTClib.h"
#include <Stepper.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#define DHTPIN A3     // Digital pin connected to the DHT sensor

#define DHTTYPE DHT11   // DHT 11

#if defined(ARDUINO) && ARDUINO >= 100
#define printByte(args)  write(args);
#else
#define printByte(args)  print(args,BYTE);
#endif

const int stepsPerRevolution = 200;

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display
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
int luminosity = 1; // <= 0 - too bright, 1 - okay, >= 2 - too dark
int engineState = -1; // <= 0 - off; >=1 - on

bool isManual = false;
bool isItDay;
bool isShutterUp = true;

bool missingDHT = false;
bool missingRTC = false;
bool missingTSL = false;

bool checkSensors() {
  float test = dht.readTemperature();
  if (isnan(test)) {
    missingDHT = true;
    Serial.print(F("Missing DHT sensor"));
  } else {
    missingDHT = false;
  }

  if (tsl.begin()) {
    tsl.enableAutoRange(true);
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);
    missingTSL = false;
  } else {
    missingTSL = true;
    Serial.print(F("Missing TSL sensor"));
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
    Serial.print(F("Failed to start TSL."));
  }
}

void configureDHT() {
  dht.begin();
  delay(1000);

  float test = dht.readTemperature();
  if (isnan(test)) {
    missingDHT = true;
    Serial.print(F("Failed to start DHT."));
  }
}

void configureLCD() {
  lcd.init();
  delay(500);

  lcd.backlight();
  lcd.home();
}

void configureRTC() {
  if (rtc.begin()) {
    // Comment this out after first run.
     rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  } else {
    missingRTC = true;
    Serial.print(F("Failed to start RTC."));
  }
}

void configureCustomChars() {
  lcd.createChar(0, nightChar);
  lcd.createChar(1, dayChar);
  lcd.createChar(2, upChar);
  lcd.createChar(3, downChar);
  lcd.createChar(4, gradeChar);
  lcd.createChar(5, automaticChar);
  lcd.createChar(6, manualChar);
}

void setup() {
  pinMode(A0, INPUT_PULLUP);
  buttonOldState = digitalRead(A0);
  buttonState = buttonOldState;

  Serial.begin(9600);
  Wire.begin();

  configureDHT();
  configureLCD();
  configureRTC();
  configureTSL();
  myStepper.setSpeed(60);


  configureCustomChars();
  lcd.print(F("So it begins"));
}

void printWithZero(uint8_t number) {
  if (number < 10) lcd.print("0");
  lcd.print(number);
}

void printDate(uint16_t year, uint8_t month, uint8_t day) {
  lcd.print(year);
  lcd.print("/");
  printWithZero(month);
  lcd.print("/");
  printWithZero(day);
  lcd.print(" ");
}

void printDayOrNight(uint8_t hour, uint8_t minute) {
  printWithZero(hour);
  lcd.print(":");
  printWithZero(minute);
  lcd.print(" ");

  if (isItDay) {
    lcd.printByte(1); // nappal karaktere
  } else {
    lcd.printByte(0); // ejszaka karaketre
  }
}

void printLuminosity() {
  lcd.print(F(" "));

  if (missingTSL) {
    lcd.print(F("No TSL"));
  } else {
    if (luminosity <= 0) {
      lcd.print(F("Bright"));
    } else if (luminosity == 1) {
      lcd.print(F("Okay"));
    } else {
      lcd.print(F("Dark"));
    }
  }
}

void printMode() {
  lcd.print(F("Mode: "));

  if (isManual) {
//    lcd.print(F("Manual"));
      lcd.printByte(6);
  } else {
//    lcd.print(F("Automatic"));
      lcd.printByte(5);
  }
}

void printShutterState() {
  lcd.print(F(" "));
  if (isShutterUp) {
    lcd.printByte(2);
  } else {
    lcd.printByte(3);
  }
}

void printEngineState() {
  lcd.print(F("Engine: "));

  if (engineState > 0) {
    lcd.print(F("On"));
  } else {
    lcd.print(F("Off"));
  }
}

void printHumidityAndTemperature(float humidity, float temperature) {
  lcd.print(humidity);
  lcd.print(F("% "));
  lcd.print(temperature);
  lcd.printByte(4);
  lcd.print(F("C "));
}

void flushScreen() {
  lcd.flush();
//  lcd.setCursor(0,0);
  lcd.clear();
  delay(50);
}

void printSensorProblems() {
  flushScreen();
  lcd.print(F("ERROR"));
  int row = 1;

  lcd.setCursor(0, row);
  if (missingDHT) {
    lcd.print(F("Missing DHT sensor"));
    row++;
  }

  lcd.setCursor(0, row);
  if (missingTSL) {
    lcd.print(F("Missing TSL sensor"));
    row++;
  } 
}

void loop() {
  // Wait a few seconds between measurements.
  delay(1000);
  

  // Assess sensor data, handling the cases where they are not available.
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

  float humidity = !missingDHT ? dht.readHumidity() : 0;
  float temperature = !missingDHT ? dht.readTemperature() : 20;

  checkDayTime(now.hour());
  automaticModeLogic(now.unixtime());
  assessButtonState(now.unixtime());

  // Print onto the LCD device.
  flushScreen();

  printDate(now.year(), now.month(), now.day());
  printDayOrNight(now.hour(), now.minute());
  lcd.setCursor(0, 1);
    
  if (missingDHT) {
    lcd.print(F("DHT sensor missing"));
  } else {
    printHumidityAndTemperature(humidity, temperature);
  }

  lcd.setCursor(0, 2);
  printMode();
  printShutterState();

  printLuminosity();

  lcd.setCursor(0, 3);
  printEngineState();
  checkEngineState();
}
