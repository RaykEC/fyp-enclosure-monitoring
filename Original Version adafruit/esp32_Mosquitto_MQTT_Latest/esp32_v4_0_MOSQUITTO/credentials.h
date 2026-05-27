#ifndef CREDENTIALS_H
#define CREDENTIALS_H

// WiFi CREDENTIALS
// ============================================================================
// Note: ESP32 only supports 2.4 GHz WiFi.
#define WIFI_SSID "User"

// Your WiFi password
#define WIFI_PASS "Password"

// MQTT BROKER CREDENTIALS (Self-Hosted Mosquitto)
// ============================================================================
// Your Mosquitto broker IP address (your computer's IP)
#define MQTT_SERVER "192.168.100.2"

// MQTT port (default: 1883)
#define MQTT_PORT 1883

// MQTT username (created with mosquitto_passwd)
#define MQTT_USER "esp32_panel"

// MQTT password (the password you set for esp32_panel)
#define MQTT_PASS "test"

// MQTT Client ID (unique name for this device on the broker)
#define MQTT_CLIENT_ID "ESP32_Panel_1"

// ============================================================================
// MQTT TOPIC STRUCTURE
// ============================================================================
// Hierarchical topic design: panel/{panel_id}/{data_type}
// This allows easy scaling to multiple panels (panel/1, panel/2, etc.)

#define PANEL_ID "1" // Change this for different panels

#define TOPIC_TEMPERATURE "panel/" PANEL_ID "/temperature"
#define TOPIC_HUMIDITY "panel/" PANEL_ID "/humidity"
#define TOPIC_DOOR_STATUS "panel/" PANEL_ID "/door_status"
#define TOPIC_FAN_STATUS "panel/" PANEL_ID "/fan_status"
#define TOPIC_ALARM_STATUS "panel/" PANEL_ID "/alarm_status"

// Control topics (for future two-way communication)
// Backend can publish to these to control the ESP32
#define TOPIC_CONTROL_FAN "panel/" PANEL_ID "/control/fan"
#define TOPIC_CONTROL_RESET "panel/" PANEL_ID "/control/reset"

#endif // CREDENTIALS_H

/*
 * ============================================================================
 * HOW TO USE THIS FILE IN ARDUINO IDE:
 * ============================================================================
 *
 * 1. Open your main sketch (.ino file)
 * 2. Click the dropdown arrow (▼) next to the tab name
 * 3. Select "New Tab"
 * 4. Name it: credentials.h
 * 5. Copy this entire file content into that tab
 * 6. Save
 *
 * ============================================================================
 * WHEN SHARING YOUR PROJECT:
 * ============================================================================
 *
 * Share: .ino file
 * DO NOT share: credentials.h (contains passwords!)
 *
 * Tell others to create their own credentials.h with their WiFi and
 * MQTT broker information.
 *
 * ============================================================================
 * TO ADD MORE PANELS:
 * ============================================================================
 *
 * Simply change PANEL_ID to "2", "3", etc. for each ESP32 device.
 * All topics will automatically update:
 *   Panel 1: panel/1/temperature, panel/1/humidity, etc.
 *   Panel 2: panel/2/temperature, panel/2/humidity, etc.
 *
 * ============================================================================
 */
