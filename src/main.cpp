// Link G4X Monitor - Clean CAN Bus Implementation
#include <Arduino.h>
#include <M5Unified.h>
#include <ESP32-TWAI-CAN.hpp>
#include <Preferences.h>

// ========== CONFIGURATION ==========
enum UnitSystem {
  METRIC = 0,    // Celsius, kPa, km/h
  IMPERIAL = 1   // Fahrenheit, PSI, mph
};

enum LoggingMode {
  LOG_DISABLED = 0,     // No logging
  LOG_ERRORS = 1,       // Only CAN errors and faults
  LOG_CHANGES = 2,      // Only when parameters change
  LOG_FULL = 3,         // All CAN frames
  LOG_SESSION = 4       // Manual session recording
};

enum LogDetail {
  LOG_BASIC = 0,        // Timestamp, CAN ID, values
  LOG_DETAILED = 1,     // + Frame info, error counters
  LOG_DIAGNOSTIC = 2    // + Raw hex, timing analysis
};

enum BufferSize {
  BUFFER_SMALL = 0,     // 500 frames (~10KB)
  BUFFER_MEDIUM = 1,    // 1000 frames (~20KB)
  BUFFER_LARGE = 2,     // 2000 frames (~40KB)
  BUFFER_CUSTOM = 3     // User defined
};

struct Config {
  uint32_t base_can_id = 864;           // Base CAN ID for Haltech IC7
  uint32_t can_speed = 500000;          // CAN bus speed (500 kbps)
  bool simulation_mode = true;          // Start in simulation mode
  bool use_custom_streams = true;       // Use custom stream configuration
  UnitSystem units = METRIC;            // Unit system (metric/imperial)

  // CAN Logging Configuration
  LoggingMode logging_mode = LOG_DISABLED;     // Logging mode
  LogDetail log_detail = LOG_BASIC;            // Detail level
  BufferSize buffer_size = BUFFER_MEDIUM;      // Buffer size
  uint16_t write_frequency_ms = 500;           // Write frequency (ms)
  uint16_t max_file_size_mb = 10;              // Max file size (MB)
  uint8_t max_files = 10;                      // Max number of files
  uint8_t auto_delete_days = 30;               // Auto delete after days (0=disabled)
  bool compression_enabled = false;            // Enable compression
  float change_threshold = 1.0;               // Change threshold for LOG_CHANGES mode (%)

  // Legacy individual unit flags (for backward compatibility)
  bool use_fahrenheit = false;          // Temperature units
  bool use_psi = false;                 // Pressure units
  bool use_mph = false;                 // Speed units
};

Config config;
Preferences preferences;

// ========== UNIT CONVERSION FUNCTIONS ==========
float convertTemperature(float celsius) {
  if (config.units == IMPERIAL) {
    return celsius * 9.0f / 5.0f + 32.0f; // Convert to Fahrenheit
  }
  return celsius; // Return Celsius
}

float convertPressure(float kpa) {
  if (config.units == IMPERIAL) {
    return kpa * 0.145038f; // Convert to PSI
  }
  return kpa; // Return kPa
}

float convertSpeed(float kmh) {
  if (config.units == IMPERIAL) {
    return kmh * 0.621371f; // Convert to mph
  }
  return kmh; // Return km/h
}

const char* getTemperatureUnit() {
  return (config.units == IMPERIAL) ? "°F" : "°C";
}

const char* getPressureUnit() {
  return (config.units == IMPERIAL) ? "PSI" : "KPA";
}

const char* getSpeedUnit() {
  return (config.units == IMPERIAL) ? "MPH" : "KM/H";
}

const char* getUnitSystemName() {
  return (config.units == IMPERIAL) ? "IMPERIAL" : "METRIC";
}

// ========== LOGGING CONFIGURATION FUNCTIONS ==========
const char* getLoggingModeName() {
  switch (config.logging_mode) {
    case LOG_DISABLED: return "DISABLED";
    case LOG_ERRORS: return "ERRORS";
    case LOG_CHANGES: return "CHANGES";
    case LOG_FULL: return "FULL";
    case LOG_SESSION: return "SESSION";
    default: return "DISABLED";
  }
}

const char* getLogDetailName() {
  switch (config.log_detail) {
    case LOG_BASIC: return "BASIC";
    case LOG_DETAILED: return "DETAILED";
    case LOG_DIAGNOSTIC: return "DIAGNOSTIC";
    default: return "BASIC";
  }
}

const char* getBufferSizeName() {
  switch (config.buffer_size) {
    case BUFFER_SMALL: return "SMALL";
    case BUFFER_MEDIUM: return "MEDIUM";
    case BUFFER_LARGE: return "LARGE";
    case BUFFER_CUSTOM: return "CUSTOM";
    default: return "MEDIUM";
  }
}

uint16_t getBufferFrameCount() {
  switch (config.buffer_size) {
    case BUFFER_SMALL: return 500;
    case BUFFER_MEDIUM: return 1000;
    case BUFFER_LARGE: return 2000;
    case BUFFER_CUSTOM: return 1500; // Default custom value
    default: return 1000;
  }
}

bool isLoggingEnabled() {
  return config.logging_mode != LOG_DISABLED;
}

// ========== CAN BUS CONFIGURATION ==========
// Custom Stream Configuration
const uint32_t CUSTOM_STREAM_ID_1 = 0x500;  // Primary Engine Data
const uint32_t CUSTOM_STREAM_ID_2 = 0x501;  // Lambda & Fuel Data  
const uint32_t CUSTOM_STREAM_ID_3 = 0x502;  // Pressures & Status
const uint32_t CONTROL_STREAM_ID = 0x600;   // Dashboard Commands

unsigned long last_can_message = 0;

// ========== ECU DATA STRUCTURE ==========
struct ECUData {
  // Primary Engine Data (Frame 0x500)
  float rpm = 2150;
  float tps = 15;
  float aps = 18;
  float mgp = 5;
  float ect = 87;
  float iat = 28;
  float battery = 12.5;
  
  // Lambda & Fuel Data (Frame 0x501)
  float lambda = 1.0;
  float lambda_target = 1.0;
  float injector_duty = 20;
  float ethanol_percent = 85;
  
  // Pressures & Status (Frame 0x502)
  float oil_press = 50;
  float fuel_press = 300;
  uint8_t current_boost_map = 1;
  uint8_t current_ethrottle_map = 1;
  
  // Control System Status
  bool boost_control_active = false;
  bool launch_control_active = false;
  bool anti_lag_active = false;

  // Additional control interface variables
  float boost_adjustment = 0.0;
  int launch_rpm = 4000;
  bool system_ready = true;
  bool safe_mode_active = false;
  uint8_t boost_target_percent = 100;
  bool ethrottle_control_active = false;
};

ECUData ecu_data;

// ========== CAN BUS FUNCTIONS ==========
bool initializeCAN() {
  ESP32Can.setPins(GPIO_NUM_26, GPIO_NUM_27);
  
  // Convert speed to enum
  TwaiSpeed speed = TWAI_SPEED_500KBPS;
  switch (config.can_speed) {
    case 125000: speed = TWAI_SPEED_125KBPS; break;
    case 250000: speed = TWAI_SPEED_250KBPS; break;
    case 500000: speed = TWAI_SPEED_500KBPS; break;
    case 1000000: speed = TWAI_SPEED_1000KBPS; break;
    default: speed = TWAI_SPEED_500KBPS; break;
  }
  
  if (!ESP32Can.begin(speed)) {
    Serial.println("CAN initialization failed!");
    return false;
  }
  
  Serial.printf("CAN initialized at %d bps\n", config.can_speed);
  return true;
}

// ========== CUSTOM STREAM PARSING ==========
void parseCustomStream1(const twai_message_t& message) {
  // Frame 0x500 - Primary Engine Data
  if (message.data_length_code >= 8) {
    ecu_data.rpm = ((message.data[1] << 8) | message.data[0]) * 0.1;
    ecu_data.tps = message.data[2] * 0.5;
    ecu_data.aps = message.data[3] * 0.5;
    ecu_data.mgp = ((message.data[5] << 8) | message.data[4]) * 0.1 - 100;
    ecu_data.ect = message.data[6] - 40;
    ecu_data.iat = message.data[7] - 40;
  }
}

void parseCustomStream2(const twai_message_t& message) {
  // Frame 0x501 - Lambda & Fuel Data
  if (message.data_length_code >= 8) {
    ecu_data.lambda = ((message.data[1] << 8) | message.data[0]) * 0.001;
    ecu_data.lambda_target = ((message.data[3] << 8) | message.data[2]) * 0.001;
    ecu_data.injector_duty = message.data[4] * 0.5;
    ecu_data.ethanol_percent = message.data[5];
    ecu_data.battery = ((message.data[7] << 8) | message.data[6]) * 0.01;
  }
}

void parseCustomStream3(const twai_message_t& message) {
  // Frame 0x502 - Pressures & Status
  if (message.data_length_code >= 8) {
    ecu_data.oil_press = ((message.data[1] << 8) | message.data[0]) * 0.1;
    ecu_data.fuel_press = ((message.data[3] << 8) | message.data[2]) * 0.1;
    ecu_data.current_boost_map = message.data[4];
    ecu_data.current_ethrottle_map = message.data[5];
    ecu_data.launch_control_active = (message.data[6] & 0x01) != 0;
    ecu_data.anti_lag_active = (message.data[6] & 0x02) != 0;
  }
}

bool readCANData() {
  twai_message_t message;
  bool data_received = false;

  // Check for received messages
  if (ESP32Can.readFrame(message, 0) == ESP_OK) {
    data_received = true;
    last_can_message = millis();

    if (config.use_custom_streams) {
      switch (message.identifier) {
        case CUSTOM_STREAM_ID_1:
          parseCustomStream1(message);
          break;
        case CUSTOM_STREAM_ID_2:
          parseCustomStream2(message);
          break;
        case CUSTOM_STREAM_ID_3:
          parseCustomStream3(message);
          break;
      }
    }
    // Add Haltech IC7 parsing here if needed
  }

  return data_received;
}

// ========== SIMULATION ==========
void simulateData() {
  static unsigned long last_update = 0;
  if (millis() - last_update < 50) return; // 20Hz update rate
  last_update = millis();
  
  // Simulate engine data
  ecu_data.rpm += random(-50, 50);
  ecu_data.rpm = constrain(ecu_data.rpm, 800, 7000);
  
  ecu_data.tps += random(-2, 2);
  ecu_data.tps = constrain(ecu_data.tps, 0, 100);
  
  ecu_data.aps = ecu_data.tps + random(-5, 5);
  ecu_data.aps = constrain(ecu_data.aps, 0, 100);
  
  ecu_data.mgp += random(-3, 8); // More realistic boost variation
  ecu_data.mgp = constrain(ecu_data.mgp, -50, 150); // -50 kPa (vacuum) to 150 kPa (boost)
  
  ecu_data.ect += random(-1, 1);
  ecu_data.ect = constrain(ecu_data.ect, 80, 95);
  
  ecu_data.iat += random(-1, 1);
  ecu_data.iat = constrain(ecu_data.iat, 20, 60);
  
  // Simulate lambda data
  ecu_data.lambda += random(-5, 5) * 0.001;
  ecu_data.lambda = constrain(ecu_data.lambda, 0.7, 1.3);
  
  if (ecu_data.mgp > 0) {
    ecu_data.lambda_target = 0.85 + random(-2, 2) * 0.01;
  } else {
    ecu_data.lambda_target = 1.00 + random(-2, 2) * 0.01;
  }
  ecu_data.lambda_target = constrain(ecu_data.lambda_target, 0.75, 1.10);
  
  float target_duty = 20 + (ecu_data.tps * 0.6) + (max(0.0f, ecu_data.mgp) * 0.3);
  ecu_data.injector_duty += (target_duty - ecu_data.injector_duty) * 0.1 + random(-2, 2);
  ecu_data.injector_duty = constrain(ecu_data.injector_duty, 10, 95);
  
  ecu_data.ethanol_percent += random(-1, 1);
  ecu_data.ethanol_percent = constrain(ecu_data.ethanol_percent, 80, 87);
  
  // Simulate map changes
  static unsigned long last_boost_change = 0;
  if (millis() - last_boost_change > 15000) {
    ecu_data.current_boost_map = (ecu_data.current_boost_map % 8) + 1;
    last_boost_change = millis();
    Serial.printf("🗺️ Boost map changed to: %d\n", ecu_data.current_boost_map);
  }
  
  static unsigned long last_ethrottle_change = 0;
  if (millis() - last_ethrottle_change > 18000) {
    ecu_data.current_ethrottle_map = (ecu_data.current_ethrottle_map % 8) + 1;
    last_ethrottle_change = millis();
    Serial.printf("⚡ E-Throttle map changed to: %d\n", ecu_data.current_ethrottle_map);
  }
}

// ========== CONFIGURATION ==========
void loadConfig() {
  preferences.begin("link_g4x", false);

  config.base_can_id = preferences.getUInt("base_can_id", 864);
  config.can_speed = preferences.getUInt("can_speed", 500000);
  config.simulation_mode = preferences.getBool("simulation", true);
  config.use_custom_streams = preferences.getBool("custom_streams", true);

  // Load unit system (new unified approach)
  config.units = (UnitSystem)preferences.getUChar("units", METRIC);

  // Load CAN logging configuration
  config.logging_mode = (LoggingMode)preferences.getUChar("log_mode", LOG_DISABLED);
  config.log_detail = (LogDetail)preferences.getUChar("log_detail", LOG_BASIC);
  config.buffer_size = (BufferSize)preferences.getUChar("buffer_size", BUFFER_MEDIUM);
  config.write_frequency_ms = preferences.getUShort("write_freq", 500);
  config.max_file_size_mb = preferences.getUShort("max_file_mb", 10);
  config.max_files = preferences.getUChar("max_files", 10);
  config.auto_delete_days = preferences.getUChar("auto_del_days", 30);
  config.compression_enabled = preferences.getBool("compression", false);
  config.change_threshold = preferences.getFloat("change_thresh", 1.0);

  // Load legacy individual unit flags for backward compatibility
  config.use_fahrenheit = preferences.getBool("fahrenheit", false);
  config.use_psi = preferences.getBool("psi", false);
  config.use_mph = preferences.getBool("mph", false);

  // If legacy flags are set, convert to new unit system
  if (config.use_fahrenheit || config.use_psi || config.use_mph) {
    config.units = IMPERIAL;
  }

  preferences.end();

  Serial.println("Configuration loaded:");
  Serial.printf("  Base CAN ID: %d\n", config.base_can_id);
  Serial.printf("  CAN Speed: %d bps\n", config.can_speed);
  Serial.printf("  Simulation: %s\n", config.simulation_mode ? "ON" : "OFF");
  Serial.printf("  Custom Streams: %s\n", config.use_custom_streams ? "ON" : "OFF");
  Serial.printf("  Units: %s\n", getUnitSystemName());
  Serial.printf("  Logging: %s (%s)\n", getLoggingModeName(), getLogDetailName());
  Serial.printf("  Buffer: %s (%d frames)\n", getBufferSizeName(), getBufferFrameCount());
}

void saveConfig() {
  preferences.begin("link_g4x", false);

  preferences.putUInt("base_can_id", config.base_can_id);
  preferences.putUInt("can_speed", config.can_speed);
  preferences.putBool("simulation", config.simulation_mode);
  preferences.putBool("custom_streams", config.use_custom_streams);

  // Save new unit system
  preferences.putUChar("units", config.units);

  // Save CAN logging configuration
  preferences.putUChar("log_mode", config.logging_mode);
  preferences.putUChar("log_detail", config.log_detail);
  preferences.putUChar("buffer_size", config.buffer_size);
  preferences.putUShort("write_freq", config.write_frequency_ms);
  preferences.putUShort("max_file_mb", config.max_file_size_mb);
  preferences.putUChar("max_files", config.max_files);
  preferences.putUChar("auto_del_days", config.auto_delete_days);
  preferences.putBool("compression", config.compression_enabled);
  preferences.putFloat("change_thresh", config.change_threshold);

  // Update legacy flags for backward compatibility
  config.use_fahrenheit = (config.units == IMPERIAL);
  config.use_psi = (config.units == IMPERIAL);
  config.use_mph = (config.units == IMPERIAL);

  preferences.putBool("fahrenheit", config.use_fahrenheit);
  preferences.putBool("psi", config.use_psi);
  preferences.putBool("mph", config.use_mph);

  preferences.end();
  Serial.printf("Configuration saved - Units: %s\n", getUnitSystemName());
}

// ========== ANIME SPLASH SCREEN ==========
void playJapaneseVoice() {
  // Japanese female voice: "Ready to go?" (Junbi wa ii desu ka?)
  // Using M5 Speaker with tone synthesis for Japanese pronunciation

  Serial.println("🎵 Playing Japanese voice: 'Junbi wa ii desu ka?'");

  // Test speaker first
  M5.Speaker.tone(1000, 100);
  delay(150);

  // "Jun" - ジュン (high-mid tone, more pronounced)
  M5.Speaker.tone(880, 250);  // A5 - longer duration
  delay(280);
  M5.Speaker.tone(660, 200);  // E5
  delay(230);

  // "bi" - ビ (mid tone, sharper)
  M5.Speaker.tone(740, 220);  // F#5
  delay(250);

  // Short pause between words
  delay(100);

  // "wa" - ワ (mid-low tone, softer)
  M5.Speaker.tone(587, 250);  // D5
  delay(280);

  // "ii" - イイ (rising tone, more dramatic)
  M5.Speaker.tone(523, 200);  // C5
  delay(230);
  M5.Speaker.tone(659, 250);  // E5
  delay(280);

  // Short pause
  delay(150);

  // "desu" - デス (falling tone, clear pronunciation)
  M5.Speaker.tone(698, 220);  // F5
  delay(250);
  M5.Speaker.tone(523, 200);  // C5
  delay(230);

  // "ka?" - カ？ (questioning rise, very pronounced)
  M5.Speaker.tone(659, 250);  // E5
  delay(280);
  M5.Speaker.tone(784, 400);  // G5 (questioning rise, longer)
  delay(450);

  // Cute ending chime
  M5.Speaker.tone(1047, 150); // C6 - high cute note
  delay(200);

  M5.Speaker.stop();
  Serial.println("🎵 Voice playback complete");
}

void animateLoadingBar(int progress_percent) {
  int screen_w = M5.Display.width();
  int screen_h = M5.Display.height();

  int bar_w = 300;
  int bar_h = 8;
  int bar_x = (screen_w - bar_w) / 2;
  int bar_y = screen_h - 20;

  // Clear previous bar
  M5.Display.fillRect(bar_x + 1, bar_y + 1, bar_w - 2, bar_h - 2,
                     M5.Display.color565(20, 20, 40));

  // Draw progress with anime-style glow effect
  int fill_width = (bar_w - 2) * progress_percent / 100;

  // Main progress bar
  uint16_t progress_color = M5.Display.color565(0, 255, 200);
  M5.Display.fillRect(bar_x + 1, bar_y + 1, fill_width, bar_h - 2, progress_color);

  // Glow effect at the leading edge
  if (fill_width > 0 && fill_width < bar_w - 2) {
    uint16_t glow_color = M5.Display.color565(100, 255, 255);
    M5.Display.drawFastVLine(bar_x + fill_width, bar_y, bar_h, glow_color);
    if (fill_width > 2) {
      M5.Display.drawFastVLine(bar_x + fill_width - 1, bar_y, bar_h, glow_color);
    }
  }

  // Update status text based on progress
  M5.Display.fillRect(30, screen_h - 50, 400, 20, M5.Display.color565(20, 20, 40));
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(M5.Display.color565(150, 255, 150));
  M5.Display.setTextDatum(textdatum_t::middle_left);

  if (progress_percent < 30) {
    M5.Display.drawString("INITIALIZING SYSTEMS...", 30, screen_h - 35);
  } else if (progress_percent < 60) {
    M5.Display.drawString("LOADING CAN PROTOCOLS...", 30, screen_h - 35);
  } else if (progress_percent < 90) {
    M5.Display.drawString("CONNECTING TO ECU...", 30, screen_h - 35);
  } else {
    M5.Display.setTextColor(M5.Display.color565(255, 255, 100));
    M5.Display.drawString("READY TO GO! 準備完了！", 30, screen_h - 35);
  }
}

void drawAnimeSplashScreen() {
  // Get screen dimensions
  int screen_w = M5.Display.width();
  int screen_h = M5.Display.height();

  // Anime-style gradient background (deep purple to black)
  for (int y = 0; y < screen_h; y++) {
    uint16_t color = M5.Display.color565(
      map(y, 0, screen_h, 80, 0),   // Red: 80 -> 0
      map(y, 0, screen_h, 20, 0),   // Green: 20 -> 0
      map(y, 0, screen_h, 120, 40)  // Blue: 120 -> 40
    );
    M5.Display.drawFastHLine(0, y, screen_w, color);
  }

  // Anime-style energy lines (diagonal streaks)
  uint16_t cyan = M5.Display.color565(0, 255, 255);
  uint16_t magenta = M5.Display.color565(255, 0, 255);
  uint16_t yellow = M5.Display.color565(255, 255, 0);

  // Draw energy streaks
  for (int i = 0; i < 8; i++) {
    int x1 = random(0, screen_w/3);
    int y1 = random(0, screen_h);
    int x2 = x1 + random(100, 300);
    int y2 = y1 + random(-50, 50);

    uint16_t streak_color = (i % 3 == 0) ? cyan : (i % 3 == 1) ? magenta : yellow;
    M5.Display.drawLine(x1, y1, x2, y2, streak_color);
    M5.Display.drawLine(x1+1, y1, x2+1, y2, streak_color);
  }

  // Main title - Japanese style
  M5.Display.setTextDatum(textdatum_t::middle_center);

  // Large main title with glow effect
  M5.Display.setTextSize(4);
  // Glow effect (multiple offset draws)
  uint16_t glow_color = M5.Display.color565(100, 200, 255);
  for (int offset = 3; offset >= 1; offset--) {
    M5.Display.setTextColor(glow_color);
    M5.Display.drawString("LINK G4X", screen_w/2 + offset, screen_h/2 - 80 + offset);
    M5.Display.drawString("LINK G4X", screen_w/2 - offset, screen_h/2 - 80 - offset);
  }

  // Main title
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString("LINK G4X", screen_w/2, screen_h/2 - 80);

  // Subtitle with Japanese characters
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(cyan);
  M5.Display.drawString("モニター", screen_w/2, screen_h/2 - 30); // "Monitor" in Japanese

  // English subtitle
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(M5.Display.color565(200, 200, 200));
  M5.Display.drawString("RACING DASHBOARD SYSTEM", screen_w/2, screen_h/2 + 10);

  // Anime-style hexagonal frame around center
  int hex_size = 120;
  int center_x = screen_w/2;
  int center_y = screen_h/2 - 20;

  // Draw hexagon outline
  uint16_t hex_color = M5.Display.color565(0, 255, 150);
  for (int i = 0; i < 6; i++) {
    float angle1 = i * PI / 3;
    float angle2 = (i + 1) * PI / 3;
    int x1 = center_x + hex_size * cos(angle1);
    int y1 = center_y + hex_size * sin(angle1);
    int x2 = center_x + hex_size * cos(angle2);
    int y2 = center_y + hex_size * sin(angle2);
    M5.Display.drawLine(x1, y1, x2, y2, hex_color);
    M5.Display.drawLine(x1+1, y1, x2+1, y2, hex_color);
  }

  // Corner decorative elements (anime-style)
  uint16_t accent_color = M5.Display.color565(255, 100, 0);

  // Top-left corner
  M5.Display.drawLine(20, 20, 80, 20, accent_color);
  M5.Display.drawLine(20, 20, 20, 80, accent_color);
  M5.Display.drawLine(20, 25, 75, 25, accent_color);
  M5.Display.drawLine(25, 20, 25, 75, accent_color);

  // Top-right corner
  M5.Display.drawLine(screen_w-80, 20, screen_w-20, 20, accent_color);
  M5.Display.drawLine(screen_w-20, 20, screen_w-20, 80, accent_color);
  M5.Display.drawLine(screen_w-75, 25, screen_w-20, 25, accent_color);
  M5.Display.drawLine(screen_w-25, 20, screen_w-25, 75, accent_color);

  // Bottom status bar
  M5.Display.fillRect(0, screen_h-60, screen_w, 60, M5.Display.color565(20, 20, 40));
  M5.Display.drawLine(0, screen_h-60, screen_w, screen_h-60, cyan);

  // Status text
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(M5.Display.color565(150, 255, 150));
  M5.Display.setTextDatum(textdatum_t::middle_left);
  M5.Display.drawString("SYSTEM INITIALIZING...", 30, screen_h-35);

  // Anime-style loading bar
  int bar_w = 300;
  int bar_h = 8;
  int bar_x = (screen_w - bar_w) / 2;
  int bar_y = screen_h - 20;

  M5.Display.drawRect(bar_x, bar_y, bar_w, bar_h, cyan);

  // Static loading bar for splash (will be animated during initialization)
  M5.Display.fillRect(bar_x + 1, bar_y + 1, bar_w - 2, bar_h - 2,
                     M5.Display.color565(0, 255, 200));

  // Version info
  M5.Display.setTextDatum(textdatum_t::middle_right);
  M5.Display.setTextColor(M5.Display.color565(100, 100, 100));
  M5.Display.drawString("v2.0.0", screen_w - 30, screen_h - 35);
}

// ========== 90's JDM CONFIGURATION PAGE ==========
void drawJDMConfigSection(const char* title, const char* japanese_title, int y, const char* value, uint16_t accent_color);
void showCANIDCalculator();
bool handleCalculatorTouch(int x, int y);

bool calculator_mode = false;
uint32_t calculator_value = 0;

// Application states
enum AppMode {
  MODE_CONFIG,
  MODE_GAUGES,
  MODE_CONTROL
};

AppMode current_mode = MODE_GAUGES;

// Control interface presets
enum ControlPreset {
  PRESET_STREET = 0,
  PRESET_TRACK = 1,
  PRESET_DRAG = 2,
  PRESET_SAFE = 3
};

ControlPreset current_preset = PRESET_STREET;

void showConfigurationPage() {
  int screen_w = M5.Display.width();
  int screen_h = M5.Display.height();

  // 90's JDM gradient background (dark blue to black with grid pattern)
  for (int y = 0; y < screen_h; y++) {
    uint16_t color = M5.Display.color565(
      map(y, 0, screen_h, 0, 20),    // Red: 0 -> 20
      map(y, 0, screen_h, 40, 0),    // Green: 40 -> 0
      map(y, 0, screen_h, 80, 30)    // Blue: 80 -> 30
    );
    M5.Display.drawFastHLine(0, y, screen_w, color);
  }

  // Draw retro grid pattern (like 90's car computers)
  uint16_t grid_color = M5.Display.color565(0, 80, 120);
  for (int x = 0; x < screen_w; x += 40) {
    M5.Display.drawFastVLine(x, 0, screen_h, grid_color);
  }
  for (int y = 0; y < screen_h; y += 30) {
    M5.Display.drawFastHLine(0, y, screen_w, grid_color);
  }

  // Header with Japanese styling
  M5.Display.fillRect(0, 0, screen_w, 80, M5.Display.color565(20, 20, 60));
  M5.Display.drawLine(0, 80, screen_w, 80, M5.Display.color565(0, 255, 255));
  M5.Display.drawLine(0, 78, screen_w, 78, M5.Display.color565(0, 200, 255));

  // Title with glow effect
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.setTextSize(3);

  // Glow effect
  uint16_t glow_color = M5.Display.color565(100, 200, 255);
  for (int offset = 2; offset >= 1; offset--) {
    M5.Display.setTextColor(glow_color);
    M5.Display.drawString("SYSTEM CONFIG", screen_w/2 + offset, 25 + offset);
    M5.Display.drawString("SYSTEM CONFIG", screen_w/2 - offset, 25 - offset);
  }

  // Main title
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString("SYSTEM CONFIG", screen_w/2, 25);

  // Japanese subtitle
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(M5.Display.color565(0, 255, 255));
  M5.Display.drawString("システム設定", screen_w/2, 55); // "System Settings" in Japanese

  // Configuration sections with 90's JDM styling
  int section_y = 100;
  int section_h = 90;
  int section_spacing = 10;

  // Data Source Section
  drawJDMConfigSection("DATA SOURCE", "データソース", section_y,
                      config.simulation_mode ? "SIMULATION" : "LIVE CAN",
                      config.simulation_mode ? M5.Display.color565(255, 150, 0) : M5.Display.color565(0, 255, 100));

  section_y += section_h + section_spacing;

  // Stream Type Section
  drawJDMConfigSection("STREAM TYPE", "ストリーム", section_y,
                      config.use_custom_streams ? "CUSTOM" : "HALTECH IC7",
                      config.use_custom_streams ? M5.Display.color565(0, 255, 200) : M5.Display.color565(255, 100, 255));

  section_y += section_h + section_spacing;

  // CAN Speed Section
  char can_speed_text[20];
  sprintf(can_speed_text, "%d KBPS", config.can_speed / 1000);
  drawJDMConfigSection("CAN SPEED", "CAN速度", section_y, can_speed_text, M5.Display.color565(255, 255, 0));

  section_y += section_h + section_spacing;

  // CAN ID Section
  char can_id_text[20];
  sprintf(can_id_text, "%d", config.base_can_id);
  drawJDMConfigSection("CAN BASE ID", "CAN ID", section_y, can_id_text, M5.Display.color565(255, 100, 255));

  section_y += section_h + section_spacing;

  // Units Section
  drawJDMConfigSection("UNITS", "単位", section_y, getUnitSystemName(),
                      config.units == METRIC ? M5.Display.color565(100, 255, 100) : M5.Display.color565(255, 165, 0));

  section_y += section_h + section_spacing;

  // CAN Logging Mode Section
  drawJDMConfigSection("LOG MODE", "ログモード", section_y, getLoggingModeName(),
                      isLoggingEnabled() ? M5.Display.color565(255, 100, 100) : M5.Display.color565(100, 100, 100));

  section_y += section_h + section_spacing;

  // Log Detail Section (only show if logging is enabled)
  if (isLoggingEnabled()) {
    drawJDMConfigSection("LOG DETAIL", "ログ詳細", section_y, getLogDetailName(),
                        M5.Display.color565(100, 255, 255));

    section_y += section_h + section_spacing;
  }

  // Buffer Size Section (only show if logging is enabled)
  if (isLoggingEnabled()) {
    char buffer_text[30];
    sprintf(buffer_text, "%s (%d)", getBufferSizeName(), getBufferFrameCount());
    drawJDMConfigSection("BUFFER SIZE", "バッファサイズ", section_y, buffer_text,
                        M5.Display.color565(255, 255, 100));

    section_y += section_h + section_spacing;
  }

  // Storage Settings Section (only show if logging is enabled)
  if (isLoggingEnabled()) {
    char storage_text[30];
    sprintf(storage_text, "%dMB x%d", config.max_file_size_mb, config.max_files);
    drawJDMConfigSection("STORAGE", "ストレージ", section_y, storage_text,
                        M5.Display.color565(255, 165, 0));
  }

  // Bottom navigation bar with retro styling
  M5.Display.fillRect(0, screen_h - 80, screen_w, 80, M5.Display.color565(30, 30, 30));
  M5.Display.drawLine(0, screen_h - 80, screen_w, screen_h - 80, M5.Display.color565(0, 255, 255));

  // Navigation buttons
  int nav_button_w = 150;
  int nav_button_h = 50;
  int nav_y = screen_h - 65;

  // GAUGES button
  M5.Display.fillRoundRect(50, nav_y, nav_button_w, nav_button_h, 8, M5.Display.color565(60, 120, 60));
  M5.Display.drawRoundRect(50, nav_y, nav_button_w, nav_button_h, 8, M5.Display.color565(100, 255, 100));
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString("GAUGES", 50 + nav_button_w/2, nav_y + nav_button_h/2);

  // Status indicator
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(M5.Display.color565(200, 200, 200));
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString("CONFIG MODE", screen_w/2, screen_h - 35);

  M5.Display.setTextDatum(textdatum_t::middle_right);
  M5.Display.setTextColor(M5.Display.color565(0, 255, 100));
  M5.Display.drawString("READY", screen_w - 20, screen_h - 35);

  // Corner accent lines (90's style)
  uint16_t accent_color = M5.Display.color565(255, 0, 150);

  // Top corners
  M5.Display.drawLine(0, 0, 60, 0, accent_color);
  M5.Display.drawLine(0, 0, 0, 40, accent_color);
  M5.Display.drawLine(screen_w-60, 0, screen_w, 0, accent_color);
  M5.Display.drawLine(screen_w, 0, screen_w, 40, accent_color);

  // Bottom corners
  M5.Display.drawLine(0, screen_h, 60, screen_h, accent_color);
  M5.Display.drawLine(0, screen_h-40, 0, screen_h, accent_color);
  M5.Display.drawLine(screen_w-60, screen_h, screen_w, screen_h, accent_color);
  M5.Display.drawLine(screen_w, screen_h-40, screen_w, screen_h, accent_color);
}

void drawJDMConfigSection(const char* title, const char* japanese_title, int y, const char* value, uint16_t accent_color) {
  int screen_w = M5.Display.width();
  int section_w = screen_w - 40;
  int section_x = 20;
  int section_h = 80;

  // Section background with 90's styling
  M5.Display.fillRoundRect(section_x, y, section_w, section_h, 8, M5.Display.color565(40, 40, 80));
  M5.Display.drawRoundRect(section_x, y, section_w, section_h, 8, accent_color);
  M5.Display.drawRoundRect(section_x+1, y+1, section_w-2, section_h-2, 7, accent_color);

  // Title section
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(textdatum_t::middle_left);
  M5.Display.drawString(title, section_x + 15, y + 20);

  // Japanese subtitle
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(M5.Display.color565(150, 150, 150));
  M5.Display.drawString(japanese_title, section_x + 15, y + 40);

  // Value with highlight
  M5.Display.fillRoundRect(section_x + section_w - 200, y + 10, 180, 30, 5, M5.Display.color565(20, 20, 20));
  M5.Display.drawRoundRect(section_x + section_w - 200, y + 10, 180, 30, 5, accent_color);

  M5.Display.setTextSize(2);
  M5.Display.setTextColor(accent_color);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString(value, section_x + section_w - 110, y + 25);

  // Status indicator (animated dot)
  static unsigned long last_blink = 0;
  static bool blink_state = false;
  if (millis() - last_blink > 500) {
    blink_state = !blink_state;
    last_blink = millis();
  }

  if (blink_state) {
    M5.Display.fillCircle(section_x + section_w - 25, y + 25, 4, accent_color);
  }

  // Decorative elements
  M5.Display.drawLine(section_x + 10, y + 55, section_x + section_w - 10, y + 55, M5.Display.color565(80, 80, 120));
  M5.Display.drawLine(section_x + 10, y + 65, section_x + section_w - 10, y + 65, M5.Display.color565(60, 60, 100));
}

void showCANIDCalculator() {
  int screen_w = M5.Display.width();
  int screen_h = M5.Display.height();

  // Semi-transparent overlay
  M5.Display.fillRect(0, 0, screen_w, screen_h, M5.Display.color565(0, 0, 0));

  // Calculator modal background
  int modal_w = 600;
  int modal_h = 500;
  int modal_x = (screen_w - modal_w) / 2;
  int modal_y = (screen_h - modal_h) / 2;

  M5.Display.fillRoundRect(modal_x, modal_y, modal_w, modal_h, 15, M5.Display.color565(30, 30, 80));
  M5.Display.drawRoundRect(modal_x, modal_y, modal_w, modal_h, 15, M5.Display.color565(255, 100, 255));
  M5.Display.drawRoundRect(modal_x+1, modal_y+1, modal_w-2, modal_h-2, 14, M5.Display.color565(255, 100, 255));

  // Title
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString("CAN BASE ID", modal_x + modal_w/2, modal_y + 30);

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(M5.Display.color565(150, 150, 150));
  M5.Display.drawString("CAN IDベース", modal_x + modal_w/2, modal_y + 55);

  // Display current value
  M5.Display.fillRoundRect(modal_x + 50, modal_y + 80, modal_w - 100, 50, 8, M5.Display.color565(0, 0, 0));
  M5.Display.drawRoundRect(modal_x + 50, modal_y + 80, modal_w - 100, 50, 8, M5.Display.color565(0, 255, 255));

  char value_text[20];
  sprintf(value_text, "%d", calculator_value);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(M5.Display.color565(0, 255, 255));
  M5.Display.drawString(value_text, modal_x + modal_w/2, modal_y + 105);

  // Calculator buttons (decimal keypad)
  int button_w = 80;
  int button_h = 60;
  int button_spacing = 10;
  int grid_x = modal_x + 50;
  int grid_y = modal_y + 150;

  const char* buttons[] = {
    "1", "2", "3", "⌫",
    "4", "5", "6", "+10",
    "7", "8", "9", "+100",
    "0", "00", "+1", "+1000"
  };

  M5.Display.setTextSize(2);
  for (int i = 0; i < 16; i++) {
    int col = i % 4;
    int row = i / 4;
    int x = grid_x + col * (button_w + button_spacing);
    int y = grid_y + row * (button_h + button_spacing);

    // Button background - different colors for different functions
    uint16_t btn_color;
    if (i == 3) btn_color = M5.Display.color565(120, 60, 60);      // Backspace (red)
    else if (i == 7 || i == 11 || i == 15) btn_color = M5.Display.color565(60, 120, 60); // Add functions (green)
    else if (i == 13 || i == 14) btn_color = M5.Display.color565(80, 80, 120); // Special buttons
    else btn_color = M5.Display.color565(60, 60, 120);            // Number buttons (blue)

    M5.Display.fillRoundRect(x, y, button_w, button_h, 8, btn_color);
    M5.Display.drawRoundRect(x, y, button_w, button_h, 8, M5.Display.color565(255, 100, 255));

    // Button text - smaller for multi-character buttons
    M5.Display.setTextColor(TFT_WHITE);
    if (i == 7 || i == 11 || i == 15) {
      M5.Display.setTextSize(1); // Smaller text for +10, +100, +1000
      M5.Display.drawString(buttons[i], x + button_w/2, y + button_h/2);
      M5.Display.setTextSize(2); // Reset to normal size
    } else {
      M5.Display.drawString(buttons[i], x + button_w/2, y + button_h/2);
    }
  }

  // Control buttons
  int ctrl_y = modal_y + modal_h - 80;

  // Clear button
  M5.Display.fillRoundRect(modal_x + 50, ctrl_y, 120, 50, 8, M5.Display.color565(120, 60, 60));
  M5.Display.drawRoundRect(modal_x + 50, ctrl_y, 120, 50, 8, M5.Display.color565(255, 100, 100));
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString("CLEAR", modal_x + 110, ctrl_y + 25);

  // OK button
  M5.Display.fillRoundRect(modal_x + 200, ctrl_y, 120, 50, 8, M5.Display.color565(60, 120, 60));
  M5.Display.drawRoundRect(modal_x + 200, ctrl_y, 120, 50, 8, M5.Display.color565(100, 255, 100));
  M5.Display.drawString("OK", modal_x + 260, ctrl_y + 25);

  // Cancel button
  M5.Display.fillRoundRect(modal_x + 350, ctrl_y, 120, 50, 8, M5.Display.color565(80, 80, 80));
  M5.Display.drawRoundRect(modal_x + 350, ctrl_y, 120, 50, 8, M5.Display.color565(200, 200, 200));
  M5.Display.drawString("CANCEL", modal_x + 410, ctrl_y + 25);
}

// ========== PLACEHOLDER GAUGE TEMPLATE ==========
void drawPlaceholderGauge(int x, int y, int w, int h, const char* title, const char* japanese_title, uint16_t accent_color) {
  // Gauge background with 90's JDM styling
  M5.Display.fillRoundRect(x, y, w, h, 8, M5.Display.color565(40, 40, 80));
  M5.Display.drawRoundRect(x, y, w, h, 8, accent_color);
  M5.Display.drawRoundRect(x+1, y+1, w-2, h-2, 7, accent_color);

  // Title at top
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(accent_color);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString(title, x + w/2, y + 25);

  // Japanese subtitle
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(M5.Display.color565(150, 150, 150));
  M5.Display.drawString(japanese_title, x + w/2, y + 45);

  // "COMING SOON" message
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString("COMING", x + w/2, y + h/2 - 10);
  M5.Display.drawString("SOON", x + w/2, y + h/2 + 15);

  // Corner accent lines (90's JDM style)
  M5.Display.drawLine(x + 5, y + 5, x + 15, y + 5, accent_color);
  M5.Display.drawLine(x + 5, y + 5, x + 5, y + 15, accent_color);
  M5.Display.drawLine(x + w - 15, y + 5, x + w - 5, y + 5, accent_color);
  M5.Display.drawLine(x + w - 5, y + 5, x + w - 5, y + 15, accent_color);
  M5.Display.drawLine(x + 5, y + h - 15, x + 5, y + h - 5, accent_color);
  M5.Display.drawLine(x + 5, y + h - 5, x + 15, y + h - 5, accent_color);
  M5.Display.drawLine(x + w - 15, y + h - 5, x + w - 5, y + h - 5, accent_color);
  M5.Display.drawLine(x + w - 5, y + h - 15, x + w - 5, y + h - 5, accent_color);
}

// ========== AUTOMOTIVE RPM GAUGE ==========
static float last_rpm_gauge_value = -1.0;
static LGFX_Sprite rpm_gauge_sprite(&M5.Display);
static bool rpm_gauge_sprite_created = false;

void drawRPMGauge(int x, int y, int w, int h) {
  Serial.printf("RPM gauge called: x=%d, y=%d, w=%d, h=%d\n", x, y, w, h);

  // Validate input parameters
  if (w <= 0 || h <= 0) {
    Serial.printf("ERROR: Invalid RPM gauge dimensions: w=%d, h=%d\n", w, h);
    return;
  }

  // Only redraw if RPM changed significantly or first time
  if (abs(ecu_data.rpm - last_rpm_gauge_value) < 10 && last_rpm_gauge_value != -1.0) {
    return; // Skip redraw if change is small
  }

  // Create sprite if not already created
  if (!rpm_gauge_sprite_created) {
    bool success = rpm_gauge_sprite.createSprite(w, h);
    if (!success) {
      Serial.printf("ERROR: Failed to create RPM sprite %dx%d\n", w, h);
      return;
    }
    rpm_gauge_sprite_created = true;
    Serial.printf("RPM sprite created successfully: %dx%d\n", w, h);
  }

  // Clear sprite with dark automotive background
  rpm_gauge_sprite.fillSprite(M5.Display.color565(20, 20, 40));

  // Modern automotive gauge border
  uint16_t border_color = M5.Display.color565(255, 80, 80); // Red accent
  rpm_gauge_sprite.drawRoundRect(0, 0, w, h, 12, border_color);
  rpm_gauge_sprite.drawRoundRect(1, 1, w-2, h-2, 11, M5.Display.color565(180, 60, 60));

  // Main RPM value (large, center) - automotive dashboard style
  rpm_gauge_sprite.setTextSize(6);
  rpm_gauge_sprite.setTextColor(TFT_WHITE);
  rpm_gauge_sprite.setTextDatum(textdatum_t::middle_center);

  char rpm_text[10];
  sprintf(rpm_text, "%.0f", ecu_data.rpm);
  rpm_gauge_sprite.drawString(rpm_text, w/2, h/2);

  // RPM label (bottom-left) - no redundant unit since label and unit match
  rpm_gauge_sprite.setTextSize(2);
  rpm_gauge_sprite.setTextColor(border_color);
  rpm_gauge_sprite.setTextDatum(textdatum_t::bottom_left);
  rpm_gauge_sprite.drawString("RPM", 15, h - 15);

  // RPM range indicator (bottom-right for context)
  rpm_gauge_sprite.setTextSize(1);
  rpm_gauge_sprite.setTextColor(M5.Display.color565(150, 150, 150));
  rpm_gauge_sprite.setTextDatum(textdatum_t::bottom_right);
  rpm_gauge_sprite.drawString("x1000", w - 15, h - 15);

  // Redline warning (visual indicator for high RPM)
  if (ecu_data.rpm > 6500) {
    // Flash red border for redline warning
    uint16_t warning_color = M5.Display.color565(255, 0, 0);
    rpm_gauge_sprite.drawRoundRect(2, 2, w-4, h-4, 10, warning_color);
    rpm_gauge_sprite.drawRoundRect(3, 3, w-6, h-6, 9, warning_color);
  } else if (ecu_data.rpm > 6000) {
    // Yellow caution zone
    uint16_t caution_color = M5.Display.color565(255, 255, 0);
    rpm_gauge_sprite.drawRoundRect(2, 2, w-4, h-4, 10, caution_color);
  }

  // Push sprite to display in one atomic operation (no flicker)
  rpm_gauge_sprite.pushSprite(x, y);

  last_rpm_gauge_value = ecu_data.rpm;
}

// ========== DIGITAL TPS GAUGE ==========
static float last_tps_value = -1.0;
static LGFX_Sprite tps_sprite(&M5.Display);
static bool tps_sprite_created = false;

void drawTPSGauge(int x, int y, int w, int h) {
  if (abs(ecu_data.tps - last_tps_value) < 1 && last_tps_value != -1.0) {
    return;
  }

  if (!tps_sprite_created) {
    tps_sprite.createSprite(w, h);
    tps_sprite_created = true;
  }

  tps_sprite.fillSprite(M5.Display.color565(40, 40, 80));
  tps_sprite.drawRoundRect(0, 0, w, h, 8, M5.Display.color565(100, 255, 100));
  tps_sprite.drawRoundRect(1, 1, w-2, h-2, 7, M5.Display.color565(80, 200, 80));

  // Main TPS value (large, center) - sized for consistency
  tps_sprite.setTextSize(6);
  tps_sprite.setTextColor(TFT_WHITE);
  tps_sprite.setTextDatum(textdatum_t::middle_center);

  char tps_text[10];
  sprintf(tps_text, "%.0f", ecu_data.tps);
  tps_sprite.drawString(tps_text, w/2, h/2);

  // Gauge label (lower-left) - smaller for better spacing
  tps_sprite.setTextSize(2);
  tps_sprite.setTextColor(M5.Display.color565(100, 255, 100));
  tps_sprite.setTextDatum(textdatum_t::bottom_left);
  tps_sprite.drawString("TPS", 10, h - 10);

  // Percent symbol (lower-right)
  tps_sprite.setTextSize(2);
  tps_sprite.setTextColor(M5.Display.color565(150, 255, 150));
  tps_sprite.setTextDatum(textdatum_t::bottom_right);
  tps_sprite.drawString("%", w - 10, h - 10);

  // Corner accent lines
  tps_sprite.drawLine(5, 5, 15, 5, M5.Display.color565(100, 255, 100));
  tps_sprite.drawLine(5, 5, 5, 15, M5.Display.color565(100, 255, 100));
  tps_sprite.drawLine(w - 15, 5, w - 5, 5, M5.Display.color565(100, 255, 100));
  tps_sprite.drawLine(w - 5, 5, w - 5, 15, M5.Display.color565(100, 255, 100));

  tps_sprite.pushSprite(x, y);
  last_tps_value = ecu_data.tps;
}

// ========== DIGITAL MGP GAUGE ==========
static float last_mgp_value = -999.0;
static LGFX_Sprite mgp_sprite(&M5.Display);
static bool mgp_sprite_created = false;

void drawMGPGauge(int x, int y, int w, int h) {
  if (abs(ecu_data.mgp - last_mgp_value) < 1 && last_mgp_value != -999.0) {
    return;
  }

  if (!mgp_sprite_created) {
    mgp_sprite.createSprite(w, h);
    mgp_sprite_created = true;
  }

  mgp_sprite.fillSprite(M5.Display.color565(40, 40, 80));
  mgp_sprite.drawRoundRect(0, 0, w, h, 8, M5.Display.color565(100, 100, 255));
  mgp_sprite.drawRoundRect(1, 1, w-2, h-2, 7, M5.Display.color565(80, 80, 200));

  // Main MGP value (large, center) - sized for consistency
  mgp_sprite.setTextSize(6);
  mgp_sprite.setTextColor(TFT_WHITE);
  mgp_sprite.setTextDatum(textdatum_t::middle_center);

  char mgp_text[10];
  sprintf(mgp_text, "%.0f", ecu_data.mgp);
  mgp_sprite.drawString(mgp_text, w/2, h/2);

  // Gauge label (lower-left) - smaller for better spacing
  mgp_sprite.setTextSize(2);
  mgp_sprite.setTextColor(M5.Display.color565(100, 100, 255));
  mgp_sprite.setTextDatum(textdatum_t::bottom_left);
  mgp_sprite.drawString("MGP", 10, h - 10);

  // kPa units (lower-right)
  mgp_sprite.setTextSize(2);
  mgp_sprite.setTextColor(M5.Display.color565(150, 150, 255));
  mgp_sprite.setTextDatum(textdatum_t::bottom_right);
  mgp_sprite.drawString("kPa", w - 10, h - 10);

  // Corner accent lines
  mgp_sprite.drawLine(5, 5, 15, 5, M5.Display.color565(100, 100, 255));
  mgp_sprite.drawLine(5, 5, 5, 15, M5.Display.color565(100, 100, 255));
  mgp_sprite.drawLine(w - 15, 5, w - 5, 5, M5.Display.color565(100, 100, 255));
  mgp_sprite.drawLine(w - 5, 5, w - 5, 15, M5.Display.color565(100, 100, 255));

  mgp_sprite.pushSprite(x, y);
  last_mgp_value = ecu_data.mgp;
}



// ========== DIGITAL IAT GAUGE ==========
static float last_iat_value = -999.0;
static LGFX_Sprite iat_sprite(&M5.Display);
static bool iat_sprite_created = false;

void drawIATGauge(int x, int y, int w, int h) {
  if (abs(ecu_data.iat - last_iat_value) < 1 && last_iat_value != -999.0) {
    return;
  }

  if (!iat_sprite_created) {
    iat_sprite.createSprite(w, h);
    iat_sprite_created = true;
  }

  iat_sprite.fillSprite(M5.Display.color565(40, 40, 80));
  iat_sprite.drawRoundRect(0, 0, w, h, 8, M5.Display.color565(100, 255, 255));
  iat_sprite.drawRoundRect(1, 1, w-2, h-2, 7, M5.Display.color565(80, 200, 200));

  // Main IAT display
  iat_sprite.setTextSize(6);
  iat_sprite.setTextColor(TFT_WHITE);
  iat_sprite.setTextDatum(textdatum_t::middle_center);

  char iat_text[10];
  sprintf(iat_text, "%.0f", ecu_data.iat);
  iat_sprite.drawString(iat_text, w/2, h/2);

  // Gauge label (lower-left) - following memory rules
  iat_sprite.setTextSize(2);
  iat_sprite.setTextColor(M5.Display.color565(100, 255, 255));
  iat_sprite.setTextDatum(textdatum_t::bottom_left);
  iat_sprite.drawString("IAT", 10, h - 10);

  // °C units (lower-right) - following memory rules
  iat_sprite.setTextSize(2);
  iat_sprite.setTextColor(M5.Display.color565(150, 255, 255));
  iat_sprite.setTextDatum(textdatum_t::bottom_right);
  iat_sprite.drawString("°C", w - 10, h - 10);

  // Corner accent lines
  iat_sprite.drawLine(5, 5, 15, 5, M5.Display.color565(100, 255, 255));
  iat_sprite.drawLine(5, 5, 5, 15, M5.Display.color565(100, 255, 255));
  iat_sprite.drawLine(w - 15, 5, w - 5, 5, M5.Display.color565(100, 255, 255));
  iat_sprite.drawLine(w - 5, 5, w - 5, 15, M5.Display.color565(100, 255, 255));

  iat_sprite.pushSprite(x, y);
  last_iat_value = ecu_data.iat;
}









// ========== FULL-WIDTH LAMBDA GAUGE ==========
static float last_lambda_value = -1.0;
static float last_lambda_target = -1.0;
static LGFX_Sprite lambda_sprite(&M5.Display); // Create sprite for off-screen rendering
static bool lambda_sprite_created = false;

void drawLambdaGauge(int x, int y, int w, int h) {
  Serial.printf("Lambda gauge called: x=%d, y=%d, w=%d, h=%d\n", x, y, w, h);
  Serial.printf("Lambda values: actual=%.3f, target=%.3f, last=%.3f\n", ecu_data.lambda, ecu_data.lambda_target, last_lambda_value);

  // Validate input parameters
  if (w <= 0 || h <= 0) {
    Serial.printf("ERROR: Invalid lambda gauge dimensions: w=%d, h=%d\n", w, h);
    return;
  }

  // Only redraw if values changed significantly or first time
  if (abs(ecu_data.lambda - last_lambda_value) < 0.005 &&
      abs(ecu_data.lambda_target - last_lambda_target) < 0.005 &&
      last_lambda_value != -1.0) {
    return; // Skip redraw if changes are small
  }

  // MEMORY-EFFICIENT: Draw directly to display (no sprite)
  // This eliminates memory allocation issues completely
  Serial.printf("Drawing lambda gauge directly to display (no sprite)\n");

  // Clear gauge area directly on display
  M5.Display.fillRect(x, y, w, h, M5.Display.color565(40, 40, 80));

  // Draw gauge border directly to display
  M5.Display.drawRoundRect(x, y, w, h, 8, M5.Display.color565(0, 255, 255));

  // Horizontal bar for rich/stoich/lean zones - smaller
  int bar_x = x + 60;
  int bar_y = y + 35;
  int bar_w = w - 120;
  int bar_h = 20; // Smaller bar to make room for labels

  // Draw rich/stoich/lean zones directly to display
  int rich_w = bar_w * 0.3;    // 0.6-0.9 (rich zone)
  int stoich_w = bar_w * 0.4;  // 0.9-1.1 (stoichiometric zone)
  int lean_w = bar_w * 0.3;    // 1.1-1.4 (lean zone)

  // Rich zone (red)
  M5.Display.fillRect(bar_x, bar_y, rich_w, bar_h, M5.Display.color565(255, 100, 100));

  // Stoich zone (green)
  M5.Display.fillRect(bar_x + rich_w, bar_y, stoich_w, bar_h, M5.Display.color565(100, 255, 100));

  // Lean zone (blue)
  M5.Display.fillRect(bar_x + rich_w + stoich_w, bar_y, lean_w, bar_h, M5.Display.color565(100, 150, 255));

  // Bar outline
  M5.Display.drawRect(bar_x, bar_y, bar_w, bar_h, TFT_WHITE);

  // Zone labels (above the bar) - smaller to fit better
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString("RICH", bar_x + rich_w/2, bar_y - 12);
  M5.Display.drawString("STOICH", bar_x + rich_w + stoich_w/2, bar_y - 12);
  M5.Display.drawString("LEAN", bar_x + rich_w + stoich_w + lean_w/2, bar_y - 12);

  // Lambda 1 triangle (pointing up, larger)
  float lambda_norm = (ecu_data.lambda - 0.6) / 0.8; // Normalize 0.6-1.4 to 0-1
  lambda_norm = constrain(lambda_norm, 0.0, 1.0);
  int lambda_x = bar_x + (lambda_norm * bar_w);

  uint16_t lambda_color = M5.Display.color565(255, 255, 100);
  M5.Display.fillTriangle(lambda_x, bar_y - 5, lambda_x - 12, bar_y - 20, lambda_x + 12, bar_y - 20, lambda_color);
  M5.Display.drawTriangle(lambda_x, bar_y - 5, lambda_x - 12, bar_y - 20, lambda_x + 12, bar_y - 20, TFT_BLACK);

  // Lambda Target triangle (pointing down, larger)
  float target_norm = (ecu_data.lambda_target - 0.6) / 0.8;
  target_norm = constrain(target_norm, 0.0, 1.0);
  int target_x = bar_x + (target_norm * bar_w);

  uint16_t target_color = M5.Display.color565(255, 255, 255);
  M5.Display.fillTriangle(target_x, bar_y + bar_h + 5, target_x - 12, bar_y + bar_h + 20, target_x + 12, bar_y + bar_h + 20, target_color);
  M5.Display.drawTriangle(target_x, bar_y + bar_h + 5, target_x - 12, bar_y + bar_h + 20, target_x + 12, bar_y + bar_h + 20, TFT_BLACK);

  // Digital readouts (large and well-spaced for driving visibility)
  int readout_y = y + h - 25; // Bottom of gauge with proper spacing

  // Lambda 1 value (left side) - large
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(lambda_color);
  M5.Display.setTextDatum(textdatum_t::middle_left);
  char lambda_text[15];
  sprintf(lambda_text, "%.3f", ecu_data.lambda);
  M5.Display.drawString(lambda_text, x + 30, readout_y);

  // Lambda Target value (right side) - large
  M5.Display.setTextColor(target_color);
  M5.Display.setTextDatum(textdatum_t::middle_right);
  char target_text[15];
  sprintf(target_text, "%.3f", ecu_data.lambda_target);
  M5.Display.drawString(target_text, x + w - 30, readout_y);

  // Labels for the values - positioned better
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(M5.Display.color565(200, 200, 200));
  M5.Display.setTextDatum(textdatum_t::middle_left);
  M5.Display.drawString("ACTUAL", x + 30, readout_y - 20);
  M5.Display.setTextDatum(textdatum_t::middle_right);
  M5.Display.drawString("TARGET", x + w - 30, readout_y - 20);

  // LAMBDA label centered at bottom
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(M5.Display.color565(0, 255, 255));
  M5.Display.setTextDatum(textdatum_t::bottom_center);
  M5.Display.drawString("LAMBDA", x + w/2, y + h - 5);

  // Update last displayed values
  last_lambda_value = ecu_data.lambda;
  last_lambda_target = ecu_data.lambda_target;
}



// Reset all gauge states to force redraw
// ========== SIMULATION DATA FUNCTIONS ==========

// Simulation state variables
float sim_rpm = 800.0;
float sim_tps = 0.0;
float sim_boost = 0.0;
float sim_iat = 25.0;
float sim_ect = 85.0;
float sim_oil_press = 0.5;
float sim_fuel_press = 3.0;
float sim_battery = 12.6;
float sim_speed = 0.0;
int sim_gear = 1;
float sim_lambda = 1.0;
float sim_lambda_target = 1.0;

// Last displayed values for smart refresh
float last_sim_rpm = -1;
float last_sim_tps = -1;
float last_sim_boost = -1;
float last_sim_iat = -1;
float last_sim_ect = -1;
float last_sim_oil_press = -1;
float last_sim_fuel_press = -1;
float last_sim_battery = -1;
float last_sim_speed = -1;
int last_sim_gear = -1;
float last_sim_lambda = -1;
float last_sim_lambda_target = -1;

// Lambda gauge sprite for smooth updates (reuse existing declaration)

// Simulation timing
unsigned long last_sim_update = 0;
float sim_time = 0.0;
bool sim_engine_running = false;
float sim_throttle_input = 0.0;

void updateSimulationData() {
  unsigned long now = millis();
  if (now - last_sim_update < 50) return; // Update every 50ms

  float dt = (now - last_sim_update) / 1000.0; // Delta time in seconds
  sim_time += dt;
  last_sim_update = now;

  // Realistic automotive simulation

  // Engine state logic
  if (sim_rpm > 600) {
    sim_engine_running = true;
  } else if (sim_rpm < 400) {
    sim_engine_running = false;
  }

  // Throttle input simulation (varies over time)
  sim_throttle_input = (sin(sim_time * 0.3) + 1.0) * 0.5; // 0-1 range
  sim_throttle_input = sim_throttle_input * 0.8 + 0.1 * sin(sim_time * 2.0); // Add variation
  sim_throttle_input = constrain(sim_throttle_input, 0.0, 1.0);

  // TPS follows throttle input with e-throttle map response
  float throttle_response = sim_throttle_input;
  switch (ecu_data.current_ethrottle_map) {
    case 1: throttle_response = pow(sim_throttle_input, 1.5); break; // Smooth/conservative
    case 2: throttle_response = sim_throttle_input; break; // Linear/sport
    case 3: throttle_response = pow(sim_throttle_input, 0.7); break; // Aggressive/sensitive
  }
  sim_tps = throttle_response * 100.0;

  // RPM simulation with realistic behavior and launch control
  float target_rpm = 800.0; // Idle RPM
  if (sim_engine_running) {
    if (ecu_data.launch_control_active && sim_throttle_input > 0.8) {
      // Launch control limits RPM
      target_rpm = ecu_data.launch_rpm;
      target_rpm += sin(sim_time * 25.0) * 100.0; // Launch control bounce
    } else {
      target_rpm = 800.0 + throttle_response * 6500.0; // Use e-throttle response
      target_rpm += sin(sim_time * 15.0) * 50.0; // Engine vibration

      // Anti-lag keeps RPM higher during deceleration
      if (ecu_data.anti_lag_active && sim_throttle_input < 0.2) {
        target_rpm = max(target_rpm, 2000.0f); // Minimum RPM with anti-lag
      }
    }
  }

  // RPM follows target with inertia
  float rpm_rate = sim_engine_running ? 2000.0 : 500.0; // RPM/sec change rate
  if (ecu_data.launch_control_active && sim_throttle_input > 0.8) {
    rpm_rate = 5000.0; // Faster response for launch control
  }

  if (sim_rpm < target_rpm) {
    sim_rpm += rpm_rate * dt;
  } else {
    sim_rpm -= rpm_rate * dt * 1.5; // Faster deceleration
  }
  sim_rpm = constrain(sim_rpm, 0.0, 8000.0);

  // Boost pressure (turbo simulation) - now responds to boost map and adjustment
  float target_boost = 0.0;
  if (sim_engine_running && sim_throttle_input > 0.3 && sim_rpm > 2000) {
    // Base boost varies by map
    float base_boost = 10.0 + (ecu_data.current_boost_map - 1) * 3.0; // Map 1=10psi, Map 4=19psi
    target_boost = (sim_throttle_input - 0.3) * base_boost;
    target_boost *= (sim_rpm - 2000.0) / 4000.0; // RPM dependent
    target_boost += ecu_data.boost_adjustment; // Apply manual adjustment
  }
  sim_boost += (target_boost - sim_boost) * dt * 3.0; // Turbo lag
  sim_boost = constrain(sim_boost, 0.0, 30.0);

  // Engine temperatures
  float target_ect = sim_engine_running ? 88.0 + sim_throttle_input * 15.0 : 25.0;
  sim_ect += (target_ect - sim_ect) * dt * 0.1; // Slow temperature change

  float target_iat = 25.0 + sim_boost * 3.0 + sim_throttle_input * 20.0;
  sim_iat += (target_iat - sim_iat) * dt * 0.5;

  // Oil pressure
  float target_oil_press = sim_engine_running ? 1.0 + sim_rpm * 0.0008 : 0.0;
  sim_oil_press += (target_oil_press - sim_oil_press) * dt * 2.0;
  sim_oil_press = constrain(sim_oil_press, 0.0, 8.0);

  // Fuel pressure
  float target_fuel_press = sim_engine_running ? 3.0 + sim_throttle_input * 1.5 : 0.5;
  sim_fuel_press += (target_fuel_press - sim_fuel_press) * dt * 1.0;

  // Battery voltage
  float target_battery = sim_engine_running ? 13.8 + sin(sim_time * 10.0) * 0.2 : 12.6;
  sim_battery += (target_battery - sim_battery) * dt * 0.5;

  // Speed simulation
  float target_speed = sim_engine_running ? sim_throttle_input * 180.0 : 0.0;
  sim_speed += (target_speed - sim_speed) * dt * 1.5;
  sim_speed = constrain(sim_speed, 0.0, 200.0);

  // Gear simulation (simplified)
  if (sim_speed < 20) sim_gear = 1;
  else if (sim_speed < 50) sim_gear = 2;
  else if (sim_speed < 80) sim_gear = 3;
  else if (sim_speed < 120) sim_gear = 4;
  else sim_gear = 5;

  // Lambda simulation
  if (sim_engine_running) {
    sim_lambda_target = 0.85 + sim_throttle_input * 0.15; // Rich under load
    sim_lambda += (sim_lambda_target - sim_lambda) * dt * 2.0;
    sim_lambda += sin(sim_time * 20.0) * 0.02; // O2 sensor noise
  } else {
    sim_lambda_target = 1.0;
    sim_lambda = 1.0;
  }
  sim_lambda = constrain(sim_lambda, 0.6, 1.4);
}

// ========== EFFICIENT GAUGE UPDATE FUNCTIONS ==========

// Forward declarations
void drawOptimalLambdaGaugeDirect(int x, int y, int w, int h);
void applyPreset(ControlPreset preset);

// Draw static parts of gauge (border, label, unit) without value
void drawGaugeStatic(int x, int y, int w, int h, const char* label, const char* unit, uint16_t color, int label_size = 3) {
  // Clear gauge area
  M5.Display.fillRect(x, y, w, h, M5.Display.color565(20, 20, 40));

  // Modern automotive gauge border
  M5.Display.drawRoundRect(x, y, w, h, 12, color);
  M5.Display.drawRoundRect(x+1, y+1, w-2, h-2, 11, M5.Display.color565(180, 180, 180));

  // Label (bottom-left)
  M5.Display.setTextSize(label_size);
  M5.Display.setTextColor(color);
  M5.Display.setTextDatum(textdatum_t::bottom_left);
  M5.Display.drawString(label, x + 15, y + h - 15);

  // Unit (bottom-right) - only if not empty
  if (strlen(unit) > 0) {
    M5.Display.setTextSize(label_size - 1);
    M5.Display.setTextColor(M5.Display.color565(150, 150, 150));
    M5.Display.setTextDatum(textdatum_t::bottom_right);
    M5.Display.drawString(unit, x + w - 15, y + h - 15);
  }
}

// Efficient digit update - clears area then redraws value
void updateGaugeValue(int x, int y, int w, int h, const char* new_value, const char* old_value, int value_size, uint16_t text_color) {
  // Only update if value changed
  if (strcmp(new_value, old_value) == 0) return;

  // Calculate value area (center of gauge)
  int value_x = x + w/2;
  int value_y = y + h/2;

  // Set text properties
  int actual_size = value_size + 1;
  M5.Display.setTextSize(actual_size);
  M5.Display.setTextDatum(textdatum_t::middle_center);

  // Clear a generous rectangular area around the text center
  // Use a fixed area based on gauge size to avoid calculation errors
  int clear_w = w * 0.6; // Use 60% of gauge width
  int clear_h = h * 0.3; // Use 30% of gauge height
  int clear_x = value_x - clear_w/2;
  int clear_y = value_y - clear_h/2;

  // Ensure clearing area stays within gauge bounds
  clear_x = max(clear_x, x + 5);
  clear_y = max(clear_y, y + 5);
  clear_w = min(clear_w, w - 10);
  clear_h = min(clear_h, h - 10);

  // Clear the area with gauge background color
  M5.Display.fillRect(clear_x, clear_y, clear_w, clear_h, M5.Display.color565(20, 20, 40));

  // Draw new value in proper color
  M5.Display.setTextColor(text_color);
  M5.Display.drawString(new_value, value_x, value_y);
}

// Gauge position storage for efficient updates
struct GaugePosition {
  int x, y, w, h;
  bool initialized;
};

GaugePosition gauge_positions[10]; // For all 10 gauges
bool gauges_layout_initialized = false;

// ========== OPTIMAL AUTOMOTIVE GAUGE FUNCTIONS ==========

// Generic optimal gauge for bunk data display
void drawOptimalGauge(int x, int y, int w, int h, const char* label, const char* value, const char* unit, uint16_t color, int value_size, int label_size = 3) {
  // Clear gauge area
  M5.Display.fillRect(x, y, w, h, M5.Display.color565(20, 20, 40));

  // Modern automotive gauge border
  M5.Display.drawRoundRect(x, y, w, h, 12, color);
  M5.Display.drawRoundRect(x+1, y+1, w-2, h-2, 11, M5.Display.color565(180, 180, 180));

  // Main value (large, center) - increased size
  M5.Display.setTextSize(value_size + 1);  // Increase by 1 size
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString(value, x + w/2, y + h/2);

  // Label (bottom-left) - configurable size
  M5.Display.setTextSize(label_size);
  M5.Display.setTextColor(color);
  M5.Display.setTextDatum(textdatum_t::bottom_left);
  M5.Display.drawString(label, x + 15, y + h - 15);

  // Unit (bottom-right) - proportional to label size
  if (strlen(unit) > 0) {
    M5.Display.setTextSize(label_size - 1);  // One size smaller than label
    M5.Display.setTextColor(M5.Display.color565(150, 150, 150));
    M5.Display.setTextDatum(textdatum_t::bottom_right);
    M5.Display.drawString(unit, x + w - 15, y + h - 15);
  }
}

// Efficient lambda gauge with sprite for smooth updates
void drawOptimalLambdaGauge(int x, int y, int w, int h) {
  // Only redraw if lambda values changed significantly
  if (abs(sim_lambda - last_sim_lambda) < 0.005 &&
      abs(sim_lambda_target - last_sim_lambda_target) < 0.005 &&
      last_sim_lambda != -1) {
    return; // Skip redraw
  }

  // Create sprite if needed (smaller size for memory efficiency)
  if (!lambda_sprite_created) {
    // Use smaller sprite size to avoid memory issues
    int sprite_w = min(w, 600);  // Max 600px width
    int sprite_h = min(h, 180);  // Max 180px height

    if (lambda_sprite.createSprite(sprite_w, sprite_h)) {
      lambda_sprite_created = true;
      Serial.printf("Lambda sprite created: %dx%d (%d KB)\n",
                    sprite_w, sprite_h, (sprite_w * sprite_h * 2) / 1024);
    } else {
      Serial.println("Lambda sprite creation failed - using direct draw");
      // Fall back to direct drawing if sprite fails
      drawOptimalLambdaGaugeDirect(x, y, w, h);
      return;
    }
  }

  // Get sprite dimensions
  int sprite_w = lambda_sprite.width();
  int sprite_h = lambda_sprite.height();

  // Clear sprite
  lambda_sprite.fillSprite(M5.Display.color565(20, 20, 40));

  // Draw to sprite (using sprite coordinates)
  lambda_sprite.drawRoundRect(0, 0, sprite_w, sprite_h, 12, M5.Display.color565(0, 255, 255));

  // Horizontal bar for rich/stoich/lean zones
  int bar_x = 80;
  int bar_y = 60;
  int bar_w = sprite_w - 160;
  int bar_h = 30;

  // Draw rich/stoich/lean zones
  int rich_w = bar_w * 0.3;
  int stoich_w = bar_w * 0.4;
  int lean_w = bar_w * 0.3;

  lambda_sprite.fillRect(bar_x, bar_y, rich_w, bar_h, M5.Display.color565(255, 100, 100));
  lambda_sprite.fillRect(bar_x + rich_w, bar_y, stoich_w, bar_h, M5.Display.color565(100, 255, 100));
  lambda_sprite.fillRect(bar_x + rich_w + stoich_w, bar_y, lean_w, bar_h, M5.Display.color565(100, 150, 255));
  lambda_sprite.drawRect(bar_x, bar_y, bar_w, bar_h, TFT_WHITE);

  // Zone labels
  lambda_sprite.setTextSize(2);
  lambda_sprite.setTextColor(TFT_WHITE);
  lambda_sprite.setTextDatum(textdatum_t::middle_center);
  lambda_sprite.drawString("RICH", bar_x + rich_w/2, bar_y - 20);
  lambda_sprite.drawString("STOICH", bar_x + rich_w + stoich_w/2, bar_y - 20);
  lambda_sprite.drawString("LEAN", bar_x + rich_w + stoich_w + lean_w/2, bar_y - 20);

  // Lambda triangles with simulated values
  float lambda_norm = (sim_lambda - 0.6) / 0.8;
  lambda_norm = constrain(lambda_norm, 0.0, 1.0);
  int lambda_x = bar_x + (lambda_norm * bar_w);

  uint16_t lambda_color = M5.Display.color565(255, 255, 100);
  lambda_sprite.fillTriangle(lambda_x, bar_y - 5, lambda_x - 15, bar_y - 25, lambda_x + 15, bar_y - 25, lambda_color);

  float target_norm = (sim_lambda_target - 0.6) / 0.8;
  target_norm = constrain(target_norm, 0.0, 1.0);
  int target_x = bar_x + (target_norm * bar_w);

  uint16_t target_color = M5.Display.color565(255, 255, 255);
  lambda_sprite.fillTriangle(target_x, bar_y + bar_h + 5, target_x - 15, bar_y + bar_h + 25, target_x + 15, bar_y + bar_h + 25, target_color);

  // Digital readouts
  lambda_sprite.setTextSize(4);
  lambda_sprite.setTextColor(lambda_color);
  lambda_sprite.setTextDatum(textdatum_t::middle_left);
  char lambda_str[10];
  sprintf(lambda_str, "%.3f", sim_lambda);
  lambda_sprite.drawString(lambda_str, 30, sprite_h - 35);

  lambda_sprite.setTextColor(target_color);
  lambda_sprite.setTextDatum(textdatum_t::middle_right);
  char target_str[10];
  sprintf(target_str, "%.3f", sim_lambda_target);
  lambda_sprite.drawString(target_str, sprite_w - 30, sprite_h - 35);

  // Labels
  lambda_sprite.setTextSize(2);
  lambda_sprite.setTextColor(M5.Display.color565(200, 200, 200));
  lambda_sprite.setTextDatum(textdatum_t::middle_left);
  lambda_sprite.drawString("ACTUAL", 30, sprite_h - 60);
  lambda_sprite.setTextDatum(textdatum_t::middle_right);
  lambda_sprite.drawString("TARGET", sprite_w - 30, sprite_h - 60);

  // LAMBDA label
  lambda_sprite.setTextSize(3);
  lambda_sprite.setTextColor(M5.Display.color565(0, 255, 255));
  lambda_sprite.setTextDatum(textdatum_t::bottom_center);
  lambda_sprite.drawString("LAMBDA", sprite_w/2, sprite_h - 5);

  // Push sprite to display
  lambda_sprite.pushSprite(x, y);

  // Update last values
  last_sim_lambda = sim_lambda;
  last_sim_lambda_target = sim_lambda_target;
}

// Fallback direct drawing for lambda gauge
void drawOptimalLambdaGaugeDirect(int x, int y, int w, int h) {
  // Direct drawing fallback if sprite creation fails
  M5.Display.fillRect(x, y, w, h, M5.Display.color565(20, 20, 40));
  M5.Display.drawRoundRect(x, y, w, h, 12, M5.Display.color565(0, 255, 255));

  // Horizontal bar for rich/stoich/lean zones
  int bar_x = x + 80;
  int bar_y = y + 60;
  int bar_w = w - 160;
  int bar_h = 30;

  // Draw rich/stoich/lean zones
  int rich_w = bar_w * 0.3;
  int stoich_w = bar_w * 0.4;
  int lean_w = bar_w * 0.3;

  M5.Display.fillRect(bar_x, bar_y, rich_w, bar_h, M5.Display.color565(255, 100, 100));
  M5.Display.fillRect(bar_x + rich_w, bar_y, stoich_w, bar_h, M5.Display.color565(100, 255, 100));
  M5.Display.fillRect(bar_x + rich_w + stoich_w, bar_y, lean_w, bar_h, M5.Display.color565(100, 150, 255));
  M5.Display.drawRect(bar_x, bar_y, bar_w, bar_h, TFT_WHITE);

  // Zone labels
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString("RICH", bar_x + rich_w/2, bar_y - 20);
  M5.Display.drawString("STOICH", bar_x + rich_w + stoich_w/2, bar_y - 20);
  M5.Display.drawString("LEAN", bar_x + rich_w + stoich_w + lean_w/2, bar_y - 20);

  // Lambda triangles
  float lambda_norm = (sim_lambda - 0.6) / 0.8;
  lambda_norm = constrain(lambda_norm, 0.0, 1.0);
  int lambda_x = bar_x + (lambda_norm * bar_w);

  uint16_t lambda_color = M5.Display.color565(255, 255, 100);
  M5.Display.fillTriangle(lambda_x, bar_y - 5, lambda_x - 15, bar_y - 25, lambda_x + 15, bar_y - 25, lambda_color);

  float target_norm = (sim_lambda_target - 0.6) / 0.8;
  target_norm = constrain(target_norm, 0.0, 1.0);
  int target_x = bar_x + (target_norm * bar_w);

  uint16_t target_color = M5.Display.color565(255, 255, 255);
  M5.Display.fillTriangle(target_x, bar_y + bar_h + 5, target_x - 15, bar_y + bar_h + 25, target_x + 15, bar_y + bar_h + 25, target_color);

  // Digital readouts
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(lambda_color);
  M5.Display.setTextDatum(textdatum_t::middle_left);
  char lambda_str[10];
  sprintf(lambda_str, "%.3f", sim_lambda);
  M5.Display.drawString(lambda_str, x + 30, y + h - 35);

  M5.Display.setTextColor(target_color);
  M5.Display.setTextDatum(textdatum_t::middle_right);
  char target_str[10];
  sprintf(target_str, "%.3f", sim_lambda_target);
  M5.Display.drawString(target_str, x + w - 30, y + h - 35);

  // Labels
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(M5.Display.color565(200, 200, 200));
  M5.Display.setTextDatum(textdatum_t::middle_left);
  M5.Display.drawString("ACTUAL", x + 30, y + h - 60);
  M5.Display.setTextDatum(textdatum_t::middle_right);
  M5.Display.drawString("TARGET", x + w - 30, y + h - 60);

  // LAMBDA label
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(M5.Display.color565(0, 255, 255));
  M5.Display.setTextDatum(textdatum_t::bottom_center);
  M5.Display.drawString("LAMBDA", x + w/2, y + h - 5);
}

void resetGaugeStates() {
  extern float last_rpm_gauge_value;
  extern float last_tps_value;
  extern float last_mgp_value;
  extern float last_iat_value;
  extern float last_lambda_value;
  extern float last_lambda_target;

  // Reset all gauge states to force full redraw
  last_rpm_gauge_value = -1.0;
  last_tps_value = -1.0;
  last_mgp_value = -999.0;
  last_iat_value = -999.0;
  last_lambda_value = -1.0;
  last_lambda_target = -1.0;

  // Reset efficient gauge system
  gauges_layout_initialized = false;
  last_sim_rpm = -1;
  last_sim_tps = -1;
  last_sim_boost = -1;
  last_sim_iat = -1;
  last_sim_ect = -1;
  last_sim_oil_press = -1;
  last_sim_fuel_press = -1;
  last_sim_battery = -1;
  last_sim_speed = -1;
  last_sim_gear = -1;
  last_sim_lambda = -1;
  last_sim_lambda_target = -1;

  Serial.println("All gauge states reset for optimal dashboard");
}

void showGaugesPage() {
  int screen_w = M5.Display.width();  // 1280px
  int screen_h = M5.Display.height(); // 720px

  // Reset gauge states to force redraw
  resetGaugeStates();

  // Clear screen with dark background
  M5.Display.fillScreen(M5.Display.color565(10, 10, 30));

  // Header (compact)
  M5.Display.fillRect(0, 0, screen_w, 50, M5.Display.color565(20, 20, 60));
  M5.Display.drawLine(0, 50, screen_w, 50, M5.Display.color565(0, 255, 255));

  // Title
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString("AUTOMOTIVE DASHBOARD", screen_w/2, 15);

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(M5.Display.color565(0, 255, 255));
  M5.Display.drawString("オートモーティブダッシュボード", screen_w/2, 35); // "Automotive Dashboard" in Japanese

  // OPTIMAL AUTOMOTIVE LAYOUT - 3 rows with hierarchical sizing
  int header_h = 50;
  int nav_h = 50;
  int gap = 10;
  int side_margin = 10;
  int row_height = 190;  // All rows same height for visual harmony

  // Available width for gauges
  int available_width = screen_w - (2 * side_margin);

  // TOP ROW - Critical Gauges (2 large gauges: 620px each)
  int top_gauge_w = (available_width - gap) / 2;  // 620px each
  int top_y = header_h + gap;

  // MIDDLE ROW - Engine Vitals (4 gauges: 310px each)
  int mid_gauge_w = (available_width - (3 * gap)) / 4;  // 310px each
  int mid_y = top_y + row_height + gap;

  // BOTTOM ROW - Secondary (5 gauges: 255px each)
  int bot_gauge_w = (available_width - (4 * gap)) / 5;  // 255px each
  int bot_y = mid_y + row_height + gap;

  Serial.printf("OPTIMAL LAYOUT: %dx%d screen\n", screen_w, screen_h);
  Serial.printf("Top row: 2×%dx%d, Mid row: 4×%dx%d, Bot row: 5×%dx%d\n",
                top_gauge_w, row_height, mid_gauge_w, row_height, bot_gauge_w, row_height);

  // Initialize layout only once or when reset
  if (!gauges_layout_initialized) {
    // Clear entire screen and redraw background
    M5.Display.fillScreen(M5.Display.color565(10, 10, 30));

    // Redraw header
    M5.Display.fillRect(0, 0, screen_w, 50, M5.Display.color565(20, 20, 60));
    M5.Display.drawLine(0, 50, screen_w, 50, M5.Display.color565(0, 255, 255));
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.drawString("AUTOMOTIVE DASHBOARD", screen_w/2, 15);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(M5.Display.color565(0, 255, 255));
    M5.Display.drawString("オートモーティブダッシュボード", screen_w/2, 35);

    // Store gauge positions for efficient updates
    gauge_positions[0] = {side_margin, top_y, top_gauge_w, row_height, true}; // RPM
    gauge_positions[1] = {side_margin + top_gauge_w + gap, top_y, top_gauge_w, row_height, true}; // Lambda
    gauge_positions[2] = {side_margin, mid_y, mid_gauge_w, row_height, true}; // TPS
    gauge_positions[3] = {side_margin + mid_gauge_w + gap, mid_y, mid_gauge_w, row_height, true}; // Boost
    gauge_positions[4] = {side_margin + 2*(mid_gauge_w + gap), mid_y, mid_gauge_w, row_height, true}; // IAT
    gauge_positions[5] = {side_margin + 3*(mid_gauge_w + gap), mid_y, mid_gauge_w, row_height, true}; // ECT
    gauge_positions[6] = {side_margin, bot_y, bot_gauge_w, row_height, true}; // Oil Press
    gauge_positions[7] = {side_margin + bot_gauge_w + gap, bot_y, bot_gauge_w, row_height, true}; // Fuel Press
    gauge_positions[8] = {side_margin + 2*(bot_gauge_w + gap), bot_y, bot_gauge_w, row_height, true}; // Battery
    gauge_positions[9] = {side_margin + 3*(bot_gauge_w + gap), bot_y, bot_gauge_w, row_height, true}; // Speed

    // Draw static parts of gauges (borders, labels, units) - these won't change
    drawGaugeStatic(gauge_positions[0].x, gauge_positions[0].y, gauge_positions[0].w, gauge_positions[0].h,
                    "RPM", "", M5.Display.color565(255, 80, 80), 3);

    drawGaugeStatic(gauge_positions[2].x, gauge_positions[2].y, gauge_positions[2].w, gauge_positions[2].h,
                    "TPS", "%", M5.Display.color565(100, 255, 100), 3);

    drawGaugeStatic(gauge_positions[3].x, gauge_positions[3].y, gauge_positions[3].w, gauge_positions[3].h,
                    "BOOST", getPressureUnit(), M5.Display.color565(255, 165, 0), 3);

    drawGaugeStatic(gauge_positions[4].x, gauge_positions[4].y, gauge_positions[4].w, gauge_positions[4].h,
                    "IAT", getTemperatureUnit(), M5.Display.color565(100, 150, 255), 3);

    drawGaugeStatic(gauge_positions[5].x, gauge_positions[5].y, gauge_positions[5].w, gauge_positions[5].h,
                    "ECT", getTemperatureUnit(), M5.Display.color565(255, 100, 255), 3);

    drawGaugeStatic(gauge_positions[6].x, gauge_positions[6].y, gauge_positions[6].w, gauge_positions[6].h,
                    "OIL PRESS", "BAR", M5.Display.color565(255, 200, 100), 2);

    drawGaugeStatic(gauge_positions[7].x, gauge_positions[7].y, gauge_positions[7].w, gauge_positions[7].h,
                    "FUEL PRESS", "BAR", M5.Display.color565(100, 255, 255), 2);

    drawGaugeStatic(gauge_positions[8].x, gauge_positions[8].y, gauge_positions[8].w, gauge_positions[8].h,
                    "BATTERY", "V", M5.Display.color565(255, 255, 100), 2);

    drawGaugeStatic(gauge_positions[9].x, gauge_positions[9].y, gauge_positions[9].w, gauge_positions[9].h,
                    "SPEED", "KM/H", M5.Display.color565(0, 255, 255), 2);

    drawGaugeStatic(side_margin + 4*(bot_gauge_w + gap), bot_y, bot_gauge_w, row_height,
                    "GEAR", "", M5.Display.color565(255, 0, 255), 2);

    // Draw initial values
    M5.Display.setTextSize(7);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.drawString("800", gauge_positions[0].x + gauge_positions[0].w/2, gauge_positions[0].y + gauge_positions[0].h/2);

    M5.Display.setTextSize(5);
    M5.Display.drawString("0.0", gauge_positions[2].x + gauge_positions[2].w/2, gauge_positions[2].y + gauge_positions[2].h/2);
    M5.Display.drawString("0.0", gauge_positions[3].x + gauge_positions[3].w/2, gauge_positions[3].y + gauge_positions[3].h/2);
    M5.Display.drawString("25", gauge_positions[4].x + gauge_positions[4].w/2, gauge_positions[4].y + gauge_positions[4].h/2);
    M5.Display.drawString("85", gauge_positions[5].x + gauge_positions[5].w/2, gauge_positions[5].y + gauge_positions[5].h/2);

    M5.Display.setTextSize(4);
    M5.Display.drawString("0.5", gauge_positions[6].x + gauge_positions[6].w/2, gauge_positions[6].y + gauge_positions[6].h/2);
    M5.Display.drawString("3.0", gauge_positions[7].x + gauge_positions[7].w/2, gauge_positions[7].y + gauge_positions[7].h/2);
    M5.Display.drawString("12.6", gauge_positions[8].x + gauge_positions[8].w/2, gauge_positions[8].y + gauge_positions[8].h/2);
    M5.Display.drawString("0", gauge_positions[9].x + gauge_positions[9].w/2, gauge_positions[9].y + gauge_positions[9].h/2);

    M5.Display.setTextSize(6);
    M5.Display.drawString("1", side_margin + 4*(bot_gauge_w + gap) + bot_gauge_w/2, bot_y + row_height/2);

    // Draw initial lambda gauge
    drawOptimalLambdaGauge(gauge_positions[1].x, gauge_positions[1].y, gauge_positions[1].w, gauge_positions[1].h);

    gauges_layout_initialized = true;
    Serial.println("Gauge layout initialized - complete gauges drawn with labels and units");
  }

  // Update simulation data
  updateSimulationData();

  // Prepare simulated values as strings
  char rpm_str[10], tps_str[10], boost_str[10], iat_str[10], ect_str[10];
  char oil_press_str[10], fuel_press_str[10], battery_str[10], speed_str[10], gear_str[10];
  char last_rpm_str[10], last_tps_str[10], last_boost_str[10], last_iat_str[10], last_ect_str[10];
  char last_oil_press_str[10], last_fuel_press_str[10], last_battery_str[10], last_speed_str[10], last_gear_str[10];

  sprintf(rpm_str, "%.0f", sim_rpm);
  sprintf(tps_str, "%.1f", sim_tps);
  sprintf(boost_str, "%.1f", convertPressure(sim_boost));
  sprintf(iat_str, "%.0f", convertTemperature(sim_iat));
  sprintf(ect_str, "%.0f", convertTemperature(sim_ect));
  sprintf(oil_press_str, "%.1f", sim_oil_press);
  sprintf(fuel_press_str, "%.1f", sim_fuel_press);
  sprintf(battery_str, "%.1f", sim_battery);
  sprintf(speed_str, "%.0f", sim_speed);
  sprintf(gear_str, "%d", sim_gear);

  sprintf(last_rpm_str, "%.0f", last_sim_rpm);
  sprintf(last_tps_str, "%.1f", last_sim_tps);
  sprintf(last_boost_str, "%.1f", convertPressure(last_sim_boost));
  sprintf(last_iat_str, "%.0f", convertTemperature(last_sim_iat));
  sprintf(last_ect_str, "%.0f", convertTemperature(last_sim_ect));
  sprintf(last_oil_press_str, "%.1f", last_sim_oil_press);
  sprintf(last_fuel_press_str, "%.1f", last_sim_fuel_press);
  sprintf(last_battery_str, "%.1f", last_sim_battery);
  sprintf(last_speed_str, "%.0f", last_sim_speed);
  sprintf(last_gear_str, "%d", last_sim_gear);

  // EFFICIENT UPDATES - Only update changed values
  uint16_t rpm_color = sim_rpm > 7000 ? M5.Display.color565(255, 0, 0) : TFT_WHITE;
  updateGaugeValue(gauge_positions[0].x, gauge_positions[0].y, gauge_positions[0].w, gauge_positions[0].h,
                   rpm_str, last_rpm_str, 6, rpm_color);

  // Lambda gauge with sprite (handles its own change detection)
  drawOptimalLambdaGauge(gauge_positions[1].x, gauge_positions[1].y, gauge_positions[1].w, gauge_positions[1].h);

  // Update other gauges efficiently
  updateGaugeValue(gauge_positions[2].x, gauge_positions[2].y, gauge_positions[2].w, gauge_positions[2].h,
                   tps_str, last_tps_str, 4, TFT_WHITE);

  updateGaugeValue(gauge_positions[3].x, gauge_positions[3].y, gauge_positions[3].w, gauge_positions[3].h,
                   boost_str, last_boost_str, 4, TFT_WHITE);

  updateGaugeValue(gauge_positions[4].x, gauge_positions[4].y, gauge_positions[4].w, gauge_positions[4].h,
                   iat_str, last_iat_str, 4, TFT_WHITE);

  updateGaugeValue(gauge_positions[5].x, gauge_positions[5].y, gauge_positions[5].w, gauge_positions[5].h,
                   ect_str, last_ect_str, 4, TFT_WHITE);

  updateGaugeValue(gauge_positions[6].x, gauge_positions[6].y, gauge_positions[6].w, gauge_positions[6].h,
                   oil_press_str, last_oil_press_str, 3, TFT_WHITE);

  updateGaugeValue(gauge_positions[7].x, gauge_positions[7].y, gauge_positions[7].w, gauge_positions[7].h,
                   fuel_press_str, last_fuel_press_str, 3, TFT_WHITE);

  updateGaugeValue(gauge_positions[8].x, gauge_positions[8].y, gauge_positions[8].w, gauge_positions[8].h,
                   battery_str, last_battery_str, 3, TFT_WHITE);

  updateGaugeValue(gauge_positions[9].x, gauge_positions[9].y, gauge_positions[9].w, gauge_positions[9].h,
                   speed_str, last_speed_str, 3, TFT_WHITE);

  // Gear gauge (special position)
  updateGaugeValue(side_margin + 4*(bot_gauge_w + gap), bot_y, bot_gauge_w, row_height,
                   gear_str, last_gear_str, 5, TFT_WHITE);

  // Update last values for next comparison
  last_sim_rpm = sim_rpm;
  last_sim_tps = sim_tps;
  last_sim_boost = sim_boost;
  last_sim_iat = sim_iat;
  last_sim_ect = sim_ect;
  last_sim_oil_press = sim_oil_press;
  last_sim_fuel_press = sim_fuel_press;
  last_sim_battery = sim_battery;
  last_sim_speed = sim_speed;
  last_sim_gear = sim_gear;

  // Bottom navigation (compact)
  M5.Display.fillRect(0, screen_h - 50, screen_w, 50, M5.Display.color565(30, 30, 30));
  M5.Display.drawLine(0, screen_h - 50, screen_w, screen_h - 50, M5.Display.color565(0, 255, 255));

  // Navigation buttons
  int nav_button_w = 100;
  int nav_button_h = 30;
  int nav_y = screen_h - 40;

  // CONFIG button
  M5.Display.fillRoundRect(20, nav_y, nav_button_w, nav_button_h, 6, M5.Display.color565(120, 60, 60));
  M5.Display.drawRoundRect(20, nav_y, nav_button_w, nav_button_h, 6, M5.Display.color565(255, 100, 100));
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString("CONFIG", 20 + nav_button_w/2, nav_y + nav_button_h/2);

  // CONTROL button
  M5.Display.fillRoundRect(140, nav_y, nav_button_w, nav_button_h, 6, M5.Display.color565(60, 60, 120));
  M5.Display.drawRoundRect(140, nav_y, nav_button_w, nav_button_h, 6, M5.Display.color565(100, 100, 255));
  M5.Display.drawString("CONTROL", 140 + nav_button_w/2, nav_y + nav_button_h/2);

  // Status
  M5.Display.setTextColor(M5.Display.color565(200, 200, 200));
  M5.Display.drawString("GAUGE MODE", screen_w/2, screen_h - 15);
}

// ========== CONTROL INTERFACE FUNCTIONS ==========

// Draw a control button with state indication
void drawControlButton(int x, int y, int w, int h, const char* label, const char* value, bool active, uint16_t color) {
  // Background color based on state
  uint16_t bg_color = active ? M5.Display.color565(0, 80, 0) : M5.Display.color565(40, 40, 40);
  uint16_t border_color = active ? M5.Display.color565(0, 255, 0) : color;

  // Draw button background
  M5.Display.fillRoundRect(x, y, w, h, 12, bg_color);
  M5.Display.drawRoundRect(x, y, w, h, 12, border_color);
  M5.Display.drawRoundRect(x+1, y+1, w-2, h-2, 11, M5.Display.color565(180, 180, 180));

  // Label (top)
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(color);
  M5.Display.setTextDatum(textdatum_t::top_center);
  M5.Display.drawString(label, x + w/2, y + 15);

  // Value (center)
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString(value, x + w/2, y + h/2 + 10);

  // Status indicator
  if (active) {
    M5.Display.fillCircle(x + w - 20, y + 20, 8, M5.Display.color565(0, 255, 0));
  }
}

// Draw boost map selector
void drawBoostMapSelector(int x, int y, int w, int h) {
  M5.Display.fillRoundRect(x, y, w, h, 12, M5.Display.color565(40, 40, 40));
  M5.Display.drawRoundRect(x, y, w, h, 12, M5.Display.color565(255, 165, 0));

  // Title
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(M5.Display.color565(255, 165, 0));
  M5.Display.setTextDatum(textdatum_t::top_center);
  M5.Display.drawString("BOOST MAP", x + w/2, y + 10);

  // Map buttons
  int btn_w = (w - 60) / 4;
  int btn_h = 40;
  int btn_y = y + 50;

  for (int i = 1; i <= 4; i++) {
    int btn_x = x + 15 + (i-1) * (btn_w + 10);
    bool active = (ecu_data.current_boost_map == i);

    uint16_t btn_color = active ? M5.Display.color565(0, 255, 0) : M5.Display.color565(100, 100, 100);
    uint16_t bg_color = active ? M5.Display.color565(0, 80, 0) : M5.Display.color565(20, 20, 20);

    M5.Display.fillRoundRect(btn_x, btn_y, btn_w, btn_h, 8, bg_color);
    M5.Display.drawRoundRect(btn_x, btn_y, btn_w, btn_h, 8, btn_color);

    M5.Display.setTextSize(3);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    char map_str[5];
    sprintf(map_str, "%d", i);
    M5.Display.drawString(map_str, btn_x + btn_w/2, btn_y + btn_h/2);
  }

  // Current boost display
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(textdatum_t::bottom_center);
  char boost_str[20];
  sprintf(boost_str, "%.1f %s", convertPressure(sim_boost), getPressureUnit());
  M5.Display.drawString(boost_str, x + w/2, y + h - 15);
}

// Draw boost adjustment controls
void drawBoostAdjustment(int x, int y, int w, int h) {
  M5.Display.fillRoundRect(x, y, w, h, 12, M5.Display.color565(40, 40, 40));
  M5.Display.drawRoundRect(x, y, w, h, 12, M5.Display.color565(255, 165, 0));

  // Title
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(M5.Display.color565(255, 165, 0));
  M5.Display.setTextDatum(textdatum_t::top_center);
  M5.Display.drawString("BOOST ADJUST", x + w/2, y + 10);

  // - Button
  int btn_w = 80;
  int btn_h = 60;
  int btn_y = y + 50;

  M5.Display.fillRoundRect(x + 20, btn_y, btn_w, btn_h, 12, M5.Display.color565(80, 0, 0));
  M5.Display.drawRoundRect(x + 20, btn_y, btn_w, btn_h, 12, M5.Display.color565(255, 100, 100));
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString("-", x + 20 + btn_w/2, btn_y + btn_h/2);

  // Current adjustment
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(TFT_WHITE);
  char adj_str[10];
  sprintf(adj_str, "%+.1f", ecu_data.boost_adjustment);
  M5.Display.drawString(adj_str, x + w/2, btn_y + btn_h/2);

  // + Button
  M5.Display.fillRoundRect(x + w - 100, btn_y, btn_w, btn_h, 12, M5.Display.color565(0, 80, 0));
  M5.Display.drawRoundRect(x + w - 100, btn_y, btn_w, btn_h, 12, M5.Display.color565(100, 255, 100));
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString("+", x + w - 100 + btn_w/2, btn_y + btn_h/2);

  // Target boost
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(M5.Display.color565(200, 200, 200));
  M5.Display.setTextDatum(textdatum_t::bottom_center);
  char target_str[30];
  sprintf(target_str, "Target: %.1f %s", convertPressure(sim_boost + ecu_data.boost_adjustment), getPressureUnit());
  M5.Display.drawString(target_str, x + w/2, y + h - 15);
}

// Draw system status display
void drawSystemStatus(int x, int y, int w, int h) {
  M5.Display.fillRoundRect(x, y, w, h, 12, M5.Display.color565(40, 40, 40));
  M5.Display.drawRoundRect(x, y, w, h, 12, M5.Display.color565(0, 255, 255));

  // Title
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(M5.Display.color565(0, 255, 255));
  M5.Display.setTextDatum(textdatum_t::top_center);
  M5.Display.drawString("SYSTEM STATUS", x + w/2, y + 10);

  // Status indicators
  M5.Display.setTextSize(1);
  int status_y = y + 40;
  int line_height = 25;

  // Engine status
  uint16_t engine_color = ecu_data.system_ready ? M5.Display.color565(0, 255, 0) : M5.Display.color565(255, 100, 100);
  M5.Display.setTextColor(engine_color);
  M5.Display.setTextDatum(textdatum_t::top_left);
  M5.Display.drawString("ENGINE:", x + 15, status_y);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString(ecu_data.system_ready ? "READY" : "FAULT", x + 80, status_y);

  // Boost system
  uint16_t boost_color = ecu_data.boost_control_active ? M5.Display.color565(0, 255, 0) : M5.Display.color565(255, 165, 0);
  M5.Display.setTextColor(boost_color);
  M5.Display.drawString("BOOST:", x + 15, status_y + line_height);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString(ecu_data.boost_control_active ? "ACTIVE" : "STANDBY", x + 80, status_y + line_height);

  // Launch control
  uint16_t launch_color = ecu_data.launch_control_active ? M5.Display.color565(255, 100, 255) : M5.Display.color565(100, 100, 100);
  M5.Display.setTextColor(launch_color);
  M5.Display.drawString("LAUNCH:", x + 15, status_y + 2*line_height);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString(ecu_data.launch_control_active ? "ARMED" : "DISARMED", x + 80, status_y + 2*line_height);

  // Anti-lag
  uint16_t antilag_color = ecu_data.anti_lag_active ? M5.Display.color565(255, 255, 100) : M5.Display.color565(100, 100, 100);
  M5.Display.setTextColor(antilag_color);
  M5.Display.drawString("ANTI-LAG:", x + 15, status_y + 3*line_height);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString(ecu_data.anti_lag_active ? "ACTIVE" : "OFF", x + 80, status_y + 3*line_height);
}

// Draw quick preset button
void drawQuickPreset(int x, int y, int w, int h, const char* name, const char* desc, bool active, uint16_t color) {
  uint16_t bg_color = active ? M5.Display.color565(0, 80, 0) : M5.Display.color565(40, 40, 40);
  uint16_t border_color = active ? M5.Display.color565(0, 255, 0) : color;

  M5.Display.fillRoundRect(x, y, w, h, 12, bg_color);
  M5.Display.drawRoundRect(x, y, w, h, 12, border_color);

  // Preset name
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(color);
  M5.Display.setTextDatum(textdatum_t::top_center);
  M5.Display.drawString(name, x + w/2, y + 20);

  // Description
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString(desc, x + w/2, y + h/2 + 20);

  // Status indicator
  if (active) {
    M5.Display.fillCircle(x + w - 15, y + 15, 6, M5.Display.color565(0, 255, 0));
  }

  // Tap instruction
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(M5.Display.color565(150, 150, 150));
  M5.Display.setTextDatum(textdatum_t::bottom_center);
  M5.Display.drawString("TAP TO APPLY", x + w/2, y + h - 10);
}

void showControlPage() {
  int screen_w = M5.Display.width();  // 1280px
  int screen_h = M5.Display.height(); // 720px

  // Clear screen with dark background
  M5.Display.fillScreen(M5.Display.color565(10, 10, 30));

  // Header
  M5.Display.fillRect(0, 0, screen_w, 50, M5.Display.color565(20, 20, 60));
  M5.Display.drawLine(0, 50, screen_w, 50, M5.Display.color565(0, 255, 255));

  // Title
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString("CONTROL INTERFACE", screen_w/2, 15);

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(M5.Display.color565(0, 255, 255));
  M5.Display.drawString("コントロールインターフェース", screen_w/2, 35);

  // Layout calculations
  int header_h = 50;
  int nav_h = 50;
  int gap = 10;
  int side_margin = 10;
  int row_height = 190;

  // Available width for controls
  int available_width = screen_w - (2 * side_margin);

  // TOP ROW - Boost Controls (3 controls: 400px each)
  int top_control_w = (available_width - (2 * gap)) / 3;
  int top_y = header_h + gap;

  // MIDDLE ROW - Engine Controls (3 controls: 400px each)
  int mid_y = top_y + row_height + gap;

  // BOTTOM ROW - Status & Quick Actions (5 controls: variable width)
  int bot_y = mid_y + row_height + gap;

  Serial.printf("CONTROL LAYOUT: %dx%d screen\n", screen_w, screen_h);
  Serial.printf("Control rows: 3×%dx%d each\n", top_control_w, row_height);

  // Draw TOP ROW - Boost Controls
  drawBoostMapSelector(side_margin, top_y, top_control_w, row_height);
  drawBoostAdjustment(side_margin + top_control_w + gap, top_y, top_control_w, row_height);

  // Boost Display (3rd control)
  char boost_current[15], boost_target[15];
  sprintf(boost_current, "%.1f %s", convertPressure(sim_boost), getPressureUnit());
  sprintf(boost_target, "%.1f %s", convertPressure(sim_boost + ecu_data.boost_adjustment), getPressureUnit());
  drawControlButton(side_margin + 2*(top_control_w + gap), top_y, top_control_w, row_height,
                    "BOOST DISPLAY", boost_current, ecu_data.boost_control_active,
                    M5.Display.color565(255, 165, 0));

  // Draw MIDDLE ROW - Engine Controls
  char ethrottle_str[20];
  const char* ethrottle_modes[] = {"SMOOTH", "SPORT", "AGGRESSIVE"};
  sprintf(ethrottle_str, "MAP %d", ecu_data.current_ethrottle_map);
  drawControlButton(side_margin, mid_y, top_control_w, row_height,
                    "E-THROTTLE", ethrottle_str, true, M5.Display.color565(100, 255, 100));

  char launch_str[15];
  sprintf(launch_str, "%s", ecu_data.launch_control_active ? "ACTIVE" : "READY");
  drawControlButton(side_margin + top_control_w + gap, mid_y, top_control_w, row_height,
                    "LAUNCH CTRL", launch_str, ecu_data.launch_control_active,
                    M5.Display.color565(255, 100, 255));

  char antilag_str[15];
  sprintf(antilag_str, "%s", ecu_data.anti_lag_active ? "ACTIVE" : "OFF");
  drawControlButton(side_margin + 2*(top_control_w + gap), mid_y, top_control_w, row_height,
                    "ANTI-LAG", antilag_str, ecu_data.anti_lag_active,
                    M5.Display.color565(255, 255, 100));

  // Draw BOTTOM ROW - Quick Presets and Status
  int bot_control_w = (available_width - (4 * gap)) / 5; // 5 controls in bottom row

  // System Status (1st control - wider)
  int status_w = bot_control_w + 80;
  drawSystemStatus(side_margin, bot_y, status_w, row_height);

  // Quick Presets (4 controls)
  int preset_x = side_margin + status_w + gap;
  int preset_w = (available_width - status_w - (4 * gap)) / 4;

  drawQuickPreset(preset_x, bot_y, preset_w, row_height,
                  "STREET", "Conservative", current_preset == PRESET_STREET,
                  M5.Display.color565(100, 255, 100));

  drawQuickPreset(preset_x + preset_w + gap, bot_y, preset_w, row_height,
                  "TRACK", "Performance", current_preset == PRESET_TRACK,
                  M5.Display.color565(255, 165, 0));

  drawQuickPreset(preset_x + 2*(preset_w + gap), bot_y, preset_w, row_height,
                  "DRAG", "Maximum", current_preset == PRESET_DRAG,
                  M5.Display.color565(255, 100, 100));

  drawQuickPreset(preset_x + 3*(preset_w + gap), bot_y, preset_w, row_height,
                  "SAFE", "Emergency", current_preset == PRESET_SAFE,
                  M5.Display.color565(255, 0, 0));

  // Bottom navigation
  M5.Display.fillRect(0, screen_h - 50, screen_w, 50, M5.Display.color565(30, 30, 30));
  M5.Display.drawLine(0, screen_h - 50, screen_w, screen_h - 50, M5.Display.color565(0, 255, 255));

  // Navigation buttons
  int nav_button_w = 100;
  int nav_button_h = 30;
  int nav_y = screen_h - 40;

  // GAUGES button
  M5.Display.fillRoundRect(20, nav_y, nav_button_w, nav_button_h, 6, M5.Display.color565(60, 120, 60));
  M5.Display.drawRoundRect(20, nav_y, nav_button_w, nav_button_h, 6, M5.Display.color565(100, 255, 100));
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString("GAUGES", 20 + nav_button_w/2, nav_y + nav_button_h/2);

  // CONFIG button
  M5.Display.fillRoundRect(140, nav_y, nav_button_w, nav_button_h, 6, M5.Display.color565(120, 60, 60));
  M5.Display.drawRoundRect(140, nav_y, nav_button_w, nav_button_h, 6, M5.Display.color565(255, 100, 100));
  M5.Display.drawString("CONFIG", 140 + nav_button_w/2, nav_y + nav_button_h/2);

  // Status
  M5.Display.setTextColor(M5.Display.color565(200, 200, 200));
  M5.Display.drawString("CONTROL MODE", screen_w/2, screen_h - 15);
}

// ========== MAIN FUNCTIONS ==========
void setup() {
  Serial.begin(115200);
  Serial.println("Link G4X Monitor - Anime Style Dashboard");

  // Initialize M5 hardware
  M5.begin();

  // Speaker initialization disabled for testing
  // auto cfg = M5.config();
  // cfg.external_spk = true;  // Enable external speaker
  // cfg.internal_mic = false; // Disable microphone
  // M5.begin(cfg);
  // M5.Speaker.begin();
  // M5.Speaker.setVolume(200); // Set volume (0-255)
  // Serial.println("Speaker initialized");

  // Set landscape orientation for racing dashboard
  M5.Display.setRotation(1); // 1 = 90° clockwise (landscape)

  // Show anime splash screen
  drawAnimeSplashScreen();
  delay(1000); // Show splash for 1 second

  // Animated initialization sequence
  for (int progress = 0; progress <= 100; progress += 2) {
    animateLoadingBar(progress);

    // Perform actual initialization at specific progress points
    if (progress == 20) {
      loadConfig();
      Serial.println("Configuration loaded");
    } else if (progress == 50) {
      // Initialize CAN bus if not in simulation mode
      if (!config.simulation_mode) {
        if (!initializeCAN()) {
          Serial.println("Falling back to simulation mode");
          config.simulation_mode = true;
        }
      }
      if (config.simulation_mode) {
        Serial.println("Starting in simulation mode");
      }
    } else if (progress == 90) {
      // Voice playback disabled for testing
      // playJapaneseVoice();
      Serial.println("Voice playback skipped (testing mode)");
    }

    delay(50); // Smooth animation timing
  }

  delay(1000); // Hold at 100% for a moment

  // Show gauges page after splash (start directly in gauge mode)
  showGaugesPage();

  Serial.println("=== SYSTEM READY ===");
}

bool handleConfigTouch(int x, int y) {
  int screen_w = M5.Display.width();
  int screen_h = M5.Display.height();

  // Check navigation button
  int nav_button_w = 150;
  int nav_button_h = 50;
  int nav_y = screen_h - 65;

  if (x >= 50 && x <= 50 + nav_button_w && y >= nav_y && y <= nav_y + nav_button_h) {
    current_mode = MODE_GAUGES;
    showGaugesPage();
    Serial.println("Switched to GAUGE mode");
    return true;
  }

  int section_y = 100;
  int section_h = 90;
  int section_spacing = 10;
  int section_w = screen_w - 40;
  int section_x = 20;

  // Check Data Source section
  if (y >= section_y && y <= section_y + section_h) {
    config.simulation_mode = !config.simulation_mode;
    saveConfig();
    showConfigurationPage(); // Refresh display
    Serial.printf("Data source changed to: %s\n", config.simulation_mode ? "Simulation" : "Live CAN");
    return true;
  }

  section_y += section_h + section_spacing;

  // Check Stream Type section
  if (y >= section_y && y <= section_y + section_h) {
    config.use_custom_streams = !config.use_custom_streams;
    saveConfig();
    showConfigurationPage(); // Refresh display
    Serial.printf("Stream type changed to: %s\n", config.use_custom_streams ? "Custom Stream" : "Haltech IC7");
    return true;
  }

  section_y += section_h + section_spacing;

  // Check CAN Speed section
  if (y >= section_y && y <= section_y + section_h) {
    // Cycle through CAN speeds
    switch (config.can_speed) {
      case 125000: config.can_speed = 250000; break;
      case 250000: config.can_speed = 500000; break;
      case 500000: config.can_speed = 1000000; break;
      case 1000000: config.can_speed = 125000; break;
      default: config.can_speed = 500000; break;
    }
    saveConfig();
    showConfigurationPage(); // Refresh display
    Serial.printf("CAN speed changed to: %d kbps\n", config.can_speed / 1000);
    return true;
  }

  section_y += section_h + section_spacing;

  // Check CAN ID section
  if (y >= section_y && y <= section_y + section_h) {
    calculator_mode = true;
    calculator_value = config.base_can_id;
    showCANIDCalculator();
    Serial.println("Opening CAN ID calculator");
    return true;
  }

  section_y += section_h + section_spacing;

  // Check Units section
  if (y >= section_y && y <= section_y + section_h) {
    config.units = (config.units == METRIC) ? IMPERIAL : METRIC;
    saveConfig();
    showConfigurationPage(); // Refresh display
    Serial.printf("Units changed to: %s\n", getUnitSystemName());
    return true;
  }

  section_y += section_h + section_spacing;

  // Check Log Mode section
  if (y >= section_y && y <= section_y + section_h) {
    // Cycle through logging modes
    switch (config.logging_mode) {
      case LOG_DISABLED: config.logging_mode = LOG_ERRORS; break;
      case LOG_ERRORS: config.logging_mode = LOG_CHANGES; break;
      case LOG_CHANGES: config.logging_mode = LOG_FULL; break;
      case LOG_FULL: config.logging_mode = LOG_SESSION; break;
      case LOG_SESSION: config.logging_mode = LOG_DISABLED; break;
    }
    saveConfig();
    showConfigurationPage(); // Refresh display
    Serial.printf("Logging mode changed to: %s\n", getLoggingModeName());
    return true;
  }

  section_y += section_h + section_spacing;

  // Check Log Detail section (only if logging is enabled)
  if (isLoggingEnabled() && y >= section_y && y <= section_y + section_h) {
    // Cycle through detail levels
    switch (config.log_detail) {
      case LOG_BASIC: config.log_detail = LOG_DETAILED; break;
      case LOG_DETAILED: config.log_detail = LOG_DIAGNOSTIC; break;
      case LOG_DIAGNOSTIC: config.log_detail = LOG_BASIC; break;
    }
    saveConfig();
    showConfigurationPage(); // Refresh display
    Serial.printf("Log detail changed to: %s\n", getLogDetailName());
    return true;
  }

  if (isLoggingEnabled()) {
    section_y += section_h + section_spacing;
  }

  // Check Buffer Size section (only if logging is enabled)
  if (isLoggingEnabled() && y >= section_y && y <= section_y + section_h) {
    // Cycle through buffer sizes
    switch (config.buffer_size) {
      case BUFFER_SMALL: config.buffer_size = BUFFER_MEDIUM; break;
      case BUFFER_MEDIUM: config.buffer_size = BUFFER_LARGE; break;
      case BUFFER_LARGE: config.buffer_size = BUFFER_CUSTOM; break;
      case BUFFER_CUSTOM: config.buffer_size = BUFFER_SMALL; break;
    }
    saveConfig();
    showConfigurationPage(); // Refresh display
    Serial.printf("Buffer size changed to: %s (%d frames)\n", getBufferSizeName(), getBufferFrameCount());
    return true;
  }

  if (isLoggingEnabled()) {
    section_y += section_h + section_spacing;
  }

  // Check Storage section (only if logging is enabled)
  if (isLoggingEnabled() && y >= section_y && y <= section_y + section_h) {
    // Cycle through file sizes: 1, 5, 10, 50, 100 MB
    switch (config.max_file_size_mb) {
      case 1: config.max_file_size_mb = 5; break;
      case 5: config.max_file_size_mb = 10; break;
      case 10: config.max_file_size_mb = 50; break;
      case 50: config.max_file_size_mb = 100; break;
      case 100: config.max_file_size_mb = 1; break;
      default: config.max_file_size_mb = 10; break;
    }
    saveConfig();
    showConfigurationPage(); // Refresh display
    Serial.printf("Storage settings changed to: %dMB x%d files\n", config.max_file_size_mb, config.max_files);
    return true;
  }

  return false;
}

bool handleCalculatorTouch(int x, int y) {
  int screen_w = M5.Display.width();
  int screen_h = M5.Display.height();
  int modal_w = 600;
  int modal_h = 500;
  int modal_x = (screen_w - modal_w) / 2;
  int modal_y = (screen_h - modal_h) / 2;

  // Calculator buttons
  int button_w = 80;
  int button_h = 60;
  int button_spacing = 10;
  int grid_x = modal_x + 50;
  int grid_y = modal_y + 150;

  // Check hex keypad
  for (int i = 0; i < 16; i++) {
    int col = i % 4;
    int row = i / 4;
    int btn_x = grid_x + col * (button_w + button_spacing);
    int btn_y = grid_y + row * (button_h + button_spacing);

    if (x >= btn_x && x <= btn_x + button_w && y >= btn_y && y <= btn_y + button_h) {
      // Handle decimal input
      if (i == 0 || i == 1 || i == 2) {        // 1, 2, 3
        calculator_value = calculator_value * 10 + (i + 1);
      } else if (i == 4 || i == 5 || i == 6) { // 4, 5, 6
        calculator_value = calculator_value * 10 + (i + 1);
      } else if (i == 8 || i == 9 || i == 10) { // 7, 8, 9
        calculator_value = calculator_value * 10 + (i - 1);
      } else if (i == 12) {                     // 0
        calculator_value = calculator_value * 10;
      } else if (i == 13) {                     // 00
        calculator_value = calculator_value * 100;
      } else if (i == 3) {                      // Backspace ⌫
        calculator_value = calculator_value / 10;
      } else if (i == 7) {                      // +10
        calculator_value += 10;
      } else if (i == 11) {                     // +100
        calculator_value += 100;
      } else if (i == 14) {                     // +1
        calculator_value += 1;
      } else if (i == 15) {                     // +1000
        calculator_value += 1000;
      }

      // Limit to reasonable CAN ID range (0-2047 for 11-bit CAN IDs)
      if (calculator_value > 2047) calculator_value = 2047;

      showCANIDCalculator(); // Refresh
      return true;
    }
  }

  // Control buttons
  int ctrl_y = modal_y + modal_h - 80;

  // Clear button
  if (x >= modal_x + 50 && x <= modal_x + 170 && y >= ctrl_y && y <= ctrl_y + 50) {
    calculator_value = 0;
    showCANIDCalculator();
    return true;
  }

  // OK button
  if (x >= modal_x + 200 && x <= modal_x + 320 && y >= ctrl_y && y <= ctrl_y + 50) {
    config.base_can_id = calculator_value;
    saveConfig();
    calculator_mode = false;
    showConfigurationPage();
    Serial.printf("CAN ID changed to: 0x%03X (%d)\n", config.base_can_id, config.base_can_id);
    return true;
  }

  // Cancel button
  if (x >= modal_x + 350 && x <= modal_x + 470 && y >= ctrl_y && y <= ctrl_y + 50) {
    calculator_mode = false;
    showConfigurationPage();
    Serial.println("CAN ID change cancelled");
    return true;
  }

  return false;
}

bool handleGaugeTouch(int x, int y) {
  int screen_w = M5.Display.width();
  int screen_h = M5.Display.height();

  // Check navigation buttons
  int nav_button_w = 100;
  int nav_button_h = 30;
  int nav_y = screen_h - 40;

  // CONFIG button
  if (x >= 20 && x <= 20 + nav_button_w && y >= nav_y && y <= nav_y + nav_button_h) {
    current_mode = MODE_CONFIG;
    showConfigurationPage();
    Serial.println("Switched to CONFIG mode");
    return true;
  }

  // CONTROL button
  if (x >= 140 && x <= 140 + nav_button_w && y >= nav_y && y <= nav_y + nav_button_h) {
    current_mode = MODE_CONTROL;
    showControlPage();
    Serial.println("Switched to CONTROL mode");
    return true;
  }

  return false;
}

bool handleControlTouch(int x, int y) {
  int screen_w = M5.Display.width();
  int screen_h = M5.Display.height();

  // Layout calculations (must match showControlPage)
  int header_h = 50;
  int gap = 10;
  int side_margin = 10;
  int row_height = 190;
  int available_width = screen_w - (2 * side_margin);
  int top_control_w = (available_width - (2 * gap)) / 3;
  int top_y = header_h + gap;
  int mid_y = top_y + row_height + gap;
  int bot_y = mid_y + row_height + gap;

  // Check navigation buttons first
  int nav_button_w = 100;
  int nav_button_h = 30;
  int nav_y = screen_h - 40;

  // GAUGES button
  if (x >= 20 && x <= 20 + nav_button_w && y >= nav_y && y <= nav_y + nav_button_h) {
    current_mode = MODE_GAUGES;
    showGaugesPage();
    Serial.println("Switched to GAUGES mode");
    return true;
  }

  // CONFIG button
  if (x >= 140 && x <= 140 + nav_button_w && y >= nav_y && y <= nav_y + nav_button_h) {
    current_mode = MODE_CONFIG;
    showConfigurationPage();
    Serial.println("Switched to CONFIG mode");
    return true;
  }

  // TOP ROW CONTROLS

  // Boost Map Selector (1st control)
  if (x >= side_margin && x <= side_margin + top_control_w &&
      y >= top_y && y <= top_y + row_height) {

    // Check which map button was pressed
    int btn_w = (top_control_w - 60) / 4;
    int btn_y = top_y + 50;

    for (int i = 1; i <= 4; i++) {
      int btn_x = side_margin + 15 + (i-1) * (btn_w + 10);
      if (x >= btn_x && x <= btn_x + btn_w && y >= btn_y && y <= btn_y + 40) {
        ecu_data.current_boost_map = i;
        Serial.printf("🗺️ Boost map changed to: %d\n", i);
        showControlPage(); // Refresh display
        return true;
      }
    }
  }

  // Boost Adjustment (2nd control)
  if (x >= side_margin + top_control_w + gap && x <= side_margin + 2*top_control_w + gap &&
      y >= top_y && y <= top_y + row_height) {

    int btn_w = 80;
    int btn_h = 60;
    int btn_y = top_y + 50;
    int control_x = side_margin + top_control_w + gap;

    // - Button
    if (x >= control_x + 20 && x <= control_x + 20 + btn_w &&
        y >= btn_y && y <= btn_y + btn_h) {
      ecu_data.boost_adjustment -= 2.5;
      ecu_data.boost_adjustment = constrain(ecu_data.boost_adjustment, -10.0, 10.0);
      Serial.printf("⬇️ Boost adjustment: %.1f PSI\n", ecu_data.boost_adjustment);
      showControlPage();
      return true;
    }

    // + Button
    if (x >= control_x + top_control_w - 100 && x <= control_x + top_control_w - 20 &&
        y >= btn_y && y <= btn_y + btn_h) {
      ecu_data.boost_adjustment += 2.5;
      ecu_data.boost_adjustment = constrain(ecu_data.boost_adjustment, -10.0, 10.0);
      Serial.printf("⬆️ Boost adjustment: %.1f PSI\n", ecu_data.boost_adjustment);
      showControlPage();
      return true;
    }
  }

  // MIDDLE ROW CONTROLS

  // E-Throttle Map (1st control)
  if (x >= side_margin && x <= side_margin + top_control_w &&
      y >= mid_y && y <= mid_y + row_height) {
    // Cycle through e-throttle maps 1-3
    ecu_data.current_ethrottle_map = (ecu_data.current_ethrottle_map % 3) + 1;
    Serial.printf("⚡ E-Throttle map changed to: %d\n", ecu_data.current_ethrottle_map);
    showControlPage();
    return true;
  }

  // Launch Control (2nd control)
  if (x >= side_margin + top_control_w + gap && x <= side_margin + 2*top_control_w + gap &&
      y >= mid_y && y <= mid_y + row_height) {
    ecu_data.launch_control_active = !ecu_data.launch_control_active;
    Serial.printf("🚀 Launch control: %s\n", ecu_data.launch_control_active ? "ACTIVE" : "OFF");
    showControlPage();
    return true;
  }

  // Anti-Lag (3rd control)
  if (x >= side_margin + 2*(top_control_w + gap) && x <= side_margin + 3*top_control_w + 2*gap &&
      y >= mid_y && y <= mid_y + row_height) {
    ecu_data.anti_lag_active = !ecu_data.anti_lag_active;
    Serial.printf("💥 Anti-lag: %s\n", ecu_data.anti_lag_active ? "ACTIVE" : "OFF");
    showControlPage();
    return true;
  }

  // BOTTOM ROW - Quick Presets
  int bot_control_w = (available_width - (4 * gap)) / 5;
  int status_w = bot_control_w + 80;
  int preset_x = side_margin + status_w + gap;
  int preset_w = (available_width - status_w - (4 * gap)) / 4;

  // Street Mode
  if (x >= preset_x && x <= preset_x + preset_w &&
      y >= bot_y && y <= bot_y + row_height) {
    applyPreset(PRESET_STREET);
    return true;
  }

  // Track Mode
  if (x >= preset_x + preset_w + gap && x <= preset_x + 2*preset_w + gap &&
      y >= bot_y && y <= bot_y + row_height) {
    applyPreset(PRESET_TRACK);
    return true;
  }

  // Drag Mode
  if (x >= preset_x + 2*(preset_w + gap) && x <= preset_x + 3*preset_w + 2*gap &&
      y >= bot_y && y <= bot_y + row_height) {
    applyPreset(PRESET_DRAG);
    return true;
  }

  // Safe Mode
  if (x >= preset_x + 3*(preset_w + gap) && x <= preset_x + 4*preset_w + 3*gap &&
      y >= bot_y && y <= bot_y + row_height) {
    applyPreset(PRESET_SAFE);
    return true;
  }

  return false;
}

// Apply control presets
void applyPreset(ControlPreset preset) {
  current_preset = preset;

  switch (preset) {
    case PRESET_STREET:
      ecu_data.current_boost_map = 1;
      ecu_data.current_ethrottle_map = 1;
      ecu_data.boost_adjustment = 0.0;
      ecu_data.launch_control_active = false;
      ecu_data.anti_lag_active = false;
      Serial.println("🏙️ STREET MODE: Conservative settings applied");
      break;

    case PRESET_TRACK:
      ecu_data.current_boost_map = 2;
      ecu_data.current_ethrottle_map = 2;
      ecu_data.boost_adjustment = 2.5;
      ecu_data.launch_control_active = false;
      ecu_data.anti_lag_active = true;
      Serial.println("🏁 TRACK MODE: Performance settings applied");
      break;

    case PRESET_DRAG:
      ecu_data.current_boost_map = 4;
      ecu_data.current_ethrottle_map = 3;
      ecu_data.boost_adjustment = 5.0;
      ecu_data.launch_control_active = true;
      ecu_data.anti_lag_active = true;
      Serial.println("🚀 DRAG MODE: Maximum performance settings applied");
      break;

    case PRESET_SAFE:
      ecu_data.current_boost_map = 1;
      ecu_data.current_ethrottle_map = 1;
      ecu_data.boost_adjustment = -5.0;
      ecu_data.launch_control_active = false;
      ecu_data.anti_lag_active = false;
      Serial.println("🛡️ SAFE MODE: Emergency conservative settings applied");
      break;
  }

  showControlPage(); // Refresh display
}

void loop() {
  M5.update();

  // Handle touch input
  if (M5.Touch.getCount()) {
    auto touch = M5.Touch.getDetail();
    if (touch.wasPressed()) {
      Serial.printf("Touch detected at: %d, %d\n", touch.x, touch.y);
      if (calculator_mode) {
        handleCalculatorTouch(touch.x, touch.y);
      } else if (current_mode == MODE_CONFIG) {
        handleConfigTouch(touch.x, touch.y);
      } else if (current_mode == MODE_GAUGES) {
        handleGaugeTouch(touch.x, touch.y);
      } else if (current_mode == MODE_CONTROL) {
        handleControlTouch(touch.x, touch.y);
      }
    }
  }

  // Read CAN data or simulate
  if (config.simulation_mode) {
    simulateData();
  } else {
    readCANData();
  }

  // Simple data output every 5 seconds (less frequent during config)
  static unsigned long last_output = 0;
  if (millis() - last_output > 5000) {
    Serial.printf("RPM: %.0f, TPS: %.1f%%, MGP: %.1f, Lambda: %.3f, Boost Map: %d, E-Throttle: %d\n",
                  ecu_data.rpm, ecu_data.tps, ecu_data.mgp, ecu_data.lambda,
                  ecu_data.current_boost_map, ecu_data.current_ethrottle_map);
    last_output = millis();
  }

  // Refresh animated elements periodically (only if not in calculator mode)
  if (!calculator_mode) {
    static unsigned long last_refresh = 0;

    if (current_mode == MODE_CONFIG && millis() - last_refresh > 800) {
      // Refresh config page blinking indicators
      int section_y = 100;
      int section_h = 90;
      int section_spacing = 10;

      for (int i = 0; i < 4; i++) { // Now 4 sections including CAN ID
        int y = section_y + i * (section_h + section_spacing);
        uint16_t accent_color;

        switch (i) {
          case 0: accent_color = config.simulation_mode ? M5.Display.color565(255, 150, 0) : M5.Display.color565(0, 255, 100); break;
          case 1: accent_color = config.use_custom_streams ? M5.Display.color565(0, 255, 200) : M5.Display.color565(255, 100, 255); break;
          case 2: accent_color = M5.Display.color565(255, 255, 0); break;
          case 3: accent_color = M5.Display.color565(255, 100, 255); break; // CAN ID
        }

        // Clear and redraw blinking dot (smaller area, faster clear)
        M5.Display.fillCircle(M5.Display.width() - 45, y + 25, 5, M5.Display.color565(40, 40, 80));

        static bool blink_state = false;
        if (i == 0) blink_state = !blink_state; // Only toggle once per refresh

        if (blink_state) {
          M5.Display.fillCircle(M5.Display.width() - 45, y + 25, 4, accent_color);
        }
      }

      last_refresh = millis();
    } else if (current_mode == MODE_GAUGES && millis() - last_refresh > 100) {
      // Efficient refresh - only update simulation and changed values
      updateSimulationData();

      // Only update if we're still on the gauges page and layout is initialized
      if (current_mode == MODE_GAUGES && gauges_layout_initialized) {
        // Efficient updates - only redraw changed values
        char rpm_str[10], tps_str[10], boost_str[10], iat_str[10], ect_str[10];
        char oil_press_str[10], fuel_press_str[10], battery_str[10], speed_str[10], gear_str[10];
        char last_rpm_str[10], last_tps_str[10], last_boost_str[10], last_iat_str[10], last_ect_str[10];
        char last_oil_press_str[10], last_fuel_press_str[10], last_battery_str[10], last_speed_str[10], last_gear_str[10];

        sprintf(rpm_str, "%.0f", sim_rpm);
        sprintf(tps_str, "%.1f", sim_tps);
        sprintf(boost_str, "%.1f", convertPressure(sim_boost));
        sprintf(iat_str, "%.0f", convertTemperature(sim_iat));
        sprintf(ect_str, "%.0f", convertTemperature(sim_ect));
        sprintf(oil_press_str, "%.1f", sim_oil_press);
        sprintf(fuel_press_str, "%.1f", sim_fuel_press);
        sprintf(battery_str, "%.1f", sim_battery);
        sprintf(speed_str, "%.0f", sim_speed);
        sprintf(gear_str, "%d", sim_gear);

        sprintf(last_rpm_str, "%.0f", last_sim_rpm);
        sprintf(last_tps_str, "%.1f", last_sim_tps);
        sprintf(last_boost_str, "%.1f", convertPressure(last_sim_boost));
        sprintf(last_iat_str, "%.0f", convertTemperature(last_sim_iat));
        sprintf(last_ect_str, "%.0f", convertTemperature(last_sim_ect));
        sprintf(last_oil_press_str, "%.1f", last_sim_oil_press);
        sprintf(last_fuel_press_str, "%.1f", last_sim_fuel_press);
        sprintf(last_battery_str, "%.1f", last_sim_battery);
        sprintf(last_speed_str, "%.0f", last_sim_speed);
        sprintf(last_gear_str, "%d", last_sim_gear);

        // Update only changed values
        uint16_t rpm_color = sim_rpm > 7000 ? M5.Display.color565(255, 0, 0) : TFT_WHITE;
        updateGaugeValue(gauge_positions[0].x, gauge_positions[0].y, gauge_positions[0].w, gauge_positions[0].h,
                         rpm_str, last_rpm_str, 6, rpm_color);

        // Lambda gauge (sprite-based, handles its own updates)
        drawOptimalLambdaGauge(gauge_positions[1].x, gauge_positions[1].y, gauge_positions[1].w, gauge_positions[1].h);

        // Other gauges
        updateGaugeValue(gauge_positions[2].x, gauge_positions[2].y, gauge_positions[2].w, gauge_positions[2].h,
                         tps_str, last_tps_str, 4, TFT_WHITE);
        updateGaugeValue(gauge_positions[3].x, gauge_positions[3].y, gauge_positions[3].w, gauge_positions[3].h,
                         boost_str, last_boost_str, 4, TFT_WHITE);
        updateGaugeValue(gauge_positions[4].x, gauge_positions[4].y, gauge_positions[4].w, gauge_positions[4].h,
                         iat_str, last_iat_str, 4, TFT_WHITE);
        updateGaugeValue(gauge_positions[5].x, gauge_positions[5].y, gauge_positions[5].w, gauge_positions[5].h,
                         ect_str, last_ect_str, 4, TFT_WHITE);
        updateGaugeValue(gauge_positions[6].x, gauge_positions[6].y, gauge_positions[6].w, gauge_positions[6].h,
                         oil_press_str, last_oil_press_str, 3, TFT_WHITE);
        updateGaugeValue(gauge_positions[7].x, gauge_positions[7].y, gauge_positions[7].w, gauge_positions[7].h,
                         fuel_press_str, last_fuel_press_str, 3, TFT_WHITE);
        updateGaugeValue(gauge_positions[8].x, gauge_positions[8].y, gauge_positions[8].w, gauge_positions[8].h,
                         battery_str, last_battery_str, 3, TFT_WHITE);
        updateGaugeValue(gauge_positions[9].x, gauge_positions[9].y, gauge_positions[9].w, gauge_positions[9].h,
                         speed_str, last_speed_str, 3, TFT_WHITE);

        // Update last values
        last_sim_rpm = sim_rpm;
        last_sim_tps = sim_tps;
        last_sim_boost = sim_boost;
        last_sim_iat = sim_iat;
        last_sim_ect = sim_ect;
        last_sim_oil_press = sim_oil_press;
        last_sim_fuel_press = sim_fuel_press;
        last_sim_battery = sim_battery;
        last_sim_speed = sim_speed;
        last_sim_gear = sim_gear;
      }

      last_refresh = millis();
    } else if (current_mode == MODE_CONTROL && millis() - last_refresh > 200) {
      // Refresh control page with updated values
      updateSimulationData();

      // Update dynamic elements that change with simulation
      // For now, boost values and system status are the main dynamic elements
      // The control interface will show real-time boost pressure and system states

      last_refresh = millis();
    }
  }

  delay(10);
}
