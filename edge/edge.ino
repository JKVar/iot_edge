#include "DHT.h"
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <DS3231.h>
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
RTClib myRTC;
DS3231 rtc;

bool missingDHT = false;

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

int buttonOldState, buttonState;

bool isManual; // false - automatic, true - manual
int lastButtonChangeTimestamp = 0;
const int automaticManualChangeDelay = 300; // in seconds
bool isItDay; // false - it's night (who would have guessed), true - it's day
bool isShutterUp = true;

short luminosity; // <= 0 - too bright, 1 - okay, >= 2 - too dark
const float normalLightThreshold = 100;
const float tooBrightThreshold = 1000;
bool isLightSensorDetected = true;

void checkSensors() {
  
}

short assessLuminosity(float light) {
  if (light < normalLightThreshold) {
    luminosity = 2;
  } else if (light < tooBrightThreshold) {
    luminosity = 1;
  } else {
    luminosity = 0;
  }
}

void checkDayTime(uint8_t hour) {
  if (hour >= 8 && hour <= 20) {
    isItDay = true;
  } else {
    isItDay = false;
  }
}

void canSwitchMode(DateTime currentDate) {
  int unixT = currentDate.unixtime();
  int timeDiff = abs(unixT - lastButtonChangeTimestamp);

  if (timeDiff >= automaticManualChangeDelay) {
    isManual = false;
  }
}

void changeShutters(bool state) {
  // TODO: make the engine go brrrr
  isShutterUp = state;
}

void automaticModeLogic(DateTime currentDate) {
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
    canSwitchMode(currentDate);
  }
}

void configureLightSensor() {
  if (tsl.begin()) {
    tsl.enableAutoRange(true);
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);
  } else {
    isLightSensorDetected = false;
  }
}

void setup() {
  pinMode(A0,INPUT_PULLUP);
  // gomb allapotanak elmentese
  buttonOldState = digitalRead(A0);
  buttonState = buttonOldState;

  Serial.begin(9600);
  Serial.println(F("DHTxx test!"));
  Wire.begin();

  configureLightSensor();
  dht.begin();
  lcd.init();
  lcd.backlight();

  // ido beallitasa
  /* rtc.setYear(25);
  rtc.setMonth(10);
  rtc.setDoW(3);
  rtc.setHour(21);
  rtc.setMinute(56); */

 
  // custom karakterek letrehozasa
  lcd.createChar(0, nightChar);
  lcd.createChar(1, dayChar);
  lcd.createChar(2, upChar);
  lcd.createChar(3, downChar);
  lcd.createChar(4, gradeChar);
  lcd.home();

  lcd.print("So it begins");
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
  lcd.print("Light: ");

  if (luminosity <= 0) {
    lcd.print("Bright");
  } else if (luminosity == 1) {
    lcd.print("Okay");
  } else {
    lcd.print("Dark");
  }
}

void loop() {
  // Wait a few seconds between measurements.
  delay(2000);
  DateTime now = myRTC.now();
  uint16_t year = now.year();
  uint8_t month = now.month();
  uint8_t day = now.day();
  uint8_t hour = now.hour();
  uint8_t minute = now.minute();

  checkDayTime(hour);
  automaticModeLogic(now);

  sensors_event_t event;
  tsl.getEvent(&event);

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
  if (event.light) {
    printLuminosity();
  } else {
    lcd.print("Sensor overload.");
  }

  buttonState = digitalRead(A0);

  if (buttonState != buttonOldState) {
    buttonOldState = buttonState;
    changeShutters(!isShutterUp);

    lastButtonChangeTimestamp = now.unixtime();
    isManual = true;

    lcd.setCursor(0, 3);
    lcd.print(F("Switch"));
  }
}
