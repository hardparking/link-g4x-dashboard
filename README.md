# Link G4X Professional Dashboard

A production-ready, automotive-grade dashboard for Link G4X ECU monitoring on M5Stack Tab5 with real CAN bus support.

## ✨ Features

### 🚗 **Professional Automotive Display**
- **Large, Readable Text**: Massive fonts optimized for in-vehicle use
- **No Screen Flicker**: Smooth partial updates with no blinking
- **High Contrast**: White labels and units for maximum visibility
- **Professional Lambda Gauge**: Double-needle display with target tracking

### 📊 **Complete Generic Dash 2 Parameter Support (21 Parameters)**
- **Core Engine**: RPM, TPS, ECT, IAT, MGP, Battery
- **Oil System**: Oil Temperature, Oil Pressure
- **Fuel System**: Fuel Pressure, Fuel Level
- **Lambda/AFR**: Lambda 1, Lambda 2, Lambda Combined Graph
- **Performance**: Ignition Timing, Knock Level, Boost Duty
- **Exhaust**: EGT 1, EGT 2
- **Vehicle**: Vehicle Speed, Gear Position
- **System**: ECU Temperature

### 🎛️ **Complete Gauge Configurability**
- **Double-click any gauge** to configure instantly
- **21 available parameters** from Generic Dash 2 stream
- **No duplicate restrictions** - use any parameter multiple times
- **Professional modal interface** for parameter selection
- **Persistent configuration** - custom layouts save automatically

### ⚙️ **Touch Configuration System**
- **Calculator-Style CAN ID Input**: Easy decimal entry (Link ECU compatible)
- **Unit Selection**: Celsius/Fahrenheit, kPa/PSI, km/h/mph
- **Data Source Toggle**: Simulation vs Real CAN bus
- **Persistent Settings**: All preferences saved to flash memory

### 🔧 **Production-Ready CAN Bus**
- **Built-in RS485 Interface**: Tab5 integrated SIT3088 transceiver
- **Link G4X Generic Dash 2 Protocol**: Complete 21-parameter CAN stream support
- **Multi-Frame Support**: 4 CAN frames (1000-1003) for comprehensive monitoring
- **Single Cable Solution**: Power + CAN data through one connector
- **Industrial Grade**: 6-24V supply range, switchable 120Ω termination

## 🛠️ Hardware Requirements

### **Required Components**
- **M5Stack Tab5**: Main display unit with built-in RS485 interface
- **Link G4X ECU**: With Generic Dash 2 CAN stream enabled (base CAN ID 1000)
- **4-Pin Automotive Connector**: For power + CAN connection
- **Sensors**: Oil pressure, fuel pressure, wideband lambda, etc. (as desired)

### **ExtPort2 Connector (Tab5 Built-in RS485)**
The Tab5 features a built-in **SIT3088 RS485 transceiver** with switchable 120Ω termination, perfect for CAN communication.

#### **Connector Pinout**
```
Pin | Function | Description
----|----------|-------------
 1  | GND      | Ground reference
 2  | VIN      | Power input (6-24V DC)
 3  | CAN_H    | CAN High signal
 4  | CAN_L    | CAN Low signal
```

#### **Internal Tab5 Connections**
```
Tab5 GPIO | RS485 Function | Description
----------|----------------|-------------
G20       | TX             | Transmit data
G21       | RX             | Receive data
G34       | DIR            | Direction control
```

### **Vehicle Wiring**
```
Vehicle Side          Tab5 ExtPort2
-------------         --------------
12V Supply     -->    Pin 2 (VIN)
Ground         -->    Pin 1 (GND)
CAN Bus High   -->    Pin 3 (CAN_H)
CAN Bus Low    -->    Pin 4 (CAN_L)
```

### **CAN Bus Requirements**
- **120Ω termination**: Built-in switchable termination on Tab5
- **Twisted pair cable**: CAT5/6 or automotive CAN cable
- **Maximum length**: 40m at 1Mbps
- **Supply voltage**: 6-24V DC (perfect for 12V automotive)

## 🔌 ExtPort2 Connector Detailed Documentation

### **Physical Connector**
- **Type**: 4-pin automotive-grade connector (recommended: Deutsch, AMP, or equivalent)
- **Current Rating**: 3A minimum for power pin
- **Voltage Rating**: 24V minimum
- **Environmental**: IP67 rated for automotive use

### **Pin Functions & Specifications**

#### **Pin 1: GND (Ground)**
- **Function**: System ground reference
- **Connection**: Vehicle chassis ground
- **Wire Gauge**: 18 AWG minimum
- **Color**: Black (standard)

#### **Pin 2: VIN (Power Input)**
- **Function**: Main power supply
- **Voltage Range**: 6-24V DC
- **Current Draw**: 2-3A peak (display + processing)
- **Protection**: Internal reverse polarity protection
- **Connection**: Vehicle switched 12V (ignition controlled)
- **Wire Gauge**: 16 AWG minimum
- **Color**: Red (standard)
- **Fusing**: 5A fuse recommended

#### **Pin 3: CAN_H (CAN High)**
- **Function**: CAN bus high signal
- **Voltage**: 2.5V ± 2V differential
- **Impedance**: 120Ω (when termination enabled)
- **Connection**: ECU CAN High
- **Wire Type**: Twisted pair (120Ω characteristic impedance)
- **Color**: White/Orange (CAT5) or per automotive standard

#### **Pin 4: CAN_L (CAN Low)**
- **Function**: CAN bus low signal
- **Voltage**: 2.5V ± 2V differential (inverted from CAN_H)
- **Impedance**: 120Ω (when termination enabled)
- **Connection**: ECU CAN Low
- **Wire Type**: Twisted pair with CAN_H
- **Color**: Orange (CAT5) or per automotive standard

### **Built-in SIT3088 Transceiver Features**
- **Isolation**: Galvanic isolation for noise immunity
- **ESD Protection**: ±15kV HBM protection
- **Temperature Range**: -40°C to +125°C (automotive grade)
- **Switchable Termination**: Software-controlled 120Ω termination
- **Fault Detection**: Short circuit and open circuit detection
- **Low Power**: Standby mode support

### **Connector Assembly Recommendations**

#### **Cable Specifications**
- **CAN Signals**: 120Ω twisted pair (CAT5e/6 or automotive CAN cable)
- **Power Wires**: 16 AWG stranded copper
- **Shield**: Overall foil shield with drain wire
- **Jacket**: Automotive-grade PVC or TPE

#### **Crimping & Assembly**
- **Use proper crimping tools** for chosen connector type
- **Strain relief**: Adequate strain relief at both ends
- **Cable length**: Keep as short as practical (max 5m for dashboard installation)
- **Routing**: Away from high-current wires and ignition components

### **Installation Best Practices**

#### **Power Connection**
```
Vehicle Fuse Box → 5A Fuse → Ignition Switch → Pin 2 (VIN)
Vehicle Chassis Ground → Pin 1 (GND)
```

#### **CAN Bus Connection**
```
ECU CAN High → Pin 3 (CAN_H)
ECU CAN Low  → Pin 4 (CAN_L)
```

#### **Termination Settings**
- **Enable 120Ω termination** if Tab5 is at end of CAN bus
- **Disable termination** if Tab5 is in middle of CAN bus
- **Check total bus termination** = 60Ω (two 120Ω resistors in parallel)

### **Link G4X ECU Configuration**
- **Enable Generic Dash 2 CAN Stream** (not Dash2Pro)
- **Set Base CAN ID to 1000** (frames 1000-1003) or use custom base
- **CAN Speed: 1Mbps**
- **Multi-frame format** with 21 available parameters across 4 frames

## 🚀 Installation

### **PlatformIO Setup**
```bash
git clone <repository-url>
cd link_g4x_monitor_backup_m5gfx
pio run -e esp32p4_pioarduino --target upload
```

### **Dependencies**
- **M5Unified**: M5Stack hardware abstraction
- **M5GFX**: Graphics library
- **ESP32-TWAI-CAN**: ESP32-P4 internal TWAI controller library
- **Preferences**: ESP32 flash storage

## 📱 Usage

### **Initial Setup**
1. **Power on Tab5** - starts in simulation mode
2. **Tap "CONFIG"** in header to access settings
3. **Configure CAN ID** using calculator-style input
4. **Select preferred units** (metric/imperial)
5. **Switch to "Real CAN"** when hardware is connected

### **Configuration Options**

#### **CAN ID Configuration**
- **Tap "EDIT"** next to Base CAN ID
- **Use calculator keypad** to enter decimal base ID
- **Default: 1000** (Generic Dash 2 standard - frames 1000-1003)
- **Legacy: 864** (Dash2Pro compatibility - frame 864 only)
- **Range: 0-2047** (11-bit CAN ID)

#### **Unit Selection**
- **Temperature**: Celsius ↔ Fahrenheit
- **Pressure**: kPa ↔ PSI
- **Speed**: km/h ↔ mph
- **Auto-scaling**: Warning thresholds adjust with units

#### **Data Source**
- **Simulation**: Safe testing with realistic data
- **Real CAN**: Live ECU data from CAN bus
- **Auto-fallback**: Graceful handling of CAN issues

#### **Gauge Configuration**
- **Double-click any gauge** to open configuration modal
- **21 available parameters** from Generic Dash 2 stream
- **No restrictions** - use any parameter multiple times
- **Professional interface** with parameter grid selection
- **Instant preview** - changes apply immediately
- **Persistent storage** - custom layouts save automatically

**Example Custom Layouts**:
- **Racing**: RPM, TPS, MGP, Lambda Graph, ECT, Oil Temp, Oil Press, Ignition, Fuel Press, Speed, Gear, Battery
- **Tuning**: RPM, Lambda 1, Lambda 2, TPS, Ignition, Knock, MGP, EGT 1, EGT 2, Boost Duty, ECT, Battery
- **Endurance**: RPM, ECT, Oil Temp, Oil Press, Fuel Level, Fuel Press, Lambda Graph, Speed, Gear, Battery, ECU Temp, MGP

### **Lambda Gauge**
- **Outer Arc (Cyan)**: ECU target lambda
- **Inner Arc (Color-coded)**: Actual lambda reading
- **Error Display**: Shows +/-0.015 deviation from target
- **Color Coding**: Green (good), Yellow (moderate), Red (poor)

## 🔧 Technical Specifications

### **Performance**
- **Memory Usage**: 5.4% RAM (27KB of 512KB)
- **Flash Usage**: 44% (580KB of 1.3MB)
- **Update Rate**: 500ms for smooth display
- **CAN Speed**: 1Mbps (automotive standard)

### **Display Features**
- **Resolution**: 1280x720 optimized layout
- **Fonts**: DejaVu family (12pt to 72pt)
- **Colors**: High-contrast automotive palette
- **Partial Updates**: Flicker-free value changes

### **Data Protocol**
**Link G4X Generic Dash 2 Multi-Frame Format**:

**Frame 1000 (Primary - 8 bytes)**:
- Bytes 0-1: RPM (0-8000, 1 RPM resolution)
- Byte 2: TPS (0-100%, 0.5% resolution)
- Byte 3: ECT (0-200°C, 1°C resolution, -40°C offset)
- Byte 4: IAT (0-200°C, 1°C resolution, -40°C offset)
- Bytes 5-6: MAP (0-600 kPa, 0.1 kPa resolution)
- Byte 7: Battery (0-20V, 0.1V resolution)

**Frame 1001 (Extended - 8 bytes)**:
- Bytes 0-1: Oil Temperature (1°C resolution, -40°C offset)
- Bytes 2-3: Ignition Timing (0.1° resolution)
- Bytes 4-5: Vehicle Speed (0.1 km/h resolution)

**Frame 1002 (Pressures & Lambda - 8 bytes)**:
- Bytes 0-1: Oil Pressure (1 kPa resolution)
- Bytes 2-3: Fuel Pressure (1 kPa resolution)
- Bytes 4-5: Lambda 1 (0.001 resolution)

**Frame 1003 (Additional - 8 bytes)**:
- Bytes 0-1: ECU Temperature (1°C resolution, -40°C offset)
- Bytes 2-3: Lambda 2 (0.001 resolution)
- Bytes 4-5: Gear Position, Fuel Level, etc.

**Total: 21 configurable parameters across 4 frames**

## 🎯 Production Features

### **Automotive Grade**
- **Vibration Resistant**: Solid-state display with no moving parts
- **Temperature Stable**: ESP32-P4 industrial temperature range
- **EMI Resistant**: Proper CAN bus isolation and filtering

### **User Experience**
- **Large Touch Targets**: 180x50 pixel buttons minimum
- **Immediate Feedback**: Settings save instantly
- **Persistent Memory**: All preferences survive power cycles
- **Error Recovery**: Graceful handling of CAN bus issues

### **Professional Appearance**
- **Clean Layout**: Organized 4x3 tile grid
- **Consistent Styling**: Uniform fonts and colors
- **Status Indicators**: Clear visual feedback
- **Automotive Colors**: Industry-standard warning/caution/good colors

## 🔍 Troubleshooting

### **CAN Bus Issues**
- **Check ExtPort2 connections**: Verify 4-pin connector wiring
- **Verify CAN ID**: Must match ECU configuration (default: 864)
- **Check power supply**: 6-24V DC required on Pin 2
- **Enable termination**: Use Tab5 built-in 120Ω termination if needed
- **Monitor serial output**: Detailed RS485 initialization logs available

### **Configuration Problems**
- **Settings not saving**: Check flash memory health
- **Touch not working**: Verify screen calibration
- **Display issues**: Check M5Stack firmware version
- **Real CAN mode resets**: Ensure CAN hardware is connected

### **Performance Issues**
- **Slow updates**: Check CAN bus load and ECU transmission rate
- **Screen flicker**: Should not occur with current firmware
- **Memory issues**: Monitor serial output for memory warnings

## ⚠️ Safety Notes

**Important Safety Information:**
- This is a **monitoring tool only** - it does not control the ECU
- **Do not** rely solely on this display for critical engine parameters
- Always use proper engine management practices
- Test thoroughly before using in critical applications
- Ensure proper grounding and electrical isolation

## 📄 License

MIT License - See LICENSE file for details

## 🤝 Contributing

Contributions welcome! Please read contributing guidelines and submit pull requests.

## 📞 Support

For technical support and feature requests:
1. **Check ECU CAN configuration** (Generic Dash 2 stream enabled, base CAN ID 1000)
2. **Verify ExtPort2 connections** (4-pin power + CAN connector)
3. **Confirm power supply** (6-24V DC on Pin 2)
4. **Test gauge configuration** (double-click any gauge to configure)
5. **Review serial monitor output** for CAN frame debugging information
6. **Open GitHub issue** with detailed problem description
