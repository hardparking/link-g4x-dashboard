# ExtPort2 Wiring Diagram
## M5Stack Tab5 to Vehicle CAN Bus Connection

### 🔌 Complete System Wiring

```
Vehicle Side                           Tab5 ExtPort2 Connector
═══════════════                        ═══════════════════════

┌─────────────────┐                    ┌─────────────────────┐
│   Battery +12V  │────────┐           │                     │
└─────────────────┘        │           │    M5Stack Tab5     │
                           │           │                     │
┌─────────────────┐        │           │  ┌───────────────┐  │
│     5A Fuse     │◄───────┤           │  │   SIT3088     │  │
└─────────────────┘        │           │  │  Transceiver  │  │
                           │           │  └───────────────┘  │
┌─────────────────┐        │           │                     │
│ Ignition Switch │◄───────┤           │                     │
│   (Switched)    │        │           │                     │
└─────────────────┘        │           │                     │
                           │           │                     │
                           ▼           │                     │
                    ┌─────────────┐    │  ┌─────────────┐    │
                    │    Pin 2    │◄───┼──┤    VIN      │    │
                    │   (VIN)     │    │  │  (6-24V)    │    │
                    └─────────────┘    │  └─────────────┘    │
                                       │                     │
┌─────────────────┐                    │  ┌─────────────┐    │
│ Chassis Ground  │────────────────────┼──┤    GND      │    │
└─────────────────┘                    │  │   (Pin 1)   │    │
                                       │  └─────────────┘    │
                                       │                     │
┌─────────────────┐                    │  ┌─────────────┐    │
│  Link G4X ECU   │                    │  │             │    │
│                 │                    │  │             │    │
│   CAN High  ────┼────────────────────┼──┤   CAN_H     │    │
│             │   │                    │  │  (Pin 3)    │    │
│   CAN Low   ────┼────────────────────┼──┤   CAN_L     │    │
│             │   │                    │  │  (Pin 4)    │    │
│             │   │                    │  └─────────────┘    │
│    120Ω     │   │                    │                     │
│ Termination │   │                    │     120Ω (SW)       │
│             │   │                    │   Termination       │
└─────────────────┘                    └─────────────────────┘
```

### 🔧 Connector Pin Details

#### **ExtPort2 4-Pin Connector (Tab5 Side)**
```
     ┌─────────────────┐
     │  ○ 1     2 ○    │  ← Looking at Tab5 connector
     │  ○ 3     4 ○    │
     └─────────────────┘

Pin 1: GND      (Black wire)   - System ground
Pin 2: VIN      (Red wire)     - Power input 6-24V
Pin 3: CAN_H    (White wire)   - CAN bus high signal  
Pin 4: CAN_L    (Orange wire)  - CAN bus low signal
```

#### **Cable Side Connector (Mating)**
```
     ┌─────────────────┐
     │  ○ 2     1 ○    │  ← Looking at cable connector
     │  ○ 4     3 ○    │     (mirror image)
     └─────────────────┘

Pin 1: GND      (Black wire)   - To vehicle ground
Pin 2: VIN      (Red wire)     - To switched 12V + fuse
Pin 3: CAN_H    (White wire)   - To ECU CAN High
Pin 4: CAN_L    (Orange wire)  - To ECU CAN Low
```

### ⚡ Power Circuit Detail

```
Vehicle Electrical System:

Battery     Fuse Box    Ignition     ExtPort2
┌─────┐    ┌───────┐   ┌────────┐   ┌────────┐
│ +12V│────│  5A   │───│ Switch │───│ Pin 2  │
│     │    │ Fuse  │   │        │   │ (VIN)  │
│     │    └───────┘   └────────┘   └────────┘
│     │                              
│ GND │──────────────────────────────┤ Pin 1  │
└─────┘                              │ (GND)  │
                                     └────────┘

Power Specifications:
- Input Voltage: 6-24V DC (12V nominal)
- Current Draw: 1.5A typical, 3A peak
- Fuse Rating: 5A automotive blade fuse
- Wire Gauge: 16 AWG minimum for power
```

### 🚌 CAN Bus Circuit Detail

```
CAN Bus Network:

ECU Side                              Tab5 Side
┌─────────────┐                      ┌─────────────┐
│ Link G4X    │                      │ M5Stack     │
│ ECU         │                      │ Tab5        │
│             │                      │             │
│ CAN_H ──────┼──────────────────────┼────── Pin 3 │
│       │     │   120Ω Twisted Pair  │       │     │
│ CAN_L ──────┼──────────────────────┼────── Pin 4 │
│       │     │                      │       │     │
│      120Ω   │                      │      120Ω   │
│   (Fixed)   │                      │ (Switchable)│
└─────────────┘                      └─────────────┘

CAN Bus Specifications:
- Protocol: ISO 11898 CAN 2.0B
- Bit Rate: 1 Mbps (1000 kbps)
- Cable: 120Ω twisted pair
- Termination: 120Ω at each end (60Ω total)
- Max Length: 40m at 1 Mbps
```

### 🔌 Cable Construction

#### **4-Conductor CAN Cable**
```
Cable Cross-Section View:

    ┌─────────────────────────────────┐
    │  ┌─────┐                       │ ← Overall Jacket
    │  │ Red │ ← VIN (16 AWG)        │
    │  └─────┘                       │
    │  ┌─────┐                       │
    │  │Black│ ← GND (16 AWG)        │
    │  └─────┘                       │
    │  ┌─────────────────┐           │
    │  │ ┌───┐     ┌───┐ │ ← Twisted │
    │  │ │Wht│     │Org│ │   Pair    │
    │  │ └───┘     └───┘ │ (24 AWG)  │
    │  └─────────────────┘           │
    │  ┌─────────────────────────────┐│
    │  │     Foil Shield + Drain     ││
    │  └─────────────────────────────┘│
    └─────────────────────────────────┘

Wire Specifications:
- VIN/GND: 16 AWG stranded (power)
- CAN_H/CAN_L: 24 AWG twisted pair (120Ω)
- Shield: Foil with drain wire
- Jacket: Automotive grade PVC/TPE
```

### 🔧 Installation Steps

#### **1. Vehicle Side Preparation**
```
Step 1: Install 5A fuse in fuse box
Step 2: Connect red wire to switched 12V (ignition controlled)
Step 3: Connect black wire to chassis ground
Step 4: Route CAN wires to ECU location
Step 5: Connect white wire to ECU CAN High
Step 6: Connect orange wire to ECU CAN Low
```

#### **2. Tab5 Side Connection**
```
Step 1: Assemble 4-pin connector with proper crimps
Step 2: Insert wires per pin assignment
Step 3: Install strain relief and connector boot
Step 4: Test continuity before connection
Step 5: Connect to Tab5 ExtPort2
Step 6: Verify power and CAN communication
```

### 📊 Signal Verification

#### **Power Verification**
```
Measurement Points:
- Pin 1 to Pin 2: 12V DC (ignition on)
- Pin 1 to chassis: 0V (good ground)
- Current draw: 1.5A typical

Troubleshooting:
- No voltage: Check fuse and ignition switch
- Low voltage: Check wire gauge and connections
- High current: Check for short circuits
```

#### **CAN Signal Verification**
```
Oscilloscope Measurements:
- CAN_H idle: ~2.5V
- CAN_L idle: ~2.5V  
- Differential: 0V idle, ±2V active
- Bit rate: 1 MHz (1 μs bit time)

Multimeter Measurements:
- Bus resistance: 60Ω (both terminators)
- Pin 3 to Pin 4: 60Ω (terminators enabled)
- No shorts to ground or power
```

### ⚠️ Safety Checklist

#### **Before Connection**
- [ ] Verify pin assignments match diagram
- [ ] Check wire continuity and insulation
- [ ] Confirm fuse rating (5A maximum)
- [ ] Test connector fit and retention
- [ ] Verify ECU CAN configuration

#### **After Connection**
- [ ] Measure supply voltage (6-24V range)
- [ ] Verify CAN bus resistance (60Ω)
- [ ] Check for proper CAN communication
- [ ] Monitor current draw (<3A peak)
- [ ] Test ignition on/off operation

---

**Document Version**: 1.0  
**Last Updated**: 2025-01-18  
**Compatible Hardware**: M5Stack Tab5 with SIT3088 RS485 interface
