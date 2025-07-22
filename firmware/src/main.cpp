#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// Injector driver outputs
const int INJ1_DRV = 2;
const int INJ2_DRV = 3;
const int INJ3_DRV = 4;
const int INJ4_DRV = 5;
const int INJ_PINS[4] = {INJ1_DRV, INJ2_DRV, INJ3_DRV, INJ4_DRV};

// Current sensing inputs (note: reversed order from drivers)
const int INJ1_ISENS = 21;  // A7
const int INJ2_ISENS = 20;  // A6
const int INJ3_ISENS = 19;  // A5
const int INJ4_ISENS = 18;  // A4
const int CURRENT_PINS[4] = {INJ1_ISENS, INJ2_ISENS, INJ3_ISENS, INJ4_ISENS};

// === CONFIGURABLE PARAMETERS ===

// Timing Configuration
unsigned long pulseWidth = 20000;        // Default pulse width in microseconds
const unsigned long peakTime = 2000;     // Peak phase duration in microseconds
const unsigned long holdTime = 1800;     // Hold phase duration in microseconds
const unsigned long holdPeriod = 500;    // Hold PWM period (2kHz = 500us)
const unsigned long holdDuty = 250;      // Hold PWM duty cycle (250us on, 250us off)

// Current Sensing Configuration (ACS712 20A, scaled to 3.3V)
const float ACS712_SENSITIVITY = 0.066;  // 66mV/A for 20A version
const float ACS712_VREF = 1.65;          // 3.3V/2 = 1.65V zero current
const float ADC_RESOLUTION = 3.3 / 1024.0;  // 10-bit ADC resolution
float currentOffsets[4] = {0, 0, 0, 0};  // Zero current calibration offsets

// Sampling Configuration
const unsigned long PERFORMANCE_SAMPLE_INTERVAL = 50;  // Performance monitoring sample interval (50us)
const int CALIBRATION_SAMPLES = 100;                   // Number of samples for sensor calibration
const unsigned long CALIBRATION_DELAY = 10;            // Delay between calibration samples (ms)
const unsigned long CALIBRATION_WAIT = 2000;           // Wait time before calibration starts (ms)

// Timing Delays
const unsigned long PULSE_DELAY = 20;          // Delay between multiple pulses (ms)
const unsigned long INJECTOR_DELAY = 20;      // Delay between injectors in sequential firing (ms)
const unsigned long SEQUENTIAL_INJ_DELAY = 20; // Delay between each injector in sequential (ms)
const unsigned long SEQUENTIAL_CYCLE_DELAY = 50; // Delay between complete cycles (ms)

// User Input Timeouts
const unsigned long PULSE_WIDTH_TIMEOUT = 10000;  // Pulse width input timeout (ms)
const unsigned long FILE_SELECT_TIMEOUT = 30000;  // File selection timeout (ms)

// Pulse Width Limits
const float MIN_PULSE_WIDTH = 0.1;   // Minimum pulse width (ms)
const float MAX_PULSE_WIDTH = 100.0; // Maximum pulse width (ms)

// Repeat Counts
const int SINGLE_REPEAT_COUNT = 1;      // Single fire repeat count
const int MULTI_REPEAT_COUNT = 50;     // Multiple fire repeat count (qwer, zxcv)
const int SEQUENTIAL_REPEAT_COUNT = 50; // Sequential fire repeat count (tgb)

// SD Card and logging
const int chipSelect = BUILTIN_SDCARD;  // Teensy 4.1 built-in SD card
bool sdLogging = false;
bool logCurrentData = false;
File dataFile;
String currentLogFile = "";
const unsigned long SAMPLE_INTERVAL_US = 100;  // 10kHz = 100us interval

// Current data structure for high-speed logging
struct CurrentSample {
  unsigned long timestamp;
  float current[4];
  bool injectorState[4];
};

// SD Logging Configuration
const int BUFFER_SIZE = 200;              // Buffer size for high-speed data logging
const int MAX_LOG_FILES = 50;             // Maximum number of log files to display
const int LOG_DUMP_PAUSE_LINES = 50;      // Lines to display before pausing (not used currently)

// Progress Indicators
const int PROGRESS_DOT_INTERVAL = 10;     // Show progress dot every N operations
const int ADC_RESOLUTION_BITS = 10;       // ADC resolution in bits

// Serial Communication
const unsigned long SERIAL_BAUD_RATE = 115200;  // Serial communication baud rate
const unsigned long SERIAL_WAIT_TIMEOUT = 3000; // Wait for serial connection timeout (ms)

CurrentSample sampleBuffer[BUFFER_SIZE];
int bufferIndex = 0;
bool bufferFull = false;

// Function to read current from ACS712 sensor
float readCurrent(int channel) {
  int adcValue = analogRead(CURRENT_PINS[channel]);
  float voltage = adcValue * ADC_RESOLUTION;
  float current = (voltage - ACS712_VREF - currentOffsets[channel]) / ACS712_SENSITIVITY;
  return max(0.0, current);  // Don't return negative current
}

// Function to calibrate current sensors
void calibrateCurrentSensors() {
  Serial.println("Calibrating current sensors...");
  Serial.println("Ensure no current is flowing through any injector.");
  delay(CALIBRATION_WAIT);
  
  const int calibSamples = CALIBRATION_SAMPLES;
  float sums[4] = {0, 0, 0, 0};
  
  for (int i = 0; i < calibSamples; i++) {
    for (int ch = 0; ch < 4; ch++) {
      int adcValue = analogRead(CURRENT_PINS[ch]);
      float voltage = adcValue * ADC_RESOLUTION;
      sums[ch] += voltage - ACS712_VREF;
    }
    delay(CALIBRATION_DELAY);
  }
  
  for (int ch = 0; ch < 4; ch++) {
    currentOffsets[ch] = sums[ch] / calibSamples;
    Serial.print("Channel ");
    Serial.print(ch + 1);
    Serial.print(" offset: ");
    Serial.print(currentOffsets[ch], 4);
    Serial.println(" V");
  }
  
  Serial.println("Calibration complete!\n");
}

// Function to set pulse width
void setPulseWidth() {
  Serial.print("Current pulse width: ");
  Serial.print(pulseWidth / 1000.0, 1);
  Serial.println(" ms");
  Serial.print("Enter new pulse width (ms): ");
  
  unsigned long timeout = millis() + PULSE_WIDTH_TIMEOUT;
  String input = "";
  
  while (millis() < timeout) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        break;
      }
      if ((c >= '0' && c <= '9') || c == '.') {
        input += c;
        Serial.print(c);  // Echo the character
      }
    }
  }
  
  if (input.length() > 0) {
    float newWidth = input.toFloat();
    if (newWidth >= MIN_PULSE_WIDTH && newWidth <= MAX_PULSE_WIDTH) {
      pulseWidth = (unsigned long)(newWidth * 1000);
      Serial.println();
      Serial.print("[LOG]Pulse width set to: ");
      Serial.print(newWidth, 1);
      Serial.println(" ms");
      sendStatusUpdate();
    } else {
      Serial.println();
      Serial.print("Invalid pulse width. Must be between ");
      Serial.print(MIN_PULSE_WIDTH);
      Serial.print(" and ");
      Serial.print(MAX_PULSE_WIDTH);
      Serial.println(" ms");
    }
  } else {
    Serial.println();
    Serial.println("No input received. Pulse width unchanged.");
  }
}

// SD Card Functions
void initializeSD() {
  Serial.println("Initializing SD card...");
  if (SD.begin(chipSelect)) {
    Serial.println("SD card initialized successfully!");
    sdLogging = true;
  } else {
    Serial.println("SD card initialization failed - continuing without SD logging");
    sdLogging = false;
  }
}

void flushBuffer() {
  if (!logCurrentData || bufferIndex == 0) return;
  
  for (int i = 0; i < bufferIndex; i++) {
    dataFile.print(sampleBuffer[i].timestamp);
    for (int ch = 0; ch < 4; ch++) {
      dataFile.print(",");
      dataFile.print(sampleBuffer[i].current[ch], 4);
    }
    for (int ch = 0; ch < 4; ch++) {
      dataFile.print(",");
      dataFile.print(sampleBuffer[i].injectorState[ch] ? "1" : "0");
    }
    dataFile.println();
  }
  
  dataFile.flush();
  bufferIndex = 0;
}

void startCurrentLogging() {
  if (!sdLogging) {
    Serial.println("SD card not available!");
    return;
  }
  
  // Create new log file with timestamp
  String filename = "CURRENT_LOG_";
  filename += millis();
  filename += ".CSV";
  currentLogFile = filename;
  
  dataFile = SD.open(filename.c_str(), FILE_WRITE);
  if (dataFile) {
    // Write CSV header
    dataFile.println("Timestamp_us,Current1_A,Current2_A,Current3_A,Current4_A,Inj1_State,Inj2_State,Inj3_State,Inj4_State");
    dataFile.flush();
    logCurrentData = true;
    bufferIndex = 0;
    bufferFull = false;
    Serial.print("Started logging to: ");
    Serial.println(filename);
  } else {
    Serial.println("Error creating log file");
  }
}

void stopCurrentLogging() {
  if (logCurrentData) {
    flushBuffer();  // Write any remaining data
    if (dataFile) {
      dataFile.close();
    }
    logCurrentData = false;
    Serial.println("Current logging stopped");
  }
}

void logCurrentSample(bool injectorStates[4]) {
  if (!logCurrentData) return;
  
  unsigned long timestamp = micros();
  
  // Add sample to buffer
  sampleBuffer[bufferIndex].timestamp = timestamp;
  for (int i = 0; i < 4; i++) {
    sampleBuffer[bufferIndex].current[i] = readCurrent(i);
    sampleBuffer[bufferIndex].injectorState[i] = injectorStates[i];
  }
  
  bufferIndex++;
  
  // Flush buffer when full
  if (bufferIndex >= BUFFER_SIZE) {
    flushBuffer();
  }
}

void toggleSDLogging() {
  if (logCurrentData) {
    stopCurrentLogging();
  } else {
    startCurrentLogging();
  }
}

void dumpLogFile(String filename) {
  if (!sdLogging) {
    Serial.println("SD card not available!");
    return;
  }
  
  File logFile = SD.open(filename.c_str());
  if (!logFile) {
    Serial.println("Error opening log file!");
    return;
  }
  
  Serial.println("\n=== LOG FILE DUMP ===");
  Serial.print("File: ");
  Serial.println(filename);
  Serial.print("Size: ");
  Serial.print(logFile.size());
  Serial.println(" bytes");
  Serial.println("=====================");
  
  // Read and output file contents
  int lineCount = 0;
  while (logFile.available()) {
    String line = logFile.readStringUntil('\n');
    Serial.println(line);
    lineCount++;
  }
  
  logFile.close();
  Serial.println("\n=== END OF LOG FILE ===");
  Serial.print("Total lines: ");
  Serial.println(lineCount);
}

void listLogFiles() {
  if (!sdLogging) {
    Serial.println("SD card not available!");
    return;
  }
  
  File root = SD.open("/");
  if (!root) {
    Serial.println("Error opening root directory");
    return;
  }
  
  Serial.println("\n=== Available Log Files ===");
  int fileCount = 0;
  String fileList[MAX_LOG_FILES];  // Store up to MAX_LOG_FILES files
  
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    
    String filename = entry.name();
    if (filename.startsWith("CURRENT_LOG_") && filename.endsWith(".CSV")) {
      fileList[fileCount] = filename;
      Serial.print(fileCount + 1);
      Serial.print(". ");
      Serial.print(filename);
      Serial.print(" (");
      Serial.print(entry.size());
      Serial.println(" bytes)");
      fileCount++;
      
      if (fileCount >= MAX_LOG_FILES) break;  // Limit to MAX_LOG_FILES files
    }
    entry.close();
  }
  root.close();
  
  if (fileCount == 0) {
    Serial.println("No log files found.");
    return;
  }
  
  Serial.println("============================");
  Serial.print("Enter file number (1-");
  Serial.print(fileCount);
  Serial.print("): ");
  
  // Wait for user input
  unsigned long timeout = millis() + FILE_SELECT_TIMEOUT;
  String input = "";
  
  while (millis() < timeout) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        break;
      }
      if (c >= '0' && c <= '9') {
        input += c;
        Serial.print(c);  // Echo the character
      }
    }
  }
  
  Serial.println();
  
  if (input.length() == 0) {
    Serial.println("No selection made - cancelled.");
    return;
  }
  
  int selection = input.toInt();
  if (selection < 1 || selection > fileCount) {
    Serial.println("Invalid selection.");
    return;
  }
  
  // Read and output the selected file
  dumpLogFile(fileList[selection - 1]);
}

// Function to send JSON status update
void sendStatusUpdate() {
  Serial.print("[STATUS]{");
  Serial.print("\"pulseWidth\":");
  Serial.print(pulseWidth / 1000.0, 1);
  Serial.print(",\"peakTime\":");
  Serial.print(peakTime / 1000.0, 1);
  Serial.print(",\"holdFreq\":2000");
  Serial.print(",\"holdDuty\":50");
  Serial.print(",\"sdAvailable\":");
  Serial.print(sdLogging ? "true" : "false");
  Serial.print(",\"logging\":");
  Serial.print(logCurrentData ? "true" : "false");
  if (currentLogFile.length() > 0) {
    Serial.print(",\"logFile\":\"");
    Serial.print(currentLogFile);
    Serial.print("\"");
  }
  Serial.print(",\"offsets\":[");
  for (int i = 0; i < 4; i++) {
    Serial.print(currentOffsets[i], 4);
    if (i < 3) Serial.print(",");
  }
  Serial.print("]}");
  Serial.println();
}

// Function to print help menu
void printHelp() {
  Serial.println("\n=== Commands ===");
  Serial.println("Single Fire:");
  Serial.println("  1,2,3,4 - Fire injector 1-4 once");
  Serial.println("  5 - Fire all 4 injectors individually");
  Serial.print("  q,w,e,r - Fire injector 1-4 ");
  Serial.print(MULTI_REPEAT_COUNT);
  Serial.println(" times");
  Serial.println();
  Serial.println("Peak & Hold:");
  Serial.println("  a,s,d,f - Fire injector 1-4 once (P&H)");
  Serial.print("  z,x,c,v - Fire injector 1-4 ");
  Serial.print(MULTI_REPEAT_COUNT);
  Serial.println(" times (P&H)");
  Serial.println();
  Serial.println("Sequential All:");
  Serial.print("  t - All injectors ");
  Serial.print(SEQUENTIAL_REPEAT_COUNT);
  Serial.println("x (normal)");
  Serial.println("  g - All injectors once (P&H)");
  Serial.print("  b - All injectors ");
  Serial.print(SEQUENTIAL_REPEAT_COUNT);
  Serial.println("x (P&H)");
  Serial.println();
  Serial.println("Configuration:");
  Serial.println("  p - Set pulse width");
  Serial.println("  k - Calibrate current sensors");
  Serial.println("  l - Toggle SD current logging (10kHz)");
  Serial.println("  m - Dump log files from SD card");
  Serial.println("  i - Get status info (JSON)");
  Serial.println("  o - Get sensor offsets");
  Serial.println("  h - Show this help");
  Serial.println();
  Serial.print("Current pulse width: ");
  Serial.print(pulseWidth / 1000.0, 1);
  Serial.println(" ms");
  Serial.print("Peak time: ");
  Serial.print(peakTime / 1000.0, 1);
  Serial.println(" ms");
  Serial.print("Hold frequency: 2000 Hz");
  Serial.println();
  Serial.print("SD logging: ");
  Serial.println(sdLogging ? "AVAILABLE" : "NOT AVAILABLE");
  Serial.print("Current logging: ");
  Serial.println(logCurrentData ? "ACTIVE" : "INACTIVE");
  Serial.println("================\n");
}

// Function to fire injector with normal pulse
void fireInjectorNormal(int injNum, float *peakCurrent, float *avgCurrent, int *samples) {
  unsigned long startTime = micros();
  unsigned long nextSample = startTime;
  bool injectorStates[4] = {false, false, false, false};
  
  digitalWrite(INJ_PINS[injNum], HIGH);
  injectorStates[injNum] = true;
  
  *peakCurrent = 0;
  *avgCurrent = 0;
  *samples = 0;
  
  // Sample current during pulse
  while (micros() - startTime < pulseWidth) {
    unsigned long currentTime = micros();
    
    // High-speed logging at 10kHz
    if (currentTime >= nextSample) {
      logCurrentSample(injectorStates);
      nextSample += SAMPLE_INTERVAL_US;
    }
    
    // Performance monitoring (slower rate)
    float current = readCurrent(injNum);
    if (current > *peakCurrent) *peakCurrent = current;
    *avgCurrent += current;
    (*samples)++;
    delayMicroseconds(PERFORMANCE_SAMPLE_INTERVAL);
  }
  
  digitalWrite(INJ_PINS[injNum], LOW);
  injectorStates[injNum] = false;
  
  // Log final state
  logCurrentSample(injectorStates);
  
  if (*samples > 0) {
    *avgCurrent /= *samples;
  }
}

// Function to fire injector with peak and hold
void fireInjectorPeakHold(int injNum, float *peakCurrent, float *avgCurrent, int *samples) {
  unsigned long totalStartTime = micros();
  unsigned long nextSample = totalStartTime;
  unsigned long nextLogSample = totalStartTime;
  bool injectorStates[4] = {false, false, false, false};
  
  *peakCurrent = 0;
  *avgCurrent = 0;
  *samples = 0;
  
  // Peak phase - full voltage
  digitalWrite(INJ_PINS[injNum], HIGH);
  injectorStates[injNum] = true;
  
  while (micros() - totalStartTime < peakTime) {
    unsigned long currentTime = micros();
    
    // High-speed logging at 10kHz
    if (currentTime >= nextLogSample) {
      logCurrentSample(injectorStates);
      nextLogSample += SAMPLE_INTERVAL_US;
    }
    
    // Performance monitoring
    if (currentTime >= nextSample) {
      float current = readCurrent(injNum);
      if (current > *peakCurrent) *peakCurrent = current;
      *avgCurrent += current;
      (*samples)++;
      nextSample += PERFORMANCE_SAMPLE_INTERVAL;
    }
  }
  
  // Hold phase - PWM at 2kHz, 50% duty
  unsigned long holdStart = micros();
  unsigned long holdEnd = holdStart + holdTime;
  unsigned long nextToggle = holdStart;
  bool holdOn = true;
  // Use global constants for PWM timing
  
  while (micros() < holdEnd) {
    unsigned long currentTime = micros();
    
    // PWM control
    if (currentTime >= nextToggle) {
      if (holdOn) {
        digitalWrite(INJ_PINS[injNum], HIGH);
        injectorStates[injNum] = true;
        nextToggle += holdDuty;
      } else {
        digitalWrite(INJ_PINS[injNum], LOW);
        injectorStates[injNum] = false;
        nextToggle += (holdPeriod - holdDuty);
      }
      holdOn = !holdOn;
    }
    
    // High-speed logging at 10kHz
    if (currentTime >= nextLogSample) {
      logCurrentSample(injectorStates);
      nextLogSample += SAMPLE_INTERVAL_US;
    }
    
    // Performance monitoring
    if (currentTime >= nextSample) {
      float current = readCurrent(injNum);
      if (current > *peakCurrent) *peakCurrent = current;
      *avgCurrent += current;
      (*samples)++;
      nextSample += PERFORMANCE_SAMPLE_INTERVAL;
    }
  }
  
  // Ensure injector is off
  digitalWrite(INJ_PINS[injNum], LOW);
  injectorStates[injNum] = false;
  
  // Log final state
  logCurrentSample(injectorStates);
  
  if (*samples > 0) {
    *avgCurrent /= *samples;
  }
}

// Function to fire a single injector multiple times
void fireInjector(int injNum, int count, bool peakHold) {
  Serial.print("Firing injector ");
  Serial.print(injNum + 1);
  Serial.print(" x");
  Serial.print(count);
  if (peakHold) Serial.print(" (Peak & Hold)");
  Serial.println();
  
  for (int i = 0; i < count; i++) {
    float peakCurrent = 0;
    float avgCurrent = 0;
    int samples = 0;
    
    if (peakHold) {
      fireInjectorPeakHold(injNum, &peakCurrent, &avgCurrent, &samples);
    } else {
      fireInjectorNormal(injNum, &peakCurrent, &avgCurrent, &samples);
    }
    
    // Print current data for single shots
    if (count == 1) {
      Serial.print("[RESULT]{\"injector\":");
      Serial.print(injNum + 1);
      Serial.print(",\"peakCurrent\":");
      Serial.print(peakCurrent, 2);
      Serial.print(",\"avgCurrent\":");
      Serial.print(avgCurrent, 2);
      Serial.print(",\"peakHold\":");
      Serial.print(peakHold ? "true" : "false");
      Serial.println("}");
    }
    
    // Delay between pulses (except for last pulse)
    if (i < count - 1) {
      delay(PULSE_DELAY);
    }
    
    // Progress indicator for multiple shots
    if (count > 1 && (i + 1) % PROGRESS_DOT_INTERVAL == 0) {
      Serial.print(".");
    }
  }
  
  if (count > 1) {
    Serial.println(" Done!");
  }
}

// Function to fire all injectors sequentially
void fireAllSequential(int count, bool peakHold) {
  Serial.print("Firing all injectors sequentially x");
  Serial.print(count);
  if (peakHold) Serial.print(" (Peak & Hold)");
  Serial.println(" (cycling 1-2-3-4)");
  
  for (int cycle = 0; cycle < count; cycle++) {
    for (int inj = 0; inj < 4; inj++) {
      float peakCurrent = 0;
      float avgCurrent = 0;
      int samples = 0;
      
      if (peakHold) {
        fireInjectorPeakHold(inj, &peakCurrent, &avgCurrent, &samples);
      } else {
        fireInjectorNormal(inj, &peakCurrent, &avgCurrent, &samples);
      }
      
      delay(SEQUENTIAL_INJ_DELAY);
    }
    
    // Progress indicator
    if ((cycle + 1) % PROGRESS_DOT_INTERVAL == 0) {
      Serial.print(".");
    }
    
    delay(SEQUENTIAL_CYCLE_DELAY);
  }
  
  Serial.println(" Sequential firing complete");
}

// Function to process serial commands
void processCommand(char cmd) {
  switch (cmd) {
    // Single fire commands (1-4)
    case '1': fireInjector(0, SINGLE_REPEAT_COUNT, false); break;
    case '2': fireInjector(1, SINGLE_REPEAT_COUNT, false); break;
    case '3': fireInjector(2, SINGLE_REPEAT_COUNT, false); break;
    case '4': fireInjector(3, SINGLE_REPEAT_COUNT, false); break;
    
    // Fire all 4 injectors individually
    case '5': 
      fireInjector(0, SINGLE_REPEAT_COUNT, false);
      fireInjector(1, SINGLE_REPEAT_COUNT, false);
      fireInjector(2, SINGLE_REPEAT_COUNT, false);
      fireInjector(3, SINGLE_REPEAT_COUNT, false);
      break;
    
    // 100x fire commands (qwer)
    case 'q': fireInjector(0, MULTI_REPEAT_COUNT, false); break;
    case 'w': fireInjector(1, MULTI_REPEAT_COUNT, false); break;
    case 'e': fireInjector(2, MULTI_REPEAT_COUNT, false); break;
    case 'r': fireInjector(3, MULTI_REPEAT_COUNT, false); break;
    
    // Single fire with peak/hold (asdf)
    case 'a': fireInjector(0, SINGLE_REPEAT_COUNT, true); break;
    case 's': fireInjector(1, SINGLE_REPEAT_COUNT, true); break;
    case 'd': fireInjector(2, SINGLE_REPEAT_COUNT, true); break;
    case 'f': fireInjector(3, SINGLE_REPEAT_COUNT, true); break;
    
    // 100x fire with peak/hold (zxcv)
    case 'z': fireInjector(0, MULTI_REPEAT_COUNT, true); break;
    case 'x': fireInjector(1, MULTI_REPEAT_COUNT, true); break;
    case 'c': fireInjector(2, MULTI_REPEAT_COUNT, true); break;
    case 'v': fireInjector(3, MULTI_REPEAT_COUNT, true); break;
    
    // Sequential all injector commands (tgb)
    case 't': fireAllSequential(SEQUENTIAL_REPEAT_COUNT, false); break;
    case 'g': fireAllSequential(SINGLE_REPEAT_COUNT, true); break;
    case 'b': fireAllSequential(SEQUENTIAL_REPEAT_COUNT, true); break;
    
    // Configuration
    case 'p': setPulseWidth(); break;
    case 'k': calibrateCurrentSensors(); break;
    case 'l': toggleSDLogging(); break;
    case 'm': listLogFiles(); break;
    case 'h': printHelp(); break;
    case 'i': sendStatusUpdate(); break;
    case 'o': 
      Serial.println("[LOG]Current sensor offsets:");
      for (int i = 0; i < 4; i++) {
        Serial.print("[LOG]Channel ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(currentOffsets[i], 4);
        Serial.println(" V");
      }
      break;
    
    default:
      if (cmd >= 32 && cmd <= 126) {  // Printable characters only
        Serial.print("Unknown command: ");
        Serial.println(cmd);
      }
      break;
  }
}

// Setup function
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  
  // Initialize injector pins as outputs
  for (int i = 0; i < 4; i++) {
    pinMode(INJ_PINS[i], OUTPUT);
    digitalWrite(INJ_PINS[i], LOW);
  }
  
  // Initialize current sensing pins as inputs
  for (int i = 0; i < 4; i++) {
    pinMode(CURRENT_PINS[i], INPUT);
  }
  
  // Set ADC resolution
  analogReadResolution(ADC_RESOLUTION_BITS);
  
  // Initialize SD card
  initializeSD();
  
  // Calibrate current sensors
  calibrateCurrentSensors();
  
  Serial.println("Fuel Injector Test Rig Ready");
  printHelp();
}

// Main loop
void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    processCommand(cmd);
  }
}