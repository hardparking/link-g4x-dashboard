# Link G4X Custom Stream Configuration Guide
## Complete Setup for Custom Transmit and Receive Streams

### 📋 Overview

This guide covers the complete configuration of custom CAN streams for the Link G4X ECU, providing enhanced dashboard functionality with **lambda target**, **injector duty cycle**, **ethanol percentage**, and **bi-directional control** capabilities.

### 🚀 Custom Stream Benefits

**Enhanced Monitoring:**
- Lambda Target vs Actual comparison
- Real-time injector duty cycle monitoring
- Ethanol percentage display
- APS (Accelerator Position Sensor) monitoring
- Comprehensive pressure monitoring

**Dashboard Control Capabilities:**
- Boost map switching (8 maps)
- Boost target adjustment (+/- percentage)
- Launch control enable/disable
- Anti-lag system control
- E-throttle map switching
- Data logging control

### 🔧 Custom Transmit Stream Specification

#### **Frame 0x500 (1280 decimal) - Primary Engine Data - 20Hz**
```
Byte 0-1: RPM (0-8000 RPM, 1 RPM resolution, little endian)
Byte 2:   TPS (0-100%, 0.5% resolution)
Byte 3:   APS (0-100%, 0.5% resolution) 
Byte 4:   ECT (0-200°C, 1°C resolution, offset -40°C)
Byte 5-6: MAP (0-600 kPa, 0.1 kPa resolution, little endian)
Byte 7:   Battery Voltage (0-20V, 0.1V resolution)
```

#### **Frame 0x501 (1281 decimal) - Lambda & Fuel Data - 20Hz**
```
Byte 0-1: Lambda 1 (0.5-1.5, 0.001 resolution, little endian)
Byte 2-3: Lambda Target (0.5-1.5, 0.001 resolution, little endian)
Byte 4:   Injector Duty Cycle (0-100%, 0.5% resolution)
Byte 5:   Ethanol Percentage (0-100%, 1% resolution)
Byte 6-7: MGP/Boost (0-300 kPa, 0.1 kPa resolution, little endian)
```

#### **Frame 0x502 (1282 decimal) - Pressures & Status - 10Hz**
```
Byte 0-1: Oil Pressure (0-1000 kPa, 1 kPa resolution, little endian)
Byte 2-3: Fuel Pressure (0-1000 kPa, 1 kPa resolution, little endian)
Byte 4:   Current Boost Map (1-8)
Byte 5:   System Status Flags
Byte 6:   Launch Control Status
Byte 7:   Reserved
```

### 🎛️ Custom Receive Stream Specification

#### **Frame 0x600 (1536 decimal) - Dashboard Commands - On Demand**
```
Byte 0: Command Type
  0x01 = Boost Map Switch
  0x02 = Boost Level Adjust
  0x03 = E-Throttle Map Switch
  0x04 = Launch Control Toggle
  0x05 = Anti-Lag Toggle
  0x06 = Data Logging Control
  0x07 = System Reset
  0x08 = Safe Mode Activate

Byte 1: Command Value
  For Map Switch: Map Number (1-8)
  For Level Adjust: Percentage (50-200, represents 50%-200%)
  For Toggle: 0=Off, 1=On

Byte 2: Command Confirmation (0xAA = confirmed)
Byte 3: Reserved
Byte 4-7: Reserved for future expansion
```

### ⚙️ PCLink Configuration Steps

#### **Step 1: Configure Custom Transmit Stream**

1. **Open PCLink** and connect to Link G4X ECU
2. **Navigate to ECU Controls > CAN Setup**
3. **Select CAN Channel** (CAN 1 or CAN 2)
4. **Set Mode** to "User Defined"
5. **Configure Bit Rate** to 1000 kbps (1 Mbps)

**Frame 0x500 Configuration:**
1. **Click "Add Stream"**
2. **Set CAN ID** to 1280 (0x500)
3. **Set Transmission Rate** to 20 Hz
4. **Add Parameters:**
   - Byte 0-1: Engine Speed (RPM)
   - Byte 2: Throttle Position (TPS)
   - Byte 3: Accelerator Position (APS)
   - Byte 4: Engine Coolant Temp (ECT)
   - Byte 5-6: Manifold Absolute Pressure (MAP)
   - Byte 7: ECU Volts (Battery Voltage)

**Frame 0x501 Configuration:**
1. **Click "Add Stream"**
2. **Set CAN ID** to 1281 (0x501)
3. **Set Transmission Rate** to 20 Hz
4. **Add Parameters:**
   - Byte 0-1: Lambda 1
   - Byte 2-3: Lambda Target
   - Byte 4: Injector Duty Cycle
   - Byte 5: Ethanol Percentage
   - Byte 6-7: Manifold Gauge Pressure (MGP)

**Frame 0x502 Configuration:**
1. **Click "Add Stream"**
2. **Set CAN ID** to 1282 (0x502)
3. **Set Transmission Rate** to 10 Hz
4. **Add Parameters:**
   - Byte 0-1: Oil Pressure
   - Byte 2-3: Fuel Pressure
   - Byte 4: Current Boost Map Number
   - Byte 5: System Status
   - Byte 6: Launch Control Status
   - Byte 7: Reserved

#### **Step 2: Configure CAN Keypad (Receive Stream)**

1. **Navigate to ECU Tuning Functions > CAN > CAN Keypads**
2. **Enable CAN Keypad** on same CAN channel
3. **Set Receive CAN ID** to 1536 (0x600)
4. **Configure Keypad Functions:**

**Button Mapping:**
- **Button 1:** Boost Map +1 (cycles through maps 1-8)
- **Button 2:** Boost Map -1 (cycles through maps 1-8)
- **Button 3:** Boost Target +5% (increase boost target)
- **Button 4:** Boost Target -5% (decrease boost target)
- **Button 5:** Launch Control Toggle
- **Button 6:** Anti-Lag Toggle
- **Button 7:** E-Throttle Map Switch
- **Button 8:** Data Logging Start/Stop

#### **Step 3: Save Configuration**

1. **Apply all changes** to ECU
2. **Save configuration** to ECU memory
3. **Export stream configuration** as LCS files:
   - "Custom_Dashboard_Transmit.lcs"
   - "Custom_Dashboard_Receive.lcs"
4. **Test transmission** with CAN analyzer

### 🔍 Parameter Details

#### **Lambda Target Implementation**
- **Source:** AFR Target Table lookup result
- **Resolution:** 0.001 lambda units
- **Range:** 0.500 - 1.500 lambda
- **Usage:** Compare with Lambda 1 for mixture control feedback

#### **Injector Duty Cycle**
- **Source:** Real-time injector pulse width percentage
- **Resolution:** 0.5% increments
- **Range:** 0-100%
- **Critical:** Monitor for injector saturation (>85% warning)

#### **Ethanol Percentage**
- **Source:** Flex fuel sensor or calculated blend
- **Resolution:** 1% increments
- **Range:** 0-100% (E0 to E100)
- **Usage:** Fuel map switching and timing compensation

#### **APS (Accelerator Position Sensor)**
- **Source:** Driver pedal position
- **Resolution:** 0.5% increments
- **Range:** 0-100%
- **Usage:** E-throttle control and driver intent monitoring

### 🛡️ Safety Considerations

#### **Critical Safety Features**
- **Confirmation Required** for boost map changes
- **Parameter Limits** prevent dangerous settings
- **Safe Mode** returns to base calibration
- **Visual Warnings** for active overrides
- **Automatic Timeouts** for temporary adjustments

#### **Emergency Procedures**
- **Safe Mode Button:** Returns all maps to base settings
- **Emergency Stop:** Disables all dashboard overrides
- **Parameter Reset:** Restores factory boost/timing limits
- **Communication Loss:** ECU reverts to base maps automatically

### 📊 Dashboard Integration

The Tab5 dashboard will provide:

#### **Monitoring Pages**
- **Lambda Comparison:** Target vs Actual with error display
- **Fuel System:** Duty cycle, ethanol %, fuel pressure
- **Boost Control:** Current map, target, actual pressure
- **System Status:** All active overrides and warnings

#### **Control Pages**
- **Boost Control:** Map selection and target adjustment
- **Launch Control:** Enable/disable and RPM setting
- **System Control:** Logging, diagnostics, safe mode

### 🔧 Troubleshooting

#### **No Transmit Data**
- Verify CAN IDs match (0x500, 0x501, 0x502)
- Check transmission rates (20Hz, 20Hz, 10Hz)
- Confirm parameter assignments in PCLink
- Verify CAN bus speed (1000 kbps)

#### **Receive Commands Not Working**
- Check CAN Keypad configuration
- Verify receive CAN ID (0x600)
- Confirm button mappings in PCLink
- Test with CAN analyzer

#### **Parameter Values Invalid**
- Check byte order (little endian)
- Verify scaling factors
- Confirm offset values (ECT -40°C)
- Validate parameter ranges

### 📋 Detailed PCLink Configuration Walkthrough

#### **Prerequisites**
- Link G4X ECU connected to PC via USB
- PCLink software installed (latest version recommended)
- ECU powered and communicating
- CAN bus properly wired and terminated

#### **Step-by-Step Custom Stream Creation**

##### **Phase 1: Backup Current Configuration**
1. **Connect to ECU** using PCLink
2. **File > ECU > Store to File** - Save current tune as backup
3. **Note current CAN settings** for reference

##### **Phase 2: Configure Custom Transmit Stream**

**Step 1: Access CAN Setup**
1. **ECU Controls > CAN Setup**
2. **Select CAN Channel** (CAN 1 or CAN 2)
3. **Set Mode** to "User Defined"
4. **Set Bit Rate** to 1000 kbps (1 Mbps)

**Step 2: Create Frame 0x500 (Primary Engine Data)**
1. **Click "Streams" tab**
2. **Click "Add Stream"**
3. **Configure Stream Properties:**
   - **CAN ID:** 1280 (decimal) or 0x500 (hex)
   - **Transmission Rate:** 20 Hz
   - **Data Length:** 8 bytes
   - **Byte Order:** Little Endian (Intel)

4. **Assign Parameters to Bytes:**
   - **Byte 0-1:** Engine Speed (RPM) - 16-bit little endian
   - **Byte 2:** Throttle Position (TPS) - Scale: × 2 (0.5% resolution)
   - **Byte 3:** Accelerator Position (APS) - Scale: × 2 (0.5% resolution)
   - **Byte 4:** Engine Coolant Temp (ECT) - Offset: +40°C
   - **Byte 5-6:** Manifold Absolute Pressure (MAP) - Scale: × 10, 16-bit little endian
   - **Byte 7:** ECU Volts (Battery) - Scale: × 10 (0.1V resolution)

**Step 3: Create Frame 0x501 (Lambda & Fuel Data)**
1. **Click "Add Stream"**
2. **Configure Stream Properties:**
   - **CAN ID:** 1281 (decimal) or 0x501 (hex)
   - **Transmission Rate:** 20 Hz
   - **Data Length:** 8 bytes
   - **Byte Order:** Little Endian (Intel)

3. **Assign Parameters to Bytes:**
   - **Byte 0-1:** Lambda 1 - Scale: × 1000, 16-bit little endian
   - **Byte 2-3:** Lambda Target - Scale: × 1000, 16-bit little endian
   - **Byte 4:** Injector Duty Cycle - Scale: × 2 (0.5% resolution)
   - **Byte 5:** Ethanol Percentage - Direct value (1% resolution)
   - **Byte 6-7:** Manifold Gauge Pressure (MGP) - Scale: × 10, 16-bit little endian

**Step 4: Create Frame 0x502 (Pressures & Status)**
1. **Click "Add Stream"**
2. **Configure Stream Properties:**
   - **CAN ID:** 1282 (decimal) or 0x502 (hex)
   - **Transmission Rate:** 10 Hz
   - **Data Length:** 8 bytes
   - **Byte Order:** Little Endian (Intel)

3. **Assign Parameters to Bytes:**
   - **Byte 0-1:** Oil Pressure - Direct value, 16-bit little endian
   - **Byte 2-3:** Fuel Pressure - Direct value, 16-bit little endian
   - **Byte 4:** Current Boost Map Number (1-8)
   - **Byte 5:** System Status Flags (bit field)
   - **Byte 6:** Launch Control Status (0=off, 1=on)
   - **Byte 7:** Reserved for future use

##### **Phase 3: Configure CAN Keypad (Receive Stream)**

**Step 1: Enable CAN Keypad**
1. **Navigate to ECU Tuning Functions > CAN > CAN Keypads**
2. **Enable CAN Keypad** checkbox
3. **Set CAN Channel** to same channel as transmit streams
4. **Set Receive CAN ID** to 1536 (decimal) or 0x600 (hex)

**Step 2: Configure Keypad Functions**
1. **Button 1:** Boost Map +1
   - **Function:** Math > Boost Map Number
   - **Action:** Increment (with wrap 1-8)
2. **Button 2:** Boost Map -1
   - **Function:** Math > Boost Map Number
   - **Action:** Decrement (with wrap 1-8)
3. **Button 3:** Boost Target +5%
   - **Function:** Boost Control > Target Adjustment
   - **Action:** Add 5% (limit 50-200%)
4. **Button 4:** Boost Target -5%
   - **Function:** Boost Control > Target Adjustment
   - **Action:** Subtract 5% (limit 50-200%)
5. **Button 5:** Launch Control Toggle
   - **Function:** Launch Control > Enable/Disable
   - **Action:** Toggle
6. **Button 6:** Anti-Lag Toggle
   - **Function:** Anti-Lag > Enable/Disable
   - **Action:** Toggle
7. **Button 7:** Data Logging Toggle
   - **Function:** Data Logging > Start/Stop
   - **Action:** Toggle
8. **Button 8:** Safe Mode Activate
   - **Function:** System > Safe Mode
   - **Action:** Activate (return to base maps)

##### **Phase 4: Parameter Mapping Details**

**Critical Parameter Sources:**
- **Lambda Target:** AFR Target Table lookup result
- **Injector Duty:** Real-time pulse width percentage
- **Ethanol Percentage:** Flex fuel sensor input or calculated blend
- **APS:** Accelerator pedal position sensor
- **Current Boost Map:** Active boost map number from ECU logic

**Scaling Calculations:**
```
RPM: Raw value (0-8000)
TPS/APS: Raw × 2 (0.5% resolution)
Temperatures: Raw + 40 (offset for negative values)
Pressures: Raw × 10 (0.1 unit resolution)
Lambda: Raw × 1000 (0.001 resolution)
```

##### **Phase 5: Testing and Validation**

**Step 1: Apply Configuration**
1. **Click "Apply"** to send configuration to ECU
2. **Click "OK"** to close CAN Setup
3. **File > ECU > Store** to save configuration to ECU memory

**Step 2: Test Transmission**
1. **Use CAN analyzer** or oscilloscope to verify transmission
2. **Check frame rates:** 20Hz for 0x500/0x501, 10Hz for 0x502
3. **Verify data integrity** with known engine states

**Step 3: Test Receive Commands**
1. **Send test CAN frame** 0x600 with command data
2. **Verify ECU responds** to boost map changes
3. **Test safety limits** (invalid commands should be rejected)

##### **Phase 6: Save and Document**

**Step 1: Export Configuration**
1. **File > Stream > Export** - Save as "Custom_Dashboard_Transmit.lcs"
2. **File > Keypad > Export** - Save as "Custom_Dashboard_Receive.lcs"
3. **File > ECU > Store to File** - Save complete tune with custom streams

**Step 2: Documentation**
1. **Record all CAN IDs** and their purposes
2. **Document parameter scaling** factors
3. **Note any ECU-specific requirements**
4. **Create troubleshooting checklist**

### 🔧 Advanced Configuration Options

#### **Custom Math Channels**
For parameters not directly available, create math channels:
```
Lambda Error = Lambda 1 - Lambda Target
Boost Error = MGP - Boost Target
Power Estimate = (RPM × MAP) / 1000
```

#### **Conditional Transmission**
Set up conditional logic for parameter transmission:
- **High-priority parameters:** Always transmit
- **Diagnostic data:** Only when engine running
- **Status flags:** Only when conditions change

#### **Error Handling**
Configure ECU behavior for communication loss:
- **Timeout period:** 5 seconds
- **Fallback action:** Return to base maps
- **Error logging:** Enable for diagnostics

---

**Next:** Tab5 dashboard implementation is complete and ready for use with custom streams.
