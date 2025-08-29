# Development Setup Guide

This guide will help you set up your local development environment for the Link G4X Dashboard project.

## ✅ System Requirements

- **Linux** (Debian/Ubuntu based - already configured)
- **Python 3.11+** ✅ (installed)
- **Git** ✅ (installed)
- **USB access** for device programming

## 🚀 Quick Start

### 1. Initial Setup (One-time)

The project is already configured with a Python virtual environment and PlatformIO. You can use the convenient build script:

```bash
# Make sure you're in the project directory
cd /home/adam/code/link-g4x-dashboard

# The environment is already set up, but you can run this to verify:
./build.sh setup
```

### 2. Build the Project

```bash
# Compile the firmware
./build.sh build
```

### 3. Upload to Device (when M5Stack Tab5 is connected)

```bash
# Upload firmware to the device
./build.sh upload
```

### 4. Monitor Serial Output

```bash
# View real-time serial output from the device
./build.sh monitor
```

## 🛠️ Development Tools

### Build Script Commands

The `build.sh` script provides convenient commands:

- `./build.sh setup` - Set up development environment
- `./build.sh build` - Compile the project
- `./build.sh upload` - Upload firmware to device
- `./build.sh monitor` - Start serial monitor
- `./build.sh clean` - Clean build files
- `./build.sh deps` - Install/update dependencies
- `./build.sh help` - Show help

### Manual PlatformIO Commands

If you prefer to use PlatformIO directly:

```bash
# Activate the virtual environment
source .venv/bin/activate

# Build
pio run --environment esp32p4_pioarduino

# Upload
pio run --environment esp32p4_pioarduino --target upload

# Monitor
pio device monitor --environment esp32p4_pioarduino

# Clean
pio run --target clean
```

### VS Code Integration

The project includes VS Code configuration files:
- `.vscode/c_cpp_properties.json` - C++ IntelliSense configuration
- `.vscode/extensions.json` - Recommended extensions
- `.vscode/launch.json` - Debug configuration

Recommended VS Code extensions:
- PlatformIO IDE
- C/C++ Extension Pack
- ESP-IDF

## 📁 Project Structure

```
link-g4x-dashboard/
├── src/
│   └── main.cpp              # Main application (3000+ lines)
├── .venv/                    # Python virtual environment
├── .pio/                     # PlatformIO build files
├── .vscode/                  # VS Code configuration
├── docs/                     # Documentation
├── platformio.ini            # PlatformIO configuration
├── build.sh                  # Build script
└── README.md                 # Project documentation
```

## 🔧 Hardware Setup

### M5Stack Tab5 Connection

1. **USB Connection**: Connect Tab5 via USB-C for programming
2. **Power**: 6-24V DC via ExtPort2 (automotive use)
3. **CAN Bus**: Connect to Link G4X ECU via ExtPort2

### ExtPort2 Pinout (Tab5)
```
Pin | Function | Description
----|----------|-------------
 1  | GND      | Ground reference
 2  | VIN      | Power input (6-24V DC)
 3  | CAN_H    | CAN High signal
 4  | CAN_L    | CAN Low signal
```

## 🐛 Troubleshooting

### Build Issues

1. **Permission errors**: Make sure `build.sh` is executable (`chmod +x build.sh`)
2. **Python environment**: Ensure virtual environment is activated
3. **Dependencies**: Run `./build.sh deps` to update dependencies

### Upload Issues

1. **Device not found**: Check USB connection and drivers
2. **Permission denied**: Add user to dialout group: `sudo usermod -a -G dialout $USER`
3. **Port busy**: Close serial monitor before uploading

### Memory Issues

Current memory usage (successful build):
- **RAM**: 5.8% (29.6KB of 512KB)
- **Flash**: 44.4% (582KB of 1.3MB)

## 📊 Performance Metrics

The dashboard achieves:
- **Display Update Rate**: 100ms (10Hz)
- **Touch Response**: <50ms
- **CAN Reception**: 600+ frames per 5 seconds
- **Memory Efficiency**: <30KB RAM usage

## 🔄 Development Workflow

1. **Edit code** in `src/main.cpp`
2. **Build** with `./build.sh build`
3. **Test** with simulation mode (default)
4. **Upload** to device with `./build.sh upload`
5. **Monitor** output with `./build.sh monitor`

## 📝 Notes

- The project starts in **simulation mode** by default for safe testing
- **Real CAN mode** requires actual Link G4X ECU connection
- All **configuration is persistent** and saved to flash memory
- The system includes **comprehensive error handling** and fallback modes
