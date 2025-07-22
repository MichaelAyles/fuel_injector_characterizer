# Fixing IntelliSense Errors for SD.h and SPI.h

## Problem
VS Code IntelliSense cannot find SD.h and SPI.h includes because PlatformIO hasn't downloaded the necessary framework files yet.

## Solution

### Step 1: Build the Project
The IntelliSense errors will be resolved after the first PlatformIO build:

1. Open VS Code in the project root directory
2. Open the PlatformIO extension (icon in the sidebar)
3. Click on "Build" under the teensy41 environment
   - Or use the shortcut: Ctrl+Alt+B
   - Or use the terminal: `pio run`

This will:
- Download the Teensy framework files
- Download the SD library dependency
- Create the `.pio` directory with all dependencies
- Generate proper IntelliSense configuration

### Step 2: Restart IntelliSense (if needed)
If errors persist after building:
1. Press Ctrl+Shift+P
2. Type "C/C++: Restart IntelliSense Server"
3. Press Enter

### Step 3: Alternative - Manual Installation
If you want to resolve IntelliSense before building:
```bash
# In the firmware directory
pio pkg install
```

## Project Structure
- `platformio.ini` - Configured for Teensy 4.1 with SD library
- `.vscode/c_cpp_properties.json` - IntelliSense configuration
- `.vscode/settings.json` - VS Code settings for PlatformIO

## Notes
- The SD and SPI libraries are part of the Teensy Arduino framework
- The SD library is explicitly listed in `lib_deps` in platformio.ini
- SPI is included automatically with the Arduino framework
- After the first build, all paths in c_cpp_properties.json will resolve correctly