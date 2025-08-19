#include <Arduino.h>
#include <M5Unified.h>
#include <ESP32-TWAI-CAN.hpp>
#include <Preferences.h>

// Link G4X Generic Dash 2 CAN Stream - Complete Parameter Set
enum Metric : uint8_t {
  METRIC_RPM = 0,           // Engine Speed (Frame 1000)
  METRIC_MGP,               // Manifold Gauge Pressure/Boost (Frame 1000)
  METRIC_ECT,               // Engine Coolant Temperature (Frame 1000)
  METRIC_IAT,               // Inlet Air Temperature (Frame 1000)
  METRIC_BATTERY,           // ECU Volts/Battery Voltage (Frame 1000)
  METRIC_TPS,               // Throttle Position Sensor (Frame 1000)
  METRIC_OIL_TEMP,          // Oil Temperature (Frame 1001)
  METRIC_IGNITION,          // Ignition Angle (Frame 1001)
  METRIC_VEHICLE_SPEED,     // Vehicle Speed (Frame 1001)
  METRIC_OIL_PRESS,         // Oil Pressure (Frame 1002)
  METRIC_FUEL_PRESS,        // Fuel Pressure (Frame 1002)
  METRIC_LAMBDA,            // Lambda 1 (Frame 1002)
  METRIC_ECU_TEMP,          // ECU Temperature (Frame 1003)
  METRIC_LAMBDA_2,          // Lambda 2 (Frame 1003)
  METRIC_GEAR_POS,          // Gear Position (Frame 1003)
  METRIC_FUEL_LEVEL,        // Fuel Level (Frame 1003)
  METRIC_EGT_1,             // Exhaust Gas Temperature 1 (Frame 1004)
  METRIC_EGT_2,             // Exhaust Gas Temperature 2 (Frame 1004)
  METRIC_KNOCK_LEVEL,       // Knock Level (Frame 1004)
  METRIC_BOOST_DUTY,        // Boost Control Duty (Frame 1004)
  METRIC_LAMBDA_COMBINED,   // Lambda + Target Combined Graph (special - uses local target)
  METRIC_G_FORCE,           // G-Force Display (IMU-based - lateral/longitudinal)
  METRIC_COUNT              // Total number of metrics
};

// ECU Data Structure - Link G4X Generic Dash 2 CAN Stream (4 Frames)
struct ECUData {
  // Frame 1000 (Primary) - Same as Dash2Pro for compatibility
  float rpm = 2150;              // Engine Speed (1 RPM resolution)
  float mgp = 15.2;              // Manifold Gauge Pressure/Boost (0.1 kPa resolution)
  float ect = 87.5;              // Engine Coolant Temperature (1°C resolution)
  float iat = 28.5;              // Inlet Air Temperature Post Intercooler (1°C resolution)
  float battery_voltage = 13.8;   // ECU Volts/Battery Voltage (0.1V resolution)
  float tps = 35.5;              // Throttle Position Sensor (0.5% resolution)

  // Frame 1001 (Extended)
  float oil_temp = 95.2;         // Oil Temperature (1°C resolution)
  float ignition_timing = 18.5;  // Ignition Angle (0.1° resolution)
  float vehicle_speed = 65.0;    // Vehicle Speed (0.1 km/h resolution)

  // Frame 1002 (Pressures & Lambda)
  float oil_pressure = 320.0;    // Oil Pressure (1 kPa resolution)
  float fuel_pressure = 350.0;   // Fuel Pressure (1 kPa resolution)
  float lambda = 0.98;           // Lambda 1 (0.001 Lambda resolution)

  // Frame 1003 (Additional)
  float ecu_temp = 45.0;         // ECU Temperature (1°C resolution)
  float lambda_2 = 0.99;         // Lambda 2 (0.001 Lambda resolution)
  float gear_pos = 3.0;          // Gear Position (1-6, 0=neutral, -1=reverse)
  float fuel_level = 75.0;       // Fuel Level (0-100%)

  // Frame 1004 (Extended sensors)
  float egt_1 = 850.0;           // Exhaust Gas Temperature 1 (1°C resolution)
  float egt_2 = 860.0;           // Exhaust Gas Temperature 2 (1°C resolution)
  float knock_level = 2.5;       // Knock Level (0-10 scale)
  float boost_duty = 45.0;       // Boost Control Duty Cycle (0-100%)

  // Calculated/derived values
  float lambda_target = 1.00;    // Lambda Target (calculated locally)

  // IMU-based G-force data (from built-in accelerometer)
  float g_force_lateral = 0.0;   // Lateral G-force (left/right)
  float g_force_longitudinal = 0.0; // Longitudinal G-force (accel/brake)
  float g_force_total = 0.0;      // Total G-force magnitude

  unsigned long last_update = 0;
};

// Colors
#define TFT_BLACK       0x0000
#define TFT_WHITE       0xFFFF
#define HEADER_BG       0x2104
#define HEADER_COLOR    0x7BEF
#define VALUE_COLOR     0xFFFF
#define LABEL_COLOR     0x8410
#define GOOD_COLOR      0x07E0
#define CAUTION_COLOR   0xFFE0
#define WARNING_COLOR   0xF800

// Layout constants
#define TILE_MARGIN 5
#define HEADER_HEIGHT 45

// CAN Speed options (avoid conflict with MCP2515 library)
enum ConfigCANSpeed {
  CONFIG_CAN_125KBPS = 0,
  CONFIG_CAN_250KBPS = 1,
  CONFIG_CAN_500KBPS = 2,
  CONFIG_CAN_1000KBPS = 3
};

// Configuration structure
struct Config {
  uint32_t can_id_base = 1000;  // Base CAN ID for Generic Dash 2 (1000, 1001, 1002, 1003)
  bool use_fahrenheit = false;  // Temperature units (false = Celsius, true = Fahrenheit)
  bool use_psi = false;         // Pressure units (false = kPa, true = PSI)
  bool use_mph = false;         // Speed units (false = km/h, true = mph)
  bool simulation_mode = false; // Data source (false = real CAN bus, true = simulation)
  ConfigCANSpeed can_speed = CONFIG_CAN_1000KBPS;  // CAN bus speed (default: 1Mbps for Link G4X)
};

// Global variables
ECUData ecu_data;
ECUData prev_ecu_data; // Store previous values to detect changes
Metric g_tile_metric[12];
Config config;
bool display_needs_update = true;
bool initial_draw = true;
bool config_mode = false;
bool config_needs_redraw = true;
bool can_id_input_mode = false;

// Touch variables
int16_t touch_x, touch_y;

// CAN ID input buffer
char can_id_buffer[8] = "1000";
int can_id_cursor = 4;

// Gauge configuration modal variables
bool gauge_config_mode = false;
int selected_tile_index = -1;
bool gauge_config_needs_redraw = false;
unsigned long last_click_time = 0;
int last_clicked_tile = -1;
const unsigned long DOUBLE_CLICK_TIMEOUT = 500; // 500ms window for double-click

// CAN Bus setup using M5Stack Tab5 built-in RS485 interface (SIT3088 transceiver)
// The Tab5 has a built-in RS485 interface with SIT3088 transceiver and switchable 120Ω terminator
// This can be used for CAN communication by configuring the ESP32-P4 TWAI controller
#define CAN_TX_PIN    20   // Tab5 RS485 TX pin (SIT3088)
#define CAN_RX_PIN    21   // Tab5 RS485 RX pin (SIT3088)
#define CAN_DIR_PIN   34   // Tab5 RS485 Direction control pin
// Note: Built-in SIT3088 transceiver handles the physical layer - no external transceiver needed!
bool can_initialized = false;
unsigned long last_can_message = 0;
unsigned long can_message_count = 0;

// CAN Message Buffer for smooth display updates
#define CAN_BUFFER_SIZE 32
struct CANBuffer {
  twai_message_t messages[CAN_BUFFER_SIZE];
  volatile int write_index = 0;
  volatile int read_index = 0;
  volatile int count = 0;
} can_buffer;

// CAN Buffer Management Functions
bool addToCANBuffer(const twai_message_t& msg) {
  if (can_buffer.count >= CAN_BUFFER_SIZE) {
    return false; // Buffer full
  }

  can_buffer.messages[can_buffer.write_index] = msg;
  can_buffer.write_index = (can_buffer.write_index + 1) % CAN_BUFFER_SIZE;
  can_buffer.count++;
  return true;
}

bool getFromCANBuffer(twai_message_t& msg) {
  if (can_buffer.count == 0) {
    return false; // Buffer empty
  }

  msg = can_buffer.messages[can_buffer.read_index];
  can_buffer.read_index = (can_buffer.read_index + 1) % CAN_BUFFER_SIZE;
  can_buffer.count--;
  return true;
}

// Process a single CAN message (moved from main loop for buffering)
void processCANMessage(const twai_message_t& canMsg) {
  // Parse Haltech IC7 CAN Protocol (Multiple frames: 0x360, 0x361, 0x362, 0x3E0, etc.)
  if (canMsg.data_length_code == 8) {

    // Frame 0x360 (864): Engine Speed, MAP, TPS (Main) - 50Hz
    if (canMsg.identifier == 864) {
      // Haltech CAN V2 Protocol - Frame 0x360 (BIG ENDIAN - MSB first)
      // Bytes 0-1: RPM (direct value, revolutions per minute)
      ecu_data.rpm = (canMsg.data[0] << 8) | canMsg.data[1];

      // Bytes 2-3: MAP (1/10th kPa)
      uint16_t map_raw = (canMsg.data[2] << 8) | canMsg.data[3];
      ecu_data.mgp = map_raw * 0.1; // Convert to kPa

      // Bytes 4-5: TPS (1/10th of 1%)
      uint16_t tps_raw = (canMsg.data[4] << 8) | canMsg.data[5];
      ecu_data.tps = tps_raw * 0.1; // Convert to percentage

      // Bytes 6-7: Reserved or additional data

      // Automatically disable simulation mode when real CAN data is received
      static bool simulation_disabled = false;
      if (config.simulation_mode && !simulation_disabled) {
        config.simulation_mode = false;
        simulation_disabled = true;
        Serial.println("HALTECH IC7 CAN DATA - Simulation disabled");
        // Note: saveConfig() will be called from main loop
      }

      // Trigger display update for responsive UI
      display_needs_update = true;

      // Minimal frame 0x360 debug (reduced frequency for performance)
      static unsigned long last_360_debug = 0;
      if (millis() - last_360_debug > 30000) { // Only every 30 seconds
        Serial.printf("Frame 0x360: RPM=%d MAP=%.1f TPS=%.1f\n", (int)ecu_data.rpm, ecu_data.mgp, ecu_data.tps);
        last_360_debug = millis();
      }
    }

    // Frame 0x361 (865): Fuel Pressure, Oil Pressure - 50Hz
    else if (canMsg.identifier == 865) {
      // Haltech CAN V2 Protocol - Frame 0x361 (BIG ENDIAN - MSB first)
      // Bytes 0-1: Fuel Pressure (1/10th kPa)
      uint16_t fuel_press_raw = (canMsg.data[0] << 8) | canMsg.data[1];
      ecu_data.fuel_pressure = fuel_press_raw * 0.1; // Convert to kPa

      // Bytes 2-3: Oil Pressure (1/10th kPa)
      uint16_t oil_press_raw = (canMsg.data[2] << 8) | canMsg.data[3];
      ecu_data.oil_pressure = oil_press_raw * 0.1; // Convert to kPa

      // Bytes 4-7: Reserved or additional data (not specified in FTY Racing spec)
      display_needs_update = true; // Trigger display update
    }

    // Frame 0x362 (866): Injector Duty Cycle, Ignition Angle - 50Hz
    else if (canMsg.identifier == 866) {
      // Haltech CAN V2 Protocol - Frame 0x362 (BIG ENDIAN - MSB first)
      // Bytes 0-1: Primary Injector Duty (1/10th of 1%)
      uint16_t inj_duty_raw = (canMsg.data[0] << 8) | canMsg.data[1];
      // ecu_data.injector_duty = inj_duty_raw * 0.1; // Could add this to ECUData structure

      // Bytes 2-3: Secondary Injector Duty (1/10th of 1%)
      uint16_t inj_duty2_raw = (canMsg.data[2] << 8) | canMsg.data[3];
      // ecu_data.injector_duty2 = inj_duty2_raw * 0.1; // Could add this to ECUData structure

      // Bytes 4-5: Ignition Angle Leading (1/10th of a degree)
      int16_t ignition_raw = (canMsg.data[4] << 8) | canMsg.data[5];
      ecu_data.ignition_timing = ignition_raw * 0.1; // Convert to degrees

      // Bytes 6-7: Ignition Angle Trailing (1/10th of a degree)
      // int16_t ignition_trail_raw = (canMsg.data[6] << 8) | canMsg.data[7];
      // ecu_data.ignition_timing_trailing = ignition_trail_raw * 0.1; // Could add this
      display_needs_update = true; // Trigger display update
    }

    // Frame 0x3E0 (992): ECT, IAT, Fuel Temperature, Oil Temperature - 5Hz
    else if (canMsg.identifier == 992) {
      // Haltech CAN V2 Protocol - Frame 0x3E0 (BIG ENDIAN - MSB first)
      // Bytes 0-1: Coolant Temp (0.1 Kelvin)
      uint16_t ect_raw = (canMsg.data[0] << 8) | canMsg.data[1];
      float new_ect = (ect_raw * 0.1) - 273.15; // Convert from Kelvin to Celsius

      // Bytes 2-3: Air Temp (0.1 Kelvin) - Apply filtering for stability
      uint16_t iat_raw = (canMsg.data[2] << 8) | canMsg.data[3];
      float new_iat = (iat_raw * 0.1) - 273.15; // Convert from Kelvin to Celsius

      // Debug temperature values occasionally
      static unsigned long last_temp_debug = 0;
      if (millis() - last_temp_debug > 10000) {
        Serial.printf("Temps: ECT_raw=%d (%.1f°C) IAT_raw=%d (%.1f°C)\n",
                      ect_raw, new_ect, iat_raw, new_iat);
        last_temp_debug = millis();
      }

      // Simple low-pass filter for IAT stability (only if value is reasonable)
      if (new_iat > -40 && new_iat < 150) { // Sanity check for reasonable temperature range
        ecu_data.iat = ecu_data.iat * 0.8 + new_iat * 0.2; // 80% old, 20% new
      }

      // ECT with less filtering (more responsive)
      if (new_ect > -40 && new_ect < 150) {
        ecu_data.ect = ecu_data.ect * 0.5 + new_ect * 0.5; // 50% old, 50% new
      }

      // Bytes 4-5: Fuel Temp (0.1 Kelvin)
      uint16_t fuel_temp_raw = (canMsg.data[4] << 8) | canMsg.data[5];
      // ecu_data.fuel_temp = (fuel_temp_raw * 0.1) - 273.15; // Could add this to ECUData structure

      // Bytes 6-7: Oil Temp (0.1 Kelvin)
      uint16_t oil_temp_raw = (canMsg.data[6] << 8) | canMsg.data[7];
      float new_oil_temp = (oil_temp_raw * 0.1) - 273.15; // Convert from Kelvin to Celsius
      if (new_oil_temp > -40 && new_oil_temp < 200) {
        ecu_data.oil_temp = ecu_data.oil_temp * 0.7 + new_oil_temp * 0.3; // 70% old, 30% new
      }
      display_needs_update = true; // Trigger display update
    }

    // Frame 0x368 (872): Lambda 1-2 - 20Hz
    else if (canMsg.identifier == 872) {
      // Haltech CAN V2 Protocol - Frame 0x368 (BIG ENDIAN - MSB first)
      // Bytes 0-1: Lambda 1 (0.001 Lambda)
      uint16_t lambda1_raw = (canMsg.data[0] << 8) | canMsg.data[1];
      ecu_data.lambda = lambda1_raw * 0.001; // Convert to lambda ratio

      // Bytes 2-3: Lambda 2 (0.001 Lambda)
      uint16_t lambda2_raw = (canMsg.data[2] << 8) | canMsg.data[3];
      // ecu_data.lambda2 = lambda2_raw * 0.001; // Could add this to ECUData structure

      // Bytes 4-7: Reserved or additional data
      display_needs_update = true; // Trigger display update
    }
  }
}

// Preferences for persistent storage
Preferences preferences;

// Function declarations
bool initializeCAN();
bool readCANData();
void saveConfig();
void loadConfig();
const char* getCANSpeedName(ConfigCANSpeed speed);
void drawGaugeConfigModal();
bool handleGaugeConfigTouch(int x, int y);
bool isMetricAvailable(Metric metric);
void saveGaugeConfiguration();
void loadGaugeConfiguration();

// Default tile layout (Link G4X Generic Dash 2 CAN Stream + G-Force Gauge)
void setDefaultTileLayout() {
  g_tile_metric[0] = METRIC_RPM;              // r1c1 - Engine Speed (Frame 1000)
  g_tile_metric[1] = METRIC_TPS;              // r1c2 - Throttle Position (Frame 1000)
  g_tile_metric[2] = METRIC_MGP;              // r1c3 - Boost Pressure (Frame 1000)
  g_tile_metric[3] = METRIC_G_FORCE;          // r1c4 - G-Force Gauge (IMU-based)
  g_tile_metric[4] = METRIC_ECT;              // r2c1 - Coolant Temperature (Frame 1000)
  g_tile_metric[5] = METRIC_OIL_TEMP;         // r2c2 - Oil Temperature (Frame 1001)
  g_tile_metric[6] = METRIC_IAT;              // r2c3 - Intake Air Temperature (Frame 1000)
  g_tile_metric[7] = METRIC_IGNITION;         // r2c4 - Ignition Angle (Frame 1001)
  g_tile_metric[8] = METRIC_OIL_PRESS;        // r3c1 - Oil Pressure (Frame 1002)
  g_tile_metric[9] = METRIC_FUEL_PRESS;       // r3c2 - Fuel Pressure (Frame 1002)
  g_tile_metric[10] = METRIC_VEHICLE_SPEED;   // r3c3 - Vehicle Speed (Frame 1001)
  g_tile_metric[11] = METRIC_BATTERY;         // r3c4 - Battery Voltage (Frame 1000)
}

// Get metric name and unit for display - Link G4X Generic Dash 2 CAN Stream
const char* getMetricName(Metric metric) {
  switch (metric) {
    case METRIC_RPM: return "RPM";
    case METRIC_MGP: return "MGP";
    case METRIC_ECT: return "ECT";
    case METRIC_IAT: return "IAT";
    case METRIC_BATTERY: return "BATTERY";
    case METRIC_OIL_TEMP: return "OIL TEMP";
    case METRIC_TPS: return "TPS";
    case METRIC_IGNITION: return "IGNITION";
    case METRIC_VEHICLE_SPEED: return "SPEED";
    case METRIC_OIL_PRESS: return "OIL PRESS";
    case METRIC_FUEL_PRESS: return "FUEL PRESS";
    case METRIC_ECU_TEMP: return "ECU TEMP";
    case METRIC_LAMBDA: return "LAMBDA";
    case METRIC_LAMBDA_2: return "LAMBDA 2";
    case METRIC_GEAR_POS: return "GEAR";
    case METRIC_FUEL_LEVEL: return "FUEL LEVEL";
    case METRIC_EGT_1: return "EGT 1";
    case METRIC_EGT_2: return "EGT 2";
    case METRIC_KNOCK_LEVEL: return "KNOCK";
    case METRIC_BOOST_DUTY: return "BOOST DUTY";
    case METRIC_LAMBDA_COMBINED: return "LAMBDA GRAPH";
    case METRIC_G_FORCE: return "G-FORCE";
    default: return "UNKNOWN";
  }
}

// Unit conversion functions
float convertTemperature(float celsius) {
  return config.use_fahrenheit ? (celsius * 9.0/5.0 + 32.0) : celsius;
}

float convertPressure(float kpa) {
  return config.use_psi ? (kpa * 0.145038) : kpa;
}

float convertSpeed(float kmh) {
  return config.use_mph ? (kmh * 0.621371) : kmh;
}

const char* getMetricUnit(Metric metric) {
  switch (metric) {
    case METRIC_RPM: return "";
    case METRIC_MGP: return config.use_psi ? "PSI" : "kPa";
    case METRIC_ECT: return config.use_fahrenheit ? "F" : "C";
    case METRIC_IAT: return config.use_fahrenheit ? "F" : "C";
    case METRIC_BATTERY: return "V";
    case METRIC_OIL_TEMP: return config.use_fahrenheit ? "F" : "C";
    case METRIC_TPS: return "";
    case METRIC_IGNITION: return "deg";
    case METRIC_VEHICLE_SPEED: return config.use_mph ? "mph" : "km/h";
    case METRIC_OIL_PRESS: return config.use_psi ? "PSI" : "kPa";
    case METRIC_FUEL_PRESS: return config.use_psi ? "PSI" : "kPa";
    case METRIC_ECU_TEMP: return config.use_fahrenheit ? "F" : "C";
    case METRIC_LAMBDA: return "";
    case METRIC_LAMBDA_2: return "";
    case METRIC_GEAR_POS: return "";
    case METRIC_FUEL_LEVEL: return "%";
    case METRIC_EGT_1: return config.use_fahrenheit ? "F" : "C";
    case METRIC_EGT_2: return config.use_fahrenheit ? "F" : "C";
    case METRIC_KNOCK_LEVEL: return "";
    case METRIC_BOOST_DUTY: return "%";
    case METRIC_LAMBDA_COMBINED: return "";
    case METRIC_G_FORCE: return "G";
    default: return "";
  }
}

// Tile positioning
void tileIndexToColRow(int index, int &col, int &row) {
  col = index % 4;
  row = index / 4;
}

void tileRect(int col, int row, int &x, int &y, int &w, int &h) {
  w = (1280 - 5 * TILE_MARGIN) / 4;
  h = (720 - HEADER_HEIGHT - 4 * TILE_MARGIN) / 3;
  x = TILE_MARGIN + col * (w + TILE_MARGIN);
  y = HEADER_HEIGHT + TILE_MARGIN + row * (h + TILE_MARGIN);
}

// Simple unified tile drawing - left-aligned and clean
void drawMetricTile(int x, int y, int w, int h, const char* label, const char* value_str, const char* unit, uint16_t color, int tile_index, bool force_redraw = false, bool is_rpm = false) {
  static String last_values[12] = {""}; // Track last values for each tile

  // Check if value changed
  String current_value = String(value_str);
  bool value_changed = (current_value != last_values[tile_index]);

  if (force_redraw) {
    // Full redraw - background and all elements
    M5.Display.fillRoundRect(x, y, w, h, 8, 0x2104);
    M5.Display.drawRoundRect(x, y, w, h, 8, 0x4208);

    // Label - left-aligned
    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setFont(&fonts::DejaVu24);
    M5.Display.drawString(label, x + 10, y + 8);

    // Unit - positioned in bottom right
    if (strlen(unit) > 0) {
      M5.Display.setTextColor(TFT_WHITE);
      M5.Display.setFont(&fonts::DejaVu24);
      M5.Display.drawString(unit, x + w - 100, y + h - 35);
    }
  }

  // Only update value if it changed or force redraw
  if (force_redraw || value_changed) {
    // Clear value area
    M5.Display.fillRect(x + 10, y + 40, w - 20, 90, 0x2104);

    // Value - center-justified within the tile
    M5.Display.setTextDatum(textdatum_t::top_center);
    M5.Display.setTextColor(color);
    M5.Display.setFont(&fonts::DejaVu72);

    // Calculate center position of the tile
    int center_x = x + w / 2;

    if (is_rpm) {
      // RPM gets bold effect, centered
      M5.Display.drawString(value_str, center_x, y + 50);
      M5.Display.drawString(value_str, center_x + 1, y + 50); // Bold effect
    } else {
      // Standard center-justified values
      M5.Display.drawString(value_str, center_x, y + 50);
    }

    last_values[tile_index] = current_value;
  }
}



// Modern automotive-style lambda display - large and easily readable
void drawLambdaDoubleArc(int x, int y, int w, int h, float lambda, float lambda_target, int tile_index, bool force_redraw = false) {
  static float last_lambda[12] = {0};
  static float last_lambda_target[12] = {0};

  if (force_redraw) {
    // Full redraw - modern dark background
    M5.Display.fillRoundRect(x, y, w, h, 12, 0x1082);
    M5.Display.drawRoundRect(x, y, w, h, 12, 0x4208);

    // Clean lambda header
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setFont(&fonts::DejaVu24);
    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.drawString("LAMBDA", x + 10, y + 8);
  }

  // Only update if values changed
  if (force_redraw || abs(lambda - last_lambda[tile_index]) > 0.01 || abs(lambda_target - last_lambda_target[tile_index]) > 0.01) {

    // Determine status color based on lambda accuracy
    uint16_t status_color = VALUE_COLOR;
    float diff = abs(lambda - lambda_target);
    if (diff > 0.15) status_color = WARNING_COLOR;
    else if (diff > 0.10) status_color = CAUTION_COLOR;
    else if (diff < 0.05) status_color = GOOD_COLOR;

    // Clear main display area
    M5.Display.fillRect(x + 10, y + 35, w - 20, h - 70, 0x1082);

    // Draw massive lambda value - primary focus, centered
    M5.Display.setTextColor(status_color);
    M5.Display.setFont(&fonts::DejaVu56);
    M5.Display.setTextDatum(textdatum_t::top_center);
    char lambda_str[16];
    sprintf(lambda_str, "%.3f", lambda);

    // Center the large lambda value
    int center_x = x + w / 2;
    M5.Display.drawString(lambda_str, center_x, y + 45);

    // Draw target value below - secondary info, centered
    M5.Display.setTextColor(0x07FF);
    M5.Display.setFont(&fonts::DejaVu24);
    M5.Display.setTextDatum(textdatum_t::top_center);
    char target_str[16];
    sprintf(target_str, "Target: %.3f", lambda_target);

    // Center the target value
    M5.Display.drawString(target_str, center_x, y + 110);

    // Draw lambda error/deviation from target, centered
    M5.Display.setTextColor(status_color);
    M5.Display.setFont(&fonts::DejaVu18);
    M5.Display.setTextDatum(textdatum_t::top_center);
    char diff_str[16];
    float error = lambda - lambda_target;
    if (abs(error) < 0.005) {
      sprintf(diff_str, "ON TARGET");
    } else if (error > 0) {
      sprintf(diff_str, "+%.3f", error);
    } else {
      sprintf(diff_str, "%.3f", error); // Negative sign included
    }

    // Center the difference indicator
    M5.Display.drawString(diff_str, center_x, y + 140);

    // Clear the bar area first to prevent artifacts
    M5.Display.fillRect(x + 15, y + h - 35, w - 30, 30, 0x1082);

    // Draw colored lambda bar - static background
    int bar_width = w - 60;
    int bar_height = 12;
    int bar_x = x + 30;
    int bar_y = y + h - 30;

    // Draw lambda range bar background (0.7 to 1.3)
    for (int i = 0; i < bar_width; i++) {
      float lambda_value = 0.7 + (1.3 - 0.7) * ((float)i / bar_width);
      uint16_t bar_color;

      if (lambda_value < 0.85) {
        // Very low lambda - red
        bar_color = 0xF800; // Red
      } else if (lambda_value < 0.95) {
        // Low lambda - orange
        bar_color = 0xFD20; // Orange
      } else if (lambda_value > 1.15) {
        // High lambda - blue
        bar_color = 0x001F; // Blue
      } else if (lambda_value > 1.05) {
        // Moderately high lambda - light blue
        bar_color = 0x07FF; // Cyan
      } else {
        // Normal operating range - green
        bar_color = 0x07E0; // Green
      }

      M5.Display.fillRect(bar_x + i, bar_y, 2, bar_height, bar_color);
    }

    // Draw bar outline
    M5.Display.drawRect(bar_x - 1, bar_y - 1, bar_width + 2, bar_height + 2, 0x8410);

    // Calculate needle positions (0.7 to 1.3 range)
    float lambda_min = 0.7;
    float lambda_max = 1.3;
    int lambda_pos = map(constrain(lambda * 100, lambda_min * 100, lambda_max * 100),
                        lambda_min * 100, lambda_max * 100, 0, bar_width);
    int target_pos = map(constrain(lambda_target * 100, lambda_min * 100, lambda_max * 100),
                        lambda_min * 100, lambda_max * 100, 0, bar_width);

    // Draw target needle (cyan triangle pointing down)
    int target_x = bar_x + target_pos;
    M5.Display.fillTriangle(target_x - 3, bar_y - 5, target_x + 3, bar_y - 5, target_x, bar_y - 1, 0x07FF);
    M5.Display.drawTriangle(target_x - 3, bar_y - 5, target_x + 3, bar_y - 5, target_x, bar_y - 1, TFT_WHITE);

    // Draw lambda needle (white triangle pointing up)
    int lambda_x = bar_x + lambda_pos;
    M5.Display.fillTriangle(lambda_x - 4, bar_y + bar_height + 5, lambda_x + 4, bar_y + bar_height + 5, lambda_x, bar_y + bar_height + 1, status_color);
    M5.Display.drawTriangle(lambda_x - 4, bar_y + bar_height + 5, lambda_x + 4, bar_y + bar_height + 5, lambda_x, bar_y + bar_height + 1, TFT_WHITE);

    // Draw scale marks below bar
    M5.Display.setTextColor(0x8410);
    M5.Display.setFont(&fonts::DejaVu9);
    M5.Display.drawString("0.7", bar_x - 5, bar_y + 18);
    M5.Display.drawString("1.0", bar_x + bar_width/2 - 8, bar_y + 18);
    M5.Display.drawString("1.3", bar_x + bar_width - 10, bar_y + 18);

    last_lambda[tile_index] = lambda;
    last_lambda_target[tile_index] = lambda_target;
  }
}

// G-Force gauge with lateral and longitudinal display
void drawGForceGauge(int x, int y, int w, int h, float lateral_g, float longitudinal_g, float total_g, int tile_index, bool force_redraw = false) {
  static float last_lateral[12] = {0};
  static float last_longitudinal[12] = {0};
  static float last_total[12] = {0};

  if (force_redraw ||
      abs(lateral_g - last_lateral[tile_index]) > 0.01 ||
      abs(longitudinal_g - last_longitudinal[tile_index]) > 0.01 ||
      abs(total_g - last_total[tile_index]) > 0.01) {

    // Full redraw - modern dark background
    M5.Display.fillRoundRect(x, y, w, h, 12, 0x1082);
    M5.Display.drawRoundRect(x, y, w, h, 12, 0x4208);

    // Header
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setFont(&fonts::DejaVu18);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.drawString("G-FORCE", x + w/2, y + 20);

    // Center point for the G-force circle
    int center_x = x + w/2;
    int center_y = y + h/2 + 10;
    int circle_radius = min(w, h) / 3;

    // Draw G-force circle background (gray)
    M5.Display.drawCircle(center_x, center_y, circle_radius, 0x4208);
    M5.Display.drawCircle(center_x, center_y, circle_radius/2, 0x2104);

    // Draw crosshairs
    M5.Display.drawLine(center_x - circle_radius, center_y, center_x + circle_radius, center_y, 0x4208);
    M5.Display.drawLine(center_x, center_y - circle_radius, center_x, center_y + circle_radius, 0x4208);

    // Calculate G-force dot position (scale to circle)
    float scale = circle_radius / 1.0f; // 1G max scale (more sensitive)
    int dot_x = center_x + (int)(lateral_g * scale);
    int dot_y = center_y - (int)(longitudinal_g * scale); // Invert Y for screen coordinates

    // Clamp to circle
    float distance = sqrt((dot_x - center_x) * (dot_x - center_x) + (dot_y - center_y) * (dot_y - center_y));
    if (distance > circle_radius) {
      float ratio = circle_radius / distance;
      dot_x = center_x + (int)((dot_x - center_x) * ratio);
      dot_y = center_y + (int)((dot_y - center_y) * ratio);
    }

    // Draw G-force dot with color based on intensity (1G scale)
    uint16_t dot_color = GOOD_COLOR;
    if (total_g > 0.8) dot_color = WARNING_COLOR;      // Red above 0.8G
    else if (total_g > 0.5) dot_color = CAUTION_COLOR; // Yellow above 0.5G

    M5.Display.fillCircle(dot_x, dot_y, 6, dot_color);
    M5.Display.drawCircle(dot_x, dot_y, 6, TFT_WHITE);

    // Display numerical values
    M5.Display.setFont(&fonts::DejaVu12);
    M5.Display.setTextColor(TFT_WHITE);

    // Total G-force (large)
    char total_str[16];
    sprintf(total_str, "%.2fG", total_g);
    M5.Display.setFont(&fonts::DejaVu18);
    M5.Display.drawString(total_str, center_x, y + h - 40);

    // Lateral and longitudinal (smaller)
    M5.Display.setFont(&fonts::DejaVu12);
    char lat_str[16], long_str[16];
    sprintf(lat_str, "LAT: %.2f", lateral_g);
    sprintf(long_str, "LON: %.2f", longitudinal_g);

    M5.Display.setTextColor(0xBDF7); // Light gray
    M5.Display.drawString(lat_str, center_x - 60, y + h - 20);
    M5.Display.drawString(long_str, center_x + 60, y + h - 20);

    // Scale labels (1G max scale)
    M5.Display.setFont(&fonts::DejaVu9);
    M5.Display.setTextColor(0x8410);
    M5.Display.drawString("0.5G", center_x + circle_radius/2 + 5, center_y - 5);
    M5.Display.drawString("1G", center_x + circle_radius + 5, center_y - 5);

    last_lateral[tile_index] = lateral_g;
    last_longitudinal[tile_index] = longitudinal_g;
    last_total[tile_index] = total_g;
  }
}

// Draw configuration page
void drawConfigPage() {
  // Only draw when needed
  if (!config_needs_redraw) return;

  M5.Display.fillScreen(TFT_BLACK);
  config_needs_redraw = false;

  // Set consistent left alignment for all config page text
  M5.Display.setTextDatum(textdatum_t::top_left);

  // Header
  M5.Display.fillRect(0, 0, 1280, HEADER_HEIGHT, HEADER_BG);
  M5.Display.setTextColor(HEADER_COLOR);
  M5.Display.setFont(&fonts::DejaVu24);
  M5.Display.drawString("CONFIGURATION", 20, 12);

  // Back button
  M5.Display.setTextColor(WARNING_COLOR);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.drawString("BACK", 1280 - 80, 15);

  int y_pos = 80;
  int line_height = 60;

  // CAN ID Configuration
  M5.Display.fillRoundRect(50, y_pos, 1180, 50, 8, 0x2104);
  M5.Display.drawRoundRect(50, y_pos, 1180, 50, 8, 0x4208);
  M5.Display.setTextColor(LABEL_COLOR);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.drawString("Base CAN ID (1000-1003):", 70, y_pos + 15);

  M5.Display.setTextColor(VALUE_COLOR);
  M5.Display.setFont(&fonts::DejaVu24);
  char can_id_str[16];
  sprintf(can_id_str, "%d", config.can_id_base);
  M5.Display.drawString(can_id_str, 300, y_pos + 12);

  // Edit button for CAN ID
  M5.Display.fillRoundRect(600, y_pos + 5, 120, 40, 5, 0x07FF);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.setTextDatum(textdatum_t::top_center); // Center text in button
  M5.Display.drawString("EDIT", 660, y_pos + 15);
  M5.Display.setTextDatum(textdatum_t::top_left); // Reset to left align

  y_pos += line_height;

  // Temperature Units
  M5.Display.fillRoundRect(50, y_pos, 1180, 50, 8, 0x2104);
  M5.Display.drawRoundRect(50, y_pos, 1180, 50, 8, 0x4208);
  M5.Display.setTextColor(LABEL_COLOR);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.drawString("Temperature Units:", 70, y_pos + 15);

  // Celsius button
  uint16_t c_color = config.use_fahrenheit ? 0x4208 : GOOD_COLOR;
  M5.Display.fillRoundRect(400, y_pos + 5, 120, 40, 5, c_color);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.setTextDatum(textdatum_t::top_center);
  M5.Display.drawString("Celsius", 460, y_pos + 15); // Center in 120px button

  // Fahrenheit button
  uint16_t f_color = config.use_fahrenheit ? GOOD_COLOR : 0x4208;
  M5.Display.fillRoundRect(540, y_pos + 5, 140, 40, 5, f_color);
  M5.Display.drawString("Fahrenheit", 610, y_pos + 15); // Center in 140px button
  M5.Display.setTextDatum(textdatum_t::top_left); // Reset to left align

  y_pos += line_height;

  // Pressure Units
  M5.Display.fillRoundRect(50, y_pos, 1180, 50, 8, 0x2104);
  M5.Display.drawRoundRect(50, y_pos, 1180, 50, 8, 0x4208);
  M5.Display.setTextColor(LABEL_COLOR);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.drawString("Pressure Units:", 70, y_pos + 15);

  // kPa button
  uint16_t kpa_color = config.use_psi ? 0x4208 : GOOD_COLOR;
  M5.Display.fillRoundRect(400, y_pos + 5, 80, 40, 5, kpa_color);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.setTextDatum(textdatum_t::top_center);
  M5.Display.drawString("kPa", 440, y_pos + 15); // Center in 80px button

  // PSI button
  uint16_t psi_color = config.use_psi ? GOOD_COLOR : 0x4208;
  M5.Display.fillRoundRect(500, y_pos + 5, 80, 40, 5, psi_color);
  M5.Display.drawString("PSI", 540, y_pos + 15); // Center in 80px button
  M5.Display.setTextDatum(textdatum_t::top_left); // Reset to left align

  y_pos += line_height;

  // Speed Units
  M5.Display.fillRoundRect(50, y_pos, 1180, 50, 8, 0x2104);
  M5.Display.drawRoundRect(50, y_pos, 1180, 50, 8, 0x4208);
  M5.Display.setTextColor(LABEL_COLOR);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.drawString("Speed Units:", 70, y_pos + 15);

  // km/h button
  uint16_t kmh_color = config.use_mph ? 0x4208 : GOOD_COLOR;
  M5.Display.fillRoundRect(400, y_pos + 5, 100, 40, 5, kmh_color);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.setTextDatum(textdatum_t::top_center);
  M5.Display.drawString("km/h", 450, y_pos + 15); // Center in 100px button

  // mph button
  uint16_t mph_color = config.use_mph ? GOOD_COLOR : 0x4208;
  M5.Display.fillRoundRect(520, y_pos + 5, 100, 40, 5, mph_color);
  M5.Display.drawString("mph", 570, y_pos + 15); // Center in 100px button
  M5.Display.setTextDatum(textdatum_t::top_left); // Reset to left align

  y_pos += line_height;

  // CAN Speed Configuration
  M5.Display.fillRoundRect(50, y_pos, 1180, 50, 8, 0x2104);
  M5.Display.drawRoundRect(50, y_pos, 1180, 50, 8, 0x4208);
  M5.Display.setTextColor(LABEL_COLOR);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.drawString("CAN Speed:", 70, y_pos + 15);

  // CAN Speed buttons
  const char* speed_labels[] = {"125k", "250k", "500k", "1M"};
  M5.Display.setTextDatum(textdatum_t::top_center);
  for (int i = 0; i < 4; i++) {
    uint16_t speed_color = (config.can_speed == i) ? GOOD_COLOR : 0x4208;
    M5.Display.fillRoundRect(350 + i * 90, y_pos + 5, 80, 40, 5, speed_color);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setFont(&fonts::DejaVu18);
    M5.Display.drawString(speed_labels[i], 390 + i * 90, y_pos + 15); // Center in 80px buttons
  }
  M5.Display.setTextDatum(textdatum_t::top_left); // Reset to left align

  y_pos += line_height;

  // Data Source Mode
  M5.Display.fillRoundRect(50, y_pos, 1180, 50, 8, 0x2104);
  M5.Display.drawRoundRect(50, y_pos, 1180, 50, 8, 0x4208);
  M5.Display.setTextColor(LABEL_COLOR);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.drawString("Data Source:", 70, y_pos + 15);

  // Simulation button - larger for easier touch
  uint16_t sim_color = config.simulation_mode ? GOOD_COLOR : 0x4208;
  M5.Display.fillRoundRect(350, y_pos + 5, 180, 50, 5, sim_color);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.setTextDatum(textdatum_t::top_center);
  M5.Display.drawString("Simulation", 440, y_pos + 20); // Center in 180px button

  // Real CAN button - larger for easier touch
  uint16_t can_color = config.simulation_mode ? 0x4208 : GOOD_COLOR;
  M5.Display.fillRoundRect(550, y_pos + 5, 180, 50, 5, can_color);
  M5.Display.drawString("Real CAN", 640, y_pos + 20); // Center in 180px button

  y_pos += line_height + 20;

  // Save button
  M5.Display.fillRoundRect(500, y_pos, 280, 60, 10, GOOD_COLOR);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setFont(&fonts::DejaVu24);
  M5.Display.drawString("SAVE & EXIT", 640, y_pos + 20); // Center in 280px button
  M5.Display.setTextDatum(textdatum_t::top_left); // Reset to left align
}

// Draw numeric keypad for CAN ID input
void drawNumericKeypad() {
  M5.Display.fillScreen(TFT_BLACK);

  // Set consistent left alignment for keypad page
  M5.Display.setTextDatum(textdatum_t::top_left);

  // Header
  M5.Display.fillRect(0, 0, 1280, HEADER_HEIGHT, HEADER_BG);
  M5.Display.setTextColor(HEADER_COLOR);
  M5.Display.setFont(&fonts::DejaVu24);
  M5.Display.drawString("ENTER CAN ID", 20, 12);

  // Back button
  M5.Display.setTextColor(WARNING_COLOR);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.drawString("BACK", 1280 - 80, 15);

  // Input display area
  M5.Display.fillRoundRect(200, 80, 880, 80, 10, 0x2104);
  M5.Display.drawRoundRect(200, 80, 880, 80, 10, 0x4208);

  M5.Display.setTextColor(LABEL_COLOR);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.drawString("Base CAN ID (Generic Dash 2):", 220, 90);

  // Display current input with cursor
  M5.Display.setTextColor(VALUE_COLOR);
  M5.Display.setFont(&fonts::DejaVu56);
  M5.Display.drawString(can_id_buffer, 400, 110);

  // Draw cursor
  int cursor_x = 400 + (can_id_cursor * 35);
  M5.Display.fillRect(cursor_x, 155, 3, 5, VALUE_COLOR);

  // Valid range indicator
  M5.Display.setTextColor(0x8410);
  M5.Display.setFont(&fonts::DejaVu12);
  M5.Display.drawString("Valid range: 1000 (default), 864 (legacy)", 220, 140);

  // Keypad layout (4x3 grid + extra buttons)
  int keypad_x = 300;
  int keypad_y = 200;
  int button_w = 120;
  int button_h = 80;
  int spacing = 20;

  // Number buttons (1-9) - properly centered
  M5.Display.setTextDatum(textdatum_t::middle_center);
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      int num = (row * 3) + col + 1;
      int x = keypad_x + col * (button_w + spacing);
      int y = keypad_y + row * (button_h + spacing);

      M5.Display.fillRoundRect(x, y, button_w, button_h, 8, 0x4208);
      M5.Display.drawRoundRect(x, y, button_w, button_h, 8, TFT_WHITE);

      M5.Display.setTextColor(TFT_WHITE);
      M5.Display.setFont(&fonts::DejaVu24);
      char num_str[2];
      sprintf(num_str, "%d", num);
      M5.Display.drawString(num_str, x + button_w/2, y + button_h/2); // Perfect center
    }
  }

  // Bottom row: 0, Clear, Backspace
  int bottom_y = keypad_y + 3 * (button_h + spacing);

  // 0 button
  M5.Display.fillRoundRect(keypad_x + (button_w + spacing), bottom_y, button_w, button_h, 8, 0x4208);
  M5.Display.drawRoundRect(keypad_x + (button_w + spacing), bottom_y, button_w, button_h, 8, TFT_WHITE);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setFont(&fonts::DejaVu24);
  M5.Display.drawString("0", keypad_x + (button_w + spacing) + button_w/2, bottom_y + button_h/2);

  // Clear button
  M5.Display.fillRoundRect(keypad_x, bottom_y, button_w, button_h, 8, WARNING_COLOR);
  M5.Display.drawRoundRect(keypad_x, bottom_y, button_w, button_h, 8, TFT_WHITE);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.drawString("CLEAR", keypad_x + button_w/2, bottom_y + button_h/2);

  // Backspace button
  M5.Display.fillRoundRect(keypad_x + 2 * (button_w + spacing), bottom_y, button_w, button_h, 8, CAUTION_COLOR);
  M5.Display.drawRoundRect(keypad_x + 2 * (button_w + spacing), bottom_y, button_w, button_h, 8, TFT_WHITE);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.drawString("DEL", keypad_x + 2 * (button_w + spacing) + button_w/2, bottom_y + button_h/2);

  // OK button
  int ok_y = bottom_y + button_h + spacing + 20;
  M5.Display.fillRoundRect(keypad_x + (button_w + spacing)/2, ok_y, button_w * 2 + spacing, button_h, 10, GOOD_COLOR);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setFont(&fonts::DejaVu24);
  int ok_button_center_x = keypad_x + (button_w + spacing)/2 + (button_w * 2 + spacing)/2;
  M5.Display.drawString("OK - APPLY", ok_button_center_x, ok_y + button_h/2);

  // Reset text alignment
  M5.Display.setTextDatum(textdatum_t::top_left);
}

// Handle numeric keypad touch
bool handleKeypadTouch(int16_t x, int16_t y) {
  // Back button
  if (x >= 1280 - 100 && x <= 1280 - 10 && y >= 10 && y <= 40) {
    can_id_input_mode = false;
    config_needs_redraw = true;
    return true;
  }

  int keypad_x = 300;
  int keypad_y = 200;
  int button_w = 120;
  int button_h = 80;
  int spacing = 20;

  // Check number buttons (1-9)
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      int num = (row * 3) + col + 1;
      int btn_x = keypad_x + col * (button_w + spacing);
      int btn_y = keypad_y + row * (button_h + spacing);

      if (x >= btn_x && x <= btn_x + button_w && y >= btn_y && y <= btn_y + button_h) {
        if (can_id_cursor < 7) {
          can_id_buffer[can_id_cursor] = '0' + num;
          can_id_cursor++;
          can_id_buffer[can_id_cursor] = '\0';
        }
        return true;
      }
    }
  }

  int bottom_y = keypad_y + 3 * (button_h + spacing);

  // 0 button
  int zero_x = keypad_x + (button_w + spacing);
  if (x >= zero_x && x <= zero_x + button_w && y >= bottom_y && y <= bottom_y + button_h) {
    if (can_id_cursor < 7) {
      can_id_buffer[can_id_cursor] = '0';
      can_id_cursor++;
      can_id_buffer[can_id_cursor] = '\0';
    }
    return true;
  }

  // Clear button
  if (x >= keypad_x && x <= keypad_x + button_w && y >= bottom_y && y <= bottom_y + button_h) {
    strcpy(can_id_buffer, "");
    can_id_cursor = 0;
    return true;
  }

  // Backspace button
  int del_x = keypad_x + 2 * (button_w + spacing);
  if (x >= del_x && x <= del_x + button_w && y >= bottom_y && y <= bottom_y + button_h) {
    if (can_id_cursor > 0) {
      can_id_cursor--;
      can_id_buffer[can_id_cursor] = '\0';
    }
    return true;
  }

  // OK button
  int ok_y = bottom_y + button_h + spacing + 20;
  int ok_x = keypad_x + (button_w + spacing)/2;
  int ok_w = button_w * 2 + spacing;
  if (x >= ok_x && x <= ok_x + ok_w && y >= ok_y && y <= ok_y + button_h) {
    // Validate and apply CAN ID
    int new_can_id = atoi(can_id_buffer);
    if (new_can_id >= 0 && new_can_id <= 2047) {
      config.can_id_base = new_can_id;
      // Save immediately when CAN ID is changed
      saveConfig();
      can_id_input_mode = false;
      config_needs_redraw = true;
      Serial.printf("Base CAN ID changed to %d (frames %d-%d)\n",
                   config.can_id_base, config.can_id_base, config.can_id_base + 3);
    }
    return true;
  }

  return false;
}

// Handle configuration page touch
bool handleConfigTouch(int16_t x, int16_t y) {
  // Back button
  if (x >= 1280 - 100 && x <= 1280 - 10 && y >= 10 && y <= 40) {
    config_mode = false;
    initial_draw = true;
    return true;
  }

  int y_pos = 80;
  int line_height = 60;

  // CAN ID EDIT button
  if (y >= y_pos + 5 && y <= y_pos + 45) {
    if (x >= 600 && x <= 720) { // EDIT button
      // Initialize input buffer with current base CAN ID
      sprintf(can_id_buffer, "%d", config.can_id_base);
      can_id_cursor = strlen(can_id_buffer);
      can_id_input_mode = true;
      return true;
    }
  }

  y_pos += line_height;

  // Temperature units
  if (y >= y_pos + 5 && y <= y_pos + 45) {
    if (x >= 400 && x <= 520) { // Celsius
      if (config.use_fahrenheit) {
        config.use_fahrenheit = false;
        config_needs_redraw = true;
      }
      return true;
    } else if (x >= 540 && x <= 680) { // Fahrenheit
      if (!config.use_fahrenheit) {
        config.use_fahrenheit = true;
        config_needs_redraw = true;
      }
      return true;
    }
  }

  y_pos += line_height;

  // Pressure units
  if (y >= y_pos + 5 && y <= y_pos + 45) {
    if (x >= 400 && x <= 480) { // kPa
      if (config.use_psi) {
        config.use_psi = false;
        config_needs_redraw = true;
      }
      return true;
    } else if (x >= 500 && x <= 580) { // PSI
      if (!config.use_psi) {
        config.use_psi = true;
        config_needs_redraw = true;
      }
      return true;
    }
  }

  y_pos += line_height;

  // Speed units
  if (y >= y_pos + 5 && y <= y_pos + 45) {
    if (x >= 400 && x <= 500) { // km/h
      if (config.use_mph) {
        config.use_mph = false;
        config_needs_redraw = true;
      }
      return true;
    } else if (x >= 520 && x <= 620) { // mph
      if (!config.use_mph) {
        config.use_mph = true;
        config_needs_redraw = true;
      }
      return true;
    }
  }

  y_pos += line_height;

  // CAN Speed selection
  if (y >= y_pos + 5 && y <= y_pos + 45) {
    for (int i = 0; i < 4; i++) {
      int btn_x = 350 + i * 90;
      if (x >= btn_x && x <= btn_x + 80) {
        if (config.can_speed != (ConfigCANSpeed)i) {
          config.can_speed = (ConfigCANSpeed)i;
          config_needs_redraw = true;
          // Save immediately when CAN speed is changed
          saveConfig();
          Serial.printf("CAN speed changed to %s\n", getCANSpeedName(config.can_speed));
        }
        return true;
      }
    }
  }

  y_pos += line_height;

  // Data source mode - larger touch areas
  if (y >= y_pos + 5 && y <= y_pos + 55) {
    Serial.printf("Touch in data source area: x=%d, y=%d\n", x, y);

    if (x >= 350 && x <= 530) { // Simulation button (350 + 180 = 530)
      Serial.println("Simulation button touched");
      if (!config.simulation_mode) {
        config.simulation_mode = true;
        config_needs_redraw = true;
        // Save immediately when switching to simulation mode
        saveConfig();
        Serial.println("Switched to simulation mode and saved");
      }
      return true;
    } else if (x >= 550 && x <= 730) { // Real CAN button (550 + 180 = 730)
      Serial.println("Real CAN button touched");
      if (config.simulation_mode) {
        config.simulation_mode = false;
        // Try to initialize CAN when switching to real mode
        // But allow the mode switch regardless of initialization result
        if (!initializeCAN()) {
          Serial.println("CAN initialization failed - but staying in real CAN mode for testing");
        } else {
          Serial.println("CAN successfully initialized");
        }
        // Save immediately when switching to real CAN mode
        saveConfig();
        Serial.println("Switched to real CAN mode and saved");
        config_needs_redraw = true;
      }
      return true;
    }
  }

  y_pos += line_height + 20;

  // Save & Exit button
  if (x >= 500 && x <= 780 && y >= y_pos && y <= y_pos + 60) {
    // Save configuration to persistent storage
    saveConfig();
    config_mode = false;
    initial_draw = true;
    return true;
  }

  return false;
}

// Draw a metric tile - Link G4X Generic Dash 2 CAN Stream - All Real Parameters
void drawMetric(int x, int y, int w, int h, Metric metric, int tile_index, bool force_redraw = false) {
  char value_str[32];
  uint16_t color = VALUE_COLOR;
  bool is_rpm = false;

  switch (metric) {
    case METRIC_RPM: {
      sprintf(value_str, "%d", (int)ecu_data.rpm);
      is_rpm = true; // Special handling for RPM (bold effect)
      break;
    }
    case METRIC_MGP: {
      float mgp_display = convertPressure(ecu_data.mgp);
      float mgp_warn = convertPressure(172);
      float mgp_caution = convertPressure(138);
      if (mgp_display > mgp_warn) color = WARNING_COLOR;
      else if (mgp_display > mgp_caution) color = CAUTION_COLOR;
      else if (mgp_display > 0) color = GOOD_COLOR;
      sprintf(value_str, "%.0f", mgp_display);
      break;
    }
    case METRIC_ECT: {
      float ect_display = convertTemperature(ecu_data.ect);
      float ect_warn = convertTemperature(100);
      float ect_caution = convertTemperature(90);
      float ect_good = convertTemperature(70);
      if (ect_display > ect_warn) color = WARNING_COLOR;
      else if (ect_display > ect_caution) color = CAUTION_COLOR;
      else if (ect_display > ect_good) color = GOOD_COLOR;
      sprintf(value_str, "%.0f", ect_display);
      break;
    }
    case METRIC_IAT: {
      float iat_display = convertTemperature(ecu_data.iat);
      float iat_caution = convertTemperature(50);
      float iat_good = convertTemperature(40);
      if (iat_display > iat_caution) color = CAUTION_COLOR;
      else if (iat_display < iat_good) color = GOOD_COLOR;
      sprintf(value_str, "%.0f", iat_display);
      break;
    }
    case METRIC_BATTERY: {
      if (ecu_data.battery_voltage < 12.0) color = WARNING_COLOR;
      else if (ecu_data.battery_voltage < 12.5) color = CAUTION_COLOR;
      else if (ecu_data.battery_voltage > 14.5) color = CAUTION_COLOR;
      else color = GOOD_COLOR;
      sprintf(value_str, "%.1f", ecu_data.battery_voltage);
      break;
    }
    case METRIC_OIL_TEMP: {
      float oil_temp_display = convertTemperature(ecu_data.oil_temp);
      float oil_temp_warn = convertTemperature(120);
      float oil_temp_caution = convertTemperature(100);
      float oil_temp_good = convertTemperature(80);
      if (oil_temp_display > oil_temp_warn) color = WARNING_COLOR;
      else if (oil_temp_display > oil_temp_caution) color = CAUTION_COLOR;
      else if (oil_temp_display > oil_temp_good) color = GOOD_COLOR;
      sprintf(value_str, "%.0f", oil_temp_display);
      break;
    }
    case METRIC_TPS: {
      sprintf(value_str, "%.0f", ecu_data.tps);
      break;
    }
    case METRIC_IGNITION: {
      sprintf(value_str, "%.1f", ecu_data.ignition_timing);
      break;
    }
    case METRIC_VEHICLE_SPEED: {
      float speed_display = convertSpeed(ecu_data.vehicle_speed);
      sprintf(value_str, "%.0f", speed_display);
      break;
    }
    case METRIC_OIL_PRESS: {
      float oil_press_display = convertPressure(ecu_data.oil_pressure);
      float oil_press_warn = convertPressure(200);
      float oil_press_caution = convertPressure(250);
      if (oil_press_display < oil_press_warn) color = WARNING_COLOR;
      else if (oil_press_display < oil_press_caution) color = CAUTION_COLOR;
      else color = GOOD_COLOR;
      sprintf(value_str, "%d", (int)oil_press_display);
      break;
    }
    case METRIC_FUEL_PRESS: {
      float fuel_press_display = convertPressure(ecu_data.fuel_pressure);
      float fuel_press_warn = convertPressure(300);
      float fuel_press_caution = convertPressure(320);
      if (fuel_press_display < fuel_press_warn) color = WARNING_COLOR;
      else if (fuel_press_display < fuel_press_caution) color = CAUTION_COLOR;
      else color = GOOD_COLOR;
      sprintf(value_str, "%d", (int)fuel_press_display);
      break;
    }
    case METRIC_ECU_TEMP: {
      float ecu_temp_display = convertTemperature(ecu_data.ecu_temp);
      float ecu_temp_warn = convertTemperature(70);
      float ecu_temp_caution = convertTemperature(60);
      if (ecu_temp_display > ecu_temp_warn) color = WARNING_COLOR;
      else if (ecu_temp_display > ecu_temp_caution) color = CAUTION_COLOR;
      else color = GOOD_COLOR;
      sprintf(value_str, "%.0f", ecu_temp_display);
      break;
    }
    case METRIC_LAMBDA: {
      sprintf(value_str, "%.3f", ecu_data.lambda);
      if (ecu_data.lambda < 0.8 || ecu_data.lambda > 1.2) color = WARNING_COLOR;
      else if (ecu_data.lambda < 0.9 || ecu_data.lambda > 1.1) color = CAUTION_COLOR;
      else color = GOOD_COLOR;
      break;
    }

    case METRIC_LAMBDA_COMBINED: {
      // Use special double arc graph for lambda vs target
      drawLambdaDoubleArc(x, y, w, h, ecu_data.lambda, ecu_data.lambda_target, tile_index, force_redraw);
      return; // Early return for special lambda display
    }
    case METRIC_LAMBDA_2: {
      sprintf(value_str, "%.3f", ecu_data.lambda_2);
      if (ecu_data.lambda_2 < 0.8 || ecu_data.lambda_2 > 1.2) color = WARNING_COLOR;
      else if (ecu_data.lambda_2 < 0.9 || ecu_data.lambda_2 > 1.1) color = CAUTION_COLOR;
      else color = GOOD_COLOR;
      break;
    }
    case METRIC_GEAR_POS: {
      if (ecu_data.gear_pos == 0) sprintf(value_str, "N");
      else if (ecu_data.gear_pos == -1) sprintf(value_str, "R");
      else sprintf(value_str, "%.0f", ecu_data.gear_pos);
      color = VALUE_COLOR;
      break;
    }
    case METRIC_FUEL_LEVEL: {
      sprintf(value_str, "%.0f", ecu_data.fuel_level);
      if (ecu_data.fuel_level < 10) color = WARNING_COLOR;
      else if (ecu_data.fuel_level < 25) color = CAUTION_COLOR;
      else color = GOOD_COLOR;
      break;
    }
    case METRIC_EGT_1: {
      float egt_display = config.use_fahrenheit ? (ecu_data.egt_1 * 9.0/5.0 + 32) : ecu_data.egt_1;
      sprintf(value_str, "%.0f", egt_display);
      if (ecu_data.egt_1 > 950) color = WARNING_COLOR;
      else if (ecu_data.egt_1 > 900) color = CAUTION_COLOR;
      else color = GOOD_COLOR;
      break;
    }
    case METRIC_EGT_2: {
      float egt_display = config.use_fahrenheit ? (ecu_data.egt_2 * 9.0/5.0 + 32) : ecu_data.egt_2;
      sprintf(value_str, "%.0f", egt_display);
      if (ecu_data.egt_2 > 950) color = WARNING_COLOR;
      else if (ecu_data.egt_2 > 900) color = CAUTION_COLOR;
      else color = GOOD_COLOR;
      break;
    }
    case METRIC_KNOCK_LEVEL: {
      sprintf(value_str, "%.1f", ecu_data.knock_level);
      if (ecu_data.knock_level > 5) color = WARNING_COLOR;
      else if (ecu_data.knock_level > 3) color = CAUTION_COLOR;
      else color = GOOD_COLOR;
      break;
    }
    case METRIC_BOOST_DUTY: {
      sprintf(value_str, "%.0f", ecu_data.boost_duty);
      color = VALUE_COLOR;
      break;
    }
    case METRIC_G_FORCE: {
      // G-force gauge drawing (debug removed)

      // Use special G-force display
      drawGForceGauge(x, y, w, h, ecu_data.g_force_lateral, ecu_data.g_force_longitudinal, ecu_data.g_force_total, tile_index, force_redraw);
      return; // Early return for special G-force display
    }
    default: {
      sprintf(value_str, "0");
      color = LABEL_COLOR;
      break;
    }
  }

  // Use unified drawing function for all standard metrics
  drawMetricTile(x, y, w, h, getMetricName(metric), value_str, getMetricUnit(metric), color, tile_index, force_redraw, is_rpm);
}

// Display functions with proper partial updates
void updateDisplay() {
  if (can_id_input_mode) {
    drawNumericKeypad();
    return;
  }

  if (config_mode) {
    drawConfigPage();
    return;
  }

  if (gauge_config_mode) {
    drawGaugeConfigModal();
    return;
  }

  // Only do full screen clear on initial draw
  if (initial_draw) {
    M5.Display.fillScreen(TFT_BLACK);

    // Header - only draw once
    M5.Display.fillRect(0, 0, 1280, HEADER_HEIGHT, HEADER_BG);
    M5.Display.setTextColor(HEADER_COLOR);
    M5.Display.setFont(&fonts::DejaVu24);
    M5.Display.drawString("Link G4X Dashboard", 20, 12);

    // Config button in header
    M5.Display.setTextColor(0x07FF);
    M5.Display.setFont(&fonts::DejaVu18);
    M5.Display.drawString("CONFIG", 1280 - 100, 15);

    // Draw all tiles with full redraw
    for (int i = 0; i < 12; i++) {
      int col, row;
      tileIndexToColRow(i, col, row);

      int x, y, w, h;
      tileRect(col, row, x, y, w, h);

      drawMetric(x, y, w, h, g_tile_metric[i], i, true); // Force full redraw
    }

    initial_draw = false;
  } else {
    // Partial updates - only redraw tiles with changed values
    for (int i = 0; i < 12; i++) {
      int col, row;
      tileIndexToColRow(i, col, row);

      int x, y, w, h;
      tileRect(col, row, x, y, w, h);

      drawMetric(x, y, w, h, g_tile_metric[i], i, false); // Partial update only
    }
  }
}

// Simulate data for testing
void simulateData() {
  static unsigned long last_sim = 0;
  if (millis() - last_sim < 500) return; // Update every 500ms to reduce blinking
  last_sim = millis();

  // Simulate realistic ECU data
  ecu_data.rpm += random(-50, 50);
  ecu_data.rpm = constrain(ecu_data.rpm, 800, 7000);

  ecu_data.tps += random(-2, 2);
  ecu_data.tps = constrain(ecu_data.tps, 0, 100);

  ecu_data.mgp += random(-2, 2);
  ecu_data.mgp = constrain(ecu_data.mgp, -10, 200);

  ecu_data.lambda += random(-5, 5) * 0.01;
  ecu_data.lambda = constrain(ecu_data.lambda, 0.7, 1.3);

  ecu_data.lambda_target += random(-2, 2) * 0.01;
  ecu_data.lambda_target = constrain(ecu_data.lambda_target, 0.8, 1.2);

  ecu_data.ect += random(-1, 1) * 0.5;
  ecu_data.ect = constrain(ecu_data.ect, 70, 105);

  ecu_data.oil_temp += random(-1, 1) * 0.3;
  ecu_data.oil_temp = constrain(ecu_data.oil_temp, 80, 130);

  ecu_data.iat += random(-1, 1) * 0.2;
  ecu_data.iat = constrain(ecu_data.iat, 20, 50);

  ecu_data.ignition_timing += random(-1, 1) * 0.5;
  ecu_data.ignition_timing = constrain(ecu_data.ignition_timing, 10, 30);

  ecu_data.vehicle_speed += random(-2, 2);
  ecu_data.vehicle_speed = constrain(ecu_data.vehicle_speed, 0, 200);

  ecu_data.oil_pressure += random(-5, 5);
  ecu_data.oil_pressure = constrain(ecu_data.oil_pressure, 100, 400);

  ecu_data.fuel_pressure += random(-3, 3);
  ecu_data.fuel_pressure = constrain(ecu_data.fuel_pressure, 250, 400);

  ecu_data.ecu_temp += random(-1, 1) * 0.1;
  ecu_data.ecu_temp = constrain(ecu_data.ecu_temp, 30, 80);

  ecu_data.battery_voltage += random(-1, 1) * 0.1;
  ecu_data.battery_voltage = constrain(ecu_data.battery_voltage, 11.5, 14.8);

  // Simulate additional parameters
  ecu_data.lambda_2 += random(-5, 5) * 0.01;
  ecu_data.lambda_2 = constrain(ecu_data.lambda_2, 0.7, 1.3);

  ecu_data.gear_pos += random(-1, 1);
  ecu_data.gear_pos = constrain(ecu_data.gear_pos, 1, 6);

  ecu_data.fuel_level += random(-1, 1);
  ecu_data.fuel_level = constrain(ecu_data.fuel_level, 0, 100);

  ecu_data.egt_1 += random(-10, 10);
  ecu_data.egt_1 = constrain(ecu_data.egt_1, 400, 1000);

  ecu_data.egt_2 += random(-10, 10);
  ecu_data.egt_2 = constrain(ecu_data.egt_2, 400, 1000);

  ecu_data.knock_level += random(-2, 2) * 0.1;
  ecu_data.knock_level = constrain(ecu_data.knock_level, 0, 10);

  ecu_data.boost_duty += random(-5, 5);
  ecu_data.boost_duty = constrain(ecu_data.boost_duty, 0, 100);

  // NOTE: G-force data is NOT simulated - always comes from real IMU
  // This ensures G-force readings are always accurate regardless of simulation mode

  display_needs_update = true;
}

// Check if a metric is available for assignment (all metrics always available)
bool isMetricAvailable(Metric metric) {
  // All metrics are always available - no duplicate restrictions
  return true;
}

// Draw gauge configuration modal
void drawGaugeConfigModal() {
  if (!gauge_config_needs_redraw) return;

  // Semi-transparent overlay
  M5.Display.fillScreen(0x2104); // Dark overlay

  // Modal background - larger to accommodate 22 metrics
  int modal_w = 1000;
  int modal_h = 650;
  int modal_x = (1280 - modal_w) / 2;
  int modal_y = (720 - modal_h) / 2;

  M5.Display.fillRoundRect(modal_x, modal_y, modal_w, modal_h, 10, TFT_WHITE);
  M5.Display.drawRoundRect(modal_x, modal_y, modal_w, modal_h, 10, 0x4208);

  // Title
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setFont(&fonts::DejaVu24);
  M5.Display.setTextDatum(textdatum_t::top_center);
  M5.Display.drawString("Configure Gauge", modal_x + modal_w/2, modal_y + 20);

  // Current selection info
  M5.Display.setFont(&fonts::DejaVu18);
  char current_info[64];
  sprintf(current_info, "Tile %d: %s", selected_tile_index + 1, getMetricName(g_tile_metric[selected_tile_index]));
  M5.Display.drawString(current_info, modal_x + modal_w/2, modal_y + 60);

  // Available metrics grid (4 columns, 6 rows = 24 slots for 22 metrics)
  int grid_start_y = modal_y + 100;
  int button_w = 200;
  int button_h = 55;
  int spacing = 15;
  int cols = 4;

  int metric_index = 0;
  for (int row = 0; row < 6 && metric_index < METRIC_COUNT; row++) {
    for (int col = 0; col < cols && metric_index < METRIC_COUNT; col++) {
      Metric metric = (Metric)metric_index;

      // Skip if this metric is not available
      if (!isMetricAvailable(metric) && metric != g_tile_metric[selected_tile_index]) {
        metric_index++;
        continue;
      }

      int btn_x = modal_x + spacing + col * (button_w + spacing);
      int btn_y = grid_start_y + row * (button_h + spacing);

      // Button color - highlight current selection
      uint16_t btn_color = (metric == g_tile_metric[selected_tile_index]) ? GOOD_COLOR : 0xDEDB;
      uint16_t text_color = (metric == g_tile_metric[selected_tile_index]) ? TFT_WHITE : TFT_BLACK;

      M5.Display.fillRoundRect(btn_x, btn_y, button_w, button_h, 5, btn_color);
      M5.Display.drawRoundRect(btn_x, btn_y, button_w, button_h, 5, 0x4208);

      M5.Display.setTextColor(text_color);
      M5.Display.setFont(&fonts::DejaVu18);
      M5.Display.setTextDatum(textdatum_t::middle_center);
      M5.Display.drawString(getMetricName(metric), btn_x + button_w/2, btn_y + button_h/2);

      metric_index++;
    }
  }

  // Cancel and Save buttons
  int btn_y = modal_y + modal_h - 80;

  // Cancel button
  M5.Display.fillRoundRect(modal_x + 50, btn_y, 150, 50, 5, WARNING_COLOR);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString("CANCEL", modal_x + 125, btn_y + 25);

  // Save button
  M5.Display.fillRoundRect(modal_x + modal_w - 200, btn_y, 150, 50, 5, GOOD_COLOR);
  M5.Display.drawString("SAVE", modal_x + modal_w - 125, btn_y + 25);

  gauge_config_needs_redraw = false;
}

// Handle touch events in gauge configuration modal
bool handleGaugeConfigTouch(int x, int y) {
  int modal_w = 1000;
  int modal_h = 650;
  int modal_x = (1280 - modal_w) / 2;
  int modal_y = (720 - modal_h) / 2;

  // Check if touch is outside modal (cancel)
  if (x < modal_x || x > modal_x + modal_w || y < modal_y || y > modal_y + modal_h) {
    gauge_config_mode = false;
    initial_draw = true;
    return true;
  }

  // Check Cancel button
  int btn_y = modal_y + modal_h - 80;
  if (x >= modal_x + 50 && x <= modal_x + 200 && y >= btn_y && y <= btn_y + 50) {
    gauge_config_mode = false;
    initial_draw = true;
    return true;
  }

  // Check Save button
  if (x >= modal_x + modal_w - 200 && x <= modal_x + modal_w - 50 && y >= btn_y && y <= btn_y + 50) {
    saveGaugeConfiguration();
    gauge_config_mode = false;
    initial_draw = true;
    return true;
  }

  // Check metric selection buttons
  int grid_start_y = modal_y + 100;
  int button_w = 200;
  int button_h = 55;
  int spacing = 15;
  int cols = 4;

  int metric_index = 0;
  for (int row = 0; row < 6 && metric_index < METRIC_COUNT; row++) {
    for (int col = 0; col < cols && metric_index < METRIC_COUNT; col++) {
      Metric metric = (Metric)metric_index;

      // Skip if this metric is not available
      if (!isMetricAvailable(metric) && metric != g_tile_metric[selected_tile_index]) {
        metric_index++;
        continue;
      }

      int btn_x = modal_x + spacing + col * (button_w + spacing);
      int btn_y = grid_start_y + row * (button_h + spacing);

      if (x >= btn_x && x <= btn_x + button_w && y >= btn_y && y <= btn_y + button_h) {
        // Update the selected tile's metric
        g_tile_metric[selected_tile_index] = metric;
        gauge_config_needs_redraw = true;
        return true;
      }

      metric_index++;
    }
  }

  return false;
}

// Save gauge configuration to persistent storage
void saveGaugeConfiguration() {
  preferences.begin("dashboard", false);
  for (int i = 0; i < 12; i++) {
    char key[16];
    sprintf(key, "tile_%d", i);
    preferences.putUChar(key, (uint8_t)g_tile_metric[i]);
  }
  preferences.end();
  Serial.println("Gauge configuration saved");
}

// Load gauge configuration from persistent storage
void loadGaugeConfiguration() {
  preferences.begin("dashboard", true);
  bool config_exists = false;

  for (int i = 0; i < 12; i++) {
    char key[16];
    sprintf(key, "tile_%d", i);
    if (preferences.isKey(key)) {
      g_tile_metric[i] = (Metric)preferences.getUChar(key, (uint8_t)g_tile_metric[i]);
      config_exists = true;
    }
  }

  preferences.end();

  if (config_exists) {
    Serial.println("Gauge configuration loaded from flash");
  } else {
    Serial.println("No saved gauge configuration found, using defaults");
  }
}

// Initialize CAN bus using ESP32-P4 internal TWAI controller
bool initializeCAN() {
  if (config.simulation_mode) {
    Serial.println("CAN: Running in simulation mode");
    return true;
  }

  Serial.println("=== CAN INITIALIZATION DEBUG ===");
  Serial.println("CAN: Initializing ESP32-P4 internal TWAI controller...");
  Serial.printf("CAN: Using TX pin G%d, RX pin G%d, DIR pin G%d\n", CAN_TX_PIN, CAN_RX_PIN, CAN_DIR_PIN);
  Serial.printf("CAN: Haltech IC7 Protocol - Base ID = %d (0x%03X)\n", config.can_id_base, config.can_id_base);
  Serial.printf("CAN: Expected Haltech IC7 frames: 864, 865, 866, 992, 872\n");

  // Set CAN pins
  ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);

  // Set queue sizes for better performance
  ESP32Can.setRxQueueSize(10);
  ESP32Can.setTxQueueSize(5);

  // Convert config enum to speed value
  int speed_kbps;
  switch (config.can_speed) {
    case CONFIG_CAN_125KBPS: speed_kbps = 125; break;
    case CONFIG_CAN_250KBPS: speed_kbps = 250; break;
    case CONFIG_CAN_500KBPS: speed_kbps = 500; break;
    case CONFIG_CAN_1000KBPS:
    default: speed_kbps = 1000; break;
  }

  Serial.printf("CAN: Setting bitrate to %s...\n", getCANSpeedName(config.can_speed));

  // Configure RS485 direction pin for Tab5 built-in SIT3088 transceiver
  pinMode(CAN_DIR_PIN, OUTPUT);
  digitalWrite(CAN_DIR_PIN, LOW);  // Set to receive mode initially
  Serial.printf("CAN: Configured RS485 direction pin %d\n", CAN_DIR_PIN);

  // Initialize TWAI controller with specified speed using Tab5 RS485 interface
  Serial.printf("CAN: Attempting to start TWAI at %d kbps...\n", speed_kbps);
  if (ESP32Can.begin(ESP32Can.convertSpeed(speed_kbps), CAN_TX_PIN, CAN_RX_PIN, 10, 10)) {
    Serial.println("CAN: Successfully initialized ESP32-P4 TWAI with Tab5 RS485 interface!");
    Serial.println("CAN: Ready to receive frames - monitoring for ECU data...");
    Serial.printf("CAN: Listening for Haltech IC7 frames: 864, 865, 866, 992, 872\n");
    Serial.println("=== END CAN INITIALIZATION ===");
    can_initialized = true;
    return true;
  } else {
    Serial.println("CAN: Failed to initialize ESP32-P4 TWAI with Tab5 RS485 interface!");
    Serial.println("CAN: Check wiring, CAN speed, and ECU configuration");
    Serial.println("=== CAN INITIALIZATION FAILED ===");
    return false;
  }
}

// Read real CAN bus data using ESP32-P4 TWAI controller
bool readCANData() {
  static unsigned long last_debug_print = 0;
  static unsigned long last_activity_check = 0;
  static unsigned long total_frames_received = 0;

  if (config.simulation_mode || !can_initialized) {
    return false;
  }

  CanFrame canMsg;
  bool data_received = false;
  int frames_this_cycle = 0;

  // Read all available CAN messages with 1ms timeout (faster polling)
  while (ESP32Can.readFrame(canMsg, 1)) {
    data_received = true;
    frames_this_cycle++;
    total_frames_received++;
    can_message_count++;
    last_can_message = millis();

    // Convert CanFrame to twai_message_t for buffering
    twai_message_t twai_msg;
    twai_msg.identifier = canMsg.identifier;
    twai_msg.data_length_code = canMsg.data_length_code;
    memcpy(twai_msg.data, canMsg.data, 8);

    // Add to buffer for smooth processing
    if (!addToCANBuffer(twai_msg)) {
      // Buffer full - process immediately to prevent data loss
      processCANMessage(twai_msg);
    }

    // Minimal debug: Only log frame count occasionally
    static unsigned long last_frame_log = 0;
    static int frame_count = 0;
    frame_count++;
    if (millis() - last_frame_log > 5000) {
      Serial.printf("CAN: %d frames received in last 5s\n", frame_count);
      frame_count = 0;
      last_frame_log = millis();
    }

    // Parse Haltech IC7 CAN Protocol (Multiple frames: 0x360, 0x361, 0x362, 0x3E0, etc.)
    if (canMsg.data_length_code == 8) {

      // Minimal parsing debug
      static unsigned long last_parse_debug = 0;
      if (millis() - last_parse_debug > 30000) {
        Serial.printf("CAN: Parsing Haltech IC7 frames starting at %d (0x%03X)\n", config.can_id_base, config.can_id_base);
        last_parse_debug = millis();
      }

      // Frame 0x360 (864): Engine Speed, MAP, TPS (Main) - 50Hz
      if (canMsg.identifier == 864) {
        // Haltech CAN V2 Protocol - Frame 0x360 (BIG ENDIAN - MSB first)
        // Bytes 0-1: RPM (direct value, revolutions per minute)
        ecu_data.rpm = (canMsg.data[0] << 8) | canMsg.data[1];

        // Bytes 2-3: MAP (1/10th kPa)
        uint16_t map_raw = (canMsg.data[2] << 8) | canMsg.data[3];
        ecu_data.mgp = map_raw * 0.1; // Convert to kPa

        // Bytes 4-5: TPS (1/10th of 1%)
        uint16_t tps_raw = (canMsg.data[4] << 8) | canMsg.data[5];
        ecu_data.tps = tps_raw * 0.1; // Convert to percentage

        // Bytes 6-7: Reserved or additional data

        // Frame 1000 (ID X): Official Link Generic Dash 2 format - BIG ENDIAN (Motorola)
        // Data 0-1: Engine Speed (RPM) = Raw, Range 0-15000 RPM
        ecu_data.rpm = (canMsg.data[0] << 8) | canMsg.data[1];

        // Data 2-3: MGP (kPa) = Raw - 100, Range -100 to 550 kPa
        ecu_data.mgp = ((canMsg.data[2] << 8) | canMsg.data[3]) - 100;

        // Data 4: ECT (°C) = Raw - 50, Range -50 to 205°C
        ecu_data.ect = canMsg.data[4] - 50;

        // Data 5: IAT (°C) = Raw - 50, Range -20 to 205°C
        ecu_data.iat = canMsg.data[5] - 50;

        // Data 6: ECU Volts (V) = Raw * 0.1, Range 0-30.0V
        ecu_data.battery_voltage = canMsg.data[6] * 0.1;

        // Data 7: Oil Temp (°C) = Raw - 50, Range -20 to 205°C
        ecu_data.oil_temp = canMsg.data[7] - 50;

        ecu_data.last_update = millis();

        // Automatically disable simulation mode when real CAN data is received
        static bool simulation_disabled = false;
        if (config.simulation_mode && !simulation_disabled) {
          config.simulation_mode = false;
          simulation_disabled = true;
          Serial.println("HALTECH IC7 CAN DATA - Simulation disabled");
          saveConfig(); // Save the change
        }

        // Trigger display update for responsive UI
        display_needs_update = true;

        // Minimal frame 0x360 debug (reduced frequency for performance)
        static unsigned long last_360_debug = 0;
        if (millis() - last_360_debug > 30000) { // Only every 30 seconds
          Serial.printf("Frame 0x360: RPM=%d MAP=%.1f TPS=%.1f\n", (int)ecu_data.rpm, ecu_data.mgp, ecu_data.tps);
          last_360_debug = millis();
        }
      }

      // Frame 0x361 (865): Fuel Pressure, Oil Pressure - 50Hz
      else if (canMsg.identifier == 865) {
        // Haltech CAN V2 Protocol - Frame 0x361 (BIG ENDIAN - MSB first)
        // Bytes 0-1: Fuel Pressure (1/10th kPa)
        uint16_t fuel_press_raw = (canMsg.data[0] << 8) | canMsg.data[1];
        ecu_data.fuel_pressure = fuel_press_raw * 0.1; // Convert to kPa

        // Bytes 2-3: Oil Pressure (1/10th kPa)
        uint16_t oil_press_raw = (canMsg.data[2] << 8) | canMsg.data[3];
        ecu_data.oil_pressure = oil_press_raw * 0.1; // Convert to kPa

        // Bytes 4-7: Reserved or additional data (not specified in FTY Racing spec)
        display_needs_update = true; // Trigger display update
      }

      // Frame 0x362 (866): Injector Duty Cycle, Ignition Angle - 50Hz
      else if (canMsg.identifier == 866) {
        // Haltech CAN V2 Protocol - Frame 0x362 (BIG ENDIAN - MSB first)
        // Bytes 0-1: Primary Injector Duty (1/10th of 1%)
        uint16_t inj_duty_raw = (canMsg.data[0] << 8) | canMsg.data[1];
        // ecu_data.injector_duty = inj_duty_raw * 0.1; // Could add this to ECUData structure

        // Bytes 2-3: Secondary Injector Duty (1/10th of 1%)
        uint16_t inj_duty2_raw = (canMsg.data[2] << 8) | canMsg.data[3];
        // ecu_data.injector_duty2 = inj_duty2_raw * 0.1; // Could add this to ECUData structure

        // Bytes 4-5: Ignition Angle Leading (1/10th of a degree)
        int16_t ignition_raw = (canMsg.data[4] << 8) | canMsg.data[5];
        ecu_data.ignition_timing = ignition_raw * 0.1; // Convert to degrees

        // Bytes 6-7: Ignition Angle Trailing (1/10th of a degree)
        // int16_t ignition_trail_raw = (canMsg.data[6] << 8) | canMsg.data[7];
        // ecu_data.ignition_timing_trailing = ignition_trail_raw * 0.1; // Could add this
        display_needs_update = true; // Trigger display update
      }

      // Frame 0x3E0 (992): ECT, IAT, Fuel Temperature, Oil Temperature - 5Hz
      else if (canMsg.identifier == 992) {
        // Debug: Check if we're actually receiving this frame
        static unsigned long last_3e0_debug = 0;
        if (millis() - last_3e0_debug > 10000) {
          Serial.printf("Frame 0x3E0 RAW: [%02X %02X %02X %02X %02X %02X %02X %02X]\n",
                        canMsg.data[0], canMsg.data[1], canMsg.data[2], canMsg.data[3],
                        canMsg.data[4], canMsg.data[5], canMsg.data[6], canMsg.data[7]);
          last_3e0_debug = millis();
        }

        // Haltech CAN V2 Protocol - Frame 0x3E0 (BIG ENDIAN - MSB first)
        // Bytes 0-1: Coolant Temp (0.1 Kelvin)
        uint16_t ect_raw = (canMsg.data[0] << 8) | canMsg.data[1];
        float new_ect = (ect_raw * 0.1) - 273.15; // Convert from Kelvin to Celsius

        // Bytes 2-3: Air Temp (0.1 Kelvin) - Apply filtering for stability
        uint16_t iat_raw = (canMsg.data[2] << 8) | canMsg.data[3];
        float new_iat = (iat_raw * 0.1) - 273.15; // Convert from Kelvin to Celsius

        // Debug temperature values
        if (millis() - last_3e0_debug > 10000) {
          Serial.printf("Temps: ECT_raw=%d (%.1f°C) IAT_raw=%d (%.1f°C)\n",
                        ect_raw, new_ect, iat_raw, new_iat);
        }

        // Simple low-pass filter for IAT stability (only if value is reasonable)
        if (new_iat > -40 && new_iat < 150) { // Sanity check for reasonable temperature range
          ecu_data.iat = ecu_data.iat * 0.8 + new_iat * 0.2; // 80% old, 20% new
        }

        // ECT with less filtering (more responsive)
        if (new_ect > -40 && new_ect < 150) {
          ecu_data.ect = ecu_data.ect * 0.5 + new_ect * 0.5; // 50% old, 50% new
        }

        // Bytes 4-5: Fuel Temp (0.1 Kelvin)
        uint16_t fuel_temp_raw = (canMsg.data[4] << 8) | canMsg.data[5];
        // ecu_data.fuel_temp = (fuel_temp_raw * 0.1) - 273.15; // Could add this to ECUData structure

        // Bytes 6-7: Oil Temp (0.1 Kelvin)
        uint16_t oil_temp_raw = (canMsg.data[6] << 8) | canMsg.data[7];
        float new_oil_temp = (oil_temp_raw * 0.1) - 273.15; // Convert from Kelvin to Celsius
        if (new_oil_temp > -40 && new_oil_temp < 200) {
          ecu_data.oil_temp = ecu_data.oil_temp * 0.7 + new_oil_temp * 0.3; // 70% old, 30% new
        }
        display_needs_update = true; // Trigger display update
      }

      // Frame 0x368 (872): Lambda 1-2 - 20Hz
      else if (canMsg.identifier == 872) {
        // Haltech CAN V2 Protocol - Frame 0x368 (BIG ENDIAN - MSB first)
        // Bytes 0-1: Lambda 1 (0.001 Lambda)
        uint16_t lambda1_raw = (canMsg.data[0] << 8) | canMsg.data[1];
        ecu_data.lambda = lambda1_raw * 0.001; // Convert to lambda ratio

        // Bytes 2-3: Lambda 2 (0.001 Lambda)
        uint16_t lambda2_raw = (canMsg.data[2] << 8) | canMsg.data[3];
        // ecu_data.lambda2 = lambda2_raw * 0.001; // Could add this to ECUData structure

        // Bytes 4-7: Reserved or additional data
        display_needs_update = true; // Trigger display update
      }
    }
  }

  // DEBUG: Print CAN statistics every 30 seconds (reduced for performance)
  if (millis() - last_debug_print > 30000) {
    Serial.printf("=== CAN DEBUG STATS ===\n");
    Serial.printf("CAN Initialized: %s\n", can_initialized ? "YES" : "NO");
    Serial.printf("Simulation Mode: %s\n", config.simulation_mode ? "YES" : "NO");
    Serial.printf("Total Frames Received: %lu\n", total_frames_received);
    Serial.printf("Frames This Cycle: %d\n", frames_this_cycle);
    Serial.printf("Last Message: %lu ms ago\n", millis() - last_can_message);
    Serial.printf("CAN Base ID: %d (0x%03X)\n", config.can_id_base, config.can_id_base);
    Serial.printf("CAN Speed: %s\n", getCANSpeedName(config.can_speed));
    Serial.printf("Expected Haltech IC7 Frame IDs: 864, 865, 866, 992, 872\n");
    Serial.println("=== END CAN DEBUG ===");
    last_debug_print = millis();
  }

  // DEBUG: Check for CAN activity timeout
  if (millis() - last_activity_check > 10000) {
    if (total_frames_received == 0) {
      Serial.println("WARNING: No CAN frames received in 10 seconds!");
      Serial.println("Check: ECU CAN output enabled, wiring, CAN speed, termination");
    }
    last_activity_check = millis();
  }

  return data_received;
}

// Get CAN speed name for display
const char* getCANSpeedName(ConfigCANSpeed speed) {
  switch (speed) {
    case CONFIG_CAN_125KBPS: return "125 kbps";
    case CONFIG_CAN_250KBPS: return "250 kbps";
    case CONFIG_CAN_500KBPS: return "500 kbps";
    case CONFIG_CAN_1000KBPS: return "1 Mbps";
    default: return "1 Mbps";
  }
}



// Save configuration to persistent storage
void saveConfig() {
  preferences.begin("dashboard", false);
  preferences.putUInt("can_id_base", config.can_id_base);
  preferences.putBool("fahrenheit", config.use_fahrenheit);
  preferences.putBool("psi", config.use_psi);
  preferences.putBool("mph", config.use_mph);
  preferences.putBool("simulation", config.simulation_mode);
  preferences.putUInt("can_speed", (uint32_t)config.can_speed);
  preferences.end();
  Serial.printf("Configuration saved: Base CAN ID=%d, Simulation=%s\n",
                config.can_id_base, config.simulation_mode ? "ON" : "OFF");
}

// Load configuration from persistent storage
void loadConfig() {
  preferences.begin("dashboard", true); // Read-only mode

  // Load with defaults - check for legacy can_id first, then use can_id_base
  if (preferences.isKey("can_id_base")) {
    config.can_id_base = preferences.getUInt("can_id_base", 1000);
  } else {
    // Legacy migration: convert old can_id to can_id_base
    uint32_t legacy_can_id = preferences.getUInt("can_id", 864);
    config.can_id_base = (legacy_can_id == 864) ? 864 : 1000; // Keep 864 if set, otherwise use 1000
  }

  config.use_fahrenheit = preferences.getBool("fahrenheit", false);
  config.use_psi = preferences.getBool("psi", false);
  config.use_mph = preferences.getBool("mph", false);
  config.simulation_mode = preferences.getBool("simulation", false);
  config.can_speed = (ConfigCANSpeed)preferences.getUInt("can_speed", CONFIG_CAN_1000KBPS);

  preferences.end();

  Serial.println("Configuration loaded from flash:");
  Serial.printf("  Base CAN ID: %d (frames %d-%d)\n", config.can_id_base, config.can_id_base, config.can_id_base + 3);
  Serial.printf("  CAN Speed: %s\n", getCANSpeedName(config.can_speed));
  Serial.printf("  Temperature: %s\n", config.use_fahrenheit ? "Fahrenheit" : "Celsius");
  Serial.printf("  Pressure: %s\n", config.use_psi ? "PSI" : "kPa");
  Serial.printf("  Speed: %s\n", config.use_mph ? "mph" : "km/h");
  Serial.printf("  Data Source: %s\n", config.simulation_mode ? "Simulation" : "Real CAN");
}

void setup() {
  M5.begin();
  M5.Display.setRotation(1);
  M5.Display.setBrightness(200);

  Serial.begin(115200);
  Serial.println("Link G4X Dashboard Starting...");

  // Initialize IMU for G-force measurements
  if (M5.Imu.isEnabled()) {
    auto imu_type = M5.Imu.getType();
    const char* imu_name = "Unknown";
    switch (imu_type) {
      case m5::imu_none: imu_name = "None"; break;
      case m5::imu_sh200q: imu_name = "SH200Q"; break;
      case m5::imu_mpu6050: imu_name = "MPU6050"; break;
      case m5::imu_mpu6886: imu_name = "MPU6886"; break;
      case m5::imu_mpu9250: imu_name = "MPU9250"; break;
      case m5::imu_bmi270: imu_name = "BMI270"; break;
      default: imu_name = "Unknown"; break;
    }
    Serial.printf("IMU initialized: %s\n", imu_name);
  } else {
    Serial.println("IMU not found - G-force measurements disabled");
  }

  // Load saved configuration
  loadConfig();

  // FORCE CORRECT CAN CONFIGURATION FOR HALTECH IC7 PROTOCOL
  bool config_changed = false;
  if (config.can_id_base != 864) {
    Serial.printf("🔧 SWITCHING TO HALTECH IC7: %d -> 864 (0x360)\n", config.can_id_base);
    config.can_id_base = 864; // 0x360 - Primary Haltech IC7 frame
    config_changed = true;
  }
  if (config.can_speed != CONFIG_CAN_500KBPS) {
    Serial.printf("🔧 FIXING CAN SPEED: %s -> 500kbps (Haltech IC7)\n", getCANSpeedName(config.can_speed));
    config.can_speed = CONFIG_CAN_500KBPS;
    config_changed = true;
  }
  if (config_changed) {
    saveConfig(); // Save the corrected configuration
  }

  // Initialize CAN bus only if in real CAN mode
  // Don't override the saved configuration if CAN fails
  if (!config.simulation_mode) {
    if (!initializeCAN()) {
      Serial.println("CAN initialization failed - but keeping Real CAN mode as configured");
      Serial.println("Dashboard will attempt to read CAN data but may show no data");
    }
  } else {
    Serial.println("Starting in simulation mode as configured");
  }

  setDefaultTileLayout();

  // Set default gauge layout and ensure proper display
  setDefaultTileLayout();

  // Force G-force gauge in position 3 (top-right)
  g_tile_metric[3] = METRIC_G_FORCE;

  // Load any saved gauge configuration (but keep G-force gauge)
  loadGaugeConfiguration();
  g_tile_metric[3] = METRIC_G_FORCE; // Ensure G-force gauge stays

  Serial.println("Dashboard gauges configured - G-Force in position 3");

  // Display complete configuration for debugging
  Serial.println("=== DASHBOARD CONFIGURATION ===");
  Serial.printf("Mode: %s\n", config.simulation_mode ? "SIMULATION" : "REAL CAN BUS");
  Serial.printf("CAN Base ID: %d (0x%03X)\n", config.can_id_base, config.can_id_base);
  Serial.printf("CAN Speed: %s\n", getCANSpeedName(config.can_speed));
  Serial.printf("Units: %s\n", config.use_fahrenheit ? "Imperial (°F/PSI)" : "Metric (°C/kPa)");
  Serial.printf("CAN Initialized: %s\n", can_initialized ? "YES" : "NO");
  Serial.printf("Expected Haltech IC7 Frames: 0x360(864), 0x361(865), 0x362(866), 0x3E0(992), 0x368(872)\n");
  Serial.println("=== DASHBOARD READY ===");

  // Force initial display refresh to show gauges
  M5.Display.fillScreen(BLACK);
  Serial.println("Display cleared - gauges will appear on first update");
}

// Update G-force data from IMU - ALWAYS reads from real hardware regardless of simulation mode
void updateGForceData() {
  static unsigned long last_debug = 0;

  if (!M5.Imu.isEnabled()) {
    if (millis() - last_debug > 5000) {
      Serial.println("IMU not enabled for G-force");
      last_debug = millis();
    }
    return;
  }

  // Update IMU data
  auto imu_update = M5.Imu.update();
  if (imu_update) {
    auto data = M5.Imu.getImuData();

    // Convert accelerometer data to G-forces
    // Device coordinate system (when mounted horizontally in car):
    // X-axis = lateral (left/right) - positive = right turn G-force
    // Y-axis = longitudinal (forward/back) - positive = acceleration G-force
    // Z-axis = vertical (up/down) - subtract 1G for gravity

    // Convert from m/s² to G (1G = 9.81 m/s²)
    float lateral_g = data.accel.x / 9.81f;      // Lateral G-force
    float longitudinal_g = data.accel.y / 9.81f; // Longitudinal G-force

    // Apply lighter filtering for more sensitivity (less smoothing = more responsive)
    static float alpha = 0.3f; // Filter coefficient (0.3 = lighter filtering, more sensitive)
    ecu_data.g_force_lateral = (alpha * lateral_g) + ((1.0f - alpha) * ecu_data.g_force_lateral);
    ecu_data.g_force_longitudinal = (alpha * longitudinal_g) + ((1.0f - alpha) * ecu_data.g_force_longitudinal);

    // Calculate total G-force magnitude (excluding vertical/gravity)
    ecu_data.g_force_total = sqrt(ecu_data.g_force_lateral * ecu_data.g_force_lateral +
                                  ecu_data.g_force_longitudinal * ecu_data.g_force_longitudinal);

    // Minimal G-force debug (removed excessive output)
  } else {
    if (millis() - last_debug > 5000) {
      Serial.println("IMU update failed");
      last_debug = millis();
    }
  }
}

void loop() {
  M5.update();

  // Update G-force data from IMU
  updateGForceData();

  // Read data from CAN bus or simulation
  if (config.simulation_mode) {
    simulateData();
  } else {
    // Try to read real CAN data
    if (!readCANData()) {
      // If no CAN data received for 5 seconds, show connection status
      if (millis() - last_can_message > 5000) {
        // Could add connection status indicator here
      }
    } else {
      display_needs_update = true;
    }
  }

  // Update display at high frequency for smooth updates
  static unsigned long last_display_update = 0;
  if (display_needs_update || (millis() - last_display_update > 100)) { // Update every 100ms (10Hz)
    updateDisplay();
    display_needs_update = false;
    last_display_update = millis();
  }

  // Handle touch input
  if (M5.Display.getTouch(&touch_x, &touch_y)) {
    if (can_id_input_mode) {
      // Handle numeric keypad touch
      if (handleKeypadTouch(touch_x, touch_y)) {
        updateDisplay();
      }
    } else if (config_mode) {
      // Handle configuration page touch
      if (handleConfigTouch(touch_x, touch_y)) {
        updateDisplay();
      }
    } else if (gauge_config_mode) {
      // Handle gauge configuration modal touch
      if (handleGaugeConfigTouch(touch_x, touch_y)) {
        updateDisplay();
      }
    } else {
      // Header tap to enter config
      if (touch_y <= 45) {
        if (touch_x >= 1280 - 120) {
          // Config button
          config_mode = true;
          config_needs_redraw = true;
          updateDisplay();
        }
      } else {
        // Check for double-click on gauge tiles
        if (touch_y > 45) { // Not in header
          // Calculate which tile was touched
          int tile_col = (touch_x - 20) / 310;
          int tile_row = (touch_y - 60) / 165;

          if (tile_col >= 0 && tile_col < 4 && tile_row >= 0 && tile_row < 3) {
            int clicked_tile = tile_row * 4 + tile_col;
            unsigned long current_time = millis();

            // Check if this is a double-click
            if (clicked_tile == last_clicked_tile &&
                (current_time - last_click_time) <= DOUBLE_CLICK_TIMEOUT) {
              // Double-click detected!
              selected_tile_index = clicked_tile;
              gauge_config_mode = true;
              gauge_config_needs_redraw = true;
              updateDisplay();

              // Reset double-click detection
              last_clicked_tile = -1;
              last_click_time = 0;
            } else {
              // First click - start double-click detection
              last_clicked_tile = clicked_tile;
              last_click_time = current_time;
            }
          }
        }
      }
    }

    delay(200); // Debounce
  }

  delay(100); // Longer delay to prevent blinking and reduce CPU usage
}
