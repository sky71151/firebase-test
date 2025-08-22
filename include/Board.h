#ifndef BOARD_H
#define BOARD_H

#include <Arduino.h>

// LED Configuration
#define LED_PIN 46

// Relay Configuration
const int RELAY_PINS[] = {15, 16, 17, 8, 14, 9, 3, 2};  // Fixed duplicate pin 14 -> 15
const int NUM_RELAYS = sizeof(RELAY_PINS) / sizeof(RELAY_PINS[0]);

// DIP Switch Configuration
const int DIP_SWITCH_PINS[] = {1, 48, 21, 18};
const int NUM_DIP_SWITCHES = sizeof(DIP_SWITCH_PINS) / sizeof(DIP_SWITCH_PINS[0]);

// Digital Input Configuration
const int DIGITAL_INPUT_PINS[] = {41, 42, 47, 40, 38, 39};
const int NUM_DIGITAL_INPUTS = sizeof(DIGITAL_INPUT_PINS) / sizeof(DIGITAL_INPUT_PINS[0]);

// Analog Input Configuration
const int ANALOG_INPUT_PINS[] = {4, 5, 6, 7};
#define NUM_ANALOG_INPUTS (sizeof(ANALOG_INPUT_PINS) / sizeof(ANALOG_INPUT_PINS[0]))
#define ANALOG_RESOLUTION_BITS 12     // ESP32-S3 has 12-bit ADC
#define ANALOG_MAX_VALUE 4095         // 2^12 - 1
#define ANALOG_REFERENCE_VOLTAGE 3.3  // ESP32-S3 reference voltage

// SPI Flash Configuration (W25Q128 - 16MB)
#define SPI_FLASH_CS_PIN    10
#define SPI_FLASH_MOSI_PIN  11
#define SPI_FLASH_CLK_PIN   12
#define SPI_FLASH_MISO_PIN  13
#define SPI_FLASH_SIZE_MB   16

// Serial Configuration
#define SERIAL_BAUD_RATE 115200

// Firebase configuratie
// Verkrijg deze waarden van je Firebase project console
#define API_KEY "AIzaSyBoYWaBsPkQ2llH4sqxL1lG7ooHrmRe-GY"
#define DATABASE_URL "https://pool-671d1-default-rtdb.europe-west1.firebasedatabase.app/"
#define USER_EMAIL "vbtechnieken@gmail.com"
#define USER_PASSWORD "qwerty"

// Firebase Project ID (afgeleid van DATABASE_URL)
#define FIREBASE_PROJECT_ID "pool-671d1"

// WiFi Configuration
#define WIFI_SSID           "Van_Baelen"       // Pas aan naar jouw WiFi netwerk
#define WIFI_PASSWORD       "Rob88333"   // Pas aan naar jouw WiFi wachtwoord
#define WIFI_TIMEOUT_MS     30000                // 30 seconden timeout voor verbinding

#endif // BOARD_H