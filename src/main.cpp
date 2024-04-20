#include <Arduino.h>
#include <LiquidCrystal.h>

#define THRESHOLD 1000
#define BUZZER_FREQUENCY 900

#define MQ2_AO_PIN 1  // ESP32's pin GPIO1 connected to AO pin of the MQ2 sensor
#define MQ2_DO_PIN 2  // ESP32's pin GPIO2 connected to DO pin of the MQ2 sensor
#define BUZZER_PIN 9

constexpr uint8_t PIN_RS = 3;
constexpr uint8_t PIN_DB4 = 4;
constexpr uint8_t PIN_DB5 = 5;
constexpr uint8_t PIN_DB6 = 6;
constexpr uint8_t PIN_DB7 = 7;
constexpr uint8_t PIN_EN = 8;

LiquidCrystal lcd(PIN_RS, PIN_EN, PIN_DB4, PIN_DB5, PIN_DB6, PIN_DB7);

void setup() {
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Gas Leakage");
  lcd.setCursor(0, 1);
  lcd.print("IoT Platform!");

  Serial.begin(115200);
  Serial.println("Warming up the MQ2 sensor");
  delay(20000);  // wait for the MQ2 to warm up
  Serial.println("Warmed up");

  pinMode(BUZZER_PIN, OUTPUT); // buzzer
}

void loop() {
  int gasValue = analogRead(MQ2_AO_PIN);
  int gasLeakage = digitalRead(MQ2_DO_PIN);

  Serial.print("MQ2 sensor AO value: ");
  Serial.println(gasValue);
  Serial.print("MQ2 sensor DO value: ");
  Serial.println(gasLeakage);

  lcd.clear();
  lcd.setCursor(0, 1);
  if (gasValue >= THRESHOLD) {
    lcd.print("Leakage Alert!");

    tone(BUZZER_PIN, BUZZER_FREQUENCY);
    neopixelWrite(RGB_BUILTIN,RGB_BRIGHTNESS,0,0);
  }
  else {
    lcd.clear();

    noTone(BUZZER_PIN);
    neopixelWrite(RGB_BUILTIN,0,0,0);
  }
  lcd.setCursor(0, 0);
  lcd.print("Gas Level: ");
  lcd.print(gasValue);

  delay(5000);
}
