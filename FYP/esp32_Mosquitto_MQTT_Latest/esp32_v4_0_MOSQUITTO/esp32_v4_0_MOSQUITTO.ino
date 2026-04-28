/*
 * ============================================================================
 * ESP32 DUAL MONITORING SYSTEM - Self-Hosted Mosquitto Edition v4.0
 * Door Access Control + Temperature Management + MQTT Broker
 * ============================================================================
 * 
 * Version: 4.0 - Migrated from Adafruit IO to Self-Hosted Mosquitto
 * Date: February 25, 2026
 * 
 * MIGRATION CHANGES (Adafruit IO → Self-Hosted Mosquitto):
 * - ✅ Library: Adafruit_MQTT → PubSubClient (industry standard)
 * - ✅ Broker: io.adafruit.com → 192.168.100.2 (local Mosquitto)
 * - ✅ Topics: Rayk90/feeds/X → panel/1/X (hierarchical structure)
 * - ✅ Auth: AIO_KEY → username/password (Mosquitto Level 2)
 * - ✅ Publishing: feed.publish() → client.publish()
 * - ✅ Connection: Adafruit-specific → standard MQTT
 * - ✅ Added: Callback function for future two-way communication
 * 
 * ALL PREVIOUS FEATURES RETAINED:
 * - ✅ Boot-up door state detection
 * - ✅ Post-reset door state detection
 * - ✅ Immediate publishing on state changes
 * - ✅ Hysteresis-based fan control
 * - ✅ LCD display with priority system
 * - ✅ Alarm flash, keypad entry, reset button
 * 
 * ============================================================================
 */

// ============================================================================
// LIBRARIES
// ============================================================================
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Keypad.h>
#include <WiFi.h>
#include <PubSubClient.h>       // ← CHANGED: Standard MQTT library

// Include credentials from separate file (credentials.h)
#include "credentials.h"

// ============================================================================
// PIN DEFINITIONS - EXPANSION BOARD
// ============================================================================
// DHT11 Temperature Sensor
#define DHT_PIN           2     // GPIO 2

// Outputs (via transistors)
#define MOTOR_PIN         23    // Expansion board pin labeled "23"
#define BUZZER_PIN        19    // Expansion board pin labeled "19"

// Inputs
#define REED_SWITCH_PIN   17    // GPIO 17 (door sensor)
#define RESET_BUTTON_PIN  18    // GPIO 18 (reset button)

// I2C
#define I2C_SDA           21    // Auto-connected to LCD via expansion board
#define I2C_SCL           22    // Auto-connected to LCD via expansion board

// Keypad Configuration (4x4)
const byte ROWS = 4;
const byte COLS = 4;
byte rowPins[ROWS] = {15, 12, 14, 27};  // GPIO 15, 12, 14, 27
byte colPins[COLS] = {32, 33, 4, 13};   // GPIO 32, 33, 4, 13

// ============================================================================
// CONSTANTS
// ============================================================================
// LCD Configuration
#define LCD_ADDRESS       0x27  // Common I2C address (try 0x3F if doesn't work)
#define LCD_COLS          16
#define LCD_ROWS          2

// Temperature Thresholds (Celsius)
#define TEMP_FAN_ON       45.0  // Fan turns ON
#define TEMP_FAN_OFF      38.0  // Fan turns OFF (hysteresis)
#define TEMP_CRITICAL     75.0  // Critical alarm threshold

// Door Access Settings
#define DOOR_TIMEOUT      60    // seconds - countdown timer
#define MAX_PASSWORD_LEN  8     // maximum password length
#define MAX_ATTEMPTS      3     // wrong password attempts before alarm
#define PASSWORD          "1234" // Default password (change as needed)

// Reset Button Timing
#define RESET_HOLD_TIME   3000  // 3 seconds for full reset
#define DOUBLE_PRESS_TIME 1000  // 1 second window for double press
#define ACCESS_GRANTED_DISPLAY_TIME 2500  // 2.5 seconds to show "ACCESS GRANTED"

// Display Timing
#define ALARM_FLASH_TIME  1000  // Alarm flash interval (ms)
#define TEMP_READ_INTERVAL 2000 // Read temperature every 2 seconds

// WiFi and MQTT Timing
#define WIFI_RECONNECT_INTERVAL 30000   // Try reconnect every 30 seconds
#define MQTT_PUBLISH_INTERVAL   10000   // Publish temp/humidity every 10 seconds

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
DHT dht(DHT_PIN, DHT11);

// Keypad layout
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ============================================================================
// MQTT CLIENT SETUP - CHANGED FROM ADAFRUIT
// ============================================================================
WiFiClient espClient;                          // ← CHANGED: renamed for clarity
PubSubClient mqttClient(espClient);            // ← CHANGED: PubSubClient instead of Adafruit_MQTT_Client

// NOTE: No more feed objects! Topics are defined as strings in credentials.h
// Old: Adafruit_MQTT_Publish feed_temperature = ...
// New: Just use client.publish(TOPIC_TEMPERATURE, value)

// ============================================================================
// DOOR ACCESS STATE MACHINE
// ============================================================================
enum DoorState {
  DOOR_SECURE,        // Door closed, system armed
  ENTRY_PENDING,      // Door open, waiting for password
  ACCESS_GRANTED,     // Correct password entered
  TIMEOUT_ALARM,      // 60 seconds elapsed
  SECURITY_ALARM      // 3 wrong passwords
};

DoorState doorState = DOOR_SECURE;
DoorState lastDoorState = DOOR_SECURE;  // For change detection
unsigned long doorTimer = 0;
unsigned long accessGrantedTime = 0;
int passwordAttempts = 0;
String enteredPassword = "";
bool doorAlarmActive = false;
bool showingAccessGranted = false;

// ============================================================================
// TEMPERATURE STATE MACHINE
// ============================================================================
enum TempState {
  TEMP_STATE_NORMAL,           // < 45°C
  TEMP_STATE_COOLING,          // ≥ 45°C, fan ON
  TEMP_STATE_CRITICAL,         // ≥ 75°C, alarm ON (unacknowledged)
  TEMP_STATE_ACKNOWLEDGED      // ≥ 75°C but acknowledged (maintenance mode)
};

TempState tempState = TEMP_STATE_NORMAL;
TempState lastTempState = TEMP_STATE_NORMAL;  // For change detection
bool tempAlarmActive = false;
bool tempAcknowledged = false;
float currentTemp = 0;
float lastPublishedTemp = -999;  // Track last published value
float currentHumidity = 0;
float lastPublishedHumidity = -999;  // Track last published value
unsigned long lastTempRead = 0;

// ============================================================================
// RESET BUTTON STATE
// ============================================================================
bool resetButtonPressed = false;
unsigned long resetButtonPressTime = 0;
unsigned long lastResetRelease = 0;
int resetPressCount = 0;
bool resetHoldDetected = false;

// ============================================================================
// DISPLAY MANAGEMENT
// ============================================================================
unsigned long lastAlarmFlash = 0;
bool alarmFlashState = false;

// LCD optimization: Track last displayed content
String lastLCDLine1 = "";
String lastLCDLine2 = "";
bool forceDisplayUpdate = false;  // Force update when needed

// ============================================================================
// WiFi AND MQTT STATE
// ============================================================================
bool wifiConnected = false;
bool mqttConnected = false;
unsigned long lastWiFiCheck = 0;
unsigned long lastMQTTPublish = 0;
bool fanStatus = false;
bool lastFanStatus = false;  // For change detection
String alarmStatusText = "NORMAL";
String lastAlarmStatusText = "NORMAL";  // For change detection

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
void connectWiFi();
void connectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishSensorData();
void publishStatusChange();
void checkWiFiConnection();

// ============================================================================
// HELPER FUNCTION: Publish string to MQTT topic
// ============================================================================
// This replaces the Adafruit feed.publish() calls with a simpler wrapper
bool mqttPublish(const char* topic, const char* payload) {
  if (!mqttConnected) return false;
  return mqttClient.publish(topic, payload);
}

// Helper overload for float values (temperature, humidity)
bool mqttPublish(const char* topic, float value) {
  if (!mqttConnected) return false;
  char buffer[10];
  dtostrf(value, 4, 1, buffer);  // Convert float to string (width 4, 1 decimal)
  return mqttClient.publish(topic, buffer);
}

// ============================================================================
// MQTT CALLBACK FUNCTION (NEW - For future two-way communication)
// ============================================================================
// This is called automatically when a message arrives on a subscribed topic
// Like a doorbell - when a message arrives, this function "rings"
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("[MQTT] Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(" → ");
  Serial.println(message);
  
  // Handle control messages from backend (future feature)
  // Example: Backend sends "ON" to panel/1/control/fan
  if (String(topic) == TOPIC_CONTROL_FAN) {
    if (message == "ON") {
      digitalWrite(MOTOR_PIN, HIGH);
      Serial.println("[MQTT] Remote command: Fan ON");
    } else if (message == "OFF") {
      digitalWrite(MOTOR_PIN, LOW);
      Serial.println("[MQTT] Remote command: Fan OFF");
    }
  }
  
  if (String(topic) == TOPIC_CONTROL_RESET) {
    if (message == "RESET") {
      Serial.println("[MQTT] Remote command: System Reset");
      performFullSystemReset();
    }
  }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println(F("\n\n"));
  Serial.println(F("========================================"));
  Serial.println(F("  ESP32 Door Access + Temp Monitor"));
  Serial.println(F("  Self-Hosted Mosquitto Edition v4.0"));
  Serial.println(F("  System Initialization..."));
  Serial.println(F("========================================"));
  
  // Initialize I2C for LCD
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println(F("[*] I2C initialized (SDA: GPIO21, SCL: GPIO22)"));
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("  System Init   "));
  lcd.setCursor(0, 1);
  lcd.print(F(" Connecting WiFi"));
  
  Serial.println(F("[OK] LCD initialized"));
  
  // Initialize DHT11
  dht.begin();
  Serial.println(F("[OK] DHT11 initialized"));
  
  // Configure GPIO pins
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(REED_SWITCH_PIN, INPUT_PULLUP);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  
  // Ensure outputs start LOW
  digitalWrite(MOTOR_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.println("[OK] GPIO pins configured");
  
  // Keypad columns pull-up configuration
  pinMode(colPins[0], INPUT_PULLUP);
  pinMode(colPins[1], INPUT_PULLUP);
  pinMode(colPins[2], INPUT_PULLUP);
  pinMode(colPins[3], INPUT_PULLUP);
  
  Serial.println("[OK] Keypad initialized");
  
  // ========================================
  // MQTT CLIENT CONFIGURATION - NEW
  // ========================================
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);  // ← Point to your Mosquitto broker
  mqttClient.setCallback(mqttCallback);           // ← Set callback for incoming messages
  mqttClient.setKeepAlive(300);                   // ← 5 min keepalive (good for WiFi)
  
  // Connect to WiFi
  Serial.println("\n[*] Connecting to WiFi...");
  Serial.print("    SSID: ");
  Serial.println(WIFI_SSID);
  connectWiFi();
  
  // Connect to Mosquitto MQTT Broker
  if (wifiConnected) {
    Serial.println("[*] Connecting to Mosquitto Broker...");
    connectMQTT();
    
    // Check actual door state on startup and publish correct initial states
    if (mqttConnected) {
      Serial.println("\n[*] Detecting initial system state...");
      
      // Check if door is actually open or closed
      bool doorIsOpen = (digitalRead(REED_SWITCH_PIN) == HIGH);
      
      if (doorIsOpen) {
        doorState = ENTRY_PENDING;
        doorTimer = millis();
        Serial.println("    [!] Door detected: OPEN - Starting countdown");
        mqttPublish(TOPIC_DOOR_STATUS, "OPEN");
        Serial.println("    [OK] Door Status: OPEN");
      } else {
        doorState = DOOR_SECURE;
        Serial.println("    [OK] Door detected: CLOSED - System secure");
        mqttPublish(TOPIC_DOOR_STATUS, "SECURE");
        Serial.println("    [OK] Door Status: SECURE");
      }
      
      mqttPublish(TOPIC_ALARM_STATUS, "NORMAL");
      Serial.println("    [OK] Alarm Status: NORMAL");
      
      mqttPublish(TOPIC_FAN_STATUS, "OFF");
      Serial.println("    [OK] Fan Status: OFF");
      
      Serial.println("[OK] Initial states published!\n");
    }
  }
  
  delay(2000);
  
  // Display ready message
  lcd.clear();
  lcd.setCursor(0, 0);
  if (wifiConnected && mqttConnected) {
    lcd.print("WiFi: CONNECTED");
    lcd.setCursor(0, 1);
    lcd.print("MQTT: READY!");
  } else if (wifiConnected) {
    lcd.print("WiFi: CONNECTED");
    lcd.setCursor(0, 1);
    lcd.print("MQTT: Offline");
  } else {
    lcd.print("WiFi: OFFLINE");
    lcd.setCursor(0, 1);
    lcd.print("Local Mode Only");
  }
  
  Serial.println("\n========================================");
  Serial.println("          SYSTEM READY!");
  Serial.println("========================================");
  Serial.println("\nConfiguration:");
  Serial.println("  Password: " + String(PASSWORD));
  Serial.println("  Fan ON: " + String(TEMP_FAN_ON) + " C");
  Serial.println("  Fan OFF: " + String(TEMP_FAN_OFF) + " C");
  Serial.println("  Critical: " + String(TEMP_CRITICAL) + " C");
  Serial.println("  Broker: " + String(MQTT_SERVER) + ":" + String(MQTT_PORT));
  
  if (mqttConnected) {
    Serial.println("\n  Topics:");
    Serial.println("   -> " + String(TOPIC_TEMPERATURE));
    Serial.println("   -> " + String(TOPIC_HUMIDITY));
    Serial.println("   -> " + String(TOPIC_DOOR_STATUS));
    Serial.println("   -> " + String(TOPIC_FAN_STATUS));
    Serial.println("   -> " + String(TOPIC_ALARM_STATUS));
    Serial.println("   <- " + String(TOPIC_CONTROL_FAN) + " (subscribe)");
    Serial.println("   <- " + String(TOPIC_CONTROL_RESET) + " (subscribe)");
  }
  
  Serial.println("\n--- Monitoring Started ---\n");
  
  delay(2000);
  updateDisplay();
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  // Check WiFi connection periodically
  checkWiFiConnection();
  
  // ========================================
  // MQTT CONNECTION MAINTENANCE - CHANGED
  // ========================================
  if (mqttConnected) {
    // PubSubClient needs client.loop() called regularly
    // This processes incoming messages and maintains the connection
    if (!mqttClient.loop()) {
      // loop() returns false if disconnected
      Serial.println("[!] MQTT connection lost - reconnecting...");
      mqttConnected = false;
      connectMQTT();
    }
  }
  
  // Read sensors
  readTemperature();
  
  // Check door sensor
  checkDoorSensor();
  
  // Check keypad
  checkKeypad();
  
  // Check reset button
  checkResetButton();
  
  // Update state machines
  updateDoorStateMachine();
  updateTempStateMachine();
  
  // Publish data to Mosquitto Broker
  if (mqttConnected) {
    publishSensorData();      // Periodic temp/humidity
    publishStatusChange();    // Immediate status updates
  }
  
  // Update display
  updateDisplay();
  
  // Small delay
  delay(50);
}

// ============================================================================
// WiFi CONNECTION (UNCHANGED)
// ============================================================================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n[OK] WiFi Connected!");
    Serial.print("    IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("    Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    wifiConnected = false;
    Serial.println("\n[FAIL] WiFi Connection Failed");
    Serial.println("System will run in LOCAL MODE only");
  }
}

// ============================================================================
// MQTT CONNECTION TO MOSQUITTO BROKER - CHANGED
// ============================================================================
void connectMQTT() {
  if (!wifiConnected) {
    mqttConnected = false;
    return;
  }
  
  // Already connected?
  if (mqttClient.connected()) {
    mqttConnected = true;
    return;
  }
  
  Serial.print("[*] Connecting to Mosquitto Broker... ");
  
  uint8_t retries = 3;
  while (retries > 0) {
    // ========================================
    // KEY CHANGE: PubSubClient connect() method
    // ========================================
    // Parameters: clientID, username, password
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
      mqttConnected = true;
      Serial.println("Connected!");
      Serial.println("    Broker: " + String(MQTT_SERVER));
      Serial.println("    Client ID: " + String(MQTT_CLIENT_ID));
      Serial.println("    User: " + String(MQTT_USER));
      
      // ========================================
      // NEW: Subscribe to control topics
      // ========================================
      // This enables two-way communication (backend → ESP32)
      mqttClient.subscribe(TOPIC_CONTROL_FAN);
      mqttClient.subscribe(TOPIC_CONTROL_RESET);
      Serial.println("    Subscribed to control topics");
      
      return;
    } else {
      Serial.print("Failed (rc=");
      Serial.print(mqttClient.state());
      Serial.println(") - Retrying in 5 seconds...");
      delay(5000);
      retries--;
    }
  }
  
  Serial.println("[FAIL] MQTT Connection Failed after 3 attempts");
  mqttConnected = false;
}

// ============================================================================
// CHECK WiFi CONNECTION (UNCHANGED LOGIC)
// ============================================================================
void checkWiFiConnection() {
  if (millis() - lastWiFiCheck >= WIFI_RECONNECT_INTERVAL) {
    lastWiFiCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED && wifiConnected) {
      Serial.println("[!] WiFi connection lost - reconnecting...");
      wifiConnected = false;
      mqttConnected = false;
      connectWiFi();
      if (wifiConnected) {
        connectMQTT();
      }
    } else if (WiFi.status() == WL_CONNECTED && !wifiConnected) {
      wifiConnected = true;
      connectMQTT();
    }
    
    if (wifiConnected && !mqttConnected) {
      connectMQTT();
    }
  }
}

// ============================================================================
// PUBLISH SENSOR DATA (Periodic - Every 10 seconds) - CHANGED PUBLISH METHOD
// ============================================================================
void publishSensorData() {
  if (!mqttConnected) return;
  
  if (millis() - lastMQTTPublish >= MQTT_PUBLISH_INTERVAL) {
    lastMQTTPublish = millis();
    
    // ========================================
    // CHANGED: mqttPublish() instead of feed.publish()
    // ========================================
    if (mqttPublish(TOPIC_TEMPERATURE, currentTemp)) {
      Serial.print("[PUB] Temperature: ");
      Serial.print(currentTemp);
      Serial.println(" C");
      lastPublishedTemp = currentTemp;
    } else {
      Serial.println("[FAIL] Failed to publish temperature");
    }
    
    if (mqttPublish(TOPIC_HUMIDITY, currentHumidity)) {
      Serial.print("[PUB] Humidity: ");
      Serial.print(currentHumidity);
      Serial.println(" %");
      lastPublishedHumidity = currentHumidity;
    } else {
      Serial.println("[FAIL] Failed to publish humidity");
    }
    
    Serial.println("[OK] Sensor data synced to broker");
  }
}

// ============================================================================
// PUBLISH STATUS CHANGES (Immediate - When status changes) - CHANGED
// ============================================================================
void publishStatusChange() {
  if (!mqttConnected) return;
  
  // Publish door status change
  if (doorState != lastDoorState) {
    String doorStatus;
    switch (doorState) {
      case DOOR_SECURE:
        doorStatus = "SECURE";
        break;
      case ENTRY_PENDING:
        doorStatus = "OPEN";
        break;
      case ACCESS_GRANTED:
        doorStatus = "GRANTED";
        break;
      case TIMEOUT_ALARM:
        doorStatus = "TIMEOUT";
        break;
      case SECURITY_ALARM:
        doorStatus = "WRONG_CODE";
        break;
    }
    
    if (mqttPublish(TOPIC_DOOR_STATUS, doorStatus.c_str())) {
      Serial.print("[PUB] Door Status: ");
      Serial.println(doorStatus);
    }
    lastDoorState = doorState;
  }
  
  // Publish fan status change
  fanStatus = (digitalRead(MOTOR_PIN) == HIGH);
  if (fanStatus != lastFanStatus) {
    String fanStatusText = fanStatus ? "ON" : "OFF";
    
    if (mqttPublish(TOPIC_FAN_STATUS, fanStatusText.c_str())) {
      Serial.print("[PUB] Fan Status: ");
      Serial.println(fanStatusText);
    }
    lastFanStatus = fanStatus;
  }
  
  // Publish alarm status change
  String currentAlarmStatus = "NORMAL";
  if (tempAlarmActive && tempState == TEMP_STATE_CRITICAL) {
    currentAlarmStatus = "CRITICAL_TEMP";
  } else if (doorState == TIMEOUT_ALARM) {
    currentAlarmStatus = "TIMEOUT";
  } else if (doorState == SECURITY_ALARM) {
    currentAlarmStatus = "WRONG_CODE";
  } else if (tempState == TEMP_STATE_ACKNOWLEDGED) {
    currentAlarmStatus = "TEMP_ACKED";
  }
  
  if (currentAlarmStatus != lastAlarmStatusText) {
    if (mqttPublish(TOPIC_ALARM_STATUS, currentAlarmStatus.c_str())) {
      Serial.print("[PUB] Alarm Status: ");
      Serial.println(currentAlarmStatus);
    }
    lastAlarmStatusText = currentAlarmStatus;
  }
}

// ============================================================================
// READ TEMPERATURE SENSOR (UNCHANGED)
// ============================================================================
void readTemperature() {
  if (millis() - lastTempRead >= TEMP_READ_INTERVAL) {
    lastTempRead = millis();
    
    float temp = dht.readTemperature();
    float humidity = dht.readHumidity();
    
    if (!isnan(temp) && !isnan(humidity)) {
      currentTemp = temp;
      currentHumidity = humidity;
      
      static float lastPrintedTemp = -999;
      static float lastPrintedHum = -999;
      
      if (abs(currentTemp - lastPrintedTemp) >= 0.5 || abs(currentHumidity - lastPrintedHum) >= 2) {
        Serial.print(F("Temp: "));
        Serial.print(currentTemp);
        Serial.print(F(" C, Humidity: "));
        Serial.print(currentHumidity);
        Serial.println(F(" %"));
        
        lastPrintedTemp = currentTemp;
        lastPrintedHum = currentHumidity;
      }
    }
  }
}

// ============================================================================
// CHECK DOOR SENSOR (Reed Switch on GPIO 17) - CHANGED PUBLISH CALLS
// ============================================================================
void checkDoorSensor() {
  static bool lastReedState = LOW;
  bool currentReedState = digitalRead(REED_SWITCH_PIN);
  
  static DoorState lastPrintedState = DOOR_SECURE;
  static bool lastPrintedReed = LOW;
  
  bool shouldPrint = (currentReedState != lastPrintedReed) || (doorState != lastPrintedState);
  
  if (shouldPrint) {
    Serial.print(F("Reed Switch: "));
    Serial.print(currentReedState == HIGH ? F("OPEN (HIGH)") : F("CLOSED (LOW)"));
    Serial.print(F(" | Door State: "));
    
    switch (doorState) {
      case DOOR_SECURE:     Serial.println(F("SECURE")); break;
      case ENTRY_PENDING:   Serial.println(F("ENTRY_PENDING")); break;
      case ACCESS_GRANTED:  Serial.println(F("ACCESS_GRANTED")); break;
      case TIMEOUT_ALARM:   Serial.println(F("TIMEOUT_ALARM")); break;
      case SECURITY_ALARM:  Serial.println(F("SECURITY_ALARM")); break;
    }
    
    lastPrintedState = doorState;
    lastPrintedReed = currentReedState;
  }
  
  switch (doorState) {
    case DOOR_SECURE:
      if (currentReedState == HIGH && lastReedState == LOW) {
        Serial.println(F("[!] Door opened - Starting timer"));
        doorState = ENTRY_PENDING;
        doorTimer = millis();
        passwordAttempts = 0;
        enteredPassword = "";
        
        if (mqttConnected) {
          mqttPublish(TOPIC_DOOR_STATUS, "OPEN");
          Serial.println(F("[PUB] Door Status = OPEN"));
        }
      }
      break;
      
    case ENTRY_PENDING:
      break;
      
    case ACCESS_GRANTED:
      if (currentReedState == LOW && lastReedState == HIGH) {
        Serial.println(F("[OK] Door closed - System secure"));
        doorState = DOOR_SECURE;
        
        if (mqttConnected) {
          mqttPublish(TOPIC_DOOR_STATUS, "SECURE");
          Serial.println(F("[PUB] Door Status = SECURE"));
          mqttPublish(TOPIC_ALARM_STATUS, "NORMAL");
          Serial.println(F("[PUB] Alarm Status = NORMAL"));
        }
      }
      break;
      
    case TIMEOUT_ALARM:
      break;
      
    case SECURITY_ALARM:
      break;
  }
  
  lastReedState = currentReedState;
}

// ============================================================================
// CHECK KEYPAD - CHANGED PUBLISH CALLS
// ============================================================================
void checkKeypad() {
  char key = keypad.getKey();
  if (key && doorState == ENTRY_PENDING) {
    Serial.print(F("Key: "));
    Serial.println(key);
    
    if (key == '#') {
      Serial.print(F("Checking password: "));
      Serial.println(enteredPassword);
      
      if (enteredPassword == PASSWORD) {
        Serial.println(F("*** ACCESS GRANTED ***"));
        doorState = ACCESS_GRANTED;
        accessGrantedTime = millis();
        showingAccessGranted = true;
        forceDisplayUpdate = true;
        
        if (mqttConnected) {
          mqttPublish(TOPIC_DOOR_STATUS, "GRANTED");
          Serial.println(F("[PUB] Door Status = GRANTED"));
          mqttPublish(TOPIC_ALARM_STATUS, "NORMAL");
          Serial.println(F("[PUB] Alarm Status = NORMAL"));
        }
      } else {
        Serial.println(F("Wrong password"));
        passwordAttempts++;
        if (passwordAttempts >= MAX_ATTEMPTS) {
          Serial.println(F("*** SECURITY ALARM ***"));
          doorState = SECURITY_ALARM;
          doorAlarmActive = true;
          digitalWrite(BUZZER_PIN, HIGH);
          forceDisplayUpdate = true;
          
          if (mqttConnected) {
            mqttPublish(TOPIC_DOOR_STATUS, "WRONG_CODE");
            Serial.println(F("[PUB] Door Status = WRONG_CODE"));
            mqttPublish(TOPIC_ALARM_STATUS, "WRONG_CODE");
            Serial.println(F("[PUB] Alarm Status = WRONG_CODE"));
          }
        }
      }
      enteredPassword = "";
    }
    else if (key == '*') {
      enteredPassword = "";
      Serial.println(F("Password cleared"));
    }
    else if (enteredPassword.length() < MAX_PASSWORD_LEN) {
      enteredPassword += key;
    }
  }
}

// ============================================================================
// CHECK RESET BUTTON (UNCHANGED)
// ============================================================================
void checkResetButton() {
  bool currentButtonState = digitalRead(RESET_BUTTON_PIN) == LOW;
  
  if (currentButtonState && !resetButtonPressed) {
    resetButtonPressed = true;
    resetButtonPressTime = millis();
    resetHoldDetected = false;
    Serial.println("[*] Reset button pressed");
  }
  else if (!currentButtonState && resetButtonPressed) {
    resetButtonPressed = false;
    unsigned long pressDuration = millis() - resetButtonPressTime;
    
    if (!resetHoldDetected) {
      if (millis() - lastResetRelease < DOUBLE_PRESS_TIME) {
        Serial.println("[**] Double-press detected");
        if (tempAlarmActive && tempState == TEMP_STATE_CRITICAL) {
          tempState = TEMP_STATE_ACKNOWLEDGED;
          tempAlarmActive = false;
          tempAcknowledged = true;
          digitalWrite(BUZZER_PIN, doorAlarmActive ? HIGH : LOW);
          Serial.println("Temp alarm acknowledged");
        }
        resetPressCount = 0;
      } else {
        resetPressCount = 1;
      }
      lastResetRelease = millis();
    }
  }
  else if (currentButtonState && resetButtonPressed) {
    if (!resetHoldDetected && (millis() - resetButtonPressTime >= RESET_HOLD_TIME)) {
      resetHoldDetected = true;
      Serial.println("[***] 3-second hold detected");
      performFullSystemReset();
    }
  }
}

// ============================================================================
// FULL SYSTEM RESET - CHANGED PUBLISH CALLS
// ============================================================================
void performFullSystemReset() {
  Serial.println("\n========================================");
  Serial.println("       FULL SYSTEM RESET");
  Serial.println("========================================");
  
  digitalWrite(MOTOR_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("FULL SYSTEM");
  lcd.setCursor(0, 1);
  lcd.print("RESET...");
  
  delay(2000);
  
  doorAlarmActive = false;
  tempAlarmActive = false;
  tempAcknowledged = false;
  passwordAttempts = 0;
  enteredPassword = "";
  showingAccessGranted = false;
  
  tempState = TEMP_STATE_NORMAL;
  
  bool doorIsOpen = (digitalRead(REED_SWITCH_PIN) == HIGH);
  
  if (doorIsOpen) {
    doorState = ENTRY_PENDING;
    doorTimer = millis();
    Serial.println("[!] Reset detected: Door is OPEN - Starting countdown");
    
    if (mqttConnected) {
      mqttPublish(TOPIC_DOOR_STATUS, "OPEN");
      Serial.println("[PUB] Door Status = OPEN (after reset)");
    }
  } else {
    doorState = DOOR_SECURE;
    Serial.println("[OK] Reset detected: Door is CLOSED - System secure");
    
    if (mqttConnected) {
      mqttPublish(TOPIC_DOOR_STATUS, "SECURE");
      mqttPublish(TOPIC_ALARM_STATUS, "NORMAL");
      Serial.println("[PUB] Door Status = SECURE (after reset)");
      Serial.println("[PUB] Alarm Status = NORMAL (after reset)");
    }
  }
  
  for (int i = 3; i > 0; i--) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SYSTEM READY IN");
    lcd.setCursor(0, 1);
    lcd.print("      ");
    lcd.print(i);
    lcd.print("...");
    delay(1000);
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   SYSTEM      ");
  lcd.setCursor(0, 1);
  lcd.print("    READY!     ");
  
  Serial.println("\n[OK] SYSTEM READY\n");
  
  delay(2000);
}

// ============================================================================
// DOOR STATE MACHINE UPDATE - CHANGED PUBLISH CALLS
// ============================================================================
void updateDoorStateMachine() {
  switch (doorState) {
    case ENTRY_PENDING:
      if (millis() - doorTimer >= (DOOR_TIMEOUT * 1000)) {
        Serial.println(F("*** TIMEOUT ALARM ***"));
        doorState = TIMEOUT_ALARM;
        doorAlarmActive = true;
        digitalWrite(BUZZER_PIN, HIGH);
        forceDisplayUpdate = true;
        
        if (mqttConnected) {
          mqttPublish(TOPIC_DOOR_STATUS, "TIMEOUT");
          Serial.println(F("[PUB] Door Status = TIMEOUT"));
          mqttPublish(TOPIC_ALARM_STATUS, "TIMEOUT");
          Serial.println(F("[PUB] Alarm Status = TIMEOUT"));
        }
      }
      break;
      
    case TIMEOUT_ALARM:
    case SECURITY_ALARM:
      break;
      
    case ACCESS_GRANTED:
      if (showingAccessGranted && (millis() - accessGrantedTime >= ACCESS_GRANTED_DISPLAY_TIME)) {
        showingAccessGranted = false;
      }
      break;
      
    case DOOR_SECURE:
      break;
  }
}

// ============================================================================
// TEMPERATURE STATE MACHINE UPDATE (UNCHANGED)
// ============================================================================
void updateTempStateMachine() {
  switch (tempState) {
    case TEMP_STATE_NORMAL:
      if (currentTemp >= TEMP_FAN_ON) {
        tempState = TEMP_STATE_COOLING;
        digitalWrite(MOTOR_PIN, HIGH);
        Serial.print(F("Fan ON - Temp: "));
        Serial.println(currentTemp);
        forceDisplayUpdate = true;
      }
      break;
      
    case TEMP_STATE_COOLING:
      if (currentTemp < TEMP_FAN_OFF) {
        tempState = TEMP_STATE_NORMAL;
        digitalWrite(MOTOR_PIN, LOW);
        Serial.print(F("Fan OFF - Temp: "));
        Serial.println(currentTemp);
        forceDisplayUpdate = true;
      }
      else if (currentTemp >= TEMP_CRITICAL && !tempAcknowledged) {
        tempState = TEMP_STATE_CRITICAL;
        tempAlarmActive = true;
        digitalWrite(BUZZER_PIN, HIGH);
        Serial.print(F("*** CRITICAL ALARM: "));
        Serial.println(currentTemp);
        forceDisplayUpdate = true;
      }
      break;
      
    case TEMP_STATE_CRITICAL:
      if (currentTemp < TEMP_CRITICAL) {
        tempState = TEMP_STATE_COOLING;
        tempAlarmActive = false;
        tempAcknowledged = false;
        digitalWrite(BUZZER_PIN, doorAlarmActive ? HIGH : LOW);
        Serial.println(F("Temp below critical"));
        forceDisplayUpdate = true;
      }
      break;
      
    case TEMP_STATE_ACKNOWLEDGED:
      if (currentTemp < TEMP_CRITICAL) {
        tempState = TEMP_STATE_COOLING;
        tempAcknowledged = false;
        Serial.println(F("Temp dropped - Ack cleared"));
        forceDisplayUpdate = true;
      }
      break;
  }
  
  if (currentTemp >= TEMP_FAN_ON) {
    digitalWrite(MOTOR_PIN, HIGH);
  } else if (currentTemp < TEMP_FAN_OFF) {
    digitalWrite(MOTOR_PIN, LOW);
  }
}

// ============================================================================
// DISPLAY UPDATE WITH PRIORITY SYSTEM (UNCHANGED)
// ============================================================================
void updateDisplay() {
  String line1 = "";
  String line2 = "";
  bool isFlashing = false;
  
  // PRIORITY 1: Critical Temp Alarm - UNACKNOWLEDGED
  if (tempAlarmActive && tempState == TEMP_STATE_CRITICAL) {
    isFlashing = true;
    if (millis() - lastAlarmFlash >= ALARM_FLASH_TIME) {
      alarmFlashState = !alarmFlashState;
      lastAlarmFlash = millis();
      forceDisplayUpdate = true;
    }
    
    if (alarmFlashState) {
      line1 = "*** ALARM ***";
      line2 = "TEMP:" + String((int)currentTemp) + "C CRITICAL";
    }
  }
  
  // PRIORITY 2: Door Alarms
  else if (doorAlarmActive) {
    isFlashing = true;
    if (millis() - lastAlarmFlash >= ALARM_FLASH_TIME) {
      alarmFlashState = !alarmFlashState;
      lastAlarmFlash = millis();
      forceDisplayUpdate = true;
    }
    
    if (alarmFlashState) {
      line1 = "*** ALARM ***";
      if (doorState == TIMEOUT_ALARM) {
        line2 = "TIMEOUT-NO CODE";
      } else {
        line2 = "3 WRONG CODES";
      }
    }
  }
  
  // PRIORITY 3: Door Entry
  else if (doorState == ENTRY_PENDING) {
    line1 = "ENTER PASSWORD";
    int remaining = DOOR_TIMEOUT - ((millis() - doorTimer) / 1000);
    line2 = "Time:";
    if (remaining < 10) line2 += " ";
    line2 += String(remaining) + "s " + String(passwordAttempts) + "/" + String(MAX_ATTEMPTS);
  }
  
  // PRIORITY 4: Initial ACCESS GRANTED
  else if (showingAccessGranted) {
    line1 = "ACCESS GRANTED";
    line2 = "   Welcome!   ";
  }
  
  // Normal display with temperature
  else {
    line1 = "T:" + String(currentTemp, 1) + "C H:" + String((int)currentHumidity) + "%";
    
    if (tempState == TEMP_STATE_ACKNOWLEDGED) {
      line2 = "FAN:ON TEMP-ACK ";
    }
    else if (tempState == TEMP_STATE_COOLING) {
      line2 = "FAN:ON          ";
    }
    else if (doorState == ACCESS_GRANTED) {
      line2 = "ACCESS-DOOR:OPN";
    }
    else if (doorState == DOOR_SECURE) {
      line2 = "Door: SECURE";
    }
  }
  
  if (forceDisplayUpdate || line1 != lastLCDLine1 || line2 != lastLCDLine2) {
    lcd.clear();
    
    if (line1.length() > 0) {
      lcd.setCursor(0, 0);
      lcd.print(line1);
    }
    
    if (line2.length() > 0) {
      lcd.setCursor(0, 1);
      lcd.print(line2);
    }
    
    lastLCDLine1 = line1;
    lastLCDLine2 = line2;
    forceDisplayUpdate = false;
  }
}
