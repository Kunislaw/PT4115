#include "EEPROM.h"
#include <Wire.h>
#include <vector>
#include <BluetoothSerial.h>
#include <DS3231.h>


#define EEPROM_SIZE 1024
#define LED_CHANNEL0 0
#define GPIO_DIMMER 16
#define RXD0 3
#define TXD0 1
#define SETTINGS_ADDRESS 0
#define START_STOP_ADDRESS 128
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
  .dimmS = 0
};
const char compileDate[] = __DATE__ " " __TIME__;
byte duty = 127;
int freq = 10000;                                     
byte resolution = 8;
char* splitChar = ";";
BluetoothSerial SerialBT;
char espName[32];
char pinChar[9];
DS3231 myRTC;

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

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
  if(event == ESP_SPP_SRV_OPEN_EVT){
     DateTime currentMoment = RTClib::now();
     SerialBT.printf("BT LED DRIVER 0.1 - %s - Karol Weyna\r\n", compileDate); 
     SerialBT.println("----------- CONFIG --------------------");
     SerialBT.printf("%d.%d.%d %d:%d:%d\r\n", currentMoment.day(), currentMoment.month(), currentMoment.year(), currentMoment.hour(), currentMoment.minute(), currentMoment.second());
     SerialBT.printf("START %d:%d STOP %d:%d DIM: %d\r\n", startStop.startHour, startStop.startMinute, startStop.stopHour, startStop.stopMinute, startStop.dimmS);               
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
    //WiFi.mode(WIFI_STA);
    //WiFi.begin(settingsFlash.ssid, settingsFlash.password);
    //SerialBT.print("Connecting to WiFi ..");
    //while (WiFi.status() != WL_CONNECTED) {
    //  Serial.print('.');
    //  delay(1000);
    //}
    //Serial.println(WiFi.localIP());
  }
  uint64_t pin = ((((uint64_t)ESP.getEfuseMac() ^ 0xFFFFFFFFFFFFFFFF) + 2137) % 100000000);
  String(pin, DEC).toCharArray(pinChar, 9);
  String("ESP32-"+String(ESP.getEfuseMac(), HEX)).toCharArray(espName, 32);
  SerialBT.begin(espName);
  SerialBT.setPin(pinChar);
  SerialBT.register_callback(callback);
  ledcSetup(LED_CHANNEL0, freq, resolution);
  ledcAttachPin(GPIO_DIMMER, LED_CHANNEL0);
  ledcWrite(LED_CHANNEL0, duty);
  Wire.begin(I2C_SDA, I2C_SCL);
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

void loop() {
    DateTime currentMoment = RTClib::now();
    DateTime startMoment = DateTime(currentMoment.year(), currentMoment.month(), currentMoment.day(), startStop.startHour, startStop.startMinute, 0);
    DateTime stopMoment = DateTime(currentMoment.year(), currentMoment.month(), currentMoment.day() + 1, startStop.stopHour, startStop.stopMinute, 0);
    if(startStop.startHour <= startStop.stopHour){
      stopMoment = DateTime(currentMoment.year(), currentMoment.month(), currentMoment.day(), startStop.stopHour, startStop.stopMinute, 0);
    }
    byte percent6000K = setProperLight(currentMoment.unixtime(), startMoment.unixtime(), stopMoment.unixtime(), 100, startStop.dimmS);
    byte percent8000K = setProperLight(currentMoment.unixtime(), startMoment.unixtime(), stopMoment.unixtime(), 100, startStop.dimmS);
    byte percentFWS = setProperLight(currentMoment.unixtime(), startMoment.unixtime(), stopMoment.unixtime(), 100, startStop.dimmS);
    byte percentBlue = setProperLight(currentMoment.unixtime(), startMoment.unixtime(), stopMoment.unixtime(), 100, startStop.dimmS);
    byte duty6000K = (byte)((percent6000K/100.0f) * 255);
    byte duty8000K = (byte)((percent8000K/100.0f) * 255);
    byte dutyFWS = (byte)((percentFWS/100.0f) * 255);
    byte dutyBlue = (byte)((percentBlue/100.0f) * 255);
    ledcWrite(LED_CHANNEL0, duty6000K);
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
              int x = splittedString[2].toInt();
              byte p = (byte)((x/100.0f) * 255);
              duty = p;
              SerialBT.printf("SETBRIGHT - %d RAW(%d)\r\n", x, p);
              ledcWrite(LED_CHANNEL0, duty);
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
              startStop.startHour = startHour;
              startStop.startMinute = startMinute;
              startStop.stopHour = stopHour;
              startStop.stopMinute = stopMinute;
              startStop.dimmS = dimmS;
              EEPROM.put(START_STOP_ADDRESS, startStop);
              EEPROM.commit();
              SerialBT.printf("START %d:%d STOP %d:%d\r\n", startHour, startMinute, stopHour, stopMinute);               
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
