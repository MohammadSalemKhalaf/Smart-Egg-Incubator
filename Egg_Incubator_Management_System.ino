/*
 * ===============================================================
 *    SMART EGG INCUBATOR - COMPLETE MANAGEMENT SYSTEM
 *    
 *    Hardware: Arduino Mega 2560
 *    
 *    Components:
 *    - LCD 20x4 I2C (0x27)
 *    - 4x4 Matrix Keypad
 *    - DHT11 Sensor
 *    - 3x 12V DC Fans (Relay 1 - NO contacts)
 *    - 220V Heating Lamps (Relay 2)
 *    - Exhaust Fan 12V (Relay 3)
 *    - Servo Motor (Exhaust Cover)
 *    - NEMA 23 Stepper Motor with Driver
 *    - Buzzer for alerts
 *    
 *    Keypad Functions:
 *    - * : Enter Configuration Mode
 *    - # : Save/Skip and go to next step
 *    - 0-9 : Enter numeric values
 *    - C : Clear current input
 *    
 * ===============================================================
 */

// ===============================================================
// LIBRARY INCLUDES
// ===============================================================
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <DHT.h>
#include <Servo.h>
#include <EEPROM.h>

// ===============================================================
// PIN DEFINITIONS
// ===============================================================

// DHT11 Sensor
#define DHT_PIN 3
#define DHT_TYPE DHT11

// LCD I2C
#define LCD_ADDRESS 0x27
#define LCD_COLS 20
#define LCD_ROWS 4

// Keypad Pins (Arduino Mega)
const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 4;
byte rowPins[KEYPAD_ROWS] = {22, 23, 24, 25};  // R1, R2, R3, R4
byte colPins[KEYPAD_COLS] = {26, 27, 28, 29};  // C1, C2, C3, C4

// Relay Pins (Active LOW)
#define RELAY_FANS     31    // Relay 1: 3x 12V DC Fans (NO contacts)
#define RELAY_HEATER   32    // Relay 2: 220V Heating Lamps
#define RELAY_EXHAUST  33    // Relay 3: Exhaust Fan 12V

// Servo Motor
#define SERVO_PIN 2

// NEMA 23 Stepper Motor Driver
#define STEPPER_PUL    10    // Pulse
#define STEPPER_DIR    11    // Direction
#define STEPPER_EN     8     // Enable (Active LOW)

// Buzzer
#define BUZZER_PIN 9

// ===============================================================
// TIMING CONSTANTS
// ===============================================================
#define DEBOUNCE_DELAY       300       // Keypad debounce (ms)
#define DHT_READ_INTERVAL    2000      // Sensor read interval (ms)
#define LCD_UPDATE_INTERVAL  1000      // Display update (ms)
#define EXHAUST_INTERVAL     60000     // Exhaust cycle interval (ms)
#define EXHAUST_DURATION     10000     // Exhaust run time (ms)
#define STEPPER_LEFT_TIME    3500      // Stepper left rotation (ms)
#define STEPPER_RIGHT_TIME   3000      // Stepper right rotation (ms)
#define STEPPER_PAUSE_TIME   7200000UL // Pause between turns (2 hours)
#define DAY_MILLIS           86400000UL // Milliseconds per day

// ===============================================================
// EEPROM ADDRESSES
// ===============================================================
#define EEPROM_INIT_FLAG     0    // 1 byte - initialization flag
#define EEPROM_TEMP_ADDR     1    // 4 bytes - float temperature
#define EEPROM_HUMIDITY_ADDR 5    // 4 bytes - float humidity
#define EEPROM_DAYS_ADDR     9    // 4 bytes - int days
#define EEPROM_START_ADDR    13   // 4 bytes - unsigned long start time
#define EEPROM_INIT_VALUE    0xAB // Magic number for initialization check

// ===============================================================
// DEFAULT VALUES (Chicken Eggs)
// ===============================================================
#define DEFAULT_TEMP         37.5
#define DEFAULT_HUMIDITY     55.0
#define DEFAULT_DAYS         21
#define TEMP_HYSTERESIS      0.5
#define HUMIDITY_HYSTERESIS  3.0

// ===============================================================
// KEYPAD CONFIGURATION
// ===============================================================
char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// ===============================================================
// OBJECTS
// ===============================================================
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);
DHT dht(DHT_PIN, DHT_TYPE);
Servo exhaustServo;

// ===============================================================
// GLOBAL VARIABLES
// ===============================================================

// Sensor readings
float currentTemp = 0.0;
float currentHumidity = 0.0;
bool sensorError = false;

// Target values (loaded from EEPROM)
float targetTemp = DEFAULT_TEMP;
float targetHumidity = DEFAULT_HUMIDITY;
int targetDays = DEFAULT_DAYS;
int remainingDays = DEFAULT_DAYS;
int elapsedHours = 0;
int elapsedMinutes = 0;

// Timing variables
unsigned long lastKeyPress = 0;
unsigned long lastDHTRead = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastExhaustRun = 0;
unsigned long lastStepperMove = 0;
unsigned long incubationStartTime = 0;
unsigned long exhaustStartTime = 0;
unsigned long dayStartTime = 0;

// System states
bool fansRunning = true;
bool heaterRunning = false;
bool exhaustRunning = false;

// Stepper states
enum StepperState { 
  STEPPER_IDLE, 
  STEPPER_LEFT, 
  STEPPER_RIGHT, 
  STEPPER_WAIT 
};
StepperState stepperState = STEPPER_WAIT;
unsigned long stepperStartTime = 0;

// Configuration mode
enum SystemMode {
  MODE_DISPLAY,      // Normal display mode
  MODE_CONFIG_TEMP,  // Configure temperature
  MODE_CONFIG_HUM,   // Configure humidity
  MODE_CONFIG_DAYS   // Configure days
};
SystemMode currentMode = MODE_DISPLAY;

// Input buffer
String inputBuffer = "";
bool inputStarted = false;

// Custom characters for LCD
byte degreeChar[8] = {
  B00110, B01001, B01001, B00110,
  B00000, B00000, B00000, B00000
};

byte dropChar[8] = {
  B00100, B00100, B01110, B11111,
  B11111, B11111, B01110, B00000
};

byte eggChar[8] = {
  B00100, B01110, B11111, B11111,
  B11111, B11111, B01110, B00000
};

byte heartChar[8] = {
  B00000, B01010, B11111, B11111,
  B01110, B00100, B00000, B00000
};

// ===============================================================
// FUNCTION PROTOTYPES
// ===============================================================
void initializePins();
void initializeLCD();
void loadSettingsFromEEPROM();
void saveSettingsToEEPROM();
void readDHTSensor();
void controlFans();
void controlHeater();
void controlExhaust();
void controlStepper();
void updateDisplayMode();
void handleKeypad();
void enterConfigMode();
void processConfigInput(char key);
void displayConfigScreen();
void saveCurrentConfig();
void goToNextConfigStep();
void calculateRemainingTime();
void beep(int duration);
void debugPrint();

// ===============================================================
// SETUP
// ===============================================================
void setup() {
  // Serial for debugging
  Serial.begin(115200);
  Serial.println(F(""));
  Serial.println(F("========================================"));
  Serial.println(F("   SMART EGG INCUBATOR SYSTEM"));
  Serial.println(F("   Starting up..."));
  Serial.println(F("========================================"));
  
  // Initialize pins
  initializePins();
  
  // Initialize LCD
  initializeLCD();
  
  // Show startup screen
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print(F("EGG INCUBATOR"));
  lcd.setCursor(4, 1);
  lcd.write(2); // Egg
  lcd.print(F(" System "));
  lcd.write(2);
  lcd.setCursor(3, 2);
  lcd.print(F("Loading..."));
  delay(1500);
  
  // Initialize DHT sensor
  dht.begin();
  Serial.println(F("[OK] DHT11 initialized"));
  
  // Initialize Servo
  exhaustServo.attach(SERVO_PIN);
  exhaustServo.write(0);
  Serial.println(F("[OK] Servo initialized"));
  
  // Load settings from EEPROM
  loadSettingsFromEEPROM();
  
  // Initial sensor read
  delay(2000);
  readDHTSensor();
  
  // Set initial relay states
  digitalWrite(RELAY_FANS, LOW);      // Fans ON (NO relay, active LOW)
  digitalWrite(RELAY_HEATER, HIGH);   // Heater OFF initially
  digitalWrite(RELAY_EXHAUST, HIGH);  // Exhaust OFF
  
  // Enable stepper
  digitalWrite(STEPPER_EN, LOW);
  
  // Record start time
  if (incubationStartTime == 0) {
    incubationStartTime = millis();
    dayStartTime = millis();
    saveSettingsToEEPROM();
  }
  
  // Show ready message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("System Ready!"));
  lcd.setCursor(0, 1);
  lcd.print(F("Press * to config"));
  lcd.setCursor(0, 2);
  lcd.print(F("Temp: "));
  lcd.print(targetTemp, 1);
  lcd.print(F("C"));
  lcd.setCursor(0, 3);
  lcd.print(F("Days: "));
  lcd.print(targetDays);
  delay(2500);
  
  beep(100);
  
  Serial.println(F("========================================"));
  Serial.println(F("   System Ready!"));
  Serial.println(F("========================================"));
}

// ===============================================================
// MAIN LOOP
// ===============================================================
void loop() {
  unsigned long currentMillis = millis();
  
  // 1. Handle keypad input with debouncing
  handleKeypad();
  
  // 2. Read sensor at intervals
  if (currentMillis - lastDHTRead >= DHT_READ_INTERVAL) {
    readDHTSensor();
    lastDHTRead = currentMillis;
  }
  
  // 3. Control systems (only in display mode)
  if (currentMode == MODE_DISPLAY) {
    controlFans();
    controlHeater();
    controlExhaust();
    controlStepper();
  }
  
  // 4. Update display at intervals
  if (currentMillis - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
    if (currentMode == MODE_DISPLAY) {
      updateDisplayMode();
    }
    calculateRemainingTime();
    lastLCDUpdate = currentMillis;
  }
  
  // 5. Debug output every 10 seconds
  static unsigned long lastDebug = 0;
  if (currentMillis - lastDebug >= 10000) {
    debugPrint();
    lastDebug = currentMillis;
  }
}

// ===============================================================
// INITIALIZATION FUNCTIONS
// ===============================================================
void initializePins() {
  // Relay pins
  pinMode(RELAY_FANS, OUTPUT);
  pinMode(RELAY_HEATER, OUTPUT);
  pinMode(RELAY_EXHAUST, OUTPUT);
  
  // Safe initial states
  digitalWrite(RELAY_FANS, HIGH);
  digitalWrite(RELAY_HEATER, HIGH);
  digitalWrite(RELAY_EXHAUST, HIGH);
  
  // Stepper pins
  pinMode(STEPPER_PUL, OUTPUT);
  pinMode(STEPPER_DIR, OUTPUT);
  pinMode(STEPPER_EN, OUTPUT);
  
  digitalWrite(STEPPER_PUL, LOW);
  digitalWrite(STEPPER_DIR, LOW);
  digitalWrite(STEPPER_EN, HIGH);
  
  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.println(F("[OK] Pins initialized"));
}

void initializeLCD() {
  lcd.init();
  lcd.backlight();
  
  // Create custom characters
  lcd.createChar(0, degreeChar);
  lcd.createChar(1, dropChar);
  lcd.createChar(2, eggChar);
  lcd.createChar(3, heartChar);
  
  Serial.println(F("[OK] LCD initialized"));
}

// ===============================================================
// EEPROM FUNCTIONS
// ===============================================================
void loadSettingsFromEEPROM() {
  Serial.println(F("[INFO] Loading settings from EEPROM..."));
  
  // Check if EEPROM is initialized
  if (EEPROM.read(EEPROM_INIT_FLAG) != EEPROM_INIT_VALUE) {
    Serial.println(F("[INFO] First run - saving defaults"));
    
    targetTemp = DEFAULT_TEMP;
    targetHumidity = DEFAULT_HUMIDITY;
    targetDays = DEFAULT_DAYS;
    incubationStartTime = 0;
    
    saveSettingsToEEPROM();
    EEPROM.write(EEPROM_INIT_FLAG, EEPROM_INIT_VALUE);
    return;
  }
  
  // Load temperature
  EEPROM.get(EEPROM_TEMP_ADDR, targetTemp);
  if (isnan(targetTemp) || targetTemp < 25 || targetTemp > 45) {
    targetTemp = DEFAULT_TEMP;
  }
  
  // Load humidity
  EEPROM.get(EEPROM_HUMIDITY_ADDR, targetHumidity);
  if (isnan(targetHumidity) || targetHumidity < 20 || targetHumidity > 90) {
    targetHumidity = DEFAULT_HUMIDITY;
  }
  
  // Load days
  EEPROM.get(EEPROM_DAYS_ADDR, targetDays);
  if (targetDays < 1 || targetDays > 60) {
    targetDays = DEFAULT_DAYS;
  }
  remainingDays = targetDays;
  
  // Load start time
  EEPROM.get(EEPROM_START_ADDR, incubationStartTime);
  
  Serial.print(F("   Temp: ")); Serial.println(targetTemp);
  Serial.print(F("   Humidity: ")); Serial.println(targetHumidity);
  Serial.print(F("   Days: ")); Serial.println(targetDays);
  Serial.println(F("[OK] Settings loaded"));
}

void saveSettingsToEEPROM() {
  EEPROM.put(EEPROM_TEMP_ADDR, targetTemp);
  EEPROM.put(EEPROM_HUMIDITY_ADDR, targetHumidity);
  EEPROM.put(EEPROM_DAYS_ADDR, targetDays);
  EEPROM.put(EEPROM_START_ADDR, incubationStartTime);
  
  Serial.println(F("[OK] Settings saved to EEPROM"));
}

// ===============================================================
// SENSOR READING
// ===============================================================
void readDHTSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  
  if (isnan(t) || isnan(h)) {
    sensorError = true;
    Serial.println(F("[ERROR] DHT sensor read failed!"));
    return;
  }
  
  sensorError = false;
  currentTemp = t;
  currentHumidity = h;
}

// ===============================================================
// CONTROL FUNCTIONS
// ===============================================================
void controlFans() {
  // Fans normally ON via NO relay contacts
  // Keep fans running for air circulation
  if (fansRunning) {
    digitalWrite(RELAY_FANS, LOW);  // ON
  } else {
    digitalWrite(RELAY_FANS, HIGH); // OFF
  }
}

void controlHeater() {
  if (sensorError) {
    // Safety: turn off heater if sensor fails
    digitalWrite(RELAY_HEATER, HIGH);
    heaterRunning = false;
    return;
  }
  
  // Hysteresis control
  if (currentTemp < (targetTemp - TEMP_HYSTERESIS)) {
    digitalWrite(RELAY_HEATER, LOW);  // ON
    heaterRunning = true;
  }
  else if (currentTemp > (targetTemp + TEMP_HYSTERESIS)) {
    digitalWrite(RELAY_HEATER, HIGH); // OFF
    heaterRunning = false;
  }
}

void controlExhaust() {
  unsigned long currentMillis = millis();
  
  // Start exhaust cycle
  if (!exhaustRunning && (currentMillis - lastExhaustRun >= EXHAUST_INTERVAL)) {
    exhaustRunning = true;
    exhaustStartTime = currentMillis;
    
    digitalWrite(RELAY_EXHAUST, LOW);  // Fan ON
    exhaustServo.write(90);            // Open cover
    
    Serial.println(F("[EXHAUST] Started"));
  }
  
  // End exhaust cycle
  if (exhaustRunning && (currentMillis - exhaustStartTime >= EXHAUST_DURATION)) {
    exhaustRunning = false;
    lastExhaustRun = currentMillis;
    
    digitalWrite(RELAY_EXHAUST, HIGH); // Fan OFF
    exhaustServo.write(0);             // Close cover
    
    Serial.println(F("[EXHAUST] Stopped"));
  }
}

void controlStepper() {
  unsigned long currentMillis = millis();
  
  switch (stepperState) {
    case STEPPER_IDLE:
      stepperState = STEPPER_LEFT;
      stepperStartTime = currentMillis;
      digitalWrite(STEPPER_EN, LOW);
      digitalWrite(STEPPER_DIR, LOW);
      Serial.println(F("[STEPPER] Rotating LEFT"));
      break;
      
    case STEPPER_LEFT:
      // Generate pulses
      digitalWrite(STEPPER_PUL, HIGH);
      delayMicroseconds(500);
      digitalWrite(STEPPER_PUL, LOW);
      delayMicroseconds(500);
      
      if (currentMillis - stepperStartTime >= STEPPER_LEFT_TIME) {
        stepperState = STEPPER_RIGHT;
        stepperStartTime = currentMillis;
        digitalWrite(STEPPER_DIR, HIGH);
        Serial.println(F("[STEPPER] Rotating RIGHT"));
      }
      break;
      
    case STEPPER_RIGHT:
      // Generate pulses
      digitalWrite(STEPPER_PUL, HIGH);
      delayMicroseconds(500);
      digitalWrite(STEPPER_PUL, LOW);
      delayMicroseconds(500);
      
      if (currentMillis - stepperStartTime >= STEPPER_RIGHT_TIME) {
        stepperState = STEPPER_WAIT;
        stepperStartTime = currentMillis;
        digitalWrite(STEPPER_EN, HIGH);  // Disable to save power
        Serial.println(F("[STEPPER] Waiting..."));
      }
      break;
      
    case STEPPER_WAIT:
      if (currentMillis - stepperStartTime >= STEPPER_PAUSE_TIME) {
        stepperState = STEPPER_IDLE;
        Serial.println(F("[STEPPER] Starting new cycle"));
      }
      break;
  }
}

// ===============================================================
// CALCULATE REMAINING TIME
// ===============================================================
void calculateRemainingTime() {
  unsigned long elapsedMillis = millis() - incubationStartTime;
  unsigned long elapsedDays = elapsedMillis / DAY_MILLIS;
  
  remainingDays = targetDays - (int)elapsedDays;
  if (remainingDays < 0) remainingDays = 0;
  
  // Calculate hours and minutes for current day
  unsigned long todayMillis = millis() - dayStartTime;
  elapsedHours = (todayMillis / 3600000) % 24;
  elapsedMinutes = (todayMillis / 60000) % 60;
  
  // Check if day changed
  if (elapsedHours == 0 && elapsedMinutes == 0) {
    dayStartTime = millis();
  }
}

// ===============================================================
// DISPLAY UPDATE (Normal Mode)
// ===============================================================
void updateDisplayMode() {
  lcd.clear();
  
  // Line 0: Temperature and Humidity (current/target)
  lcd.setCursor(0, 0);
  lcd.print(F("T:"));
  lcd.print(currentTemp, 1);
  lcd.print(F("/"));
  lcd.print(targetTemp, 1);
  lcd.print(F("C "));
  
  lcd.print(F("H:"));
  lcd.print((int)currentHumidity);
  lcd.print(F("/"));
  lcd.print((int)targetHumidity);
  lcd.print(F("%"));
  
  // Line 1: Days countdown with time
  lcd.setCursor(0, 1);
  lcd.write(2); // Egg icon
  lcd.print(F(" Day:"));
  lcd.print(targetDays - remainingDays + 1);
  lcd.print(F("/"));
  lcd.print(targetDays);
  lcd.print(F(" Left:"));
  lcd.print(remainingDays);
  
  // Line 2: System status
  lcd.setCursor(0, 2);
  lcd.print(F("Heat:"));
  lcd.print(heaterRunning ? F("ON ") : F("OFF"));
  lcd.print(F(" Fan:"));
  lcd.print(fansRunning ? F("ON ") : F("OFF"));
  lcd.print(F(" "));
  lcd.print(exhaustRunning ? F("V") : F(" "));
  
  // Line 3: Instructions or status
  lcd.setCursor(0, 3);
  if (sensorError) {
    lcd.print(F("!! SENSOR ERROR !!"));
  } else {
    lcd.print(F("* Config  "));
    // Show stepper status
    if (stepperState == STEPPER_LEFT) {
      lcd.print(F("[<-Turn]"));
    } else if (stepperState == STEPPER_RIGHT) {
      lcd.print(F("[Turn->]"));
    } else {
      // Show time
      if (elapsedHours < 10) lcd.print(F("0"));
      lcd.print(elapsedHours);
      lcd.print(F(":"));
      if (elapsedMinutes < 10) lcd.print(F("0"));
      lcd.print(elapsedMinutes);
    }
  }
}

// ===============================================================
// KEYPAD HANDLING WITH DEBOUNCING
// ===============================================================
void handleKeypad() {
  char key = keypad.getKey();
  
  if (key == NO_KEY) return;
  
  // Debouncing check
  unsigned long currentMillis = millis();
  if (currentMillis - lastKeyPress < DEBOUNCE_DELAY) {
    return;  // Ignore key press - too soon
  }
  lastKeyPress = currentMillis;
  
  // Feedback beep
  beep(30);
  
  Serial.print(F("[KEY] Pressed: "));
  Serial.println(key);
  
  // Handle based on current mode
  if (currentMode == MODE_DISPLAY) {
    // In display mode, only * enters config
    if (key == '*') {
      enterConfigMode();
    }
  } else {
    // In config mode
    processConfigInput(key);
  }
}

// ===============================================================
// CONFIGURATION MODE
// ===============================================================
void enterConfigMode() {
  currentMode = MODE_CONFIG_TEMP;
  inputBuffer = "";
  inputStarted = false;
  
  Serial.println(F("[CONFIG] Entering configuration mode"));
  beep(200);
  
  displayConfigScreen();
}

void displayConfigScreen() {
  lcd.clear();
  
  switch (currentMode) {
    case MODE_CONFIG_TEMP:
      lcd.setCursor(0, 0);
      lcd.print(F("=== TEMPERATURE ==="));
      lcd.setCursor(0, 1);
      lcd.print(F("Current: "));
      lcd.print(targetTemp, 1);
      lcd.print(F(" C"));
      lcd.setCursor(0, 2);
      lcd.print(F("New: "));
      if (inputBuffer.length() > 0) {
        lcd.print(inputBuffer);
      } else {
        lcd.print(F("_____"));
      }
      lcd.setCursor(0, 3);
      lcd.print(F("# Save/Skip  C Clear"));
      break;
      
    case MODE_CONFIG_HUM:
      lcd.setCursor(0, 0);
      lcd.print(F("=== HUMIDITY ==="));
      lcd.setCursor(0, 1);
      lcd.print(F("Current: "));
      lcd.print((int)targetHumidity);
      lcd.print(F(" %"));
      lcd.setCursor(0, 2);
      lcd.print(F("New: "));
      if (inputBuffer.length() > 0) {
        lcd.print(inputBuffer);
      } else {
        lcd.print(F("_____"));
      }
      lcd.setCursor(0, 3);
      lcd.print(F("# Save/Skip  C Clear"));
      break;
      
    case MODE_CONFIG_DAYS:
      lcd.setCursor(0, 0);
      lcd.print(F("=== INCUBATION DAYS ==="));
      lcd.setCursor(0, 1);
      lcd.print(F("Current: "));
      lcd.print(targetDays);
      lcd.print(F(" days"));
      lcd.setCursor(0, 2);
      lcd.print(F("New: "));
      if (inputBuffer.length() > 0) {
        lcd.print(inputBuffer);
      } else {
        lcd.print(F("_____"));
      }
      lcd.setCursor(0, 3);
      lcd.print(F("# Save/Skip  C Clear"));
      break;
      
    default:
      break;
  }
}

void processConfigInput(char key) {
  // Handle numeric input (0-9)
  if (key >= '0' && key <= '9') {
    if (inputBuffer.length() < 5) {  // Max 5 digits
      inputBuffer += key;
      inputStarted = true;
      
      // Update display with new digit
      lcd.setCursor(5, 2);
      lcd.print(inputBuffer);
      lcd.print(F("     "));  // Clear remaining
    }
    return;
  }
  
  // Handle decimal point (only for temperature)
  if (key == 'A' && currentMode == MODE_CONFIG_TEMP) {
    if (inputBuffer.indexOf('.') == -1 && inputBuffer.length() < 4) {
      inputBuffer += '.';
      lcd.setCursor(5, 2);
      lcd.print(inputBuffer);
    }
    return;
  }
  
  // Handle Clear (C key)
  if (key == 'C') {
    inputBuffer = "";
    inputStarted = false;
    lcd.setCursor(5, 2);
    lcd.print(F("_____    "));
    Serial.println(F("[CONFIG] Input cleared"));
    return;
  }
  
  // Handle Save/Skip (# key)
  if (key == '#') {
    saveCurrentConfig();
    goToNextConfigStep();
    return;
  }
  
  // Handle Cancel (* key) - exit config mode
  if (key == '*') {
    currentMode = MODE_DISPLAY;
    inputBuffer = "";
    Serial.println(F("[CONFIG] Cancelled - returning to display"));
    beep(100);
    return;
  }
}

void saveCurrentConfig() {
  switch (currentMode) {
    case MODE_CONFIG_TEMP:
      if (inputBuffer.length() > 0) {
        float newTemp = inputBuffer.toFloat();
        if (newTemp >= 25.0 && newTemp <= 42.0) {
          targetTemp = newTemp;
          Serial.print(F("[CONFIG] Temperature set to: "));
          Serial.println(targetTemp);
        } else {
          Serial.println(F("[CONFIG] Invalid temp - keeping previous"));
          // Show error briefly
          lcd.setCursor(0, 3);
          lcd.print(F("Invalid! Range:25-42"));
          delay(1000);
        }
      } else {
        Serial.println(F("[CONFIG] Temp skipped - keeping: "));
        Serial.println(targetTemp);
      }
      break;
      
    case MODE_CONFIG_HUM:
      if (inputBuffer.length() > 0) {
        int newHum = inputBuffer.toInt();
        if (newHum >= 20 && newHum <= 90) {
          targetHumidity = (float)newHum;
          Serial.print(F("[CONFIG] Humidity set to: "));
          Serial.println(targetHumidity);
        } else {
          Serial.println(F("[CONFIG] Invalid humidity - keeping previous"));
          lcd.setCursor(0, 3);
          lcd.print(F("Invalid! Range:20-90"));
          delay(1000);
        }
      } else {
        Serial.println(F("[CONFIG] Humidity skipped - keeping: "));
        Serial.println(targetHumidity);
      }
      break;
      
    case MODE_CONFIG_DAYS:
      if (inputBuffer.length() > 0) {
        int newDays = inputBuffer.toInt();
        if (newDays >= 1 && newDays <= 60) {
          targetDays = newDays;
          remainingDays = newDays;
          incubationStartTime = millis();  // Reset timer
          dayStartTime = millis();
          Serial.print(F("[CONFIG] Days set to: "));
          Serial.println(targetDays);
        } else {
          Serial.println(F("[CONFIG] Invalid days - keeping previous"));
          lcd.setCursor(0, 3);
          lcd.print(F("Invalid! Range:1-60 "));
          delay(1000);
        }
      } else {
        Serial.println(F("[CONFIG] Days skipped - keeping: "));
        Serial.println(targetDays);
      }
      break;
      
    default:
      break;
  }
  
  inputBuffer = "";
  inputStarted = false;
}

void goToNextConfigStep() {
  switch (currentMode) {
    case MODE_CONFIG_TEMP:
      currentMode = MODE_CONFIG_HUM;
      Serial.println(F("[CONFIG] Moving to Humidity"));
      break;
      
    case MODE_CONFIG_HUM:
      currentMode = MODE_CONFIG_DAYS;
      Serial.println(F("[CONFIG] Moving to Days"));
      break;
      
    case MODE_CONFIG_DAYS:
      // Configuration complete - save and return to display
      saveSettingsToEEPROM();
      currentMode = MODE_DISPLAY;
      
      // Show confirmation
      lcd.clear();
      lcd.setCursor(2, 0);
      lcd.print(F("SETTINGS SAVED!"));
      lcd.setCursor(0, 1);
      lcd.print(F("Temp: "));
      lcd.print(targetTemp, 1);
      lcd.print(F(" C"));
      lcd.setCursor(0, 2);
      lcd.print(F("Humidity: "));
      lcd.print((int)targetHumidity);
      lcd.print(F(" %"));
      lcd.setCursor(0, 3);
      lcd.print(F("Days: "));
      lcd.print(targetDays);
      
      beep(500);
      delay(2500);
      
      Serial.println(F("[CONFIG] Complete - saved to EEPROM"));
      return;
      
    default:
      break;
  }
  
  displayConfigScreen();
}

// ===============================================================
// UTILITY FUNCTIONS
// ===============================================================
void beep(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

void debugPrint() {
  Serial.println(F("----------------------------------------"));
  Serial.println(F("SYSTEM STATUS:"));
  Serial.print(F("  Mode: "));
  switch (currentMode) {
    case MODE_DISPLAY: Serial.println(F("DISPLAY")); break;
    case MODE_CONFIG_TEMP: Serial.println(F("CONFIG TEMP")); break;
    case MODE_CONFIG_HUM: Serial.println(F("CONFIG HUM")); break;
    case MODE_CONFIG_DAYS: Serial.println(F("CONFIG DAYS")); break;
  }
  Serial.print(F("  Temperature: "));
  Serial.print(currentTemp);
  Serial.print(F(" / Target: "));
  Serial.println(targetTemp);
  Serial.print(F("  Humidity: "));
  Serial.print(currentHumidity);
  Serial.print(F(" / Target: "));
  Serial.println(targetHumidity);
  Serial.print(F("  Days: "));
  Serial.print(targetDays - remainingDays + 1);
  Serial.print(F(" / "));
  Serial.print(targetDays);
  Serial.print(F(" (Remaining: "));
  Serial.print(remainingDays);
  Serial.println(F(")"));
  Serial.print(F("  Heater: "));
  Serial.println(heaterRunning ? F("ON") : F("OFF"));
  Serial.print(F("  Fans: "));
  Serial.println(fansRunning ? F("ON") : F("OFF"));
  Serial.print(F("  Exhaust: "));
  Serial.println(exhaustRunning ? F("ON") : F("OFF"));
  Serial.print(F("  Sensor: "));
  Serial.println(sensorError ? F("ERROR!") : F("OK"));
  Serial.println(F("----------------------------------------"));
}
