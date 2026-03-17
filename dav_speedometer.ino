#include <Wire.h>
#include <LiquidCrystal.h>


// Pins
const int sensorPin = A0;
const int buttonPin = A3;
const int goalPin = A2;
const int buzzerPin = A1;

//LCD Pins
const int lcd_rs = 2;
const int lcd_e  = 4;
const int lcd_d4 = 9;
const int lcd_d5 = 10;
const int lcd_d6 = 11;
const int lcd_d7 = 12;


LiquidCrystal lcd(lcd_rs, lcd_e, lcd_d4, lcd_d5, lcd_d6, lcd_d7);

// Sensorwerte
int sensorValue = 0;

// Defaultwerte, falls noch nicht kalibriert
int thresholdLow = 800;
int thresholdHigh = 900;

bool matOccupied = false;

// Timer
unsigned long startTime = 0;
unsigned long stopTime = 0;

// Zustände
enum State {
  READY,
  ARMED,
  RUNNING,
  FINISHED,
  DEBUG
};

State state = DEBUG;

// Button Timing
unsigned long buttonPressTime = 0;
bool buttonHeld = false;

// ----------------------------

void beep(int freq, int duration) {
  tone(buzzerPin, freq, duration);
  delay(duration);
  noTone(buzzerPin);
}

// Dreiklang beim Start
void startupBeep() {
  beep(1000, 100);
  delay(50);
  beep(1300, 100);
  delay(50);
  beep(1600, 120);
}

void lcd_print(const char* msg, int x, int y) {
  lcd.setCursor(x, y);
  lcd.print(msg);

  Serial.print("LCD[");
  Serial.print(x);
  Serial.print(",");
  Serial.print(y);
  Serial.print("]: ");
  Serial.println(msg);
}

void lcd_print(float value, int x, int y) {

  lcd.setCursor(x, y);
  lcd.print(value);

  Serial.print("LCD[");
  Serial.print(x);
  Serial.print(",");
  Serial.print(y);
  Serial.print("]: ");
  Serial.println(value);
}
// ----------------------------

void updateMatState() {
  if (sensorValue <= thresholdLow) {
    if( matOccupied != true){
      beep(2800, 60);
    }
    matOccupied = true;
  } 
  else if (sensorValue >= thresholdHigh) {
    if( matOccupied == true){
      beep(400, 60);
    }
    matOccupied = false;
  }
}

int readSensorAverage(int pin, int samples = 20) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  return sum / samples;
}

// ----------------------------

void setup() {

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(goalPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);

  Serial.begin(115200);

  lcd.begin(16, 2);

  startupBeep();

  lcd_print("Speedwall Timer",0, 0);
  lcd_print("Bereit",0,1);
  state = DEBUG;

  delay(1500);
  lcd.clear();
}

// ----------------------------

void loop() {

  sensorValue = readSensorAverage(sensorPin, 10);
  updateMatState();

  bool buttonPressed = !digitalRead(buttonPin);
  bool goalPressed = !digitalRead(goalPin);

  handleButton(buttonPressed);
  
  switch (state) {

    case READY:{

      lcd_print("Bereit        ",0,0);

      if (matOccupied) {
        state = ARMED;
        beep(2000, 80);
      }

      break;
    }
    // ------------------------

    case ARMED:{

      lcd_print("Start bereit  ",0,0);

      if (!matOccupied) {

        startTime = millis();
        state = RUNNING;

        beep(2500, 80);
      }

      break;
    }
    // ------------------------

    case RUNNING:{

      unsigned long t = millis() - startTime;

      lcd_print("Zeit:",0,0);

      lcd_print(t / 1000.0 ,6 ,0);

      if (goalPressed) {

        stopTime = millis();
        state = FINISHED;

        beep(3000, 150);
      }

      break;
    }
    // ------------------------

    case FINISHED:{

      lcd_print("Ergebnis:",0,0);


      float result = (stopTime - startTime) / 1000.0;
      lcd_print(result,0,1);

      break;
    }
    // ------------------------

    case DEBUG: {

      //lcd_print("Sensor:",0,0);

      lcd_print(sensorValue,0,1);

      break;
    }
  }
}

// ----------------------------

void handleButton(bool pressed) {
  if (pressed){
    beep(800, 60);
  }
  
  if (pressed && !buttonHeld) {
    buttonHeld = true;
    buttonPressTime = millis();
  }

  if (!pressed && buttonHeld) {

    unsigned long pressDuration = millis() - buttonPressTime;

    buttonHeld = false;

    beep(1800, 60);

    // kurzer Druck
    if (pressDuration < 1500) {
      startCalibration();
    }

    // langer Druck
    else {
      state = DEBUG;
    }
  }
}


// ----------------------------

void startCalibration() {
  lcd.clear();
  lcd_print("Von Matte", 0, 0);
  lcd_print("absteigen", 0, 1);
  delay(3000);

  int emptyValue = readSensorAverage(sensorPin, 30);

  lcd.clear();
  lcd_print("Auf Matte", 0, 0);
  lcd_print("stellen", 0, 1);
  delay(3000);

  int loadValue = readSensorAverage(sensorPin, 30);

  int diff = emptyValue - loadValue;
  if (diff < 0) diff = -diff;

  int mid = (emptyValue + loadValue) / 2;
  int hyst = max(20, diff / 6);

  thresholdLow  = mid - hyst;
  thresholdHigh = mid + hyst;

  lcd.clear();
  lcd_print("Leer:", 0, 0);
  lcd_print((float)emptyValue, 6, 0);
  lcd.setCursor(0, 1);
  lcd.print("Last:");
  lcd.print(loadValue);
  delay(2000);

  lcd.clear();
  lcd_print("Low:", 0, 0);
  lcd_print((float)thresholdLow, 5, 0);
  lcd.setCursor(0, 1);
  lcd.print("High:");
  lcd.print(thresholdHigh);
  delay(2000);

  state = READY;
}
