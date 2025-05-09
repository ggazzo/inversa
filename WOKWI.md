# Running Inversa Project in Wokwi

This guide explains how to run and test the Inversa project using the Wokwi emulator.

## Prerequisites

1. A web browser (Chrome, Firefox, or Edge recommended)
2. Access to [Wokwi](https://wokwi.com)

## Setup Instructions

1. Clone this repository to your local machine
2. Open [Wokwi](https://wokwi.com)
3. Click on "New Project" and select "Import from GitHub"
4. Enter the repository URL
5. Select the `wokwi.toml` file as the configuration file

## Circuit Components

The emulator includes the following components:

- ESP32-S3 Mini board
- NTC Temperature Sensor (connected to A3)
- 100kΩ Resistor (for temperature sensor)
- LED with 220Ω resistor (connected to pin 2)
- Power connections (3.3V and GND)

## Pin Configuration

- A3: Temperature sensor input
- Pin 2: Heater control output
- Pin 5: SD card CS
- Pin 0: SD card MISO
- Pin 4: SD card MOSI
- Pin 1: SD card SCK
- Pin 21: TX
- Pin 20: RX
- Pin 8: SDA (I2C)
- Pin 10: SCL (I2C)
- Pin 7: OneWire
- Pin 6: Free

## Testing

1. Click the "Start Simulation" button in Wokwi
2. The ESP32 will boot and connect to WiFi
3. You can monitor the serial output in the Serial Monitor
4. The temperature sensor can be adjusted using the slider in the emulator
5. The LED will indicate the heater status

## Limitations

- WiFi functionality is simulated but limited
- SD card operations are not fully simulated
- Some timing-sensitive operations may behave differently than on real hardware

## Troubleshooting

If you encounter issues:

1. Check the serial monitor for error messages
2. Verify all connections in the diagram
3. Ensure the correct firmware is selected
4. Try resetting the simulation

## Contributing

If you find any issues with the Wokwi configuration or want to improve it:

1. Fork the repository
2. Make your changes
3. Submit a pull request

## License

This project is licensed under the same terms as the main Inversa project.
