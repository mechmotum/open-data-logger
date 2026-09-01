#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <ICM_20948.h>
#include <Adafruit_BNO055.h>
#include <SoftwareSerial.h>
#include <SD.h>
#include <TimeLib.h>

U8G2_SSD1305_128X32_ADAFRUIT_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
ICM_20948_I2C ICM;
SoftwareSerial ss(4, 3);
Adafruit_BNO055 BNO = Adafruit_BNO055(55, 0x29);

const int rotaryPinA = 34;
const int rotaryPinB = 33;
const int rotaryButtonPin = 37;
const int backButtonPin = 36;
const int chipSelect = BUILTIN_SDCARD;
const int steerMin = -135;
const int steerMax = 135;
int steerRawValue = 0;
float steerAngleValue = 0.0;
const int redPin = 39;
const int greenPin = 35;

// Pins worden later via de config overschreven
int steerPin = -1; 
int wheelSpeedPin = -1; 

bool magnetDetected = false;
unsigned long lastPulseTime = 0;

const float wheelCircumference = 2.0;
float processedWheelRpm = 0.0;
float speedKmh = 0.0;
volatile unsigned long isrLastPulseTime = 0;
volatile unsigned long isrPulseInterval = 0;
volatile bool newPulse = false;

// Booleans om bij te houden of sensoren in de config zijn geactiveerd
bool isSteerAngleConfigured = false;
bool isWheelSpeedConfigured = false;

int potVal = 0;
float steerVal = 0.0;
float wheelRpm = 0.0;

const int maxFiles = 10;
int fileCounter = 0;

String fileList[maxFiles];
String configDir = "/config/";

int lastRTCSec = -1;
char cachedDateTime[25];
String logLine = "";

unsigned long measurementStartTime = 0;
String currentFileName = "DATA_001.CSV";

enum StateID {
  STATE_MENU_START,
  STATE_MAIN_MENU,
  STATE_SENSOR_MENU,
  STATE_SETTINGS_MENU,
  STATE_MEASURE_MENU,
  STATE_VIEW_CONFIG,
  STATE_MAINIMU_MENU,
  STATE_MENU_END,
// DIVIDER
  STATE_VIEW_ANGLES,
  STATE_VIEW_ACCEL,
  STATE_VIEW_GYRO,
  STATE_VIEW_MAG,
  STATE_ACTION_ZERO_RESET,
  STATE_VIEW_GPS,
  STATE_VIEW_STEER,
  STATE_VIEW_WHEELSPEED,
  STATE_MEASURING,
  STATE_VIEW_MEASUREMENTS,
  STATE_VIEW_RTC
};

StateID currentState = STATE_MAIN_MENU;

struct MenuItem {
  const char* label;
  StateID nextState;
};

const MenuItem mainMenu[] = {
  { "Sensors >", STATE_SENSOR_MENU },
  { "Settings >", STATE_SETTINGS_MENU },
  { "Measure >", STATE_MEASURE_MENU }  
};

String mainIMULabel = "Main IMU ()";

MenuItem sensorMenu[] = {
  { "GPS", STATE_VIEW_GPS },
  { mainIMULabel.c_str(), STATE_MAINIMU_MENU },
  { "Steering Angle", STATE_VIEW_STEER },
  { "Wheel Speed", STATE_VIEW_WHEELSPEED }
};

const MenuItem mainIMUMenu[] = { 
  { "Angles", STATE_VIEW_ANGLES }, 
  { "Accel", STATE_VIEW_ACCEL }, 
  { "Gyro", STATE_VIEW_GYRO }, 
  { "Magnet", STATE_VIEW_MAG } 
};

const MenuItem settingsMenu[] = {
  { "Set Zero", STATE_ACTION_ZERO_RESET },
  { "Info", STATE_MAIN_MENU },
  { "Load Config", STATE_VIEW_CONFIG },
  { "View RTC Time", STATE_VIEW_RTC }
};

const MenuItem measureMenu[] = {
  { "Start Measuring", STATE_MEASURING},
  { "Saved Measurements", STATE_VIEW_MEASUREMENTS}
};

MenuItem configMenu[maxFiles];

const MenuItem* currentMenuPtr = mainMenu;
int currentMenuLength = 3;
int menuIndex = 0;

int aState, aLastState;
int selectBtnState, lastSelectBtnState = HIGH;
int backBtnState, lastBackBtnState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
unsigned long msAtLastRTCSecond = 0;

String mainIMU;
bool singleFile = false;

unsigned long lastDisplayTime = 0;
const unsigned long displayInterval = 50;

unsigned long lastLogTime = 0;
const unsigned long logInterval = 10;

double roll = 0.0, pitch = 0.0, yaw = 0.0;
double roll0 = 0.0, pitch0 = 0.0, yaw0 = 0.0;
double rollZ = 0.0, pitchZ = 0.0, yawZ = 0.0;

bool menuCheck(StateID state);
void drawMenu();
void drawData();
void applyConfig(String selectedFile);
void listFiles(File dir);
time_t getTeensy3Time();

template <size_t N>
void switchMenu(const MenuItem (&newMenu)[N]) {
  currentMenuPtr = newMenu;
  currentMenuLength = N;
  menuIndex = 0;
}

void switchMenu(const MenuItem* newMenu, int length) {
  currentMenuPtr = newMenu;
  currentMenuLength = length;
  menuIndex = 0;
}

void setup() {
  Serial.begin(115200);
  setSyncProvider(getTeensy3Time);

  delay(1000);

  if (timeStatus() != timeSet) {
    Serial.println("Unable to sync with the RTC");
  } else {
    Serial.println("RTC has set the system time");
  }

  pinMode(rotaryPinA, INPUT_PULLUP);
  pinMode(rotaryPinB, INPUT_PULLUP);
  pinMode(rotaryButtonPin, INPUT_PULLUP);
  pinMode(backButtonPin, INPUT_PULLUP);
  digitalWrite(greenPin, HIGH);

  aLastState = digitalRead(rotaryPinA);

  if (SD.begin(chipSelect)) {
    File folder = SD.open("/config/");
    if (folder) {
      listFiles(folder);
      folder.close();
    } else {
      Serial.println("Kan map /config/ niet vinden!");
    }
  } else {
    Serial.println("SD failed");
  }

  // 1. Configuratie inladen
  if (SD.exists("/config/config.txt")) {
    applyConfig("config.txt");
  }

  // 2. Pas instellen na het inladen van de config
  if (isSteerAngleConfigured && steerPin != -1) {
    pinMode(steerPin, INPUT);
    Serial.print("Steer angle sensor geconfigureerd op pin: ");
    Serial.println(steerPin);
  }
  if (isWheelSpeedConfigured && wheelSpeedPin != -1) {
    pinMode(wheelSpeedPin, INPUT_PULLUP);
    Serial.print("Wheel speed sensor geconfigureerd op pin: ");
    Serial.println(wheelSpeedPin);
  }

  Wire.begin();
  Wire.setClock(400000);
  u8g2.begin();

  ICM.begin(Wire, 1);
  ICM.initializeDMP();
  ICM.enableDMPSensor(INV_ICM20948_SENSOR_GAME_ROTATION_VECTOR);

  ICM.setDMPODRrate(DMP_ODR_Reg_Quat6, 0);
  ICM.enableFIFO();
  ICM.enableDMP();
  ICM.resetDMP();
  ICM.resetFIFO();
}

void handleBackPress() {
  switch (currentState) {
    case STATE_VIEW_CONFIG:
    case STATE_VIEW_RTC:
      currentState = STATE_SETTINGS_MENU;
      switchMenu(settingsMenu);
      break;
    case STATE_VIEW_ANGLES:
    case STATE_VIEW_ACCEL:
    case STATE_VIEW_GYRO:
    case STATE_VIEW_MAG:
      currentState = STATE_MAINIMU_MENU;
      switchMenu(mainIMUMenu); 
      break;
    case STATE_MAINIMU_MENU:
    case STATE_VIEW_GPS:
    case STATE_VIEW_STEER:
    case STATE_VIEW_WHEELSPEED:
      currentState = STATE_SENSOR_MENU;
      switchMenu(sensorMenu); 
      break;
    case STATE_SENSOR_MENU:
    case STATE_SETTINGS_MENU:
    case STATE_MEASURE_MENU:
    case STATE_MEASURING:
      currentState = STATE_MAIN_MENU;
      switchMenu(mainMenu);
      break;
    case STATE_MAIN_MENU:
      break;
  }
}

void handleSelectPress() {
  bool isMenu = menuCheck(currentState);
  if (isMenu) {
    if (currentState == STATE_VIEW_CONFIG) {
      String selectedFile = fileList[menuIndex];
      Serial.print("Loading file: ");
      Serial.println(selectedFile);
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.setCursor(5, 15);
      u8g2.print("Applied:");
      u8g2.setCursor(5, 28);
      u8g2.print(selectedFile);
      u8g2.sendBuffer();
      delay(1000);
      currentState = STATE_SETTINGS_MENU;
      switchMenu(settingsMenu);
      applyConfig(selectedFile);
      return; 
    }

    StateID target = currentMenuPtr[menuIndex].nextState;
    switch (target) {
      case STATE_MAINIMU_MENU:
        currentState = STATE_MAINIMU_MENU;
        switchMenu(mainIMUMenu); 
        break;
      case STATE_SENSOR_MENU:
        currentState = STATE_SENSOR_MENU;
        switchMenu(sensorMenu); 
        break;
      case STATE_SETTINGS_MENU:
        currentState = STATE_SETTINGS_MENU;
        switchMenu(settingsMenu);
        break;
      case STATE_MEASURE_MENU:
        currentState = STATE_MEASURE_MENU;
        switchMenu(measureMenu);
        break;
      case STATE_MEASURING: {
        currentState = STATE_MEASURING;
        measurementStartTime = millis();
        
        int highestIndex = 0;
        char tempName[20];
        
        for (int i = 1; i <= 999; i++) {
          snprintf(tempName, sizeof(tempName), "/data/DATA%03d.CSV", i);
          if (SD.exists(tempName)) {
            highestIndex = i;
          } else {
            break;
          }
        }
        
        int fileIndex;
        if (singleFile) {
          fileIndex = (highestIndex > 0) ? highestIndex : 1;
        } else {
          fileIndex = (highestIndex > 0) ? highestIndex + 1 : 1;
        }
        
        snprintf(tempName, sizeof(tempName), "/data/DATA%03d.CSV", fileIndex);
        currentFileName = String(tempName);
      
        File dataFile = SD.open(currentFileName.c_str(), FILE_WRITE);
        if (dataFile) {
          if (dataFile.size() == 0) {
            dataFile.println("Timestamp,Roll,Pitch,Yaw,AccX,AccY,AccZ,GyrX,GyrY,GyrZ,MagX,MagY,MagZ,SteerAngle,RearWheelSpeed");
          }
          dataFile.close();
          Serial.print("Klaar voor meting in bestand: ");
          Serial.println(currentFileName);
        } else {
          Serial.println("Fout bij openen/maken van meetbestand!");
          currentFileName = "SD ERROR";
        }
        
        break; 
      }
      case STATE_ACTION_ZERO_RESET:
        roll0 = roll;
        pitch0 = pitch;
        yaw0 = yaw;
        break;
      case STATE_VIEW_CONFIG:
        if (fileCounter > 0) {
          currentState = STATE_VIEW_CONFIG;
          switchMenu((const MenuItem*)configMenu, fileCounter);
        } else {
          Serial.println("Geen config bestanden gevonden.");
        }
        break;
      default:
        currentState = target;
        break;
    }
  }
}

void applyConfig(String selectedFile) {
  String path = "/config/" + selectedFile;
  File file = SD.open(path.c_str());

  if (!file) {
    Serial.println("Kan configuratiebestand niet openen!");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0 || line.startsWith("//") || line.startsWith("#")) {
      continue;
    }

    int equalsIndex = line.indexOf('=');
    if (equalsIndex > 0) {
      String key = line.substring(0, equalsIndex);
      String value = line.substring(equalsIndex + 1);

      key.trim();
      value.trim();
      if (key == "mainIMU") {
        mainIMU = value;
        Serial.print("mainIMU ingeladen: ");
        Serial.println(mainIMU);
        mainIMULabel = "Main IMU (" + mainIMU + ")";
        sensorMenu[1].label = mainIMULabel.c_str();
      } else if (key == "singleFile") {
        value.toLowerCase(); 
        singleFile = (value.indexOf("true") >= 0 || value.indexOf("1") >= 0);
        Serial.print("singleFile ingeladen: ");
        Serial.println(singleFile ? "true" : "false");
      } else if (key.startsWith("slot")) {
        int pin = -1;
        // Koppel de geconfigureerde slots aan de hardware pinnen volgens de README
        if (key == "slot1") pin = A1;       // Pin A1 / 15
        else if (key == "slot2") pin = 16;  // Pin A2 / 16
        else if (key == "slot3") pin = 17;  // Pin A3 / 17
        else if (key == "slot4") pin = 20;  // Pin A6 / 20

        value.toLowerCase();
        if (value == "steerangle" && pin != -1) {
          steerPin = pin;
          isSteerAngleConfigured = true;
        } else if (value == "wheelspeed" && pin != -1) {
          wheelSpeedPin = pin;
          isWheelSpeedConfigured = true;
        }
      }
    }
  }
  file.close();
}

void listFiles(File dir) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }
    if (fileCounter < maxFiles) {
      fileList[fileCounter] = String(entry.name());
      configMenu[fileCounter].label = fileList[fileCounter].c_str();
      configMenu[fileCounter].nextState = STATE_VIEW_CONFIG;
      fileCounter++;
    }
    Serial.println(entry.name());
    entry.close();
  }
}

void getICMdata() {
  icm_20948_DMP_data_t data;

  ICM.getAGMT();
  ICM.readDMPdataFromFIFO(&data);

  while ((ICM.status == ICM_20948_Stat_Ok) || (ICM.status == ICM_20948_Stat_FIFOMoreDataAvail)) {
    if ((data.header & DMP_header_bitmap_Quat6) > 0) {
      double q1 = ((double)data.Quat6.Data.Q1) / 1073741824.0;
      double q2 = ((double)data.Quat6.Data.Q2) / 1073741824.0;
      double q3 = ((double)data.Quat6.Data.Q3) / 1073741824.0;

      double sumSq = (q1 * q1) + (q2 * q2) + (q3 * q3);
      if (sumSq > 1.0) sumSq = 1.0;
      double q0 = sqrt(1.0 - sumSq);

      double qw = q0;
      double qx = q2;
      double qy = q1;
      double qz = -q3;

      double t0 = +2.0 * (qw * qx + qy * qz);
      double t1 = +1.0 - 2.0 * (qx * qx + qy * qy);
      pitch = -atan2(t0, t1) * 180.0 / PI;
      pitchZ = pitch - pitch0;

      double t2 = +2.0 * (qw * qy - qx * qz);
      t2 = t2 > 1.0 ? 1.0 : t2;
      t2 = t2 < -1.0 ? -1.0 : t2;
      roll = asin(t2) * 180.0 / PI;
      rollZ = roll - roll0;

      double t3 = +2.0 * (qw * qz + qx * qy);
      double t4 = +1.0 - 2.0 * (qy * qy + qz * qz);
      yaw = atan2(t3, t4) * 180.0 / PI;
      yawZ = yaw - yaw0;
    }
    ICM.readDMPdataFromFIFO(&data);
  }
}

void getBNOdata() {
}

void wheelSpeedISR() {
  unsigned long currentTime = millis();
  unsigned long timeDiff = currentTime - isrLastPulseTime;
  
  if (timeDiff > 100) {
    isrPulseInterval = timeDiff;
    isrLastPulseTime = currentTime;
    newPulse = true;
  }
}

time_t getTeensy3Time() {
  return Teensy3Clock.get();
}

#define TIME_HEADER "T"
unsigned long processSyncMessage() {
  unsigned long pctime = 0L;
  const unsigned long DEFAULT_TIME = 1357041600;

  if (Serial.find(TIME_HEADER)) {
    pctime = Serial.parseInt();
    if (pctime < DEFAULT_TIME) {
      pctime = 0L;
    }
  }
  return pctime;
}

String getFormattedTimestamp() {
  if (timeStatus() == timeSet) {
    int currentSec = second();

    if (currentSec != lastRTCSec) {
      snprintf(cachedDateTime, sizeof(cachedDateTime), "[%04d-%02d-%02d %02d:%02d:%02d",
               year(), month(), day(), hour(), minute(), currentSec);
      lastRTCSec = currentSec;
      
      msAtLastRTCSecond = millis(); 
    }

    unsigned long currentMs = millis() - msAtLastRTCSecond;
    
    if (currentMs > 999) {
      currentMs = 999;
    }

    char fullTimestamp[35];
    snprintf(fullTimestamp, sizeof(fullTimestamp), "%s.%03d]", cachedDateTime, currentMs);

    return String(fullTimestamp);
  } else {
    char fallback[20];
    snprintf(fallback, sizeof(fallback), "[ms: %lu]", millis());
    return String(fallback);
  }
}

void saveDataToFile() {
  if (millis() - lastLogTime >= logInterval) {
    lastLogTime = millis();

    File dataFile = SD.open(currentFileName.c_str(), FILE_WRITE);
    
    if (dataFile) {
      dataFile.print(logLine); dataFile.print(",");
      
      dataFile.print(rollZ, 2); dataFile.print(",");
      dataFile.print(pitchZ, 2); dataFile.print(",");
      dataFile.print(yawZ, 2); dataFile.print(",");
      
      dataFile.print(ICM.agmt.acc.axes.x / 1000.00, 3); dataFile.print(",");
      dataFile.print(ICM.agmt.acc.axes.y / 1000.00, 3); dataFile.print(",");
      dataFile.print(ICM.agmt.acc.axes.z / 1000.00, 3); dataFile.print(",");
      
      dataFile.print(ICM.agmt.gyr.axes.x); dataFile.print(",");
      dataFile.print(ICM.agmt.gyr.axes.y); dataFile.print(",");
      dataFile.print(ICM.agmt.gyr.axes.z); dataFile.print(",");
      
      dataFile.print(ICM.agmt.mag.axes.x); dataFile.print(",");
      dataFile.print(ICM.agmt.mag.axes.y); dataFile.print(",");
      dataFile.print(ICM.agmt.mag.axes.z); dataFile.print(",");
      
      dataFile.print(steerVal, 2); dataFile.print(",");
      dataFile.println(speedKmh,2);

      dataFile.close();
    } else {
      Serial.println("Fout bij schrijven van data naar SD!");
    }
  }
}

void loop() {
  getICMdata();
  
  // Alleen analogRead uitvoeren als steerPin correct geconfigureerd is via config.txt
  if (isSteerAngleConfigured && steerPin != -1) {
    potVal = analogRead(steerPin);
    steerVal = map(potVal, 0, 1023, steerMin * 100, steerMax * 100) / 100.0;
  }
  
  if (newPulse) {
    noInterrupts();
    unsigned long interval = isrPulseInterval;  
    newPulse = false;
    interrupts();
    
    if (interval > 0) {
      wheelRpm = 60000.0 / interval;
      speedKmh = (wheelRpm * wheelCircumference * 60.0) / 1000.0;
    }
  }

  if (millis() - isrLastPulseTime > 2000) {
    wheelRpm = 0.0;
    speedKmh = 0.0;
  }
  
  while (ss.available() > 0) {
    byte gpsData = ss.read();
    Serial.write(gpsData);
  }

  if (Serial.available()) {
    time_t t = processSyncMessage();
    if (t != 0) {
      Teensy3Clock.set(t);
      setTime(t);
    }
  }

  logLine = getFormattedTimestamp();

  if (currentState == STATE_MEASURING) {
    saveDataToFile();
  }

  bool isMenu = menuCheck(currentState);

  aState = digitalRead(rotaryPinA);
  if (isMenu && aState == LOW && aLastState == HIGH) {
    if (digitalRead(rotaryPinB) == LOW) {
      menuIndex++;
    } else {
      menuIndex--;
    }

    if (menuIndex >= currentMenuLength) menuIndex = 0;
    if (menuIndex < 0) menuIndex = currentMenuLength - 1;
  }
  aLastState = aState;

  int readingSelect = digitalRead(rotaryButtonPin);
  if (readingSelect != lastSelectBtnState && readingSelect == LOW) {
    if (millis() - lastDebounceTime > debounceDelay) {
      handleSelectPress();
      lastDebounceTime = millis();
    }
  }
  lastSelectBtnState = readingSelect;

  int readingBack = digitalRead(backButtonPin);
  if (readingBack != lastBackBtnState && readingBack == LOW) {
    delay(50);
    if (digitalRead(backButtonPin) == LOW) handleBackPress();
  }
  lastBackBtnState = readingBack;

  if (millis() - lastDisplayTime > displayInterval) {
    lastDisplayTime = millis();

    u8g2.clearBuffer();
    if (isMenu) {
      drawMenu();
    } else {
      drawData();
    }
    u8g2.sendBuffer();
  }
}

void drawMenu() {
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setFontPosBaseline();

  int prevIndex = (menuIndex - 1 + currentMenuLength) % currentMenuLength;
  int nextIndex = (menuIndex + 1) % currentMenuLength;

  u8g2.setCursor(12, 8);
  u8g2.print(currentMenuPtr[prevIndex].label);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 19);
  u8g2.print("> ");
  u8g2.print(currentMenuPtr[menuIndex].label);

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(12, 30);
  u8g2.print(currentMenuPtr[nextIndex].label);
}

void drawData() {
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setDrawColor(1);

  u8g2.drawBox(0, 0, 128, 9);
  u8g2.setDrawColor(0);
  u8g2.setCursor(5, 7);

  double v1 = 0, v2 = 0, v3 = 0;
  const char *l1 = "", *l2 = "", *l3 = "";

  switch (currentState) {
    case STATE_VIEW_RTC: {
      u8g2.print("RTC TIME");
      
      if (timeStatus() == timeSet) {
        char dateStr[20];
        char timeStr[20];
        
        snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", year(), month(), day());
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", hour(), minute(), second());

        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.setDrawColor(1);
        
        u8g2.setCursor(5, 18);
        u8g2.print("Date: ");
        u8g2.print(dateStr);
        
        u8g2.setCursor(5, 28);
        u8g2.print("Time: ");
        u8g2.print(timeStr);
      } else {
        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.setDrawColor(1);
        u8g2.setCursor(5, 18);
        u8g2.print("RTC Not Synced!");
        u8g2.setCursor(5, 28);
        u8g2.print("Waiting for GPS/PC...");
      }
      return;
    }
    case STATE_MEASURING: {
      u8g2.print("MEASURING");
      
      unsigned long elapsedMillis = millis() - measurementStartTime;
      unsigned long elapsedSeconds = elapsedMillis / 1000;
      int mins = elapsedSeconds / 60;
      int secs = elapsedSeconds % 60;
      
      char timeStr[10];
      snprintf(timeStr, sizeof(timeStr), "%02d:%02d", mins, secs);

      u8g2.setFont(u8g2_font_5x7_tr);
      u8g2.setDrawColor(1);
      u8g2.setCursor(5, 18);
      u8g2.print("File: ");
      u8g2.print(currentFileName);
      
      u8g2.setCursor(5, 28);
      u8g2.print("Time: ");
      u8g2.print(timeStr);
      
      return;
    }
    case STATE_VIEW_ANGLES:
      u8g2.print("ANGLES");
      l1 = "Roll : ";
      v1 = rollZ;
      l2 = "Pitch: ";
      v2 = pitchZ;
      l3 = "Yaw  : ";
      v3 = yawZ;
      break;
    case STATE_VIEW_ACCEL:
      u8g2.print("ACCEL");
      l1 = "Acc X: ";
      v1 = ICM.agmt.acc.axes.x / 1000.00;
      l2 = "Acc Y: ";
      v2 = ICM.agmt.acc.axes.y / 1000.00;
      l3 = "Acc Z: ";
      v3 = ICM.agmt.acc.axes.z / 1000.00;
      break;
    case STATE_VIEW_GYRO:
      u8g2.print("GYRO");
      l1 = "Gyr X: ";
      v1 = ICM.agmt.gyr.axes.x;
      l2 = "Gyr Y: ";
      v2 = ICM.agmt.gyr.axes.y;
      l3 = "Gyr Z: ";
      v3 = ICM.agmt.gyr.axes.z;
      break;
    case STATE_VIEW_MAG:
      u8g2.print("MAGNET");
      l1 = "Mag X: "; v1 = ICM.agmt.mag.axes.x;
      l2 = "Mag Y: "; v2 = ICM.agmt.mag.axes.y;
      l3 = "Mag Z: "; v3 = ICM.agmt.mag.axes.z;
      break;
    case STATE_VIEW_STEER:
      u8g2.print("STEER");
      l1 = "Raw Value: "; v1 = potVal;
      l2 = "Angle: "; v2 = steerVal;
      l3 = ""; v3 = 0;
      break;
    case STATE_VIEW_WHEELSPEED:
      u8g2.print("WHEEL SPEED");
      l1 = "Raw: "; v1 = newPulse;
      l2 = "RPM: "; v2 = wheelRpm;
      l3 = "Km/h: "; v3 = speedKmh;
      break;
    default:
      u8g2.setDrawColor(1);
      return;
  }
  
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setDrawColor(1);
  u8g2.setCursor(5, 18);
  u8g2.print(l1);
  u8g2.print(v1);
  u8g2.setCursor(5, 25);
  u8g2.print(l2);
  u8g2.print(v2);
  u8g2.setCursor(5, 32);
  u8g2.print(l3);
  if (l3[0] != '\0') u8g2.print(v3);
}

bool menuCheck(StateID state) {
  return (state > STATE_MENU_START && state < STATE_MENU_END);
}