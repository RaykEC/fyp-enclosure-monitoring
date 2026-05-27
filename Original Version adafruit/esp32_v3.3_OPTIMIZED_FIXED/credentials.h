#ifndef CREDENTIALS_H
#define CREDENTIALS_H

// WiFi CREDENTIALS
// ============================================================================
// Note: ESP32 only supports 2.4 GHz WiFi.
#define WIFI_SSID "user"

// Your WiFi password
#define WIFI_PASS "Password"

// ADAFRUIT IO CREDENTIALS
// ============================================================================
// Your Adafruit IO username
#define AIO_USERNAME "Rayk90"

// Your Adafruit IO Key (AIO Key)
// Find it at: https://io.adafruit.com -> My Key (yellow key icon)
#define AIO_KEY "your_aio_key_here"

// ============================================================================
// ADAFRUIT IO FEED NAMES
// ============================================================================
// These must match EXACTLY the feed names you created in Adafruit.io
// (case-sensitive!)

#define AIO_FEED_TEMPERATURE "Temperature"
#define AIO_FEED_HUMIDITY "Humidity"
#define AIO_FEED_DOOR_STATUS "Door_Status"
#define AIO_FEED_FAN_STATUS "Fan_Status"
#define AIO_FEED_ALARM_STATUS "Alarm_Status"

#endif // CREDENTIALS_H

/*
 * ============================================================================
 * HOW TO USE THIS FILE IN ARDUINO IDE:
 * ============================================================================
 *
 * 1. Open your main sketch (esp32_FINAL_REFINED_system_WiFi.ino)
 * 2. Click the dropdown arrow (▼) next to the tab name
 * 3. Select "New Tab"
 * 4. Name it: credentials.h
 * 5. Copy this entire file content into that tab
 * 6. Save
 *
 * Result: You'll see two tabs in Arduino IDE:
 *   Tab 1: esp32_FINAL_REFINED_system_WiFi.ino (main code)
 *   Tab 2: credentials.h (this file - your passwords)
 *
 * ============================================================================
 * WHEN SHARING YOUR PROJECT:
 * ============================================================================
 *
 * Share: esp32_FINAL_REFINED_system_WiFi.ino
 * DO NOT share: credentials.h
 *
 * Tell others to create their own credentials.h with their WiFi and
 * Adafruit.io information.
 *
 * ============================================================================
 */
