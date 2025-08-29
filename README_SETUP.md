# Link G4X Dashboard - Complete Setup Guide
## M5Stack Tab5 Real-Time Engine Monitoring

### 🎯 What You Get

**Real-Time Engine Monitoring:**
- RPM, TPS, Boost Pressure, Lambda (actual vs target)
- Engine temperatures (ECT, IAT)
- Pressures (Oil, Fuel)
- **Ethanol Percentage** (replaces gear display)
- Battery voltage and injector duty cycle

**Professional Dashboard Features:**
- Automotive-grade gauge layout
- Color-coded warnings (redline, lean conditions)
- Touch interface for configuration
- Simulation mode for testing
- Japanese styling with English labels

---

## 🚀 Quick Start (30 Minutes)

### Step 1: Flash Dashboard Firmware ✅ COMPLETE
The dashboard firmware is already built and uploaded with:
- ✅ Fixed CAN data wiring (live vs simulation)
- ✅ Ethanol gauge replacing gear gauge
- ✅ Corrected CAN bus speed (1 Mbps)
- ✅ Optimized custom stream parsing

### Step 2: Configure Your Link G4X ECU

**Follow the detailed guide:** [`docs/Link_G4X_ECU_Setup_Guide.md`](docs/Link_G4X_ECU_Setup_Guide.md)

**Quick Summary:**
1. **Backup your tune** (always do this first!)
2. **Set CAN speed** to 1000 kbps (1 Mbps)
3. **Create 3 custom streams:**
   - 0x500: Primary engine data (RPM, TPS, ECT, etc.)
   - 0x501: Lambda & fuel data (includes ethanol %)
   - 0x502: Pressures & status

### Step 3: Wire CAN Bus

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

### Step 4: Switch to Live Mode

1. **Touch gear icon** on dashboard
2. **Change "DATA SOURCE"** from "SIMULATION" to "LIVE CAN"
3. **Verify "STREAM TYPE"** is set to "CUSTOM"
4. **Exit configuration**

---

## 📚 Documentation

### Essential Guides
- **[ECU Setup Guide](docs/Link_G4X_ECU_Setup_Guide.md)** - Complete PCLink configuration
- **[CAN Frame Reference](docs/CAN_Frame_Reference.md)** - Technical specifications
- **[Troubleshooting Guide](docs/Troubleshooting_Guide.md)** - Common issues and solutions

### Technical References
- **[Custom Stream Configuration](docs/Custom_Stream_Configuration.md)** - Advanced configuration
- **[Implementation Summary](docs/Custom_Stream_Implementation_Summary.md)** - Code details

---

## 🔧 Current Configuration

### Dashboard Settings
```
CAN Bus Speed: 1000 kbps (1 Mbps)
Data Source: LIVE CAN (toggle in config)
Stream Type: CUSTOM (Link G4X specific)
GPIO Pins: 26 (CAN L), 27 (CAN H)
```

### CAN Frame Layout
```
0x500 (20Hz): RPM, TPS, APS, ECT, MAP, Battery
0x501 (20Hz): Lambda, Lambda Target, Duty, Ethanol%, Battery  
0x502 (10Hz): Oil Press, Fuel Press, Boost Map, Status
```

### Gauge Layout
```
Top Row:    [RPM - Large]     [Lambda - Large]
Middle Row: [TPS] [Boost] [IAT] [ECT]
Bottom Row: [Oil] [Fuel] [Batt] [Speed] [Ethanol%]
```

---

## ✅ What's Fixed

### CAN Data Issues ✅
- **Problem:** Dashboard used simulation data even in "LIVE CAN" mode
- **Solution:** Fixed data source logic throughout codebase
- **Result:** Live CAN mode now uses real ECU data

### Ethanol Gauge ✅  
- **Change:** Replaced gear gauge with ethanol percentage
- **Benefit:** More useful for flex fuel tuning
- **Data Source:** CAN frame 0x501, byte 5 (flex fuel sensor)

### Configuration Corrections ✅
- **CAN Speed:** Fixed from 500 kbps to 1000 kbps (matches documentation)
- **Parsing:** Corrected MGP calculation and scaling factors
- **Documentation:** Created comprehensive setup guides

---

## 🎛️ Dashboard Features

### Real-Time Monitoring
- **RPM:** Large gauge with redline warning (>6500 RPM)
- **Lambda:** Actual vs target comparison with color coding
- **Boost:** Real-time manifold pressure with target display
- **Ethanol:** Flex fuel percentage for E85 monitoring

### Configuration Interface
- **Touch Controls:** Tap gear icon for settings
- **Data Source Toggle:** Switch between simulation and live CAN
- **Stream Type:** Choose between custom and Haltech IC7
- **Units:** Metric (°C, kPa) or Imperial (°F, PSI)

### Safety Features
- **Redline Warning:** Visual alerts for high RPM
- **Lean Condition:** Lambda gauge color changes
- **Communication Loss:** Fallback to last known values
- **Simulation Mode:** Safe testing without ECU

---

## 🔍 Troubleshooting Quick Reference

### No Data Displayed
1. Check dashboard is in "LIVE CAN" mode
2. Verify ECU is transmitting (PCLink CAN monitor)
3. Check CAN wiring (H-to-H, L-to-L)
4. Confirm 120Ω termination at both ends

### Wrong Values
1. Verify parameter scaling in PCLink
2. Check byte order (Little Endian)
3. Confirm CAN IDs match (0x500, 0x501, 0x502)

### Ethanol Shows 0%
1. Check flex fuel sensor connection
2. Verify sensor calibration in PCLink
3. Confirm ethanol parameter in frame 0x501, byte 5

**Full troubleshooting:** [`docs/Troubleshooting_Guide.md`](docs/Troubleshooting_Guide.md)

---

## 🛠️ Development

### Build System
```bash
./build.sh build    # Compile firmware
./build.sh upload   # Upload to device
./build.sh monitor  # View serial debug
```

### Code Structure
```
src/main.cpp              # Main dashboard code
docs/                     # Setup and reference guides
platformio.ini            # Build configuration
build.sh                  # Build automation
```

### Memory Usage
- **RAM:** 5.8% (29,640 / 512,000 bytes)
- **Flash:** 44.5% (583,877 / 1,310,720 bytes)
- **Performance:** Smooth 20Hz updates

---

## 📞 Support

### Before Asking for Help
1. **Read the setup guide** - Most issues are configuration-related
2. **Check troubleshooting guide** - Common problems and solutions
3. **Collect information:**
   - ECU model and firmware version
   - PCLink stream configuration
   - CAN monitor screenshots
   - Dashboard configuration settings

### Debug Tools
- **PCLink CAN Monitor:** View transmitted frames
- **Dashboard Serial Output:** Debug CAN reception
- **Multimeter:** Check CAN bus electrical levels

---

## 🎉 Ready to Use!

Your Link G4X dashboard is now configured with:
- ✅ **Real CAN data integration** (no more simulation bugs)
- ✅ **Ethanol percentage monitoring** (perfect for flex fuel)
- ✅ **Professional gauge layout** (automotive-grade display)
- ✅ **Comprehensive documentation** (easy ECU setup)

**Next Steps:**
1. Configure your ECU using the setup guide
2. Wire the CAN bus between ECU and dashboard  
3. Switch to live mode and enjoy real-time monitoring!

The dashboard will display live engine data including ethanol content for optimal flex fuel tuning and monitoring.
