# Link G4X ECU Setup Guide for M5Stack Dashboard
## Complete Step-by-Step Configuration

### 🎯 Overview

This guide provides complete instructions for configuring your Link G4X ECU to work with the M5Stack Tab5 dashboard. The setup enables real-time monitoring of engine parameters and ethanol content through custom CAN streams.

### 📋 Prerequisites

**Hardware Required:**
- Link G4X ECU (any variant)
- M5Stack Tab5 with dashboard firmware
- CAN bus wiring (twisted pair cable)
- 120Ω termination resistors (2x)
- PC with PCLink software

**Software Required:**
- PCLink (latest version from Link ECU website)
- USB cable for ECU connection

### ⚡ Quick Start Checklist

- [ ] Backup current ECU tune
- [ ] Configure CAN bus speed to 1 Mbps
- [ ] Create 3 custom transmit streams (0x500, 0x501, 0x502)
- [ ] Wire CAN bus between ECU and dashboard
- [ ] Test communication
- [ ] Switch dashboard from simulation to live mode

---

## 🔧 Part 1: PCLink Configuration

### Step 1: Backup Your Current Tune

**⚠️ IMPORTANT: Always backup before making changes!**

1. **Connect ECU to PC** via USB cable
2. **Open PCLink** and establish communication
3. **File → ECU → Store to File**
4. **Save as:** `Original_Tune_Backup_[DATE].pclr`
5. **Verify backup** by checking file size (should be >100KB)

### Step 2: Configure CAN Bus Settings

1. **Navigate to:** `ECU Controls → CAN Setup`
2. **Select CAN Channel:** Choose CAN 1 or CAN 2 (note which one)
3. **Set Mode:** "User Defined"
4. **Set Bit Rate:** 1000 kbps (1 Mbps)
5. **Click Apply** to save settings

### Step 3: Create Custom Transmit Streams

#### Stream 1: Primary Engine Data (0x500)

1. **Click "Streams" tab**
2. **Click "Add Stream"**
3. **Configure Stream:**
   - **CAN ID:** 1280 (decimal) or 0x500 (hex)
   - **Transmission Rate:** 20 Hz
   - **Data Length:** 8 bytes
   - **Byte Order:** Little Endian

4. **Assign Parameters:**
   ```
   Byte 0-1: Engine Speed (RPM) - 16-bit, Scale: ÷10
   Byte 2:   Throttle Position (TPS) - Scale: ×2
   Byte 3:   Accelerator Position (APS) - Scale: ×2  
   Byte 4:   Engine Coolant Temp (ECT) - Offset: +40°C
   Byte 5-6: Manifold Absolute Pressure (MAP) - 16-bit, Scale: ×10
   Byte 7:   ECU Volts (Battery) - Scale: ×10
   ```

#### Stream 2: Lambda & Fuel Data (0x501)

1. **Click "Add Stream"**
2. **Configure Stream:**
   - **CAN ID:** 1281 (decimal) or 0x501 (hex)
   - **Transmission Rate:** 20 Hz
   - **Data Length:** 8 bytes
   - **Byte Order:** Little Endian

3. **Assign Parameters:**
   ```
   Byte 0-1: Lambda 1 - 16-bit, Scale: ×1000
   Byte 2-3: Lambda Target - 16-bit, Scale: ×1000
   Byte 4:   Injector Duty Cycle - Scale: ×2
   Byte 5:   Ethanol Percentage - Direct value (0-100%)
   Byte 6-7: Battery Voltage - 16-bit, Scale: ×100
   ```

#### Stream 3: Pressures & Status (0x502)

1. **Click "Add Stream"**
2. **Configure Stream:**
   - **CAN ID:** 1282 (decimal) or 0x502 (hex)
   - **Transmission Rate:** 10 Hz
   - **Data Length:** 8 bytes
   - **Byte Order:** Little Endian

3. **Assign Parameters:**
   ```
   Byte 0-1: Oil Pressure - 16-bit, Scale: ×10
   Byte 2-3: Fuel Pressure - 16-bit, Scale: ×10
   Byte 4:   Current Boost Map (1-8)
   Byte 5:   Current E-Throttle Map (1-8)
   Byte 6:   Launch Control Status (bit 0) + Anti-Lag Status (bit 1)
   Byte 7:   Reserved
   ```

### Step 4: Save Configuration

1. **File → Stream → Export** → Save as `Dashboard_Streams.lcs`
2. **File → ECU → Store to File** → Save complete tune
3. **Test communication** by checking stream transmission

---

## 🔌 Part 2: Hardware Wiring

### CAN Bus Wiring Diagram

```
Link G4X ECU          M5Stack Tab5
┌─────────────┐      ┌─────────────┐
│   CAN H     │──────│   GPIO 27   │
│   CAN L     │──────│   GPIO 26   │  
│   GND       │──────│   GND       │
└─────────────┘      └─────────────┘
      │                      │
   120Ω Term              120Ω Term
```

### Wiring Instructions

1. **Use twisted pair cable** (CAT5/6 or dedicated CAN cable)
2. **Connect CAN H to CAN H** (typically white/orange wire)
3. **Connect CAN L to CAN L** (typically orange wire)
4. **Connect grounds together**
5. **Install 120Ω termination resistors** at both ends
6. **Keep cable length under 5 meters** for reliability

### Pin Locations

**Link G4X ECU:**
- Check your ECU manual for CAN pin locations
- Usually on main connector or dedicated CAN connector

**M5Stack Tab5:**
- GPIO 26: CAN L (Low)
- GPIO 27: CAN H (High)
- GND: Any ground pin

---

## 📱 Part 3: Dashboard Configuration

### Step 1: Power On Dashboard

1. **Connect power** to M5Stack Tab5
2. **Wait for boot** (shows Link G4X logo)
3. **Touch screen** to activate

### Step 2: Access Configuration

1. **Touch the gear icon** (bottom right)
2. **Navigate to "BASIC" tab**
3. **Check current settings:**
   - Data Source: Should show "SIMULATION" initially
   - Stream Type: Should show "CUSTOM"

### Step 3: Switch to Live CAN Data

1. **Touch "DATA SOURCE" section**
2. **Toggle from "SIMULATION" to "LIVE CAN"**
3. **Verify "STREAM TYPE" is set to "CUSTOM"**
4. **Exit configuration** by touching "EXIT"

---

## ✅ Part 4: Testing & Verification

### Initial Communication Test

1. **Start your engine** (or turn on ECU)
2. **Check dashboard display:**
   - RPM should show actual engine RPM
   - TPS should respond to throttle input
   - ECT should show actual coolant temperature
   - Ethanol % should show flex fuel sensor reading

### Parameter Verification

**Expected Values:**
- **RPM:** 800-1000 at idle, responds to throttle
- **TPS:** 0% at idle, 100% at full throttle
- **ECT:** Actual coolant temperature (80-95°C typical)
- **Lambda:** 0.85-1.10 range (depends on tune)
- **Ethanol %:** Your actual fuel blend (E85 = ~85%)

### Troubleshooting

**No Data Displayed:**
- [ ] Check CAN wiring connections
- [ ] Verify ECU is transmitting (use PCLink CAN monitor)
- [ ] Confirm dashboard is in "LIVE CAN" mode
- [ ] Check CAN bus speed matches (1 Mbps)

**Incorrect Values:**
- [ ] Verify parameter scaling in PCLink
- [ ] Check byte order (should be Little Endian)
- [ ] Confirm CAN IDs match (0x500, 0x501, 0x502)

**Intermittent Connection:**
- [ ] Check termination resistors (120Ω at each end)
- [ ] Verify twisted pair cable quality
- [ ] Ensure cable length <5 meters

---

## 🎛️ Part 5: Advanced Features

### Ethanol Content Monitoring

The dashboard now displays ethanol percentage instead of gear position:
- **Location:** Bottom row, rightmost gauge
- **Color:** Purple accent
- **Range:** 0-100% (E0 to E100)
- **Source:** Flex fuel sensor via CAN

### Lambda Target Comparison

- **Actual Lambda:** Large gauge (top row)
- **Target Lambda:** Displayed within lambda gauge
- **Color coding:** Green = good, Yellow = lean, Red = rich

### Real-time Injector Duty

- **Critical for monitoring:** Injector saturation
- **Warning threshold:** >85% duty cycle
- **Helps prevent:** Lean conditions at high load

---

## 📄 Part 6: Configuration Files

### Save Your Settings

**PCLink Files to Save:**
1. `Dashboard_Streams.lcs` - Custom stream configuration
2. `Complete_Tune_with_Dashboard.pclr` - Full ECU tune
3. `CAN_Setup_Backup.txt` - Notes on your configuration

### Sharing Configuration

If sharing with others, provide:
- ECU model and firmware version
- CAN channel used (CAN 1 or CAN 2)
- Any custom math channels created
- Sensor calibrations used

---

## 🔄 Part 7: Maintenance

### Regular Checks

- **Monthly:** Verify CAN communication
- **Before events:** Test all parameters
- **After tune changes:** Confirm stream compatibility

### Updates

- **Dashboard firmware:** Check for updates periodically
- **PCLink software:** Keep current for best compatibility
- **ECU firmware:** Update as recommended by Link

---

**✅ Setup Complete!**

Your Link G4X ECU is now configured to send real-time data to the M5Stack dashboard. The system will display live engine parameters including the new ethanol percentage gauge for flex fuel monitoring.

For support or questions, refer to the troubleshooting section or consult the Link ECU documentation.
