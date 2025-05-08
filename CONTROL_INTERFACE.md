# Inversa Device Control Interface

## Communication Interfaces

- Serial (9600 baud)
- BLE Serial (device appears as "ESP32-BLE-Slave")
- WebSocket interface

## Core Control Commands

### Basic Operations

- `START` - Start the operation
- `ABORT` or `END` - Stop the operation
- `BREAK` - Skip current step
- `SYNC` - Get current state information

### Temperature Control

- `TEMPERATURE <value>` - Set target temperature
- `POWER <value>` - Set power in watts
- `VOLUME <value>` - Set volume in liters
- `PREPARE_RELATIVE <temp> <hours>` - Prepare temperature for a relative time
- `PREPARE_ABSOLUTE <temp> <datetime>` - Prepare temperature for a specific time

### PID Control

- `PID <kp> <ki> <kd> <pOn> <time>` - Set PID parameters
- `TUNING` - Start PID auto-tuning
- `HYSTERESIS_TEMP <value>` - Set temperature hysteresis
- `HYSTERESIS_TIME <value>` - Set time hysteresis

### File Operations (SD Card)

- `LIST_FILES` - List files on SD card
- `OPEN_FILE <filename>` - Open a file
- `WRITE_FILE <filename>` - Write to a file
- `FILE_STOP` - Stop file operations
- `DELETE_FILE <filename>` - Delete a file

### Network Configuration

- `SSID <value>` - Set WiFi SSID
- `PASSWORD <value>` - Set WiFi password
- Device creates AP: "Inversa" (password: "12345678")

### Time Management

- `SET_DATE <iso_date>` - Set RTC date/time
- `GET_TIME` - Get current time
- `TOTAL_TIME_START` - Start time counter
- `TOTAL_TIME_STOP` - Stop time counter
- `TOTAL_TIME_RESET` - Reset time counter
- `TOTAL_TIME_MILESTONE` - Record time milestone

### Status and Monitoring

- `GET_TEMP` - Get current and target temperature
- `SYNC` - Get comprehensive state information including:
  - Current temperature
  - Target temperature
  - Output value
  - Operation status
  - PID parameters
  - Volume and power settings
  - Current state
  - SD card status

## System Features

- Temperature control with PID
- Power management
- Volume-based calculations
- Time-based operations
- File-based command sequences
- Network connectivity (WiFi and BLE)
- Real-time clock synchronization
- SD card support for command sequences

## State Machine Architecture

The device uses a state machine for operation control, supporting various states:

- Idle
- Preparing
- Timer
- Confirmation
- Temperature control
- File operations

## Technical Specifications

- CPU Frequency: 160MHz
- PSRAM Support: Yes
- Filesystem: LittleFS
- Default Monitor Speed: 9600 baud
- Upload Speed: 921600 baud (OTA)
