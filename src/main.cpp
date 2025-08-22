
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "Board.h"

// Firebase en WiFi objecten
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 60000; // 1 minuut

void connectToWiFi() {
  Serial.print("Verbinden met WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi verbonden!");
    Serial.print("IP adres: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi verbinding mislukt!");
  }
}

void initFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

String bootTimeStr = "";
bool bootTimeUploaded = false;

void updateTimeToFirebase() {
  // Huidige tijd
  time_t now = time(nullptr);
  char timeStr[32];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
  String pathTime = "/tijd";
  if (Firebase.RTDB.setString(&fbdo, pathTime, timeStr)) {
    Serial.print("Tijd geüpload: ");
    Serial.println(timeStr);
  } else {
    Serial.print("Fout bij uploaden tijd: ");
    Serial.println(fbdo.errorReason());
  }

  // Runtime sinds boot
  unsigned long runtimeMillis = millis();
  unsigned long totalMinutes = runtimeMillis / 60000;
  unsigned int hours = totalMinutes / 60;
  unsigned int minutes = totalMinutes % 60;
  char runtimeStr[16];
  snprintf(runtimeStr, sizeof(runtimeStr), "%02u:%02u", hours, minutes);
  String pathRuntime = "/runtime";
  if (Firebase.RTDB.setString(&fbdo, pathRuntime, runtimeStr)) {
    Serial.print("Runtime geüpload: ");
    Serial.println(runtimeStr);
  } else {
    Serial.print("Fout bij uploaden runtime: ");
    Serial.println(fbdo.errorReason());
  }

  // Boot time (eenmalig uploaden)
  if (!bootTimeUploaded && bootTimeStr.length() > 0) {
    String pathBoot = "/boot_time";
    if (Firebase.RTDB.setString(&fbdo, pathBoot, bootTimeStr)) {
      Serial.print("Boot time geüpload: ");
      Serial.println(bootTimeStr);
      bootTimeUploaded = true;
    } else {
      Serial.print("Fout bij uploaden boot time: ");
      Serial.println(fbdo.errorReason());
    }
  }
}


void gpioConfig();

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  gpioConfig();
  connectToWiFi();
  if (WiFi.status() == WL_CONNECTED) {
    initFirebase();
    configTime(0, 0, "pool.ntp.org"); // NTP tijd synchroniseren
    // Wacht tot tijd gesynchroniseerd is
    time_t now = 0;
    while (now < 100000) {
      delay(100);
      now = time(nullptr);
    }
    char bootStr[32];
    strftime(bootStr, sizeof(bootStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
    bootTimeStr = String(bootStr);
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
    
    if (millis() - lastUpdate > updateInterval) {
      digitalWrite(LED_PIN, HIGH);
      updateTimeToFirebase();
      lastUpdate = millis();
      digitalWrite(LED_PIN, LOW);
    } else { //reconnect to wifi
      //connectToWiFi();
    }
  } 
}



void gpioConfig() {
  // Configure GPIO pins here
  pinMode(LED_PIN, OUTPUT);
  for (int i = 0; i < NUM_RELAYS; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
  }
  for (int i = 0; i < NUM_DIP_SWITCHES; i++) {
    pinMode(DIP_SWITCH_PINS[i], INPUT);
  }
  for (int i = 0; i < NUM_DIGITAL_INPUTS; i++) {
    pinMode(DIGITAL_INPUT_PINS[i], INPUT);
  }
  for (int i = 0; i < NUM_ANALOG_INPUTS; i++) {
    pinMode(ANALOG_INPUT_PINS[i], INPUT);
  }
}