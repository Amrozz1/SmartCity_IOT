#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ---------- WiFi ----------
const char* WIFI_SSID     = "Lenovo-LOQ";     
const char* WIFI_PASSWORD = "12345678";  

// ---------- HiveMQ Cloud ----------
const char* MQTT_BROKER   = "e10ae8e9b2d54a6d96198879f7d83758.s1.eu.hivemq.cloud";  
const int   MQTT_PORT     = 8883;  
const char* MQTT_USER     = "hivemq.webclient.1756145271255";     
const char* MQTT_PASS     = "1KTtGo38;2$pMw:Ze>Fd";     

WiFiClientSecure secureClient;
PubSubClient client(secureClient);

// ---------- LCD ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------- Pins ----------
const int LDR_PIN   = 34;                
const int IR1_PIN   = 14;   
const int IR2_PIN   = 27;   
const int LEDS_IR1[2] = {2, 4};   
const int LEDS_IR2[2] = {18, 19}; 
const int FLAME_PIN = 32;   // HIGH = fire
const int BUZZER_PIN= 15;                 
const int SERVO_PIN = 33;                 

// ---------- Timing control ----------
unsigned long lastMotionTime1 = 0;
unsigned long lastMotionTime2 = 0;
const unsigned long LIGHT_DURATION = 3000; // 3 seconds
bool ledsActive1 = false;
bool ledsActive2 = false;

// ---------- Servo ----------
Servo servo;

// ---------- Thresholds ----------
int  LDR_THRESHOLD = 2000;  

// ---------- States ----------
bool fireActive = false;

// ---------- MQTT Topics ----------
const char* TOPIC_SERVO  = "home/servo";
const char* TOPIC_BUZZER = "home/buzzer";
const char* TOPIC_LEDS   = "home/leds";

// ---------- Helpers ----------
void lcdPrint16(int row, const String &msg) {
  lcd.setCursor(0, row);
  String s = msg;
  while (s.length() < 16) s += ' ';
  lcd.print(s.substring(0, 16));
}

// ---------- MQTT ----------
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) message += (char)payload[i];
  Serial.print("MQTT msg [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  // Servo control
  if (String(topic) == TOPIC_SERVO) {
    if (message == "90") {
      servo.write(90);
    } else if (message == "0") {
      servo.write(0);
    }
  }

  // Buzzer control
  else if (String(topic) == TOPIC_BUZZER) {
    if (message == "ON") {
      digitalWrite(BUZZER_PIN, HIGH);
    } else if (message == "OFF") {
      digitalWrite(BUZZER_PIN, LOW);
    }
  }

  // LEDs control (all LEDs)
  else if (String(topic) == TOPIC_LEDS) {
    if (message == "ON") {
      for (int i=0;i<2;i++) {
        digitalWrite(LEDS_IR1[i], HIGH);
        digitalWrite(LEDS_IR2[i], HIGH);
      }
    } else if (message == "OFF") {
      for (int i=0;i<2;i++) {
        digitalWrite(LEDS_IR1[i], LOW);
        digitalWrite(LEDS_IR2[i], LOW);
      }
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", MQTT_USER, MQTT_PASS)) {
      Serial.println("connected");
      client.subscribe(TOPIC_SERVO);  
      client.subscribe(TOPIC_BUZZER);  // ✅ new
      client.subscribe(TOPIC_LEDS);    // ✅ new
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // LCD
  lcd.init();
  lcd.backlight();
  lcdPrint16(0, "WELCOME TO THE");
  lcdPrint16(1, "FINAL PROJECT");

  // IO
  pinMode(IR1_PIN, INPUT);
  pinMode(IR2_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT); 
  for (int i = 0; i < 2; i++) {
    pinMode(LEDS_IR1[i], OUTPUT);
    digitalWrite(LEDS_IR1[i], LOW);
    pinMode(LEDS_IR2[i], OUTPUT);
    digitalWrite(LEDS_IR2[i], LOW);
  }
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Servo
  servo.attach(SERVO_PIN);
  servo.write(0);

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secureClient.setInsecure();   // ignore TLS cert
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("connected");

  // MQTT
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  int ldrValue = analogRead(LDR_PIN);     
  bool isNight = (ldrValue < LDR_THRESHOLD);
  bool ir1Triggered = (digitalRead(IR1_PIN) == LOW);
  bool ir2Triggered = (digitalRead(IR2_PIN) == LOW);
  bool flameDetected = (digitalRead(FLAME_PIN) == HIGH); 

  // Fire logic
  if (!fireActive && flameDetected) {
    fireActive = true;
    digitalWrite(BUZZER_PIN, HIGH);
    servo.write(90);
  } else if (fireActive && !flameDetected) {
    fireActive = false;
    digitalWrite(BUZZER_PIN, LOW);
    servo.write(0);
  }

  // Street lights
  if (isNight) {
    if (ir1Triggered) { lastMotionTime1 = millis(); ledsActive1 = true; for (int i=0;i<2;i++) digitalWrite(LEDS_IR1[i], HIGH); }
    if (ledsActive1 && millis()-lastMotionTime1 >= LIGHT_DURATION) { ledsActive1=false; for (int i=0;i<2;i++) digitalWrite(LEDS_IR1[i], LOW); }

    if (ir2Triggered) { lastMotionTime2 = millis(); ledsActive2 = true; for (int i=0;i<2;i++) digitalWrite(LEDS_IR2[i], HIGH); }
    if (ledsActive2 && millis()-lastMotionTime2 >= LIGHT_DURATION) { ledsActive2=false; for (int i=0;i<2;i++) digitalWrite(LEDS_IR2[i], LOW); }
  } else {
    for (int i=0;i<2;i++) { digitalWrite(LEDS_IR1[i], LOW); digitalWrite(LEDS_IR2[i], LOW); }
    ledsActive1 = ledsActive2 = false;
  }

  // LCD
  lcdPrint16(0, (isNight ? "Night" : "Day "));
  if (fireActive) lcdPrint16(1, "!!! FIRE ALERT !!!");
  else {
    String leds = "";
    leds += (digitalRead(LEDS_IR1[0]) ? '1' : '0');
    leds += (digitalRead(LEDS_IR1[1]) ? '1' : '0');
    leds += (digitalRead(LEDS_IR2[0]) ? '1' : '0');
    leds += (digitalRead(LEDS_IR2[1]) ? '1' : '0');
    lcdPrint16(1, "LEDs:" + leds);
  }

  // --- Publish to MQTT ---
  client.publish("home/ldr", String(ldrValue).c_str());
  client.publish("home/ir1", String(ir1Triggered ? 1 : 0).c_str());
  client.publish("home/ir2", String(ir2Triggered ? 1 : 0).c_str());
  client.publish("home/fire", String(flameDetected ? 1 : 0).c_str());

  delay(500);
}




// #include <Arduino.h>
// #include <Wire.h>
// #include <LiquidCrystal_I2C.h>
// #include <WiFi.h>
// #include <WiFiClientSecure.h>
// #include <PubSubClient.h>

// // ---------- WiFi ----------
// const char* WIFI_SSID     = "Lenovo-LOQ";     
// const char* WIFI_PASSWORD = "12345678";  

// // ---------- HiveMQ Cloud ----------
// const char* MQTT_BROKER   = "e10ae8e9b2d54a6d96198879f7d83758.s1.eu.hivemq.cloud";  
// const int   MQTT_PORT     = 8883;  
// const char* MQTT_USER     = "hivemq.webclient.1756145271255";     
// const char* MQTT_PASS     = "1KTtGo38;2$pMw:Ze>Fd";     

// WiFiClientSecure secureClient;
// PubSubClient client(secureClient);

// // ---------- LCD ----------
// LiquidCrystal_I2C lcd(0x27, 16, 2);

// // ---------- Pins ----------
// const int LDR_PIN   = 34;                
// const int IR1_PIN   = 14;   
// const int IR2_PIN   = 27;   
// const int LEDS_IR1[2] = {2, 4};   
// const int LEDS_IR2[2] = {18, 19}; 
// const int FLAME_PIN = 32;   // HIGH = fire
// const int BUZZER_PIN= 15;                 
// const int SERVO_PIN = 33;                 

// // ---------- Timing control ----------
// unsigned long lastMotionTime1 = 0;
// unsigned long lastMotionTime2 = 0;
// const unsigned long LIGHT_DURATION = 3000; // 3 seconds
// bool ledsActive1 = false;
// bool ledsActive2 = false;

// // ---------- Servo (PWM) ----------
// const int SERVO_CHANNEL = 0;
// const int SERVO_FREQ = 50;      // 50Hz for servos
// const int SERVO_RESOLUTION = 16; // 16-bit resolution (0–65535)

// // Convert angle (0–180) to duty cycle
// uint32_t angleToDuty(int angle) {
//   // Typical servo: 0° → 500µs, 180° → 2400µs
//   int us = map(angle, 0, 180, 500, 2400);  
//   return (uint32_t)((us * 65535UL * SERVO_FREQ) / 1000000UL);
// }

// void moveServo(int angle) {
//   ledcWrite(SERVO_CHANNEL, angleToDuty(angle));
// }

// // ---------- Thresholds ----------
// int  LDR_THRESHOLD = 2000;  

// // ---------- States ----------
// bool fireActive = false;

// // ---------- Helpers ----------
// void lcdPrint16(int row, const String &msg) {
//   lcd.setCursor(0, row);
//   String s = msg;
//   while (s.length() < 16) s += ' ';
//   lcd.print(s.substring(0, 16));
// }

// // ---------- MQTT ----------
// void callback(char* topic, byte* payload, unsigned int length) {
//   String message;
//   for (int i = 0; i < length; i++) message += (char)payload[i];
//   Serial.print("MQTT msg [");
//   Serial.print(topic);
//   Serial.print("]: ");
//   Serial.println(message);

//   if (String(topic) == "home/servo") {
//     if (message == "90") {
//       moveServo(90);
//     } else if (message == "0") {
//       moveServo(0);
//     }
//   }
// }

// void reconnect() {
//   while (!client.connected()) {
//     Serial.print("Attempting MQTT connection...");
//     if (client.connect("ESP32Client", MQTT_USER, MQTT_PASS)) {
//       Serial.println("connected");
//       client.subscribe("home/servo");  
//     } else {
//       Serial.print("failed, rc=");
//       Serial.println(client.state());
//       delay(2000);
//     }
//   }
// }

// void setup() {
//   Serial.begin(115200);

//   // LCD
//   lcd.init();
//   lcd.backlight();
//   lcdPrint16(0, "WELCOME TO THE");
//   lcdPrint16(1, "FINAL PROJECT");

//   // IO
//   pinMode(IR1_PIN, INPUT);
//   pinMode(IR2_PIN, INPUT);
//   pinMode(FLAME_PIN, INPUT); 
//   for (int i = 0; i < 2; i++) {
//     pinMode(LEDS_IR1[i], OUTPUT);
//     digitalWrite(LEDS_IR1[i], LOW);
//     pinMode(LEDS_IR2[i], OUTPUT);
//     digitalWrite(LEDS_IR2[i], LOW);
//   }
//   pinMode(BUZZER_PIN, OUTPUT);
//   digitalWrite(BUZZER_PIN, LOW);

//   // Servo with PWM
//   ledcSetup(SERVO_CHANNEL, SERVO_FREQ, SERVO_RESOLUTION);
//   ledcAttachPin(SERVO_PIN, SERVO_CHANNEL);
//   moveServo(0);  // start at 0°

//   // WiFi
//   WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
//   secureClient.setInsecure();   // ignore TLS cert
//   Serial.print("Connecting to WiFi");
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println("connected");

//   // MQTT
//   client.setServer(MQTT_BROKER, MQTT_PORT);
//   client.setCallback(callback);
// }

// void loop() {
//   if (!client.connected()) reconnect();
//   client.loop();

//   int ldrValue = analogRead(LDR_PIN);     
//   bool isNight = (ldrValue < LDR_THRESHOLD);
//   bool ir1Triggered = (digitalRead(IR1_PIN) == LOW);
//   bool ir2Triggered = (digitalRead(IR2_PIN) == LOW);
//   bool flameDetected = (digitalRead(FLAME_PIN) == HIGH); 

//   // Fire logic
//   if (!fireActive && flameDetected) {
//     fireActive = true;
//     digitalWrite(BUZZER_PIN, HIGH);
//     moveServo(90);
//   } else if (fireActive && !flameDetected) {
//     fireActive = false;
//     digitalWrite(BUZZER_PIN, LOW);
//     moveServo(0);
//   }

//   // Street lights
//   if (isNight) {
//     if (ir1Triggered) { lastMotionTime1 = millis(); ledsActive1 = true; for (int i=0;i<2;i++) digitalWrite(LEDS_IR1[i], HIGH); }
//     if (ledsActive1 && millis()-lastMotionTime1 >= LIGHT_DURATION) { ledsActive1=false; for (int i=0;i<2;i++) digitalWrite(LEDS_IR1[i], LOW); }

//     if (ir2Triggered) { lastMotionTime2 = millis(); ledsActive2 = true; for (int i=0;i<2;i++) digitalWrite(LEDS_IR2[i], HIGH); }
//     if (ledsActive2 && millis()-lastMotionTime2 >= LIGHT_DURATION) { ledsActive2=false; for (int i=0;i<2;i++) digitalWrite(LEDS_IR2[i], LOW); }
//   } else {
//     for (int i=0;i<2;i++) { digitalWrite(LEDS_IR1[i], LOW); digitalWrite(LEDS_IR2[i], LOW); }
//     ledsActive1 = ledsActive2 = false;
//   }

//   // LCD
//   lcdPrint16(0, (isNight ? "Night" : "Day "));
//   if (fireActive) lcdPrint16(1, "!!! FIRE ALERT !!!");
//   else {
//     String leds = "";
//     leds += (digitalRead(LEDS_IR1[0]) ? '1' : '0');
//     leds += (digitalRead(LEDS_IR1[1]) ? '1' : '0');
//     leds += (digitalRead(LEDS_IR2[0]) ? '1' : '0');
//     leds += (digitalRead(LEDS_IR2[1]) ? '1' : '0');
//     lcdPrint16(1, "LEDs:" + leds);
//   }

//   // --- Publish to MQTT ---
//   client.publish("home/ldr", String(ldrValue).c_str());
//   client.publish("home/ir1", String(ir1Triggered ? 1 : 0).c_str());
//   client.publish("home/ir2", String(ir2Triggered ? 1 : 0).c_str());
//   client.publish("home/fire", String(flameDetected ? 1 : 0).c_str());

//   delay(500);
// }
