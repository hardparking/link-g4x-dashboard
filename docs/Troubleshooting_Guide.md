# Link G4X Dashboard Troubleshooting Guide
## Common Issues and Solutions

### 🚨 Quick Diagnostic Checklist

**Before diving into detailed troubleshooting, check these basics:**

- [ ] ECU is powered and communicating with PCLink
- [ ] Dashboard shows "LIVE CAN" mode (not "SIMULATION")
- [ ] Stream type is set to "CUSTOM" (not "HALTECH IC7")
- [ ] CAN bus speed matches: 1 Mbps (1000 kbps)
- [ ] CAN wiring is correct: H-to-H, L-to-L
- [ ] Termination resistors installed (120Ω at each end)

---

## 🔍 Problem Categories

### 1. No Data Displayed (All Gauges Show Default Values)

**Symptoms:**
- Dashboard shows static values (RPM=2150, TPS=15%, etc.)
- No response to engine changes
- "LIVE CAN" mode selected but no live data

**Diagnostic Steps:**

**Step 1: Verify Dashboard Configuration**
```
1. Touch gear icon → Configuration
2. Check "DATA SOURCE" = "LIVE CAN" 
3. Check "STREAM TYPE" = "CUSTOM"
4. If wrong, touch to toggle and save
```

**Step 2: Check CAN Communication**
```
1. Connect laptop to ECU via USB
2. Open PCLink → Tools → CAN Monitor
3. Look for frames 0x500, 0x501, 0x502
4. Should see regular transmission (20Hz, 20Hz, 10Hz)
```

**Step 3: Verify Physical Wiring**
```
ECU Side          Dashboard Side
CAN H    ←────→   GPIO 27 (CAN H)
CAN L    ←────→   GPIO 26 (CAN L)  
GND      ←────→   GND
```

**Solutions:**
- **No frames in PCLink:** Configure custom streams (see setup guide)
- **Frames present but no dashboard data:** Check wiring
- **Wrong configuration:** Reset to "LIVE CAN" + "CUSTOM"

---

### 2. Incorrect Parameter Values

**Symptoms:**
- RPM shows wrong value (e.g., 21500 instead of 2150)
- Temperatures way off (e.g., -40°C when engine warm)
- Percentages >100% or negative values

**Common Causes & Fixes:**

**Wrong Scaling in PCLink:**
```
Parameter          Correct Scaling
RPM               ÷10 (or ×0.1)
TPS/APS           ×2 (or ×0.5)  
ECT               +40°C offset
Lambda            ×1000 (or ×0.001)
Pressures         ×10 (or ×0.1)
```

**Byte Order Issues:**
- **Symptom:** 16-bit values completely wrong
- **Fix:** Ensure "Little Endian" in PCLink stream config
- **Example:** RPM 2150 should be [0x40 0x08], not [0x08 0x40]

**Parameter Assignment Errors:**
- **Check:** Each parameter assigned to correct byte position
- **Verify:** Frame 0x500 byte assignments match code expectations

---

### 3. Intermittent Connection Issues

**Symptoms:**
- Data appears then disappears
- Some parameters update, others don't
- Random gauge freezing

**Diagnostic Steps:**

**Check Cable Quality:**
```
1. Use proper twisted pair cable (CAT5/6 or CAN cable)
2. Keep length under 5 meters
3. Avoid running parallel to power cables
4. Check for damaged/pinched wires
```

**Verify Termination:**
```
1. Measure resistance between CAN H and CAN L
2. Should read ~60Ω (two 120Ω resistors in parallel)
3. If wrong, check termination resistor installation
```

**Check Ground Connections:**
```
1. Ensure solid ground connection between ECU and dashboard
2. Use same ground reference point if possible
3. Check for ground loops or voltage differences
```

---

### 4. Ethanol Gauge Shows Wrong Values

**Symptoms:**
- Ethanol shows 0% when running E85
- Value doesn't change with fuel blend
- Shows impossible values (>100%)

**Solutions:**

**Check Flex Fuel Sensor:**
```
1. Verify sensor is connected to ECU
2. Check sensor calibration in PCLink
3. Confirm sensor is reading correctly in PCLink
```

**Verify CAN Stream Assignment:**
```
Frame 0x501, Byte 5: Ethanol Percentage
- Should be direct value (0-100)
- No scaling required
- Check parameter assignment in PCLink
```

**Test with Known Fuel:**
```
1. Fill with pump gas (E10) - should show ~10%
2. Fill with E85 - should show ~85%
3. If wrong, check sensor calibration
```

---

### 5. Lambda Values Incorrect

**Symptoms:**
- Lambda shows 0.000 or very high values
- No response to mixture changes
- Target vs actual don't make sense

**Check Lambda Sensor:**
```
1. Verify wideband sensor connected and calibrated
2. Check sensor heating (should reach operating temp)
3. Confirm sensor type matches ECU configuration
```

**Verify Scaling:**
```
Lambda values should be ×1000 in CAN stream
Example: 1.000 lambda = 1000 in CAN frame
Dashboard divides by 1000: 1000 × 0.001 = 1.000
```

**Check Target Lambda:**
```
1. Verify target lambda table exists in tune
2. Check that target is being calculated correctly
3. Confirm target transmission in Frame 0x501 bytes 2-3
```

---

### 6. Performance Issues

**Symptoms:**
- Slow gauge updates
- Dashboard lag or freezing
- Missing data points

**Optimize Transmission Rates:**
```
Critical data (0x500, 0x501): 20 Hz
Less critical (0x502): 10 Hz
Don't exceed these rates - causes bus congestion
```

**Check CAN Bus Load:**
```
1. Use PCLink CAN monitor to check bus utilization
2. Should be <50% for reliable operation
3. Reduce rates if bus overloaded
```

**Dashboard Performance:**
```
1. Ensure adequate power supply (5V, 2A minimum)
2. Check for overheating (dashboard throttles when hot)
3. Restart dashboard if performance degrades
```

---

## 🛠️ Advanced Diagnostics

### Using PCLink CAN Monitor

**Access:** Tools → CAN Monitor

**What to Look For:**
```
Frame 0x500: Should appear every 50ms (20 Hz)
Frame 0x501: Should appear every 50ms (20 Hz)  
Frame 0x502: Should appear every 100ms (10 Hz)
```

**Interpreting Data:**
```
Frame 0x500 example: [0x40 0x08 0x1E 0x20 0x7F 0x90 0x01 0x7D]
Byte 0-1: 0x0840 = 2112 → 2112 × 0.1 = 211.2 RPM ❌ (should be ÷10)
Correct: 0x0840 = 2112 → 2112 ÷ 10 = 211.2 RPM ✅
```

### Dashboard Serial Debug

**Enable Debug Output:**
```
1. Connect USB cable to dashboard
2. Open serial monitor (115200 baud)
3. Look for CAN frame reception messages
4. Check parameter parsing output
```

**Debug Output Example:**
```
CAN Frame Received: ID=0x500, Data=[40 08 1E 20 7F 90 01 7D]
Parsed: RPM=2112, TPS=15.0%, APS=16.0%, ECT=87°C
```

### Multimeter Testing

**CAN Bus Voltage Levels:**
```
CAN H: 2.5V idle, 3.5V dominant
CAN L: 2.5V idle, 1.5V dominant
Differential: 0V idle, 2V dominant
```

**Power Supply Check:**
```
Dashboard: 5V ±0.25V, 2A minimum
ECU: 12V ±1V (engine off), 13.8V (engine running)
```

---

## 📋 Step-by-Step Diagnostic Procedure

### When Nothing Works

**1. Verify Basic Setup (5 minutes)**
- [ ] Dashboard in "LIVE CAN" mode
- [ ] Stream type set to "CUSTOM"
- [ ] ECU powered and communicating with PCLink

**2. Check PCLink Configuration (10 minutes)**
- [ ] Custom streams configured (0x500, 0x501, 0x502)
- [ ] Transmission rates set (20Hz, 20Hz, 10Hz)
- [ ] Parameter assignments correct
- [ ] CAN speed = 1 Mbps

**3. Verify Physical Layer (15 minutes)**
- [ ] CAN wiring correct (H-to-H, L-to-L)
- [ ] Termination resistors installed (120Ω each end)
- [ ] Cable quality good (twisted pair, <5m)
- [ ] Ground connections solid

**4. Test Communication (10 minutes)**
- [ ] PCLink CAN monitor shows frames
- [ ] Frame rates correct (20/20/10 Hz)
- [ ] Data values reasonable
- [ ] No bus errors or timeouts

**5. Debug Dashboard (10 minutes)**
- [ ] Serial debug output shows frame reception
- [ ] Parameter parsing working correctly
- [ ] No error messages in debug output

---

## 🆘 Emergency Procedures

### Dashboard Not Responding
```
1. Power cycle dashboard (hold power button 10 seconds)
2. Check power supply (5V, adequate current)
3. Try different USB cable for power
4. Factory reset if necessary (contact support)
```

### ECU Communication Lost
```
1. Check USB cable to ECU
2. Restart PCLink software
3. Power cycle ECU (turn ignition off/on)
4. Verify ECU firmware compatibility
```

### CAN Bus Completely Dead
```
1. Disconnect dashboard from CAN bus
2. Check ECU still transmits (PCLink monitor)
3. Test dashboard CAN pins with multimeter
4. Replace CAN cable if damaged
5. Check for short circuits
```

---

## 📞 Getting Help

### Information to Collect Before Asking for Help

**Hardware:**
- ECU model and firmware version
- Dashboard firmware version
- CAN cable type and length
- Power supply specifications

**Configuration:**
- PCLink stream configuration (export .lcs files)
- Dashboard configuration screenshots
- CAN monitor screenshots showing frames

**Symptoms:**
- Exact behavior observed
- When problem started
- Any recent changes made
- Error messages (if any)

### Support Channels

**Documentation:**
- Setup guide: `Link_G4X_ECU_Setup_Guide.md`
- Frame reference: `CAN_Frame_Reference.md`
- Implementation details: `Custom_Stream_Configuration.md`

**Debug Tools:**
- PCLink CAN monitor
- Dashboard serial debug output
- Multimeter for electrical testing

---

**Remember:** Most issues are configuration-related, not hardware failures. Work through the checklist systematically and you'll find the problem!
