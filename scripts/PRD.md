# Inversa Single Vessel Brewery - Product Requirements Document

## Overview

The Inversa is an advanced, IoT-enabled single vessel brewing system designed for precise temperature control and automation of the brewing process. It combines PID temperature control, real-time monitoring, and remote control capabilities to provide a robust and flexible brewing platform. The system is specifically designed for homebrewers and small-scale commercial brewers who want precise control over their brewing process while maintaining simplicity and reliability.

## Core Features

### 1. Temperature Control System

- **What it does**: Provides precise temperature control for mashing, boiling, and cooling
- **Why it's important**: Ensures consistent brewing results and proper enzyme activity
- **How it works**:
  - Uses PID control algorithm with configurable parameters
  - Supports auto-tuning for different vessel sizes and heating elements
  - Implements hysteresis control to prevent rapid cycling
  - Calculates minimum power requirements based on wort volume and vessel characteristics
  - Maintains precise temperature control during:
    - Mash-in and protein rest
    - Saccharification rest
    - Mash-out
    - Boiling
    - Whirlpool
    - Cooling

### 2. Multi-Interface Control

- **What it does**: Enables control through multiple interfaces (Serial, BLE, WebSocket)
- **Why it's important**: Provides flexibility in how brewers interact with the system
- **How it works**:
  - Serial interface (9600 baud) for direct control
  - BLE interface (appears as "ESP32-BLE-Slave") for mobile control
  - WebSocket interface for web-based control
  - Command-based control system with JSON responses
  - Support for brewing-specific commands and parameters

### 3. Time-Based Operations

- **What it does**: Supports scheduled temperature changes and timed operations
- **Why it's important**: Enables automated brewing sequences
- **How it works**:
  - Real-time clock synchronization
  - Support for relative and absolute time-based operations
  - Total time tracking and milestone recording
  - Timer-based state management
  - Pre-programmed brewing profiles:
    - Single-step infusion
    - Multi-step infusion
    - Decoction
    - Boil timing
    - Hop additions
    - Whirlpool timing

### 4. File-Based Command Sequences

- **What it does**: Allows storage and execution of brewing recipes and command sequences
- **Why it's important**: Enables repeatable brewing processes
- **How it works**:
  - SD card file system support
  - Recipe storage and retrieval
  - Brewing process logging
  - Command playback functionality
  - Recipe sharing capability
  - Brewing session recording

### 5. Network Connectivity

- **What it does**: Provides WiFi connectivity and access point functionality
- **Why it's important**: Enables remote monitoring and control of the brewing process
- **How it works**:
  - WiFi client and access point modes
  - Configurable SSID and password
  - OTA update support
  - WebSocket server for real-time communication
  - Remote monitoring of:
    - Current temperature
    - Target temperature
    - Time remaining
    - Process stage
    - Alarms and notifications

## User Experience

### User Personas

1. **Home Brewer**

   - Needs precise temperature control for consistent results
   - Values automation to reduce manual intervention
   - Wants to focus on recipe development
   - Needs reliable timing for hop additions
   - Uses basic to advanced brewing techniques

2. **Small-Scale Commercial Brewer**

   - Requires repeatable processes
   - Needs precise temperature control
   - Values automation for efficiency
   - Uses complex brewing schedules
   - Requires process documentation

3. **Brewing Developer**
   - Needs API access for custom applications
   - Requires customization options
   - Values documentation
   - Uses advanced features and automation
   - Develops custom brewing profiles

### Key User Flows

1. **Basic Brewing Process**

   - Set target temperature for mashing
   - Monitor current temperature
   - Adjust PID parameters if needed
   - Start/stop operation
   - Track time for each step
   - Monitor hop additions

2. **Scheduled Brewing**

   - Set target temperature
   - Configure timing for each step
   - Set hop addition times
   - Start operation
   - Monitor progress
   - Receive completion notification

3. **Recipe Execution**
   - Load recipe from SD card
   - Review brewing schedule
   - Execute recipe
   - Monitor progress
   - Review brewing logs
   - Save brewing session data

## Technical Architecture

### System Components

1. **Hardware Components**

   - ESP32 microcontroller
   - Temperature sensor (waterproof, food-grade)
   - Power control circuit (SSR-based)
   - SD card interface
   - Real-time clock
   - WiFi/BLE module
   - Brewing vessel interface
   - Heating element control

2. **Software Components**
   - State machine engine
   - PID control system
   - Command parser
   - File system manager
   - Network stack
   - WebSocket server
   - Brewing process manager

### Data Models

1. **State Management**

   - Current temperature
   - Target temperature
   - PID parameters
   - Operation status
   - Timer values
   - System configuration
   - Brewing stage
   - Hop addition schedule

2. **Command Structure**
   - Command type
   - Parameters
   - Response format
   - Error handling
   - Brewing-specific commands

### APIs and Integrations

1. **Control Interfaces**

   - Serial command interface
   - BLE command interface
   - WebSocket API
   - File-based command interface
   - Brewing recipe format

2. **External Services**
   - NTP time synchronization
   - OTA update system
   - WebSocket client support
   - Recipe sharing platform

## Development Roadmap

### Phase 1: Core Temperature Control

- Basic PID implementation
- Temperature sensor integration
- Power control circuit
- Basic command interface
- Serial communication
- Basic brewing modes

### Phase 2: Enhanced Control

- PID auto-tuning
- Hysteresis control
- Volume-based calculations
- Basic state machine
- Command sequence support
- Advanced brewing modes

### Phase 3: Connectivity

- WiFi integration
- BLE support
- WebSocket server
- OTA updates
- Network configuration
- Remote monitoring

### Phase 4: Advanced Features

- SD card support
- File-based operations
- Time-based scheduling
- Advanced state management
- Comprehensive logging
- Recipe management

### Phase 5: Production Features

- Error handling
- Recovery mechanisms
- Performance optimization
- Security features
- Production testing
- User interface improvements

## Logical Dependency Chain

1. **Foundation Layer**

   - Hardware initialization
   - Basic temperature reading
   - Power control
   - Serial communication
   - Basic brewing modes

2. **Control Layer**

   - PID implementation
   - State machine
   - Command parser
   - Basic error handling
   - Temperature profiles

3. **Connectivity Layer**

   - WiFi/BLE setup
   - WebSocket server
   - Network configuration
   - OTA support
   - Remote monitoring

4. **Storage Layer**

   - SD card interface
   - File system
   - Recipe storage
   - Logging system
   - Session recording

5. **Advanced Features**
   - Auto-tuning
   - Time-based operations
   - Advanced error handling
   - Performance optimization
   - Recipe sharing

## Risks and Mitigations

### Technical Risks

1. **Temperature Control Accuracy**

   - Risk: PID tuning may not be optimal for different vessel sizes
   - Mitigation: Implement auto-tuning and provide manual override

2. **Network Reliability**

   - Risk: WiFi/BLE connection may be unstable
   - Mitigation: Implement fallback modes and connection recovery

3. **Power Management**

   - Risk: Power control may be inaccurate
   - Mitigation: Implement safety limits and monitoring

4. **Storage Reliability**
   - Risk: SD card operations may fail
   - Mitigation: Implement error checking and recovery

### Resource Constraints

1. **Memory Usage**

   - Risk: Limited RAM for complex operations
   - Mitigation: Optimize memory usage and implement streaming

2. **Processing Power**
   - Risk: Complex calculations may impact performance
   - Mitigation: Optimize algorithms and implement task scheduling

## Appendix

### Technical Specifications

1. **Hardware Requirements**

   - CPU: 160MHz
   - RAM: PSRAM supported
   - Storage: SD card support
   - Communication: WiFi/BLE
   - Temperature Sensor: Waterproof, food-grade
   - Power Control: SSR-based

2. **Software Requirements**

   - PlatformIO environment
   - ESP32 Arduino core
   - Required libraries:
     - RTClib
     - AsyncTCP
     - ESPAsyncWebServer
     - ArduinoJson
     - NimBLE-Arduino

3. **Performance Targets**
   - Temperature control accuracy: ±0.1°C
   - Response time: < 1 second
   - Command processing: < 100ms
   - Network latency: < 200ms
   - Brewing process timing accuracy: ±1 second

### Command Reference

See `CONTROL_INTERFACE.md` for detailed command documentation.

### Brewing-Specific Features

- Support for common brewing temperature ranges (30°C - 100°C)
- Pre-programmed brewing profiles
- Hop addition timing system
- Whirlpool control
- Mash step management
- Boil control
- Cooling management
