/*
 * ============================================================================
 * ESP32 DUAL MONITORING SYSTEM - WiFi Cloud Edition v3.3 COMPLETE FIX
 * Door Access Control + Temperature Management + Adafruit.io Cloud
 * ============================================================================
 * 
 * Version: 3.3 - COMPLETE FIX for All 4 Issues
 * Date: November 21, 2025
 * 
 * ALL ISSUES FIXED IN THIS VERSION:
 * - ✅ Issue 1: Door_Status and Alarm_Status now show data on startup
 * - ✅ Issue 2: Initial states published immediately (no more "0 records")
 * - ✅ Issue 3: Alarm_Status shows "NORMAL" from the beginning
 * - ✅ Issue 4: System detects actual door state after reset/boot
 * 
 * NEW IN v3.3:
 * - ✅ Boot-up door state detection (checks reed switch on startup)
 * - ✅ Post-reset door state detection (checks reed switch after 3s reset)
 * - ✅ LCD displays correct state based on actual door position
 * - ✅ Cloud publishes correct initial state (OPEN or SECURE)
 * 
 * CARRIED OVER FROM v3.2:
 * - ✅ Added immediate publishing when door state changes
 * - ✅ Added initial state publishing on startup
 * - ✅ Door_Status now updates instantly on password entryc:\Users\Admin\Downloads\Current version\esp32_v3.3_OPTIMIZED\esp32_v3.3_OPTIMIZED_FIXED\credentials.h
 * - ✅ All alarm conditions publish immediately to cloud
 * 
 * PREVIOUS FIXES (v3.1):
 * - ✅ Corrected feed paths to use "/feeds/" format
 * - ✅ All feeds now publish correctly to Adafruit IO
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
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

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
#define MQTT_PING_INTERVAL      3000    // Ping MQTT broker every 3 seconds
#define MQTT_KEEPALIVE          300     // MQTT keepalive in seconds (5 min - better for hotspots)

// Adafruit IO Configuration
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883  // Use 8883 for SSL

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

// WiFi Client
WiFiClient client;

// Adafruit MQTT Client
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// ============================================================================
// FIXED: Adafruit IO Feeds with correct "/feeds/" path
// ============================================================================
Adafruit_MQTT_Publish feed_temperature  = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/" AIO_FEED_TEMPERATURE);
Adafruit_MQTT_Publish feed_humidity     = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/" AIO_FEED_HUMIDITY);
Adafruit_MQTT_Publish feed_door_status  = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/" AIO_FEED_DOOR_STATUS);
Adafruit_MQTT_Publish feed_fan_status   = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/" AIO_FEED_FAN_STATUS);
Adafruit_MQTT_Publish feed_alarm_status = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/" AIO_FEED_ALARM_STATUS);

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
unsigned long lastMQTTPing = 0;  // Track last MQTT ping time
bool fanStatus = false;
bool lastFanStatus = false;  // For change detection
String alarmStatusText = "NORMAL";
String lastAlarmStatusText = "NORMAL";  // For change detection

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
void connectWiFi();
void connectMQTT();
void publishSensorData();
void publishStatusChange();
void checkWiFiConnection();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println(F("\n\n"));
  Serial.println(F("╔════════════════════════════════════════╗"));
  Serial.println(F("║  ESP32 Door Access + Temp Monitor      ║"));
  Serial.println(F("║  WiFi Cloud Edition v3.3 OPTIMIZED     ║"));
  Serial.println(F("║  System Initialization...              ║"));
  Serial.println(F("╚════════════════════════════════════════╝"));
  
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
  
  Serial.println(F("[✓] LCD initialized"));
  
  // Initialize DHT11
  dht.begin();
  Serial.println(F("[✓] DHT11 initialized"));
  
  // Configure GPIO pins
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(REED_SWITCH_PIN, INPUT_PULLUP);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  
  // Ensure outputs start LOW
  digitalWrite(MOTOR_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.println("[✓] GPIO pins configured");
  
  // Keypad columns pull-up configuration
  pinMode(colPins[0], INPUT_PULLUP);
  pinMode(colPins[1], INPUT_PULLUP);
  pinMode(colPins[2], INPUT_PULLUP);
  pinMode(colPins[3], INPUT_PULLUP);
  
  Serial.println("[✓] Keypad initialized");
  
  // Connect to WiFi
  Serial.println("\n[*] Connecting to WiFi...");
  Serial.print("    SSID: ");
  Serial.println(WIFI_SSID);
  connectWiFi();
  
  // Connect to Adafruit IO via MQTT
  if (wifiConnected) {
    Serial.println("[*] Connecting to Adafruit IO...");
    connectMQTT();
    
    // Check actual door state on startup and publish correct initial states
    if (mqttConnected) {
      Serial.println("\n[*] Detecting initial system state...");
      
      // Check if door is actually open or closed
      bool doorIsOpen = (digitalRead(REED_SWITCH_PIN) == HIGH);
      
      if (doorIsOpen) {
        // Door is open on startup - start entry pending
        doorState = ENTRY_PENDING;
        doorTimer = millis();
        Serial.println("    ⚠️ Door detected: OPEN - Starting countdown");
        
        if (feed_door_status.publish("OPEN")) {
          Serial.println("    ✅ Door Status: OPEN");
        }
      } else {
        // Door is closed on startup - secure
        doorState = DOOR_SECURE;
        Serial.println("    ✅ Door detected: CLOSED - System secure");
        
        if (feed_door_status.publish("SECURE")) {
          Serial.println("    ✅ Door Status: SECURE");
        }
      }
      
      if (feed_alarm_status.publish("NORMAL")) {
        Serial.println("    ✅ Alarm Status: NORMAL");
      }
      
      if (feed_fan_status.publish("OFF")) {
        Serial.println("    ✅ Fan Status: OFF");
      }
      
      Serial.println("[✓] Initial states published!\n");
    }
  }
  
  delay(2000);
  
  // Display ready message
  lcd.clear();
  lcd.setCursor(0, 0);
  if (wifiConnected && mqttConnected) {
    lcd.print("WiFi: CONNECTED");
    lcd.setCursor(0, 1);
    lcd.print("Cloud: READY!");
  } else if (wifiConnected) {
    lcd.print("WiFi: CONNECTED");
    lcd.setCursor(0, 1);
    lcd.print("Cloud: Offline");
  } else {
    lcd.print("WiFi: OFFLINE");
    lcd.setCursor(0, 1);
    lcd.print("Local Mode Only");
  }
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║          SYSTEM READY!                 ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println("\nConfiguration:");
  Serial.println("  Password: " + String(PASSWORD));
  Serial.println("  Fan ON: " + String(TEMP_FAN_ON) + "°C");
  Serial.println("  Fan OFF: " + String(TEMP_FAN_OFF) + "°C");
  Serial.println("  Critical: " + String(TEMP_CRITICAL) + "°C");
  
  if (mqttConnected) {
    Serial.println("   Feed 1: " + String(AIO_USERNAME) + "/feeds/" + String(AIO_FEED_TEMPERATURE));
    Serial.println("   Feed 2: " + String(AIO_USERNAME) + "/feeds/" + String(AIO_FEED_HUMIDITY));
    Serial.println("   Feed 3: " + String(AIO_USERNAME) + "/feeds/" + String(AIO_FEED_DOOR_STATUS));
    Serial.println("   Feed 4: " + String(AIO_USERNAME) + "/feeds/" + String(AIO_FEED_FAN_STATUS));
    Serial.println("   Feed 5: " + String(AIO_USERNAME) + "/feeds/" + String(AIO_FEED_ALARM_STATUS));
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
  
  // Keep MQTT connection alive
  if (mqttConnected) {
    mqtt.processPackets(10);  // Process incoming packets
    
    // Only ping every 3 seconds (not every loop!)
    if (millis() - lastMQTTPing >= MQTT_PING_INTERVAL) {
      lastMQTTPing = millis();
      
      if (!mqtt.ping()) {
        Serial.println("[!] MQTT ping failed - reconnecting...");
        mqttConnected = false;
        connectMQTT();
      }
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
  
  // Publish data to Adafruit IO
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
// WiFi CONNECTION
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
    Serial.println("\n[✓] WiFi Connected!");
    Serial.print("    IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("    Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    wifiConnected = false;
    Serial.println("\n[✗] WiFi Connection Failed");
    Serial.println("System will run in LOCAL MODE only");
  }
}

// ============================================================================
// MQTT CONNECTION TO ADAFRUIT IO
// ============================================================================
void connectMQTT() {
  if (!wifiConnected) {
    mqttConnected = false;
    return;
  }
  
  int8_t ret;
  
  // Stop if already connected
  if (mqtt.connected()) {
    mqttConnected = true;
    return;
  }
  
  Serial.print("[*] Connecting to Adafruit IO... ");
  
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("    Retrying in 5 seconds...");
    mqtt.disconnect();
    delay(5000);
    retries--;
    if (retries == 0) {
      Serial.println("[✗] MQTT Connection Failed");
      mqttConnected = false;
      return;
    }
  }
  
  mqttConnected = true;
  Serial.println("Adafruit IO connection Established!");
  Serial.println("      - " + String(AIO_USERNAME) + "/feeds/" + String(AIO_FEED_TEMPERATURE));
  Serial.println("      - " + String(AIO_USERNAME) + "/feeds/" + String(AIO_FEED_HUMIDITY));
  Serial.println("      - " + String(AIO_USERNAME) + "/feeds/" + String(AIO_FEED_DOOR_STATUS));
  Serial.println("      - " + String(AIO_USERNAME) + "/feeds/" + String(AIO_FEED_FAN_STATUS));
  Serial.println("      - " + String(AIO_USERNAME) + "/feeds/" + String(AIO_FEED_ALARM_STATUS));
}

// ============================================================================
// CHECK WiFi CONNECTION
// ============================================================================
void checkWiFiConnection() {
  // Check WiFi status every 30 seconds
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
    
    // Try to reconnect MQTT if WiFi is OK but MQTT is down
    if (wifiConnected && !mqttConnected) {
      connectMQTT();
    }
  }
}

// ============================================================================
// PUBLISH SENSOR DATA (Periodic - Every 10 seconds)
// ============================================================================
void publishSensorData() {
  if (!mqttConnected) return;
  
  if (millis() - lastMQTTPublish >= MQTT_PUBLISH_INTERVAL) {
    lastMQTTPublish = millis();
    
    // Always publish temperature and humidity (remove the change check for debugging)
    if (feed_temperature.publish(currentTemp)) {
      Serial.print("📊 Published Temperature: ");
      Serial.print(currentTemp);
      Serial.println("°C");
      lastPublishedTemp = currentTemp;
    } else {
      Serial.println("[✗] Failed to publish temperature");
    }
    
    if (feed_humidity.publish(currentHumidity)) {
      Serial.print("📊 Published Humidity: ");
      Serial.print(currentHumidity);
      Serial.println("%");
      lastPublishedHumidity = currentHumidity;
    } else {
      Serial.println("[✗] Failed to publish humidity");
    }
    
    Serial.println("✅ Sensor data synced to cloud");
  }
}

// ============================================================================
// PUBLISH STATUS CHANGES (Immediate - When status changes)
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
    
    if (feed_door_status.publish(doorStatus.c_str())) {
      Serial.print("🚪 Published Door Status: ");
      Serial.println(doorStatus);
    }
    lastDoorState = doorState;
  }
  
  // Publish fan status change
  fanStatus = (digitalRead(MOTOR_PIN) == HIGH);
  if (fanStatus != lastFanStatus) {
    String fanStatusText = fanStatus ? "ON" : "OFF";
    
    if (feed_fan_status.publish(fanStatusText.c_str())) {
      Serial.print("🌀 Published Fan Status: ");
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
    if (feed_alarm_status.publish(currentAlarmStatus.c_str())) {
      Serial.print("⚠️ Published Alarm Status: ");
      Serial.println(currentAlarmStatus);
    }
    lastAlarmStatusText = currentAlarmStatus;
  }
}

// ============================================================================
// READ TEMPERATURE SENSOR
// ============================================================================
void readTemperature() {
  if (millis() - lastTempRead >= TEMP_READ_INTERVAL) {
    lastTempRead = millis();
    
    float temp = dht.readTemperature();
    float humidity = dht.readHumidity();
    
    if (!isnan(temp) && !isnan(humidity)) {
      currentTemp = temp;
      currentHumidity = humidity;
      
      // Print only when values change significantly (0.5°C or 2% humidity)
      static float lastPrintedTemp = -999;
      static float lastPrintedHum = -999;
      
      if (abs(currentTemp - lastPrintedTemp) >= 0.5 || abs(currentHumidity - lastPrintedHum) >= 2) {
        Serial.print(F("🌡️ Temp: "));
        Serial.print(currentTemp);
        Serial.print(F("°C, Humidity: "));
        Serial.print(currentHumidity);
        Serial.println(F("%"));
        
        lastPrintedTemp = currentTemp;
        lastPrintedHum = currentHumidity;
      }
    }
  }
}

// ============================================================================
// CHECK DOOR SENSOR (Reed Switch on GPIO 17)
// ============================================================================
void checkDoorSensor() {
  static bool lastReedState = LOW;
  bool currentReedState = digitalRead(REED_SWITCH_PIN);
  
  // Only print when reed switch or door state changes
  static DoorState lastPrintedState = DOOR_SECURE;
  static bool lastPrintedReed = LOW;
  
  bool shouldPrint = (currentReedState != lastPrintedReed) || (doorState != lastPrintedState);
  
  if (shouldPrint) {
    Serial.print(F("🚪 Reed Switch: "));
    Serial.print(currentReedState == HIGH ? F("OPEN (HIGH)") : F("CLOSED (LOW)"));
    Serial.print(F(" | Door State: "));
    
    switch (doorState) {
      case DOOR_SECURE:
        Serial.println(F("SECURE"));
        break;
      case ENTRY_PENDING:
        Serial.println(F("ENTRY_PENDING"));
        break;
      case ACCESS_GRANTED:
        Serial.println(F("ACCESS_GRANTED"));
        break;
      case TIMEOUT_ALARM:
        Serial.println(F("TIMEOUT_ALARM"));
        break;
      case SECURITY_ALARM:
        Serial.println(F("SECURITY_ALARM"));
        break;
    }
    
    lastPrintedState = doorState;
    lastPrintedReed = currentReedState;
  }
  
  switch (doorState) {
    case DOOR_SECURE:
      if (currentReedState == HIGH && lastReedState == LOW) {
        Serial.println(F("🚨 Door opened - Starting timer"));
        doorState = ENTRY_PENDING;
        doorTimer = millis();
        passwordAttempts = 0;
        enteredPassword = "";
        
        // Immediately publish to Adafruit IO
        if (mqttConnected) {
          if (feed_door_status.publish("OPEN")) {
            Serial.println(F("📤 Published: Door Status = OPEN"));
          }
        }
      }
      break;
      
    case ENTRY_PENDING:
      break;
      
    case ACCESS_GRANTED:
      if (currentReedState == LOW && lastReedState == HIGH) {
        Serial.println(F("✅ Door closed - System secure"));
        doorState = DOOR_SECURE;
        
        // Immediately publish to Adafruit IO
        if (mqttConnected) {
          if (feed_door_status.publish("SECURE")) {
            Serial.println(F("📤 Published: Door Status = SECURE"));
          }
          if (feed_alarm_status.publish("NORMAL")) {
            Serial.println(F("📤 Published: Alarm Status = NORMAL"));
          }
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
// CHECK KEYPAD
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
        forceDisplayUpdate = true;  // Force LCD update
        
        // Immediately publish to Adafruit IO
        if (mqttConnected) {
          if (feed_door_status.publish("GRANTED")) {
            Serial.println(F("📤 Published: Door Status = GRANTED"));
          }
          if (feed_alarm_status.publish("NORMAL")) {
            Serial.println(F("📤 Published: Alarm Status = NORMAL"));
          }
        }
      } else {
        Serial.println(F("Wrong password"));
        passwordAttempts++;
        if (passwordAttempts >= MAX_ATTEMPTS) {
          Serial.println(F("*** SECURITY ALARM ***"));
          doorState = SECURITY_ALARM;
          doorAlarmActive = true;
          digitalWrite(BUZZER_PIN, HIGH);
          forceDisplayUpdate = true;  // Force LCD update
          
          // Immediately publish to Adafruit IO
          if (mqttConnected) {
            if (feed_door_status.publish("WRONG_CODE")) {
              Serial.println(F("📤 Published: Door Status = WRONG_CODE"));
            }
            if (feed_alarm_status.publish("WRONG_CODE")) {
              Serial.println(F("📤 Published: Alarm Status = WRONG_CODE"));
            }
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
// CHECK RESET BUTTON
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
// FULL SYSTEM RESET
// ============================================================================
void performFullSystemReset() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║       FULL SYSTEM RESET                ║");
  Serial.println("╚════════════════════════════════════════╝");
  
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
  
  // Reset temperature state
  tempState = TEMP_STATE_NORMAL;
  
  // Check actual reed switch state and set correct door state
  bool doorIsOpen = (digitalRead(REED_SWITCH_PIN) == HIGH);
  
  if (doorIsOpen) {
    // Door is actually open - start entry pending state
    doorState = ENTRY_PENDING;
    doorTimer = millis();
    Serial.println("[!] Reset detected: Door is OPEN - Starting countdown");
    
    // Publish correct state to cloud
    if (mqttConnected) {
      feed_door_status.publish("OPEN");
      Serial.println("📤 Published: Door Status = OPEN (after reset)");
    }
  } else {
    // Door is closed - secure state
    doorState = DOOR_SECURE;
    Serial.println("[✓] Reset detected: Door is CLOSED - System secure");
    
    // Publish correct state to cloud
    if (mqttConnected) {
      feed_door_status.publish("SECURE");
      feed_alarm_status.publish("NORMAL");
      Serial.println("📤 Published: Door Status = SECURE (after reset)");
      Serial.println("📤 Published: Alarm Status = NORMAL (after reset)");
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
  
  Serial.println("\n[✓] SYSTEM READY\n");
  
  delay(2000);
}

// ============================================================================
// DOOR STATE MACHINE UPDATE
// ============================================================================
void updateDoorStateMachine() {
  switch (doorState) {
    case ENTRY_PENDING:
      if (millis() - doorTimer >= (DOOR_TIMEOUT * 1000)) {
        Serial.println(F("*** TIMEOUT ALARM ***"));
        doorState = TIMEOUT_ALARM;
        doorAlarmActive = true;
        digitalWrite(BUZZER_PIN, HIGH);
        forceDisplayUpdate = true;  // Force LCD update
        
        // Immediately publish to Adafruit IO
        if (mqttConnected) {
          if (feed_door_status.publish("TIMEOUT")) {
            Serial.println(F("📤 Published: Door Status = TIMEOUT"));
          }
          if (feed_alarm_status.publish("TIMEOUT")) {
            Serial.println(F("📤 Published: Alarm Status = TIMEOUT"));
          }
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
// TEMPERATURE STATE MACHINE UPDATE
// ============================================================================
void updateTempStateMachine() {
  switch (tempState) {
    case TEMP_STATE_NORMAL:
      if (currentTemp >= TEMP_FAN_ON) {
        tempState = TEMP_STATE_COOLING;
        digitalWrite(MOTOR_PIN, HIGH);
        Serial.print(F("Fan ON - Temp: "));
        Serial.println(currentTemp);
        forceDisplayUpdate = true;  // Force LCD update
      }
      break;
      
    case TEMP_STATE_COOLING:
      if (currentTemp < TEMP_FAN_OFF) {
        tempState = TEMP_STATE_NORMAL;
        digitalWrite(MOTOR_PIN, LOW);
        Serial.print(F("Fan OFF - Temp: "));
        Serial.println(currentTemp);
        forceDisplayUpdate = true;  // Force LCD update
      }
      else if (currentTemp >= TEMP_CRITICAL && !tempAcknowledged) {
        tempState = TEMP_STATE_CRITICAL;
        tempAlarmActive = true;
        digitalWrite(BUZZER_PIN, HIGH);
        Serial.print(F("*** CRITICAL ALARM: "));
        Serial.println(currentTemp);
        forceDisplayUpdate = true;  // Force LCD update
      }
      break;
      
    case TEMP_STATE_CRITICAL:
      if (currentTemp < TEMP_CRITICAL) {
        tempState = TEMP_STATE_COOLING;
        tempAlarmActive = false;
        tempAcknowledged = false;
        digitalWrite(BUZZER_PIN, doorAlarmActive ? HIGH : LOW);
        Serial.println(F("Temp below critical"));
        forceDisplayUpdate = true;  // Force LCD update
      }
      break;
      
    case TEMP_STATE_ACKNOWLEDGED:
      if (currentTemp < TEMP_CRITICAL) {
        tempState = TEMP_STATE_COOLING;
        tempAcknowledged = false;
        Serial.println(F("Temp dropped - Ack cleared"));
        forceDisplayUpdate = true;  // Force LCD update
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
// DISPLAY UPDATE WITH PRIORITY SYSTEM (OPTIMIZED)
// ============================================================================
void updateDisplay() {
  String line1 = "";
  String line2 = "";
  bool isFlashing = false;  // Track if display is flashing
  
  // PRIORITY 1: Critical Temp Alarm - UNACKNOWLEDGED
  if (tempAlarmActive && tempState == TEMP_STATE_CRITICAL) {
    isFlashing = true;
    if (millis() - lastAlarmFlash >= ALARM_FLASH_TIME) {
      alarmFlashState = !alarmFlashState;
      lastAlarmFlash = millis();
      forceDisplayUpdate = true;  // Force update for flash
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
      forceDisplayUpdate = true;  // Force update for flash
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
    // Line 1: Temperature and Humidity
    line1 = "T:" + String(currentTemp, 1) + "C H:" + String((int)currentHumidity) + "%";
    
    // Line 2: Status based on priority
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
  
  // Only update LCD if content has changed or force update (for flashing)
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
