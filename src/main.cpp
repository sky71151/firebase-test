#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "Board.h"
#include "version.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ===================== VARIABELEN =====================
typedef struct
{
  const char *name;
  TaskHandle_t *handle;
  uint32_t stackWords;
} TaskStackInfo;

#define WIFI_STACK 4096
#define FIREBASE_STACK 8192
#define STATUS_STACK 8192
#define UPDATE_STACK 8192

TaskHandle_t wifiHandle = nullptr;
TaskHandle_t firebaseHandle = nullptr;
TaskHandle_t statusHandle = nullptr;
TaskHandle_t updateHandle = nullptr;

TaskStackInfo taskStackInfos[] = {
    {"WiFiTask", &wifiHandle, WIFI_STACK},
    {"FirebaseTask", &firebaseHandle, FIREBASE_STACK},
    {"StatusTask", &statusHandle, STATUS_STACK},
    {"UpdateTask", &updateHandle, UPDATE_STACK},
};
const int numTasks = sizeof(taskStackInfos) / sizeof(taskStackInfos[0]);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

SemaphoreHandle_t serialMutex;

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 60000; // 1 minuut
String bootTimeStr = "";

bool bootTimeUploaded = false;
bool firebaseInitialized = false;
String deviceId;

// Forward declarations
void safePrint(const String &msg);
void safePrintln(const String &msg);
void gpioConfig();
String getUniqueClientId();
void connectToWiFiTask(void *pvParameters);
void initFirebaseTask(void *pvParameters);
void systemStatusTask(void *pvParameters);
void updateTimeToFirebaseTask(void *pvParameters);

// ===================== SETUP & LOOP =====================

void setup()
{
  Serial.begin(SERIAL_BAUD_RATE);
  delay(2000);
  serialMutex = xSemaphoreCreateMutex();
  gpioConfig();
  deviceId = getUniqueClientId(); // Unieke ID (voorbeeld)
  safePrintln("Apparaat gestart, unieke ID: " + deviceId);
  xTaskCreatePinnedToCore(connectToWiFiTask, "WiFiTask", WIFI_STACK, NULL, 1, &wifiHandle, 0);
  // xTaskCreatePinnedToCore(systemStatusTask, "StatusTask", STATUS_STACK, NULL, 1, &statusHandle, 1);
  while (WiFi.status() != WL_CONNECTED)
  {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  // Belgische tijdzone: CET/CEST (winter/zomertijd automatisch)
  // TZ string: "CET-1CEST,M3.5.0,M10.5.0/3"

  configTime(3600, 3600, "pool.ntp.org");
  time_t now = 0;
  while (now < 100000)
  {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    now = time(nullptr);
  }
  char bootStr[32];
  strftime(bootStr, sizeof(bootStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
  bootTimeStr = String(bootStr);
  safePrintln("Huidige tijd: " + bootTimeStr);

  xTaskCreatePinnedToCore(initFirebaseTask, "FirebaseTask", FIREBASE_STACK, NULL, 1, &firebaseHandle, 1);
  xTaskCreatePinnedToCore(updateTimeToFirebaseTask, "UpdateTask", UPDATE_STACK, NULL, 1, &updateHandle, 1);
}

void loop()
{
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

// ===================== FREERTOS TAKEN (alfabetisch) =====================

void connectToWiFiTask(void *pvParameters)
{
  while (true)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      safePrint("(Re)connect WiFi: ");
      safePrintln(WIFI_SSID);
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      unsigned long startAttemptTime = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS)
      {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        safePrint(".");
      }
      if (WiFi.status() == WL_CONNECTED)
      {
        safePrintln("\nWiFi verbonden!");
        safePrint("IP adres: ");
        safePrintln(WiFi.localIP().toString());
      }
      else
      {
        safePrintln("\nWiFi verbinding mislukt!");
      }
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void initFirebaseTask(void *pvParameters)
{
  String regPath = String("/devices/");
  regPath += deviceId;
  for (;;)
  {
    if (WiFi.status() == WL_CONNECTED && !Firebase.ready())
    {
      // Firebase.reset(&config);
      config.api_key = API_KEY;
      config.database_url = DATABASE_URL;
      // Anonieme login: geen e-mail/wachtwoord invullen
      // auth.user.email en auth.user.password NIET instellen
      auth.user.email = USER_EMAIL;
      auth.user.password = USER_PASSWORD;
      Firebase.begin(&config, &auth); // auth leeg laat anonieme login toe
      Firebase.reconnectWiFi(true);
      safePrintln("Firebase opnieuw geïnitialiseerd (anoniem)");
      firebaseInitialized = false; // reset status bij herinitialisatie
    }
    if (WiFi.status() == WL_CONNECTED && Firebase.ready() && !firebaseInitialized)
    {
      // Voorbeeld: controleer of device geregistreerd is met unieke ID
      if (Firebase.RTDB.pathExisted(&fbdo, regPath.c_str()))
      {
        {
          String msg = "Pad bestaat: ";
          msg.concat(regPath);
          safePrintln(msg);
        }
        time_t now = time(nullptr);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
        String pathTime = "devices/";
        pathTime.concat(deviceId);
        pathTime.concat("/Registration/lastBoot");
        if (Firebase.RTDB.setString(&fbdo, pathTime, timeStr))
        {
          safePrint("Boot Time update: ");
          safePrintln(timeStr);
        }
        else
        {
          safePrint("Fout bij uploaden boot tijd: ");
          safePrintln(fbdo.errorReason());
        }
        String pathFirmware = "devices/";
        pathFirmware.concat(deviceId);
        pathFirmware.concat("/DeviceInfo/firmware");
        if (Firebase.RTDB.setString(&fbdo, pathFirmware.c_str(), FIRMWARE_VERSION))
        {
          safePrint("Firmware version update: ");
          safePrintln(FIRMWARE_VERSION);
        }
        else
        {
          safePrint("Fout bij uploaden firmware versie: ");
          safePrintln(fbdo.errorReason());
        }
        firebaseInitialized = true;
      }
      else
      {
        {
          String msg = "Pad bestaat niet: ";
          msg.concat(regPath);
          safePrintln(msg);
        }
        {
          String msg = "Device wordt geregistreerd: ";
          msg.concat(deviceId);
          safePrintln(msg);
        }
        // Create device data JSON

        // Create device data JSON
        FirebaseJson deviceJson;

        // Device info section
        FirebaseJson DeviceInfo;
        DeviceInfo.set("clientId", deviceId);
        DeviceInfo.set("boardType", "ESP32-S3  R1N16");
        DeviceInfo.set("firmware", FIRMWARE_VERSION);
        DeviceInfo.set("freeHeap", ESP.getFreeHeap());
        deviceJson.set("DeviceInfo", DeviceInfo);

        // Registration section
        FirebaseJson Registration;
        Registration.set("firstRegistratione", bootTimeStr);
        Registration.set("lastBoot", bootTimeStr);
        Registration.set("lastSeen", bootTimeStr);
        Registration.set("uptime", "0");
        deviceJson.set("Registration", Registration);

        if (Firebase.RTDB.setJSON(&fbdo, regPath.c_str(), &deviceJson))
        {
          safePrintln("Device geregistreerd in Firebase Realtime Database.");
          firebaseInitialized = true;
        }
        else
        {
          safePrint("Fout bij registreren device: ");
          safePrintln(fbdo.errorReason());
        }
      }

      // firebaseInitialized = true;
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void systemStatusTask(void *pvParameters)
{
  while (true)
  {
    uint32_t freeHeap = ESP.getFreeHeap();
    UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    String wifiStatus = (WiFi.status() == WL_CONNECTED) ? "Verbonden" : "Niet verbonden";
    String firebaseStatus = Firebase.ready() ? "Verbonden" : "Niet verbonden";
    safePrint("[STATUS] Heap: ");
    safePrint(String(freeHeap));
    safePrint(" bytes | Stack: ");
    safePrint(String(stackHighWaterMark * sizeof(StackType_t)));
    safePrint(" bytes | WiFi: ");
    safePrint(wifiStatus);
    safePrint(" | Firebase: ");
    safePrintln(firebaseStatus);
    safePrintln("[TASKS] Naam | Stack gebruikt (bytes) | Stack totaal (bytes) | % gebruikt");
    for (int i = 0; i < numTasks; i++)
    {
      TaskStackInfo &info = taskStackInfos[i];
      if (*(info.handle))
      {
        UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(*(info.handle));
        uint32_t stackTotalBytes = info.stackWords * sizeof(StackType_t);
        uint32_t stackFreeBytes = highWaterMark * sizeof(StackType_t);
        uint32_t stackUsedBytes = stackTotalBytes - stackFreeBytes;
        int percentUsed = (stackUsedBytes * 100) / stackTotalBytes;
        safePrint("  ");
        safePrint(info.name);
        safePrint(" | ");
        safePrint(String(stackUsedBytes));
        safePrint("/");
        safePrint(String(stackTotalBytes));
        safePrint(" = ");
        safePrint(String(percentUsed));
        safePrint("%");
        safePrintln(" used");
      }
    }
    vTaskDelay(30000 / portTICK_PERIOD_MS);
  }
}

void updateTimeToFirebaseTask(void *pvParameters)
{
  while (true)
  {
    if (WiFi.status() == WL_CONNECTED && Firebase.ready() && firebaseInitialized)
    {
      time_t now = time(nullptr);
      char timeStr[32];
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
      String pathTime = "devices/";
      pathTime.concat(deviceId);
      pathTime.concat("/Registration/lastSeen");
      if (Firebase.RTDB.setString(&fbdo, pathTime, timeStr))
      {
        safePrint("Tijd geüpload: ");
        safePrintln(timeStr);
      }
      else
      {
        safePrint("Fout bij uploaden tijd: ");
        safePrintln(fbdo.errorReason());
      }
      unsigned long runtimeMillis = millis();
      unsigned long totalMinutes = runtimeMillis / 60000;
      unsigned int hours = totalMinutes / 60;
      unsigned int minutes = totalMinutes % 60;
      char runtimeStr[16];
      snprintf(runtimeStr, sizeof(runtimeStr), "%02u:%02u", hours, minutes);
      String pathRuntime = "devices/";
      pathRuntime.concat(deviceId);
      pathRuntime.concat("/Registration/uptime");
      if (Firebase.RTDB.setString(&fbdo, pathRuntime, runtimeStr))
      {
        safePrint("Runtime geüpload: ");
        safePrintln(runtimeStr);
      }
      else
      {
        safePrint("Fout bij uploaden runtime: ");
        safePrintln(fbdo.errorReason());
      }
    }
    vTaskDelay(updateInterval / portTICK_PERIOD_MS);
  }
}

// ===================== OVERIGE FUNCTIES =====================

void safePrint(const String &msg)
{
  if (serialMutex)
  {
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE)
    {
      Serial.print(msg);
      xSemaphoreGive(serialMutex);
    }
  }
  else
  {
    Serial.print(msg);
  }
}

void safePrintln(const String &msg)
{
  if (serialMutex)
  {
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE)
    {
      Serial.println(msg);
      xSemaphoreGive(serialMutex);
    }
  }
  else
  {
    Serial.println(msg);
  }
}

void gpioConfig()
{
  pinMode(LED_PIN, OUTPUT);
  for (int i = 0; i < NUM_RELAYS; i++)
  {
    pinMode(RELAY_PINS[i], OUTPUT);
  }
  for (int i = 0; i < NUM_DIP_SWITCHES; i++)
  {
    pinMode(DIP_SWITCH_PINS[i], INPUT);
  }
  for (int i = 0; i < NUM_DIGITAL_INPUTS; i++)
  {
    pinMode(DIGITAL_INPUT_PINS[i], INPUT);
  }
  for (int i = 0; i < NUM_ANALOG_INPUTS; i++)
  {
    pinMode(ANALOG_INPUT_PINS[i], INPUT);
  }
}

String getUniqueClientId()
{
  uint64_t mac = ESP.getEfuseMac();
  char macStr[13];
  snprintf(macStr, sizeof(macStr), "%012llX", mac);
  // return String("ESP32S3_") + String(macStr);
  return String(macStr);
}

String HuidigeTijd()
{
  time_t now = time(nullptr);
  char timeStr[32];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
  return String(timeStr);
}
