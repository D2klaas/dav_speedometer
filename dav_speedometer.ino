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

//beeper
unsigned long beepEnd = 0;

// ----------------------------

// Big font segments (custom chars 0..4)
const byte SEG_L = 0;
const byte SEG_R = 1;
const byte SEG_T = 2;
const byte SEG_B = 3;
const byte SEG_DOT = 4;
const byte SEG_SPACE = 255;

void initBigFont() {
  byte leftBar[8]  = { B10000, B10000, B10000, B10000, B10000, B10000, B10000, B10000 };
  byte rightBar[8] = { B00001, B00001, B00001, B00001, B00001, B00001, B00001, B00001 };
  byte topBar[8]   = { B11111, B00000, B00000, B00000, B00000, B00000, B00000, B00000 };
  byte botBar[8]   = { B00000, B00000, B00000, B00000, B00000, B00000, B00000, B11111 };
  byte dotChar[8]  = { B00000, B00000, B00000, B00000, B00000, B00000, B00100, B00100 };

  lcd.createChar(SEG_L, leftBar);
  lcd.createChar(SEG_R, rightBar);
  lcd.createChar(SEG_T, topBar);
  lcd.createChar(SEG_B, botBar);
  lcd.createChar(SEG_DOT, dotChar);
}

void writeSeg(byte seg) {
  if (seg == SEG_SPACE) {
    lcd.print(" ");
  } else {
    lcd.write(seg);
  }
}

void drawBigChar(char c, int x) {
  byte t0 = SEG_SPACE, t1 = SEG_SPACE, t2 = SEG_SPACE;
  byte b0 = SEG_SPACE, b1 = SEG_SPACE, b2 = SEG_SPACE;

  switch (c) {
    case '0': t0=SEG_L; t1=SEG_T; t2=SEG_R; b0=SEG_L; b1=SEG_B; b2=SEG_R; break;
    case '1': t2=SEG_R; b2=SEG_R; break;
    case '2': t1=SEG_T; t2=SEG_R; b0=SEG_L; b1=SEG_B; break;
    case '3': t1=SEG_T; t2=SEG_R; b1=SEG_B; b2=SEG_R; break;
    case '4': t0=SEG_L; t2=SEG_R; b1=SEG_B; b2=SEG_R; break;
    case '5': t0=SEG_L; t1=SEG_T; b1=SEG_B; b2=SEG_R; break;
    case '6': t0=SEG_L; t1=SEG_T; b0=SEG_L; b1=SEG_B; b2=SEG_R; break;
    case '7': t1=SEG_T; t2=SEG_R; b2=SEG_R; break;
    case '8': t0=SEG_L; t1=SEG_T; t2=SEG_R; b0=SEG_L; b1=SEG_B; b2=SEG_R; break;
    case '9': t0=SEG_L; t1=SEG_T; t2=SEG_R; b1=SEG_B; b2=SEG_R; break;
    case '.': b1=SEG_DOT; break;
    default: break;
  }

  lcd.setCursor(x, 0);
  writeSeg(t0); writeSeg(t1); writeSeg(t2);
  lcd.setCursor(x, 1);
  writeSeg(b0); writeSeg(b1); writeSeg(b2);
}

void lcd_print_big_time(float seconds, int x, int decimals = 2) {
  char buf[16];
  dtostrf(seconds, 0, decimals, buf);

  Serial.print("BIGTIME: ");
  Serial.println(buf);

  int len = strlen(buf);
  int maxChars = (16 - x) / 3;
  if (len > maxChars) len = maxChars;

  // clear area (both rows)
  lcd.setCursor(x, 0);
  for (int i = 0; i < len * 3; i++) lcd.print(" ");
  lcd.setCursor(x, 1);
  for (int i = 0; i < len * 3; i++) lcd.print(" ");

  int pos = x;
  for (int i = 0; i < len; i++) {
    drawBigChar(buf[i], pos);
    pos += 3;
  }
}

void handleBeep() {
  if( beepEnd < millis() ){
    noTone(buzzerPin);
  }
}

void beep(int freq, int duration) {
	tone(buzzerPin, freq, duration);
	beepEnd = millis() + duration;
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

void lcd_print_padded(float value, int x, int y, int width, int precision = 2) {
	char buf[16];
	if (width > (int)(sizeof(buf) - 1)) {
		width = sizeof(buf) - 1;
	}
	dtostrf(value, width, precision, buf);
	lcd_print(buf, x, y);
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
  initBigFont();

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
	handleBeep();
	
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

      lcd_print_big_time(t / 1000.0, 0, 2);

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
			lcd_print_padded(t / 1000.0, 9, 0, 10, 2);

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
	if (pressed && !buttonHeld) {
		beep(800, 60);
		buttonHeld = true;
		buttonPressTime = millis();
	}

	if (!pressed && buttonHeld) {
		beep(1800, 60);

		unsigned long pressDuration = millis() - buttonPressTime;
		buttonHeld = false;

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
