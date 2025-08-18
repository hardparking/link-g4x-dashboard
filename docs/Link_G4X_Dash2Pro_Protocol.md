# Link G4X Generic Dash 2 CAN Protocol Documentation
## Complete Technical Specification for Multi-Frame CAN Stream Decoding

### 📋 Overview
The Link G4X ECU provides a comprehensive **Generic Dash 2 CAN stream** designed for advanced aftermarket dashboard integration. This multi-frame protocol uses **4 CAN frames** (IDs 1000-1003) to transmit **13+ engine parameters** including oil pressure, fuel pressure, lambda, and ignition timing - providing complete engine monitoring capability.

### 🔧 CAN Frame Specification

#### **Multi-Frame Protocol Properties**
```
Property          | Value
------------------|------------------
Protocol          | CAN 2.0B (ISO 11898)
Frame Type        | Standard (11-bit identifier)
Data Length       | 8 bytes per frame (64 bits)
Frame Count       | 4 frames (1000, 1001, 1002, 1003)
Base CAN ID       | 1000 (0x3E8 hex) - Official default
Transmission Rate | 20 Hz per frame (50ms interval)
Total Data Rate   | 80 Hz combined (4 frames × 20 Hz)
Bit Rate          | 1 Mbps (1000 kbps)
Byte Order        | Little Endian
```

#### **Multi-Frame Structure (Generic Dash 2 Protocol)**

##### **Frame 1000 (Primary) - Core Engine Parameters**
```
Byte Position | Parameter              | Data Type | Resolution | Range
--------------|------------------------|-----------|------------|------------------
0-1           | Engine RPM             | uint16    | 1 RPM      | 0 - 65535 RPM
2             | Throttle Position (TPS)| uint8     | 0.5%       | 0 - 100%
3             | Engine Coolant (ECT)   | uint8     | 1°C        | -40 to +215°C
4             | Intake Air Temp (IAT)  | uint8     | 1°C        | -40 to +215°C
5-6           | Manifold Pressure (MGP)| uint16    | 0.1 kPa    | 0 - 6553.5 kPa
7             | Battery Voltage        | uint8     | 0.1V       | 0 - 25.5V
```

##### **Frame 1001 (Extended) - Temperature, Timing & Speed**
```
Byte Position | Parameter              | Data Type | Resolution | Range
--------------|------------------------|-----------|------------|------------------
0-1           | Oil Temperature        | int16     | 1°C        | -40 to +215°C
2-3           | Ignition Timing        | int16     | 0.1°       | -50 to +50°
4-5           | Vehicle Speed          | uint16    | 0.1 km/h   | 0 - 6553.5 km/h
6-7           | Reserved               | -         | -          | -
```

##### **Frame 1002 (Pressures & Lambda) - Critical Monitoring**
```
Byte Position | Parameter              | Data Type | Resolution | Range
--------------|------------------------|-----------|------------|------------------
0-1           | Oil Pressure           | uint16    | 1 kPa      | 0 - 65535 kPa
2-3           | Fuel Pressure          | uint16    | 1 kPa      | 0 - 65535 kPa
4-5           | Lambda 1               | uint16    | 0.001 λ    | 0 - 65.535 λ
6-7           | Reserved               | -         | -          | -
```

##### **Frame 1003 (Additional) - System Monitoring**
```
Byte Position | Parameter              | Data Type | Resolution | Range
--------------|------------------------|-----------|------------|------------------
0-1           | ECU Temperature        | int16     | 1°C        | -40 to +215°C
2-7           | Reserved               | -         | -          | Future expansion
```

### � Complete Available Parameters (Official Race Technology Specification)

According to the official Race Technology documentation, the Link G4X Dash2Pro CAN stream contains **13 parameters** transmitted across multiple CAN frames. However, the **primary 8-byte frame** (CAN ID 1000/0x3E8) contains only **6 core parameters**:

#### **Primary Frame Parameters (8-byte CAN frame)**
```
Parameter Name          | Race Technology Channel | Resolution | Units
------------------------|-------------------------|------------|--------
Engine Speed            | RPM (Engine RPM)        | 1 RPM      | RPM
MGP                     | Boost Pressure (pressure 5) | 1 kPa | kPa
ECT                     | Coolant temp (temperature 8) | 1°C   | °C
IAT                     | Inlet Post Intercooler 1 (temperature 6) | 1°C | °C
ECU Volts               | Battery Voltage (misc 3) | 0.1V      | V
TPS                     | Throttle Position (aux 1) | Raw Value | Raw
```

#### **Additional Parameters (Separate CAN frames or not in primary frame)**
```
Parameter Name          | Race Technology Channel | Resolution | Units
------------------------|-------------------------|------------|--------
Oil Temperature         | Oil Temperature (temperature 9) | 1°C | °C
Ignition Angle          | Ignition Angle (angle 2) | 1°       | degrees
Non Driven Wheel Speed  | Speed From ECU (Misc 4) | 0.1 km/h  | km/h
Oil Pressure            | Oil Pressure (Pressure 2) | 1 kPa   | kPa
Fuel Pressure           | Fuel Pressure (pressure 3) | 1 kPa  | kPa
ECU Temperature         | ECU temperature (temperature 16) | 1°C | °C
Lambda 1                | Lambda 1 (misc 1)       | 1 Lambda  | λ
```

**Important Note**: The additional parameters listed above are **NOT included in the standard 8-byte Dash2Pro frame**. They may be available through:
- Additional CAN frames with different IDs
- User-defined CAN streams
- Extended Dash2Pro configurations

### �📊 Detailed Parameter Specifications (8-byte Frame Only)

#### **Byte 0-1: Engine RPM (Little Endian)**
```
Data Type:    uint16 (2 bytes)
Byte Order:   Little Endian (LSB first)
Resolution:   1 RPM
Range:        0 - 65535 RPM
Units:        Revolutions Per Minute

Calculation:
RPM = (Byte1 << 8) | Byte0
RPM = Byte0 + (Byte1 * 256)

Example:
Bytes: [0x40, 0x1F] = [64, 31]
RPM = 64 + (31 * 256) = 64 + 7936 = 8000 RPM
```

#### **Byte 2: Throttle Position Sensor (TPS)**
```
Data Type:    uint8 (1 byte)
Resolution:   0.5%
Range:        0 - 100%
Units:        Percentage

Calculation:
TPS_Percent = Byte2 * 0.5

Example:
Byte: 0xC8 = 200
TPS = 200 * 0.5 = 100%

Byte: 0x64 = 100  
TPS = 100 * 0.5 = 50%
```

#### **Byte 3: Engine Coolant Temperature (ECT)**
```
Data Type:    uint8 (1 byte)
Resolution:   1°C
Range:        -40°C to +215°C
Units:        Degrees Celsius
Offset:       -40°C

Calculation:
ECT_Celsius = Byte3 - 40

Example:
Byte: 0x64 = 100
ECT = 100 - 40 = 60°C

Byte: 0x28 = 40
ECT = 40 - 40 = 0°C
```

#### **Byte 4: Intake Air Temperature (IAT)**
```
Data Type:    uint8 (1 byte)
Resolution:   1°C
Range:        -40°C to +215°C
Units:        Degrees Celsius
Offset:       -40°C

Calculation:
IAT_Celsius = Byte4 - 40

Example:
Byte: 0x46 = 70
IAT = 70 - 40 = 30°C

Byte: 0x00 = 0
IAT = 0 - 40 = -40°C
```

#### **Byte 5-6: Manifold Absolute Pressure (MAP) (Little Endian)**
```
Data Type:    uint16 (2 bytes)
Byte Order:   Little Endian (LSB first)
Resolution:   0.1 kPa
Range:        0 - 6553.5 kPa
Units:        Kilopascals

Calculation:
MAP_Raw = (Byte6 << 8) | Byte5
MAP_kPa = MAP_Raw * 0.1

Example:
Bytes: [0x84, 0x03] = [132, 3]
MAP_Raw = 132 + (3 * 256) = 132 + 768 = 900
MAP = 900 * 0.1 = 90.0 kPa

Atmospheric Pressure Reference:
- Sea Level: ~101.3 kPa (1013 mbar)
- Vacuum: 0-101.3 kPa
- Boost: >101.3 kPa
```

#### **Byte 7: Battery Voltage**
```
Data Type:    uint8 (1 byte)
Resolution:   0.1V
Range:        0 - 25.5V
Units:        Volts DC

Calculation:
Battery_Volts = Byte7 * 0.1

Example:
Byte: 0x78 = 120
Battery = 120 * 0.1 = 12.0V

Byte: 0x8C = 140
Battery = 140 * 0.1 = 14.0V
```

### 🔍 Complete Multi-Frame Decode Example (Generic Dash 2)

#### **Raw CAN Frames (All 4 Frames)**
```
Frame 1000: [0x40, 0x1F, 0xC8, 0x64, 0x46, 0x84, 0x03, 0x78]
Frame 1001: [0x87, 0x00, 0xB4, 0x00, 0x90, 0x01, 0x00, 0x00]
Frame 1002: [0x40, 0x01, 0x5E, 0x01, 0xE8, 0x03, 0x00, 0x00]
Frame 1003: [0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
```

#### **Step-by-Step Multi-Frame Decoding**

##### **Frame 1000 (Primary Parameters)**
```
Byte 0-1: RPM
  Raw: [0x40, 0x1F] = [64, 31]
  RPM = 64 + (31 * 256) = 8000 RPM

Byte 2: TPS
  Raw: 0xC8 = 200
  TPS = 200 * 0.5 = 100%

Byte 3: ECT
  Raw: 0x64 = 100
  ECT = 100 - 40 = 60°C

Byte 4: IAT
  Raw: 0x46 = 70
  IAT = 70 - 40 = 30°C

Byte 5-6: MAP
  Raw: [0x84, 0x03] = [132, 3]
  MAP_Raw = 132 + (3 * 256) = 900
  MAP = 900 * 0.1 = 90.0 kPa

Byte 7: Battery
  Raw: 0x78 = 120
  Battery = 120 * 0.1 = 12.0V
```

##### **Frame 1001 (Extended Parameters)**
```
Byte 0-1: Oil Temperature
  Raw: [0x87, 0x00] = [135, 0]
  Oil_Temp_Raw = 135 + (0 * 256) = 135
  Oil_Temp = 135 - 40 = 95°C

Byte 2-3: Ignition Timing
  Raw: [0xB4, 0x00] = [180, 0]
  Ignition_Raw = 180 + (0 * 256) = 180
  Ignition = 180 * 0.1 = 18.0°

Byte 4-5: Vehicle Speed
  Raw: [0x90, 0x01] = [144, 1]
  Speed_Raw = 144 + (1 * 256) = 400
  Speed = 400 * 0.1 = 40.0 km/h
```

##### **Frame 1002 (Pressures & Lambda)**
```
Byte 0-1: Oil Pressure
  Raw: [0x40, 0x01] = [64, 1]
  Oil_Press_Raw = 64 + (1 * 256) = 320
  Oil_Pressure = 320 kPa

Byte 2-3: Fuel Pressure
  Raw: [0x5E, 0x01] = [94, 1]
  Fuel_Press_Raw = 94 + (1 * 256) = 350
  Fuel_Pressure = 350 kPa

Byte 4-5: Lambda 1
  Raw: [0xE8, 0x03] = [232, 3]
  Lambda_Raw = 232 + (3 * 256) = 1000
  Lambda = 1000 * 0.001 = 1.000 λ
```

##### **Frame 1003 (Additional Parameters)**
```
Byte 0-1: ECU Temperature
  Raw: [0x55, 0x00] = [85, 0]
  ECU_Temp_Raw = 85 + (0 * 256) = 85
  ECU_Temp = 85 - 40 = 45°C
```

#### **Complete Decoded Results (All Parameters)**
```
Frame 1000 (Primary):
  Engine RPM:           8000 RPM
  Throttle Position:    100%
  Coolant Temperature:  60°C
  Intake Air Temp:      30°C
  Manifold Pressure:    90.0 kPa
  Battery Voltage:      12.0V

Frame 1001 (Extended):
  Oil Temperature:      95°C
  Ignition Timing:      18.0°
  Vehicle Speed:        40.0 km/h

Frame 1002 (Pressures):
  Oil Pressure:         320 kPa
  Fuel Pressure:        350 kPa
  Lambda 1:             1.000 λ

Frame 1003 (Additional):
  ECU Temperature:      45°C

Total Parameters:       13 (vs 6 in Dash2Pro)
```

### 💻 Implementation Code Examples

#### **C/C++ Decode Function**
```c
typedef struct {
    uint16_t rpm;           // Engine RPM
    float    tps;           // Throttle position (%)
    int16_t  ect;           // Coolant temp (°C)
    int16_t  iat;           // Intake air temp (°C)
    float    map;           // Manifold pressure (kPa)
    float    battery;       // Battery voltage (V)
} Dash2ProData;

void decodeDash2Pro(uint8_t* data, Dash2ProData* result) {
    // RPM (bytes 0-1, little endian)
    result->rpm = data[0] | (data[1] << 8);

    // TPS (byte 2)
    result->tps = data[2] * 0.5f;

    // ECT (byte 3, -40°C offset)
    result->ect = data[3] - 40;

    // IAT (byte 4, -40°C offset)
    result->iat = data[4] - 40;

    // MAP (bytes 5-6, little endian, 0.1 kPa resolution)
    uint16_t map_raw = data[5] | (data[6] << 8);
    result->map = map_raw * 0.1f;

    // Battery (byte 7, 0.1V resolution)
    result->battery = data[7] * 0.1f;
}
```

#### **Actual M5Stack Tab5 Implementation (Generic Dash 2)**
```cpp
// ECU Data Structure - Link G4X Generic Dash 2 CAN Stream (4 Frames)
struct ECUData {
  // Frame 1000 (Primary) - Same as Dash2Pro for compatibility
  float rpm;              // Engine Speed (1 RPM resolution)
  float mgp;              // Manifold Gauge Pressure/Boost (0.1 kPa resolution)
  float ect;              // Engine Coolant Temperature (1°C resolution)
  float iat;              // Inlet Air Temperature Post Intercooler (1°C resolution)
  float battery_voltage;  // ECU Volts/Battery Voltage (0.1V resolution)
  float tps;              // Throttle Position Sensor (0.5% resolution)

  // Frame 1001 (Extended)
  float oil_temp;         // Oil Temperature (1°C resolution)
  float ignition_timing;  // Ignition Angle (0.1° resolution)
  float vehicle_speed;    // Vehicle Speed (0.1 km/h resolution)

  // Frame 1002 (Pressures & Lambda)
  float oil_pressure;     // Oil Pressure (1 kPa resolution)
  float fuel_pressure;    // Fuel Pressure (1 kPa resolution)
  float lambda;           // Lambda 1 (0.001 Lambda resolution)

  // Frame 1003 (Additional)
  float ecu_temp;         // ECU Temperature (1°C resolution)

  unsigned long last_update; // Timestamp of last CAN message
};

// Multi-Frame CAN Processing (from actual Tab5 implementation)
bool readCANData() {
  CanFrame canMsg;
  bool data_received = false;

  // Read all available CAN messages with 10ms timeout
  while (ESP32Can.readFrame(canMsg, 10)) {
    data_received = true;

    // Parse Link G4X Generic Dash 2 CAN messages (4 frames: 1000, 1001, 1002, 1003)
    if (canMsg.data_length_code == 8) {

      // Frame 1000: Primary data (same as Dash2Pro for compatibility)
      if (canMsg.identifier == config.can_id_base) {
        ecu_data.rpm = (canMsg.data[1] << 8) | canMsg.data[0];
        ecu_data.tps = canMsg.data[2] * 0.5;
        ecu_data.ect = canMsg.data[3] - 40;
        ecu_data.iat = canMsg.data[4] - 40;
        ecu_data.mgp = ((canMsg.data[6] << 8) | canMsg.data[5]) * 0.1;
        ecu_data.battery_voltage = canMsg.data[7] * 0.1;
        ecu_data.last_update = millis();
      }

      // Frame 1001: Extended data (Oil temp, ignition, speed)
      else if (canMsg.identifier == config.can_id_base + 1) {
        int16_t oil_temp_raw = (canMsg.data[1] << 8) | canMsg.data[0];
        ecu_data.oil_temp = oil_temp_raw - 40;

        int16_t ignition_raw = (canMsg.data[3] << 8) | canMsg.data[2];
        ecu_data.ignition_timing = ignition_raw * 0.1;

        uint16_t speed_raw = (canMsg.data[5] << 8) | canMsg.data[4];
        ecu_data.vehicle_speed = speed_raw * 0.1;
      }

      // Frame 1002: Pressures and Lambda
      else if (canMsg.identifier == config.can_id_base + 2) {
        uint16_t oil_press_raw = (canMsg.data[1] << 8) | canMsg.data[0];
        ecu_data.oil_pressure = oil_press_raw;

        uint16_t fuel_press_raw = (canMsg.data[3] << 8) | canMsg.data[2];
        ecu_data.fuel_pressure = fuel_press_raw;

        uint16_t lambda_raw = (canMsg.data[5] << 8) | canMsg.data[4];
        ecu_data.lambda = lambda_raw * 0.001;
      }

      // Frame 1003: Additional data
      else if (canMsg.identifier == config.can_id_base + 3) {
        int16_t ecu_temp_raw = (canMsg.data[1] << 8) | canMsg.data[0];
        ecu_data.ecu_temp = ecu_temp_raw - 40;
      }
    }
  }

  return data_received;
}
```

#### **Arduino/ESP32 Implementation Template**
```cpp
void processCANFrame(uint32_t id, uint8_t* data, uint8_t length) {
    if (id == 864 && length == 8) {  // Dash2Pro frame
        // Decode RPM (little endian)
        uint16_t rpm = data[0] | (data[1] << 8);

        // Decode TPS (0.5% resolution)
        float tps = data[2] * 0.5;

        // Decode temperatures (-40°C offset)
        int16_t ect = data[3] - 40;
        int16_t iat = data[4] - 40;

        // Decode MAP (little endian, 0.1 kPa resolution)
        uint16_t map_raw = data[5] | (data[6] << 8);
        float map_kpa = map_raw * 0.1;

        // Decode battery (0.1V resolution)
        float battery = data[7] * 0.1;

        // Update display
        updateDashboard(rpm, tps, ect, iat, map_kpa, battery);
    }
}
```

### ⚙️ Link G4X ECU Configuration

#### **Required ECU Settings (Generic Dash 2 Configuration)**
```
Parameter                | Setting
------------------------|------------------
CAN Mode                | User Defined
CAN Stream              | Transmit Generic Dash 2
Base CAN ID             | 1000 (0x3E8 hex) - Frames 1000-1003
Transmission Rate       | 20 Hz per frame (50ms interval)
CAN Bus Speed           | 1 Mbps (1000 kbps)
Frame Format            | Standard (11-bit)
Data Length             | 8 bytes per frame
Auto-transmission       | Enabled
Frame Count             | 4 frames (1000, 1001, 1002, 1003)
```

**Note**: Generic Dash 2 provides **13+ parameters** across 4 frames vs Dash2Pro's 6 parameters in 1 frame. This includes oil pressure, fuel pressure, lambda, ignition timing, and more.

#### **ECU Configuration Steps (PCLink Software)**
1. **Open PCLink** and connect to Link G4X ECU
2. **Navigate to ECU Controls > CAN Setup**
3. **Select Channel 1** (or Channel 2 if preferred)
4. **Set Mode** to "User Defined" at 1 Mbit/s
5. **Select "Transmit Generic Dash 2"** from mode dropdown
6. **Set Transmit Rate** to 20 Hz (recommended)
7. **Set Base CAN ID** to 1000 (frames 1000-1003)
8. **Verify all 4 frames** are enabled in configuration
9. **Click Apply** and then OK
10. **Apply changes to ECU** and save configuration
11. **Verify all 4 streams** are transmitting using CAN analyzer

### 📈 Data Validation & Range Checking

#### **Typical Operating Ranges**
```
Parameter    | Idle      | Normal    | Maximum   | Warning
-------------|-----------|-----------|-----------|----------
RPM          | 800-1200  | 2000-6000 | 8000+     | >7000
TPS          | 0-5%      | 10-80%    | 100%      | N/A
ECT          | 80-95°C   | 85-105°C  | 120°C     | >110°C
IAT          | 20-60°C   | 30-80°C   | 100°C     | >90°C
MAP          | 20-40 kPa | 50-200 kPa| 300+ kPa  | <20 or >250
Battery      | 12.0-12.8V| 13.5-14.5V| 15.0V     | <11V or >15V
```

#### **Error Detection**
```cpp
bool validateDash2ProData(Dash2ProData* data) {
    // Check for reasonable ranges
    if (data->rpm > 10000) return false;        // Unrealistic RPM
    if (data->tps > 100.0) return false;        // TPS over 100%
    if (data->ect < -50 || data->ect > 200) return false;  // ECT range
    if (data->iat < -50 || data->iat > 200) return false;  // IAT range
    if (data->map < 0 || data->map > 500) return false;    // MAP range
    if (data->battery < 5.0 || data->battery > 20.0) return false; // Battery
    
    return true;  // Data appears valid
}
```

### 🔧 Troubleshooting

#### **No CAN Data Received**
- **Check CAN ID**: Verify ECU transmits on expected ID (864)
- **Check bus speed**: Both ECU and receiver must use 1000 kbps
- **Verify wiring**: CAN_H and CAN_L connections
- **Check termination**: 120Ω resistors at both ends
- **ECU configuration**: Ensure Dash2Pro stream is enabled

#### **Invalid Data Values**
- **Check byte order**: Ensure little-endian decoding for multi-byte values
- **Verify offsets**: Temperature values have -40°C offset
- **Check scaling**: MAP uses 0.1 kPa resolution, TPS uses 0.5%
- **Range validation**: Implement bounds checking for all parameters

#### **Intermittent Data**
- **Bus loading**: Check for CAN bus overload
- **Electrical noise**: Verify proper shielding and grounding
- **ECU health**: Monitor ECU status and error codes
- **Transmission rate**: Confirm 10 Hz transmission rate

### 📋 Frame Timing & Performance

#### **Transmission Characteristics**
```
Frequency:        10 Hz (100ms interval)
Frame Length:     108 bits total
  - Arbitration:  12 bits (SOF + ID + RTR)
  - Control:      6 bits (IDE + r0 + DLC)
  - Data:         64 bits (8 bytes)
  - CRC:          16 bits (15-bit CRC + delimiter)
  - ACK:          2 bits (ACK slot + delimiter)
  - EOF:          7 bits
  - IFS:          3 bits minimum

Transmission Time: 108 μs @ 1 Mbps
Bus Loading:      1.08% (108 μs every 100 ms)
```

#### **Real-Time Performance**
- **Latency**: <1ms from sensor to CAN transmission
- **Jitter**: <5ms typical transmission timing variation
- **Update Rate**: 10 Hz provides smooth dashboard updates
- **Bandwidth**: Very low bus utilization allows other CAN traffic

---

**Document Version**: 1.0  
**Last Updated**: 2025-01-18  
**Compatible ECUs**: Link G4X series with Dash2Pro CAN stream capability
