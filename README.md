# Fuel Injector Characterizer

A comprehensive system for testing and characterizing automotive fuel injectors, featuring a Teensy 4.1-based hardware controller and a web-based GUI using WebSerial API.

## Project Purpose

This system is designed to verify that fuel injector flow characteristics remain consistent across different drive voltages and control strategies. Specifically, it aims to:

- **Confirm flow consistency** between 12V static, 12V peak & hold, and 24V peak & hold drive modes
- **Enable safe 24V operation** of 12V-rated injectors using peak & hold control
- **Measure power consumption** to ensure injectors don't overheat when driven at higher voltages
- **Validate flow uniformity** by measuring actual fuel delivery into cylinders under different conditions

The key insight is that peak & hold control at 24V can deliver the same flow characteristics as 12V operation while preventing injector burnout through reduced average power consumption.

## Overview

This project provides a complete solution for testing fuel injectors with the following capabilities:
- Individual and sequential injector firing
- Normal pulse and Peak & Hold drive modes
- Real-time current measurement using ACS712 sensors
- High-speed data logging to SD card (10kHz sampling)
- Web-based GUI for control and monitoring
- JSON-based structured communication protocol

## System Components

### Hardware (Firmware)
- **Platform**: Teensy 4.1 microcontroller
- **Injector Drivers**: 4 independent channels with MOSFETs
- **Current Sensing**: ACS712 20A current sensors (one per channel)
- **Data Storage**: Built-in SD card for high-speed logging
- **Communication**: USB Serial at 115200 baud

### Software (GUI)
- **Technology**: HTML5, CSS3, JavaScript with WebSerial API
- **Browser Support**: Chrome, Edge, Opera (requires WebSerial support)
- **Features**: Real-time parameter display, current monitoring, and comprehensive controls

## Features

### Firing Modes
1. **Single Fire**: Fire individual injectors once
2. **Multi Fire**: Fire individual injectors 50 times
3. **Sequential**: Fire all injectors in sequence (1-2-3-4)
4. **All Fire**: Fire all injectors individually in one command

### Drive Modes
1. **Normal Pulse**: Full voltage for entire pulse duration
2. **Peak & Hold**: 
   - Peak phase: Full voltage for 2ms
   - Hold phase: PWM at 2kHz, 50% duty cycle for 1.8ms

### Current Monitoring
- Real-time current measurement for each injector
- Peak and average current calculation
- Automatic sensor calibration on startup
- Results displayed in GUI after each single fire
- Power consumption calculation for thermal analysis

### Data Logging
- 10kHz sampling rate to SD card
- CSV format with timestamps
- Records current and injector state for all channels
- File browser and viewer in firmware

## Installation

### Firmware Setup
1. Install Arduino IDE with Teensyduino
2. Open `firmware/src/main.cpp` in Arduino IDE
3. Select Tools > Board > Teensy 4.1
4. Upload to Teensy

### GUI Setup
1. Open `gui/index.html` in a WebSerial-compatible browser
2. No installation required - runs directly from file

## Usage

### Connecting
1. Open the GUI in your browser
2. Click "Connect to Device"
3. Select the Teensy serial port
4. Connection status will show "Connected"

### Basic Operation
1. **Set Pulse Width**: Use the input field or 'p' command (0.1-100ms)
2. **Fire Injectors**: Click buttons or use keyboard commands
3. **Monitor Results**: View peak/average current in results panel
4. **View Console**: See all communication in the console panel

### Command Reference

#### Single Fire Commands
- `1-4`: Fire injector 1-4 once (normal)
- `5`: Fire all 4 injectors individually
- `a-d`: Fire injector 1-4 once (peak & hold)

#### Multi Fire Commands (50x)
- `q-r`: Fire injector 1-4 (normal)
- `z-v`: Fire injector 1-4 (peak & hold)

#### Sequential Commands
- `t`: All injectors 50x (normal)
- `g`: All injectors 1x (peak & hold)
- `b`: All injectors 50x (peak & hold)

#### Configuration Commands
- `p`: Set pulse width
- `k`: Calibrate current sensors
- `l`: Toggle SD logging
- `m`: List/view log files
- `i`: Get status (JSON)
- `o`: Get sensor offsets
- `h`: Show help

## Communication Protocol

The system uses structured messages for reliable communication:

### Status Messages
```json
[STATUS]{"pulseWidth":20.0,"peakTime":2.0,"holdFreq":2000,"holdDuty":50,"sdAvailable":true,"logging":false,"offsets":[0.0,0.0,0.0,0.0]}
```

### Result Messages
```json
[RESULT]{"injector":1,"peakCurrent":2.45,"avgCurrent":1.82,"peakHold":false}
```

### Log Messages
```
[LOG]General information message
[ERROR]Error message
```

## Technical Specifications

### Timing Parameters
- Pulse Width: 0.1 - 100ms (configurable)
- Peak Time: 2ms (fixed)
- Hold Time: 1.8ms (fixed)
- Hold PWM: 2kHz, 50% duty cycle
- Sample Rate: 10kHz for logging

### Current Sensing
- Sensor: ACS712 20A
- Sensitivity: 66mV/A
- Reference: 1.65V (3.3V/2)
- Resolution: 10-bit ADC

### Performance
- Command Response: <1ms
- Current Sampling: 20kHz during measurement
- Data Logging: 10kHz continuous
- Serial Communication: 115200 baud

## File Structure
```
fuel_injector_characterizer/
├── firmware/
│   └── src/
│       └── main.cpp        # Teensy firmware
├── gui/
│   ├── index.html         # Web interface
│   ├── css/
│   │   └── style.css      # Styling
│   └── js/
│       └── webserial.js   # Serial communication
└── README.md             # This file
```

## Troubleshooting

### Connection Issues
- Ensure browser supports WebSerial API
- Check USB cable and connections
- Verify correct COM port selection

### Current Reading Issues
- Run calibration with no injectors powered
- Check sensor connections
- Verify 3.3V power to sensors

### SD Card Issues
- Format SD card as FAT32
- Check card is properly inserted
- Verify card capacity (<32GB recommended)

## Test Methodology

### Recommended Test Procedure
1. **Baseline at 12V Static**: Measure flow rate and power consumption
2. **12V Peak & Hold**: Verify flow remains consistent with reduced power
3. **24V Peak & Hold**: Confirm flow matches 12V while monitoring temperature
4. **Power Analysis**: Calculate average power to ensure thermal limits aren't exceeded

### Key Measurements
- **Flow Rate**: Measure actual fuel delivered per 1000 pulses
- **Peak Current**: Verify injector opening current
- **Average Current**: Calculate power consumption
- **Duty Cycle**: Ensure sustainable operation

## Safety Warnings
- Always use appropriate power supply for injectors
- Current sensors must be properly isolated
- Do not exceed injector duty cycle ratings
- Use flyback diodes on injector drivers
- **Monitor injector temperature** when testing at 24V
- **Start with short test cycles** when validating 24V operation

## License
This project is provided as-is for educational and testing purposes.

## Future Enhancements
- Waveform visualization in GUI
- Batch testing sequences
- Injector characterization profiles
- Export test reports
- Multi-injector comparison charts