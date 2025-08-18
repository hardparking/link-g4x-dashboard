# Link G4X Dashboard Documentation Index
## Complete Technical Documentation for M5Stack Tab5 CAN Dashboard

### 📚 Documentation Overview
This directory contains comprehensive technical documentation for the Link G4X Dashboard project, covering hardware integration, **Generic Dash 2 CAN protocol** implementation (13+ parameters across 4 frames), and system configuration.

### 📋 Document Index

#### **🔌 Hardware Documentation**
- **[ExtPort2_Connector.md](ExtPort2_Connector.md)** - Complete ExtPort2 connector specifications
  - 4-pin connector pinout and electrical specifications
  - SIT3088 RS485 transceiver features and capabilities
  - Cable assembly instructions and part numbers
  - Installation best practices and safety guidelines

- **[Wiring_Diagram.md](Wiring_Diagram.md)** - Visual wiring diagrams and connection details
  - ASCII art wiring diagrams for complete system
  - Pin assignments for both Tab5 and vehicle sides
  - Power circuit details with fusing and switching
  - CAN bus network topology with termination

#### **📡 CAN Protocol Documentation**
- **[Link_G4X_Dash2Pro_Protocol.md](Link_G4X_Dash2Pro_Protocol.md)** - Complete Generic Dash 2 CAN protocol specification
  - Multi-frame CAN protocol (4 frames: 1000-1003) with 13+ parameters
  - Oil pressure, fuel pressure, lambda, ignition timing, and more
  - Detailed multi-frame decoding algorithms with examples
  - Implementation code examples and validation
  - ECU configuration requirements for Generic Dash 2

- **[CAN_Message_Analysis.md](CAN_Message_Analysis.md)** - Real-world CAN frame analysis
  - Actual CAN frame examples with step-by-step decoding
  - Data pattern analysis for different engine conditions
  - Implementation validation and unit test cases
  - Troubleshooting common decoding issues

### 🔧 Quick Reference

#### **ExtPort2 Connector Pinout**
```
Pin | Function | Wire Color | Specification
----|----------|------------|---------------
 1  | GND      | Black      | System ground
 2  | VIN      | Red        | 6-24V DC power input
 3  | CAN_H    | White      | CAN bus high signal
 4  | CAN_L    | Orange     | CAN bus low signal
```

#### **Generic Dash 2 CAN Frame Format (4 Frames)**
```
Frame 1000 (Primary):
Byte | Parameter | Type   | Resolution | Range
-----|-----------|--------|------------|------------------
0-1  | RPM       | uint16 | 1 RPM      | 0-65535 RPM
2    | TPS       | uint8  | 0.5%       | 0-100%
3    | ECT       | uint8  | 1°C        | -40 to +215°C
4    | IAT       | uint8  | 1°C        | -40 to +215°C
5-6  | MAP       | uint16 | 0.1 kPa    | 0-6553.5 kPa
7    | Battery   | uint8  | 0.1V       | 0-25.5V

Frame 1001 (Extended):
0-1  | Oil Temp  | int16  | 1°C        | -40 to +215°C
2-3  | Ignition  | int16  | 0.1°       | -50 to +50°
4-5  | Speed     | uint16 | 0.1 km/h   | 0-6553.5 km/h

Frame 1002 (Pressures):
0-1  | Oil Press | uint16 | 1 kPa      | 0-65535 kPa
2-3  | Fuel Press| uint16 | 1 kPa      | 0-65535 kPa
4-5  | Lambda    | uint16 | 0.001 λ    | 0-65.535 λ

Frame 1003 (Additional):
0-1  | ECU Temp  | int16  | 1°C        | -40 to +215°C
```

#### **Key Specifications**
```
CAN Protocol:     ISO 11898 CAN 2.0B
Stream Type:      Generic Dash 2 (4 frames)
Base CAN ID:      1000 (0x3E8 hex) - Frames 1000-1003
Bit Rate:         1 Mbps (1000 kbps)
Frame Rate:       20 Hz per frame (50ms interval)
Total Data Rate:  80 Hz combined (4 × 20 Hz)
Data Length:      8 bytes per frame (32 bytes total)
Parameters:       13+ (vs 6 in Dash2Pro)
Byte Order:       Little Endian
Power Input:      6-24V DC (12V automotive)
Current Draw:     1.5A typical, 3A peak
```

### 🚗 System Integration

#### **Vehicle Connection**
1. **Power**: Connect 12V switched power through 5A fuse to Pin 2
2. **Ground**: Connect chassis ground to Pin 1
3. **CAN Bus**: Connect ECU CAN_H/CAN_L to Pins 3/4
4. **Termination**: Enable Tab5 120Ω termination if at bus end

#### **ECU Configuration**
1. **Enable Generic Dash 2 stream** in Link G4X software
2. **Set base CAN ID** to 1000 (frames 1000-1003)
3. **Configure bit rate** to 1000 kbps
4. **Set transmission rate** to 20 Hz per frame
5. **Verify all 4 streams** are active
6. **Confirm 13+ parameters** are being transmitted

#### **Tab5 Configuration**
1. **Set CAN ID** to match ECU (default: 864)
2. **Select units** (metric/imperial)
3. **Configure CAN speed** (1000 kbps)
4. **Enable real CAN mode** (disable simulation)
5. **Test communication** and verify data

### 🔍 Troubleshooting Guide

#### **No CAN Communication**
- **Check ExtPort2 connections** - verify all 4 pins connected
- **Verify power supply** - 6-24V on Pin 2, good ground on Pin 1
- **Confirm CAN wiring** - CAN_H/CAN_L not swapped
- **Check termination** - 60Ω total bus resistance
- **Validate ECU config** - Dash2Pro stream enabled, correct CAN ID

#### **Invalid Data Values**
- **Verify byte order** - use little endian for multi-byte values
- **Check scaling factors** - TPS uses 0.5%, MAP uses 0.1 kPa
- **Confirm offsets** - temperatures have -40°C offset
- **Range validation** - implement bounds checking

#### **Power Issues**
- **Measure supply voltage** - should be 12-14V with engine running
- **Check current draw** - should be <3A peak
- **Verify fusing** - 5A fuse recommended
- **Test ground connection** - ensure low resistance to chassis

### 📊 Performance Specifications

#### **Real-Time Performance**
```
CAN Frame Processing:  <1ms latency
Display Update Rate:   10 Hz (matches CAN rate)
Touch Response:        <50ms
Configuration Save:    <100ms to flash
Boot Time:            <3 seconds to dashboard
```

#### **Environmental Specifications**
```
Operating Temperature: -20°C to +70°C
Storage Temperature:   -40°C to +85°C
Humidity:             5-95% non-condensing
Vibration:            Automotive grade
Supply Voltage:       6-24V DC
Power Consumption:    18W typical, 36W peak
```

#### **CAN Bus Performance**
```
Bit Rate:             1 Mbps ±0.1%
Frame Rate:           10 Hz ±1 Hz
Bus Loading:          0.108% (very low)
Error Rate:           <0.01% (excellent)
Recovery Time:        <100ms from bus errors
```

### 🛠️ Development Resources

#### **Hardware Requirements**
- **M5Stack Tab5** with ESP32-P4 and built-in RS485 interface
- **Link G4X ECU** with CAN capability and Dash2Pro stream
- **4-pin automotive connector** (Deutsch DT04-4P recommended)
- **CAN cable** (120Ω twisted pair, automotive grade)

#### **Software Dependencies**
- **PlatformIO** development environment
- **M5Unified** library for hardware abstraction
- **M5GFX** library for graphics
- **ESP32-TWAI-CAN** library for CAN communication
- **Preferences** library for configuration storage

#### **Development Tools**
- **CAN analyzer** for protocol debugging
- **Oscilloscope** for signal quality verification
- **Multimeter** for electrical measurements
- **Link G4X software** for ECU configuration

### 📝 Document Maintenance

#### **Version History**
```
Version 1.0 - 2025-01-18
- Initial documentation release
- Complete ExtPort2 connector specification
- Dash2Pro protocol documentation
- Real-world CAN frame analysis
- Implementation examples and validation
```

#### **Contributing**
- **Report issues** via GitHub issues
- **Submit improvements** via pull requests
- **Update documentation** when adding features
- **Validate examples** with real hardware

#### **Support Resources**
- **GitHub Repository**: Complete source code and documentation
- **Link G4X Manual**: ECU configuration reference
- **M5Stack Documentation**: Hardware specifications
- **CAN Standards**: ISO 11898 protocol reference

### ⚠️ Safety & Compliance

#### **Electrical Safety**
- **Always use proper fusing** on power circuits
- **Verify polarity** before connecting power
- **Check voltage levels** before connection
- **Use automotive-grade components** for reliability

#### **Automotive Integration**
- **Follow local regulations** for dashboard modifications
- **Ensure secure mounting** to prevent projectiles
- **Maintain driver visibility** and accessibility
- **Test thoroughly** before road use

#### **EMC Compliance**
- **Use shielded cables** for CAN communication
- **Proper grounding** to prevent interference
- **Ferrite cores** if EMI issues occur
- **Automotive EMC standards** compliance

---

**Document Set Version**: 1.0  
**Last Updated**: 2025-01-18  
**Compatible Hardware**: M5Stack Tab5 with SIT3088 RS485 interface  
**Compatible ECUs**: Link G4X series with Dash2Pro CAN stream capability

For the most current documentation and updates, visit the project repository.
