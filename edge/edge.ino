#include "DHT.h"
#include "RTClib.h"
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

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

DHT dht(DHTPIN, DHTTYPE);
RTC_DS3231 rtc;

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

const int automaticManualChangeDelay = 10;
const int normalLightThreshold = 100;
const int tooBrightThreshold = 1000;

uint32_t lastButtonChangeTimestamp = 0;
int buttonOldState, buttonState;
int luminosity = 1; // <= 0 - too bright, 1 - okay, >= 2 - too dark

bool isManual = false;
bool isItDay;
bool isShutterUp = true;
bool isEngineOn = false;

bool missingDHT = false;
bool missingRTC = false;
bool missingTSL = false;

void checkSensors() {
  
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
  // TODO: make the engine go brrrr
  isShutterUp = state;
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
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
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
  lcd.print(F("Light: "));

  if (missingTSL) {
    lcd.print(F("Okay (No Sensor)"));
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
    lcd.print(F("Manual"));
  } else {
    lcd.print(F("Automatic"));
  }
}

void printShutterState() {
  lcd.print(F("Shutter "));

  if (isShutterUp) {
    lcd.printByte(2);
  } else {
    lcd.printByte(3);
  }
}

void printEngineState() {
  lcd.print(F("Engine: "));

  if (isEngineOn) {
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
  lcd.clear();
  delay(50);
}

void loop() {
  // Wait a few seconds between measurements.
  delay(2000);

  // Assess sensor data, handling the cases where they are not available.
  DateTime now = !missingRTC ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);
  delay(50);

  if (missingTSL) {
    sensors_event_t event;
    tsl.getEvent(&event);
    delay(50);
    if (event.light) {
      assessLuminosity(event.light);
    }
  }

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
  printHumidityAndTemperature(humidity, temperature);
  lcd.setCursor(0, 2);
  printMode();
  lcd.setCursor(0, 3);
  printLuminosity();

  // Wait before printing the next batch of data.
  delay(2000);
  flushScreen();

  printShutterState();
  lcd.setCursor(0, 1);
  printEngineState();

  // Ellenőrizzük-e, hogy egyes szenzorok elérhetőek-e újból?
}

/*void loop() {
  // Wait a few seconds between measurements.
  delay(2000);
  
  DateTime now = rtc.now();
  delay(50);
  
  uint16_t year = now.year();
  uint8_t month = now.month();
  uint8_t day = now.day();
  uint8_t hour = now.hour();
  uint8_t minute = now.minute();

  sensors_event_t event;
  tsl.getEvent(&event);
  delay(50);

  if(event.light) {
    assessLuminosity(event.light);
  }
  checkDayTime(hour);
  automaticModeLogic(now);
  assessButtonState(now);

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);
  
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    lcd.flush();
    lcd.clear();
    lcd.print(F("Failed to read from DHT sensor!"));
    return;
  }

  lcd.flush();
  lcd.clear();
  
  printDate(year, month, day);
  printDayOrNight(hour, minute);
  
  lcd.setCursor(0, 1);
  lcd.print(h);
  lcd.print(F("% "));
  lcd.print(t);
  lcd.printByte(4);
  lcd.print(F("C "));

  lcd.setCursor(0, 2);
  printMode();

  lcd.setCursor(0, 3);
  if (event.light) {
    printLuminosity();
  } else {
    lcd.print(F("Sensor overload."));
  }

  delay(2000);
  lcd.flush();
  lcd.clear();

  printShutterState();

  lcd.setCursor(0, 1);
  printEngineState();
}*/


  // Serial.print(F("Humidity: "));
  // Serial.print(h);
  // Serial.print(F("%  Temperature: "));
  // Serial.print(t);
  // Serial.print(F("°C "));
  // Serial.print(f);
  // Serial.print(F("°F  Heat index: "));
  // Serial.print(hic);
  // Serial.print(F("°C "));
  // Serial.print(hif);
  // Serial.println(F("°F"));
