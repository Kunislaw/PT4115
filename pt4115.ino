#include "EEPROM.h"
#include <Wire.h>
#include <vector>
#include <BluetoothSerial.h>
#include <DS3231.h>
#include <time.h>
#include <WiFi.h>

#define EEPROM_SIZE 1024
#define LED_CHANNEL_8000K 0
#define LED_CHANNEL_6000K 1
#define LED_CHANNEL_FWS 2
#define LED_CHANNEL_BLUE 3
#define GPIO_DIMMER_8000K 16
#define GPIO_DIMMER_6000K 17
#define GPIO_DIMMER_FWS 4
#define GPIO_DIMMER_BLUE 0
#define PWM_FREQ 10000
#define PWM_RESOLUTION 12
#define PWM_RESOLUTION_VALUE 4095
#define RXD0 3
#define TXD0 1
#define SETTINGS_ADDRESS 0
#define START_STOP_ADDRESS 128
#define MAX_LEVEL_ADDRESS 144
#define I2C_SDA 21
#define I2C_SCL 22
struct Settings{
  char ssid[64];
  char password[64];
};
struct StartStop{
  byte startHour;
  byte startMinute;
  byte stopHour;
  byte stopMinute;
  int dimmS;
  byte startHourBlue;
  byte startMinuteBlue;
  byte stopHourBlue;
  byte stopMinuteBlue;
  int dimmSBlue;
};
struct MaxLevels{
  byte white;
  byte blue;
};


Settings settingsFlash = {
  .ssid = {'\0'},
  .password = {'\0'}
};
StartStop startStop = {
  .startHour = 0,
  .startMinute = 0,
  .stopHour = 0,
  .stopMinute = 0,
  .dimmS = 0,
  .startHourBlue = 0,
  .startMinuteBlue = 0,
  .stopHourBlue = 0,
  .stopMinuteBlue = 0,
  .dimmSBlue = 0
};
MaxLevels maxLevels = {
  .white = 0,
  .blue = 0
};
const char* ntpServer = "pool.ntp.org";
const char compileDate[] = __DATE__ " " __TIME__;
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
byte duty = 127;
int freq = 10000;                                     
byte resolution = 8;
char* splitChar = ";";
BluetoothSerial SerialBT;
char espName[32];
char pinChar[9];
DS3231 myRTC;

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    SerialBT.println("Failed to obtain time");
    return;
  }
  SerialBT.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

std::vector<String> splitString(String str, char splitter){
    std::vector<String> result;
    String current = ""; 
    for(int i = 0; i < str.length(); i++){
        if(str[i] == splitter){
            if(current != ""){
                result.push_back(current);
                current = "";
            } 
            continue;
        }
        current += str[i];
    }
    if(current.length() != 0)
        result.push_back(current);
    return result;
}

int convertPercentToPWMValue(byte percent){
    return (int)((percent/100.0f) * PWM_RESOLUTION_VALUE);
}

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
  if(event == ESP_SPP_SRV_OPEN_EVT){
     DateTime currentMoment = RTClib::now();
     SerialBT.printf("BT LED DRIVER 0.1 - %s - Karol Weyna\r\n", compileDate); 
     SerialBT.println("----------- CONFIG --------------------");
     SerialBT.printf("MAX LEVEL WHITE %d BLUE %d\r\n", maxLevels.white, maxLevels.blue);
     SerialBT.printf("PWM_HZ: %d PWM_RES: %d PWM_MAX: %d\r\n", PWM_FREQ, PWM_RESOLUTION, PWM_RESOLUTION_VALUE);
     SerialBT.printf("%d.%d.%d %d:%d:%d\r\n", currentMoment.day(), currentMoment.month(), currentMoment.year(), currentMoment.hour(), currentMoment.minute(), currentMoment.second());
     SerialBT.printf("START %d:%d STOP %d:%d DIM: %d\r\n", startStop.startHour, startStop.startMinute, startStop.stopHour, startStop.stopMinute, startStop.dimmS);               
     SerialBT.printf("BLUE START %d:%d STOP %d:%d DIM: %d\r\n", startStop.startHourBlue, startStop.startMinuteBlue, startStop.stopHourBlue, startStop.stopMinuteBlue, startStop.dimmSBlue);               
     SerialBT.printf("Name: %s\r\n", espName);
     SerialBT.printf("FLASH SIZE: %d\r\n", EEPROM.length());
     SerialBT.printf("SSID: %s PASSWORD: %s\r\n", settingsFlash.ssid, settingsFlash.password);
     SerialBT.println("---------------------------------------");
     
    
  }
}

void setup() {
  if(!EEPROM.begin(EEPROM_SIZE)){
    SerialBT.println("failed to initialise EEPROM");
  } else {
    EEPROM.get(SETTINGS_ADDRESS, settingsFlash);
    EEPROM.get(START_STOP_ADDRESS, startStop);
    EEPROM.get(MAX_LEVEL_ADDRESS, maxLevels);
  }
  
  uint64_t pin = ((((uint64_t)ESP.getEfuseMac() ^ 0xFFFFFFFFFFFFFFFF) + 2137) % 100000000);
  String(pin, DEC).toCharArray(pinChar, 9);
  String("ESP32-"+String(ESP.getEfuseMac(), HEX)).toCharArray(espName, 32);
  SerialBT.begin(espName);
  SerialBT.setPin(pinChar);
  SerialBT.register_callback(callback);
  //--------------- 8000K --------------------------- //
  ledcSetup(LED_CHANNEL_8000K, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(GPIO_DIMMER_8000K, LED_CHANNEL_8000K);
  //--------------- 6000K --------------------------- //
  ledcSetup(LED_CHANNEL_6000K, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(GPIO_DIMMER_6000K, LED_CHANNEL_6000K);
  //--------------- FWS --------------------------- //
  ledcSetup(LED_CHANNEL_FWS, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(GPIO_DIMMER_FWS, LED_CHANNEL_FWS);
  //--------------- BLUE --------------------------- //
  ledcSetup(LED_CHANNEL_BLUE, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(GPIO_DIMMER_BLUE, LED_CHANNEL_BLUE);
  //------------------------------------------------//
  byte startDuty = convertPercentToPWMValue(10);
  ledcWrite(LED_CHANNEL_8000K, startDuty);
  ledcWrite(LED_CHANNEL_6000K, startDuty);
  ledcWrite(LED_CHANNEL_FWS, startDuty);
  ledcWrite(LED_CHANNEL_BLUE, startDuty);
  Wire.begin(I2C_SDA, I2C_SCL);

  // ------------------ SYNC TIME ------------------------------- //
  WiFi.mode(WIFI_STA);
  WiFi.begin(settingsFlash.ssid, settingsFlash.password);
  SerialBT.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  // ----------------------------------------------------------- //
}


byte setProperLight(long currentTimestamp, long startTimestamp, long stopTimestamp, byte maxLevel, int dimmerS){
    if(startTimestamp <= currentTimestamp && currentTimestamp <= stopTimestamp){
        return maxLevel;
    }
    if(startTimestamp > currentTimestamp){
        long diffBeforeStart = startTimestamp - currentTimestamp;
        if(diffBeforeStart < dimmerS){
            float step = ((float)maxLevel/dimmerS);
            long steps = dimmerS - diffBeforeStart;
            long percentage = step * steps;
            return percentage;

        }
    }
    if(stopTimestamp < currentTimestamp){
        long diffAfterStop = currentTimestamp - stopTimestamp;
        if(diffAfterStop < dimmerS){
            float step = ((float)maxLevel/dimmerS);
            long steps = dimmerS - diffAfterStop;
            long percentage = step * steps;
            return percentage;
        }
    }
    return 0;
}
long millis250msInterval = 0;
void loop() {

    if(millis() > millis250msInterval + 250){
      millis250msInterval = millis();
      DateTime currentMoment = RTClib::now();
      DateTime startMoment = DateTime(currentMoment.year(), currentMoment.month(), currentMoment.day(), startStop.startHour, startStop.startMinute, 0);
      DateTime stopMoment = DateTime(currentMoment.year(), currentMoment.month(), currentMoment.day() + 1, startStop.stopHour, startStop.stopMinute, 0);
      DateTime startMomentBlue = DateTime(currentMoment.year(), currentMoment.month(), currentMoment.day(), startStop.startHourBlue, startStop.startMinuteBlue, 0);
      DateTime stopMomentBlue = DateTime(currentMoment.year(), currentMoment.month(), currentMoment.day() + 1, startStop.stopHourBlue, startStop.stopMinuteBlue, 0);
      if(startStop.startHour <= startStop.stopHour){
        stopMoment = DateTime(currentMoment.year(), currentMoment.month(), currentMoment.day(), startStop.stopHour, startStop.stopMinute, 0);
        stopMomentBlue = DateTime(currentMoment.year(), currentMoment.month(), currentMoment.day(), startStop.stopHourBlue, startStop.stopMinuteBlue, 0);
      }
      byte percent6000K = setProperLight(currentMoment.unixtime(), startMoment.unixtime(), stopMoment.unixtime(), maxLevels.white, startStop.dimmS);
      byte percent8000K = setProperLight(currentMoment.unixtime(), startMoment.unixtime(), stopMoment.unixtime(), maxLevels.white, startStop.dimmS);
      byte percentFWS = setProperLight(currentMoment.unixtime(), startMoment.unixtime(), stopMoment.unixtime(), maxLevels.white, startStop.dimmS);
      byte percentBlue = setProperLight(currentMoment.unixtime(), startMomentBlue.unixtime(), stopMomentBlue.unixtime(), maxLevels.blue, startStop.dimmSBlue);
      int duty6000K = convertPercentToPWMValue(percent6000K);
      int duty8000K = convertPercentToPWMValue(percent8000K);
      int dutyFWS = convertPercentToPWMValue(percentFWS);
      int dutyBlue = convertPercentToPWMValue(percentBlue);
      ledcWrite(LED_CHANNEL_6000K, duty6000K);
      ledcWrite(LED_CHANNEL_8000K, duty8000K);
      ledcWrite(LED_CHANNEL_FWS, dutyFWS);
      ledcWrite(LED_CHANNEL_BLUE, dutyBlue);


      SerialBT.printf("6000K %d 8000K %d FWS %d BLUE %d\r\n", duty6000K, duty8000K, dutyFWS, dutyBlue);
    }
    if(SerialBT.available())
    {
        try {
          String str = SerialBT.readStringUntil('\r');
          SerialBT.println(String("REC-"+str));
          std::vector<String> splittedString = splitString(str, *splitChar);
          String password = splittedString[0];
          if(password.equals(String(pinChar))){
            String command = splittedString[1];
            if(command.equals("setwifi")){
                char ssid[64];
                splittedString[2].toCharArray(ssid, 64);
                char password[64];
                splittedString[3].toCharArray(password, 64);
                Settings settings;
                strcpy(settings.password, password);
                strcpy(settings.ssid, ssid);
                EEPROM.put(SETTINGS_ADDRESS, settings);
                EEPROM.commit();
                SerialBT.printf("Settings Wifi - SSID: %s PASSWORD: %s, Restarting...\r\n", ssid, password);
                ESP.restart();
            }
            if(command.equals("setbright")){
              int channel = splittedString[2].toInt();
              int percent = splittedString[3].toInt();
              SerialBT.printf("SETBRIGHT - CHANNEL %d PERCENT %d\r\n", channel, percent);
              ledcWrite(channel, convertPercentToPWMValue(percent));
            }
            if(command.equals("setdate")){
              int day = splittedString[2].toInt();
              int month = splittedString[3].toInt();
              int year = splittedString[4].toInt();
              year -= 2000;
              int hour = splittedString[5].toInt();
              int minutes = splittedString[6].toInt();
              int seconds = splittedString[7].toInt();
              
              myRTC.setClockMode(false);//24-hour mode
              myRTC.setSecond(seconds);
              myRTC.setMinute(minutes);
              myRTC.setHour(hour);
              myRTC.setDate(day);
              myRTC.setMonth(month);
              myRTC.setYear(year);
              SerialBT.printf("SET DATE %d.%d.%d %d:%d:%d\r\n", day, month, year, hour, minutes, seconds);
            }
            if(command.equals("setinterval")){
              int startHour = splittedString[2].toInt();
              int startMinute = splittedString[3].toInt();
              int stopHour = splittedString[4].toInt();
              int stopMinute = splittedString[5].toInt();
              int dimmS = splittedString[6].toInt();
              int startHourBlue = splittedString[7].toInt();
              int startMinuteBlue = splittedString[8].toInt();
              int stopHourBlue = splittedString[9].toInt();
              int stopMinuteBlue = splittedString[10].toInt();
              int dimmSBlue = splittedString[11].toInt();
              startStop.startHour = startHour;
              startStop.startMinute = startMinute;
              startStop.stopHour = stopHour;
              startStop.stopMinute = stopMinute;
              startStop.dimmS = dimmS;
              startStop.startHourBlue = startHourBlue;
              startStop.startMinuteBlue = startMinuteBlue;
              startStop.stopHourBlue = stopHourBlue;
              startStop.stopMinuteBlue = stopMinuteBlue;
              startStop.dimmSBlue = dimmSBlue;
              EEPROM.put(START_STOP_ADDRESS, startStop);
              EEPROM.commit();
              SerialBT.printf("START %d:%d STOP %d:%d\r\n", startHour, startMinute, stopHour, stopMinute);
              SerialBT.printf("START BLUE %d:%d STOP %d:%d\r\n", startHourBlue, startMinuteBlue, stopHourBlue, stopMinuteBlue);                          
            }
            if(command.equals("maxlevel")){
              int maxWhite = splittedString[2].toInt();
              int maxBlue = splittedString[3].toInt();
              maxLevels.white = maxWhite;
              maxLevels.blue = maxBlue;
              EEPROM.put(MAX_LEVEL_ADDRESS, maxLevels);
              EEPROM.commit();
              SerialBT.printf("MAX LEVEL SET WHITE %d BLUE \r\n", maxWhite, maxBlue);                          
            }
         } else {
           SerialBT.println("WRONG PASSWORD");
         }
        } catch (int exc){
          SerialBT.println("ERROR READ UNTIL");
          SerialBT.println(exc);
        }
    }
}
