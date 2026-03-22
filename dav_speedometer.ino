#include <Wire.h>
#include <LiquidCrystal.h>
#include "BigNum.h"

// Pins
const int sensorPin = A0;
const int buttonPin = A3;
const int goalPin = A2;
const int buzzerPin = 5;

//LCD Pins
const int lcd_rs = 2;
const int lcd_e  = 4;
const int lcd_d4 = 9;
const int lcd_d5 = 10;
const int lcd_d6 = 11;
const int lcd_d7 = 12;


LiquidCrystal lcd(lcd_rs, lcd_e, lcd_d4, lcd_d5, lcd_d6, lcd_d7);
static BigNum bn(&lcd);

// Sensorwerte
int sensorValue = 0;

// Defaultwerte, falls noch nicht kalibriert
int thresholdLow = 800;
int thresholdHigh = 900;

bool matOccupied = false;
bool matUp = false;
bool matDown = false;
unsigned long matPressedTime;

bool flipflap = true;
bool flipflapUp = true;
unsigned long flipflapTime;
const unsigned long flipflapPause = 500;

// Timer
unsigned long startTime = 0;
unsigned long stopTime = 0;

// Zustände
enum State {
	READY,
	ARMED,
	RUNNING,
	FINISHED,
	DEBUG,
	PREDEBUG,
};

State state = READY;

// Button Timing
unsigned long buttonPressTime = 0;
bool buttonHeld = false;
bool buttonUp = false;
bool buttonDown = false;

//beeper
unsigned long beepEnd = 0;


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

void lcd_print(const __FlashStringHelper* msg, int x, int y) {
  lcd.setCursor(x, y);
  lcd.print(msg);

  Serial.print(F("LCD["));
  Serial.print(x);
  Serial.print(F(","));
  Serial.print(y);
  Serial.print(F("]: "));
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
	matUp = false;
	matDown = false;
	if (sensorValue <= thresholdLow) {
		if( matOccupied != true){
			matDown = true;
			matPressedTime = millis();
			beep(500, 60);
		}
		matOccupied = true;
	} 
	else if (sensorValue >= thresholdHigh) {
		if( matOccupied == true){
			matUp = true;
			beep(100, 60);
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
	bn.begin();


	lcd.clear();
	lcd.noCursor();
	lcd_print(F("DAV Speedometer"),0, 0);
	lcd_print(F("version 0.1"),0,1);

	startupBeep();

	delay(1500);
	startReady();
}

// ----------------------------

void loop() {
	flipflapUp = false;
	if( flipflapTime < millis() ){
		flipflap = !flipflap;
		flipflapTime = millis() + flipflapPause;
		flipflapUp = true;
	}

	sensorValue = readSensorAverage(sensorPin, 10);
	updateMatState();

	bool buttonPressed = !digitalRead(buttonPin);
	bool goalPressed = !digitalRead(goalPin);

	handleBeep();
	
	switch (state) {

		case READY:{
			handleButton(buttonPressed);

			if( flipflap && flipflapUp ){
				lcd_print(F("betreten"),7,0);
			}else if( flipflapUp){
				lcd_print(F("        "),7,0);
			}

			if (matDown) {
				startArmed();
			}

			break;
		}
		// ------------------------

		case ARMED:{
			if( flipflapUp ){
				beep( 3000, 20 );
			}

			if( flipflap ){
				lcd_print("Go",6,1);
			}else{
				lcd_print("  ",6,1);
			}

			//kletterer verlässt Matte
			if (matUp) {
				startTime = millis();
				startRunning();
			}

			break;
		}
		// ------------------------

		case RUNNING:{
			unsigned long t = millis() - startTime;

			if( matDown && t < 2000 ){
				//läuft, aber abbruch wenn unter 2 sekunden (kurzes abheben)
				startArmed();
			} else if( matOccupied ){
				//nach 2 sekunden, auf die matte gehen ist abbruch
				//neustart kondition wenn ziel nicht erreicht wird
				if( flipflap ){
					lcd_print(F("    Abbruch!    "),0,0);
					lcd_print(F("                "),0,1);
				}else{
					lcd_print(F("                "),0,0);
					lcd_print(F("                "),0,1);
				}
			} else if( matUp ){
				//wenn nach dem abbruch die matte verlassen wird, gehe wieder auf ready
				startReady();
			} else {
				//keine mattenberührung, timer läuft weiter
				bn.setDoubleFont();
				bn.writeNumber(0,4,t / 1000.0, 4,false);
				bn.writeNumber(0,8,t % 1000, 4);
				bn.setSingleFont();
				lcd_print("sec",13,1);
				lcd_print(".",8,1);
			}

			if (goalPressed) {
				startFinished();
			}

			break;
		}
		// ------------------------

		case FINISHED:{
			if( flipflap && flipflapUp ){
				lcd_print(F("Zeit"),0,0);
			}else if( flipflapUp){
				lcd_print(F("    "),0,0);
			}
			if (matUp) {
				startReady();
			}

			break;
		}
		
		// ------------------------
		case DEBUG: {
			lcd_print_padded(sensorValue,0,1,4,0);

			handleButton(buttonPressed);
			if (buttonUp) {
				startReady();
			}

			break;
		}

		case PREDEBUG: {
			handleButton(buttonPressed);
			if (buttonUp) {
				state = DEBUG;
			}
			break;
		}
	}
}

// ----------------------------

void handleButton(bool pressed) {
	buttonUp = false;
	buttonDown = false;

	if (pressed && !buttonHeld) {
		beep(800, 60);
		buttonHeld = true;
		buttonPressTime = millis();
		buttonDown = true;
	}

	if (!pressed && buttonHeld) {
		buttonUp = true;
	}

	if (pressed && buttonHeld && state != DEBUG) {
		unsigned long pressDuration = millis() - buttonPressTime;

		// langer Druck
		if (pressDuration > 1500){
			startDebug();
		}
	}

	if (buttonUp && state == READY) {
		beep(1800, 60);

		unsigned long pressDuration = millis() - buttonPressTime;
		buttonHeld = false;

		// kurzer Druck
		if (pressDuration < 1500) {
			startCalibration();
		}
	}
	if (!pressed){
		buttonHeld = false;
	}
}


// ----------------------------

void startReady() {
	lcd.clear();
	lcd_print(F("Bereit"), 0, 0);
	state = READY;
}

// ----------------------------

void startFinished() {
	bn.setSingleFont();
	stopTime = millis();
	state = FINISHED;
	startupBeep();
}

// ----------------------------

void startArmed() {
	lcd.clear();
	lcd_print(F("Bereit zum Start"), 0, 0);
	beep(2000, 80);
	state = ARMED;
}

// ----------------------------

void startRunning() {
	lcd.clear();
	beep(3000, 80);
	state = RUNNING;
}

// ----------------------------

void startDebug() {
	lcd_print(F("Now   Lo    Hi  "), 0, 0);
	lcd_print(F("0000  0000  0000"), 0, 1);
	lcd_print_padded(thresholdLow,6,1,4,0);
	lcd_print_padded(thresholdHigh,12,1,4,0);
	state = PREDEBUG;
}

// ----------------------------

void startCalibration() {
	lcd.clear();
	delay(2);
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
	lcd_print("Low:", 0, 0);
	lcd_print(thresholdLow, 6, 0);
	lcd_print("High:",0,1);
	lcd_print(thresholdHigh,6,1);
	delay(4000);

	startReady();
}
