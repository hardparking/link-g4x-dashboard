# ExtPort2 Connector Documentation
## M5Stack Tab5 Built-in RS485 Interface for CAN Communication

### 🔌 Overview
The M5Stack Tab5 features a built-in **SIT3088 RS485 transceiver** that can be used for CAN bus communication. This eliminates the need for external CAN controllers and provides a single-cable solution for both power and data.

### 📋 Connector Specifications

#### **Physical Connector**
- **Type**: 4-pin automotive connector (Deutsch DT04-4P or equivalent)
- **Current Rating**: 5A minimum
- **Voltage Rating**: 30V minimum  
- **Environmental**: IP67 rated for automotive use
- **Mating Cycles**: 1000+ cycles

#### **Pin Assignment**
```
Pin | Function | Wire Color | Description
----|----------|------------|-------------
 1  | GND      | Black      | System ground reference
 2  | VIN      | Red        | Power input (6-24V DC)
 3  | CAN_H    | White      | CAN bus high signal
 4  | CAN_L    | Orange     | CAN bus low signal
```

### ⚡ Electrical Specifications

#### **Power Requirements (Pin 2: VIN)**
- **Input Voltage**: 6-24V DC
- **Typical Current**: 1.5A @ 12V
- **Peak Current**: 3A (display backlight + processing)
- **Power Consumption**: ~18W typical, 36W peak
- **Protection**: Reverse polarity protection
- **Recommended Fuse**: 5A automotive blade fuse

#### **CAN Bus Signals (Pins 3 & 4)**
- **Protocol**: ISO 11898 CAN 2.0B
- **Bit Rate**: Up to 1 Mbps
- **Voltage Levels**: 
  - CAN_H: 2.5V ± 2V
  - CAN_L: 2.5V ∓ 2V
- **Differential Voltage**: 2V typical
- **Input Impedance**: 120Ω (when termination enabled)
- **Common Mode Range**: -2V to +7V

### 🔧 Tab5 Internal Connections

#### **ESP32-P4 GPIO Mapping**
```
GPIO | Function | SIT3088 Pin | Description
-----|----------|-------------|-------------
G20  | TX       | DI          | Transmit data to transceiver
G21  | RX       | RO          | Receive data from transceiver
G34  | DIR      | DE/RE       | Direction control (TX/RX enable)
```

#### **SIT3088 Transceiver Features**
- **Galvanic Isolation**: 2.5kV isolation voltage
- **ESD Protection**: ±15kV HBM on bus pins
- **Temperature Range**: -40°C to +125°C
- **Switchable Termination**: Software-controlled 120Ω
- **Fault Detection**: Open/short circuit detection
- **Low EMI**: Integrated common-mode chokes

### 🚗 Vehicle Integration

#### **Power Connection**
```
Vehicle 12V System:
┌─────────────┐    ┌─────┐    ┌──────────┐    ┌─────────┐
│ Battery +12V│────│ 5A  │────│ Ignition │────│ Pin 2   │
│             │    │Fuse │    │ Switch   │    │ (VIN)   │
└─────────────┘    └─────┘    └──────────┘    └─────────┘

┌─────────────┐                               ┌─────────┐
│ Chassis GND │───────────────────────────────│ Pin 1   │
│             │                               │ (GND)   │
└─────────────┘                               └─────────┘
```

#### **CAN Bus Connection**
```
Link G4X ECU:                                 Tab5 ExtPort2:
┌─────────────┐                               ┌─────────┐
│ CAN High    │───────────────────────────────│ Pin 3   │
│             │                               │ (CAN_H) │
│ CAN Low     │───────────────────────────────│ Pin 4   │
│             │                               │ (CAN_L) │
└─────────────┘                               └─────────┘
```

### 🔌 Cable Assembly

#### **Recommended Cable**
- **Type**: 4-conductor automotive cable
- **CAN Pair**: 120Ω twisted pair (CAT5e or automotive CAN cable)
- **Power Wires**: 16 AWG stranded copper
- **Shield**: Overall foil shield with drain wire
- **Jacket**: Automotive PVC or TPE (-40°C to +105°C)
- **Length**: 2-5 meters maximum for dashboard installation

#### **Wire Specifications**
```
Function | AWG  | Stranding | Insulation | Color
---------|------|-----------|------------|--------
VIN      | 16   | 19/29     | PVC        | Red
GND      | 16   | 19/29     | PVC        | Black  
CAN_H    | 24   | 7/32      | PVC        | White
CAN_L    | 24   | 7/32      | PVC        | Orange
Shield   | 24   | 7/32      | Bare       | Bare
```

#### **Connector Assembly**
1. **Strip cable jacket** 25mm from end
2. **Separate twisted pairs** - keep CAN pair twisted
3. **Strip individual wires** 6mm from end
4. **Crimp terminals** using proper crimping tool
5. **Insert into connector** per pin assignment
6. **Install strain relief** and connector boot
7. **Test continuity** before installation

### ⚙️ Configuration & Setup

#### **CAN Bus Termination**
- **Tab5 Termination**: Software-controlled 120Ω resistor
- **Enable when**: Tab5 is at end of CAN bus
- **Disable when**: Tab5 is in middle of CAN bus
- **Total Bus Resistance**: Should measure 60Ω (two 120Ω in parallel)

#### **ECU Configuration (Link G4X)**
```
Parameter          | Setting
-------------------|----------
CAN Stream         | Dash2Pro
CAN ID             | 864 (decimal)
Bit Rate           | 1000 kbps
Frame Format       | Standard (11-bit)
Data Length        | 8 bytes
Transmission Rate  | 10 Hz (100ms)
```

#### **Tab5 Software Configuration**
- **CAN ID**: 864 (default) or custom
- **Bit Rate**: 1000 kbps (1 Mbps)
- **Mode**: Real CAN (not simulation)
- **Units**: Metric or Imperial as preferred

### 🔍 Troubleshooting

#### **Power Issues**
- **No Display**: Check Pin 1 (GND) and Pin 2 (VIN) connections
- **Intermittent Operation**: Check fuse and voltage under load
- **Voltage Drop**: Use adequate wire gauge (16 AWG minimum)

#### **CAN Communication Issues**
- **No Data**: Verify CAN_H and CAN_L connections (Pins 3 & 4)
- **Intermittent Data**: Check twisted pair integrity
- **Bus Errors**: Verify termination settings (120Ω at each end)
- **Wrong Data**: Confirm ECU CAN ID matches Tab5 configuration

#### **Signal Quality**
- **Use oscilloscope** to verify CAN signal levels
- **Check for noise** on power and ground connections  
- **Verify cable routing** away from ignition and high-current wires
- **Ensure proper shielding** and grounding

### 📏 Mechanical Considerations

#### **Connector Mounting**
- **Panel Mount**: Use appropriate panel cutout for connector
- **Strain Relief**: Adequate cable strain relief required
- **Sealing**: IP67 sealing for harsh environments
- **Accessibility**: Easy access for maintenance and troubleshooting

#### **Installation Location**
- **Dashboard Mount**: Secure mounting with vibration resistance
- **Cable Routing**: Protect cable from sharp edges and heat
- **Service Access**: Maintain access for connector disconnection
- **Environmental**: Protect from moisture and extreme temperatures

### ⚠️ Safety & Compliance

#### **Electrical Safety**
- **Fusing**: Always use appropriate fusing on power circuit
- **Isolation**: Maintain galvanic isolation between vehicle and Tab5
- **Grounding**: Proper grounding to prevent ground loops
- **Protection**: Reverse polarity and overvoltage protection

#### **Automotive Standards**
- **EMC Compliance**: Meets automotive EMC requirements
- **Temperature Rating**: -40°C to +85°C operating range
- **Vibration Resistance**: Automotive vibration specifications
- **Connector Standards**: SAE J1939 compatible physical layer

### 📋 Parts List

#### **Required Components**
```
Qty | Part Number        | Description
----|-------------------|----------------------------------
1   | DT04-4P           | 4-pin receptacle (panel mount)
1   | DT06-4S           | 4-pin plug (cable mount)  
4   | 0462-201-16141    | Socket contacts (16-20 AWG)
4   | 0460-202-16141    | Pin contacts (16-20 AWG)
1   | W4S               | Wedge lock (4-position)
1   | Custom            | 4-conductor CAN cable (2-5m)
```

#### **Optional Components**
- **Panel Mount Nut**: For secure panel mounting
- **Dust Cap**: Protection when disconnected
- **Cable Gland**: Additional strain relief
- **Ferrite Core**: EMI suppression if needed

---

**Document Version**: 1.0  
**Last Updated**: 2025-01-18  
**Compatible Hardware**: M5Stack Tab5 with SIT3088 RS485 interface
