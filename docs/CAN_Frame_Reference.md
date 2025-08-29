# CAN Frame Reference - Link G4X Dashboard
## Quick Reference for Custom Stream Configuration

### 🔧 CAN Bus Settings

**Bus Configuration:**
- **Speed:** 1000 kbps (1 Mbps)
- **Mode:** User Defined
- **Byte Order:** Little Endian (Intel)
- **Termination:** 120Ω at each end

---

## 📡 Transmit Frames (ECU → Dashboard)

### Frame 0x500 (1280) - Primary Engine Data
**Rate:** 20 Hz | **Length:** 8 bytes

| Byte | Parameter | Type | Scaling | Range | Units |
|------|-----------|------|---------|-------|-------|
| 0-1  | Engine Speed (RPM) | uint16 | ÷10 | 0-8000 | RPM |
| 2    | Throttle Position (TPS) | uint8 | ×2 | 0-100 | % |
| 3    | Accelerator Position (APS) | uint8 | ×2 | 0-100 | % |
| 4    | Engine Coolant Temp (ECT) | uint8 | +40°C | -40 to +215 | °C |
| 5-6  | Manifold Absolute Pressure | uint16 | ×10 | 0-6553.5 | kPa |
| 7    | ECU Volts (Battery) | uint8 | ×10 | 0-25.5 | V |

**Example Frame:**
```
CAN ID: 0x500
Data: [0x40 0x08 0x1E 0x20 0x7F 0x90 0x01 0x7D]
Decoded: RPM=2112, TPS=15%, APS=16%, ECT=87°C, MAP=400kPa, Batt=12.5V
```

---

### Frame 0x501 (1281) - Lambda & Fuel Data  
**Rate:** 20 Hz | **Length:** 8 bytes

| Byte | Parameter | Type | Scaling | Range | Units |
|------|-----------|------|---------|-------|-------|
| 0-1  | Lambda 1 | uint16 | ×1000 | 0.500-1.500 | λ |
| 2-3  | Lambda Target | uint16 | ×1000 | 0.500-1.500 | λ |
| 4    | Injector Duty Cycle | uint8 | ×2 | 0-100 | % |
| 5    | Ethanol Percentage | uint8 | Direct | 0-100 | % |
| 6-7  | Battery Voltage | uint16 | ×100 | 0-25.5 | V |

**Example Frame:**
```
CAN ID: 0x501
Data: [0xE8 0x03 0xF4 0x03 0x28 0x55 0x04 0x05]
Decoded: Lambda=1.000, Target=1.012, Duty=20%, Ethanol=85%, Batt=12.8V
```

---

### Frame 0x502 (1282) - Pressures & Status
**Rate:** 10 Hz | **Length:** 8 bytes

| Byte | Parameter | Type | Scaling | Range | Units |
|------|-----------|------|---------|-------|-------|
| 0-1  | Oil Pressure | uint16 | ×10 | 0-655.3 | kPa |
| 2-3  | Fuel Pressure | uint16 | ×10 | 0-655.3 | kPa |
| 4    | Current Boost Map | uint8 | Direct | 1-8 | Map# |
| 5    | Current E-Throttle Map | uint8 | Direct | 1-8 | Map# |
| 6    | Status Flags | uint8 | Bitfield | See below | - |
| 7    | Reserved | uint8 | - | - | - |

**Status Flags (Byte 6):**
- Bit 0: Launch Control Active (1=on, 0=off)
- Bit 1: Anti-Lag Active (1=on, 0=off)
- Bit 2-7: Reserved for future use

**Example Frame:**
```
CAN ID: 0x502
Data: [0x20 0x03 0x58 0x0B 0x02 0x01 0x03 0x00]
Decoded: Oil=80kPa, Fuel=290kPa, BoostMap=2, EThrottle=1, Launch+AntiLag=On
```

---

## 📥 Receive Frames (Dashboard → ECU)

### Frame 0x600 (1536) - Dashboard Commands
**Rate:** On demand | **Length:** 8 bytes

| Byte | Parameter | Type | Description |
|------|-----------|------|-------------|
| 0    | Command Type | uint8 | See command types below |
| 1    | Command Value | uint8 | Command-specific value |
| 2    | Confirmation | uint8 | 0xAA = confirmed command |
| 3-7  | Reserved | uint8 | Future expansion |

**Command Types:**
- 0x01: Boost Map Switch (Value = 1-8)
- 0x02: Boost Target Adjust (Value = ±10 kPa)
- 0x03: Launch Control Toggle (Value = 0/1)
- 0x04: Anti-Lag Toggle (Value = 0/1)
- 0x05: Safe Mode Activate (Value = 1)

---

## 🔍 Parsing Examples

### C++ Parsing Code
```cpp
// Frame 0x500 - Primary Engine Data
void parseFrame500(const uint8_t* data) {
    uint16_t rpm_raw = (data[1] << 8) | data[0];
    float rpm = rpm_raw * 0.1;
    
    float tps = data[2] * 0.5;
    float aps = data[3] * 0.5;
    float ect = data[4] - 40;
    
    uint16_t map_raw = (data[5] << 8) | data[4];
    float map = map_raw * 0.1;
    
    float battery = data[7] * 0.1;
}

// Frame 0x501 - Lambda & Fuel Data  
void parseFrame501(const uint8_t* data) {
    uint16_t lambda_raw = (data[1] << 8) | data[0];
    float lambda = lambda_raw * 0.001;
    
    uint16_t target_raw = (data[3] << 8) | data[2];
    float lambda_target = target_raw * 0.001;
    
    float duty = data[4] * 0.5;
    uint8_t ethanol = data[5];
    
    uint16_t batt_raw = (data[7] << 8) | data[6];
    float battery = batt_raw * 0.01;
}
```

---

## 🛠️ PCLink Configuration Templates

### Stream 0x500 Template
```
Stream Name: Primary Engine Data
CAN ID: 1280 (0x500)
Rate: 20 Hz
Length: 8 bytes
Byte Order: Little Endian

Parameters:
- Byte 0-1: Engine Speed, Scale: 0.1, Offset: 0
- Byte 2: Throttle Position, Scale: 0.5, Offset: 0  
- Byte 3: Accelerator Position, Scale: 0.5, Offset: 0
- Byte 4: Engine Coolant Temp, Scale: 1, Offset: 40
- Byte 5-6: Manifold Absolute Pressure, Scale: 0.1, Offset: 0
- Byte 7: ECU Volts, Scale: 0.1, Offset: 0
```

### Stream 0x501 Template
```
Stream Name: Lambda & Fuel Data
CAN ID: 1281 (0x501)  
Rate: 20 Hz
Length: 8 bytes
Byte Order: Little Endian

Parameters:
- Byte 0-1: Lambda 1, Scale: 0.001, Offset: 0
- Byte 2-3: Lambda Target, Scale: 0.001, Offset: 0
- Byte 4: Injector Duty Cycle, Scale: 0.5, Offset: 0
- Byte 5: Ethanol Percentage, Scale: 1, Offset: 0
- Byte 6-7: ECU Volts, Scale: 0.01, Offset: 0
```

### Stream 0x502 Template
```
Stream Name: Pressures & Status
CAN ID: 1282 (0x502)
Rate: 10 Hz  
Length: 8 bytes
Byte Order: Little Endian

Parameters:
- Byte 0-1: Oil Pressure, Scale: 0.1, Offset: 0
- Byte 2-3: Fuel Pressure, Scale: 0.1, Offset: 0
- Byte 4: Current Boost Map, Scale: 1, Offset: 0
- Byte 5: Current E-Throttle Map, Scale: 1, Offset: 0
- Byte 6: Status Flags (bitfield), Scale: 1, Offset: 0
- Byte 7: Reserved, Scale: 1, Offset: 0
```

---

## 🔧 Troubleshooting

### Common Issues

**Wrong Values Displayed:**
- Check byte order (Little Endian)
- Verify scaling factors
- Confirm parameter assignments

**No Communication:**
- Verify CAN speed (1 Mbps)
- Check wiring (CAN H/L not swapped)
- Confirm termination resistors

**Intermittent Data:**
- Check cable quality
- Verify ground connections
- Monitor for electrical interference

### Validation Tools

**PCLink CAN Monitor:**
- View raw CAN frames
- Verify transmission rates
- Check data integrity

**Dashboard Debug Mode:**
- Serial output shows received frames
- Parameter parsing verification
- Communication status

---

**📋 Quick Setup Checklist:**
- [ ] CAN speed: 1 Mbps
- [ ] Stream 0x500: Primary data @ 20Hz
- [ ] Stream 0x501: Lambda/fuel @ 20Hz  
- [ ] Stream 0x502: Pressures @ 10Hz
- [ ] Wiring: CAN H/L + termination
- [ ] Dashboard: Live CAN mode enabled
