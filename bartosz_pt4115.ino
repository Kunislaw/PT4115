#include "EEPROM.h"
//#include <WiFi.h>
#include <vector>
#include <BluetoothSerial.h>
#define EEPROM_SIZE 1024
#define LED_CHANNEL0 0
#define GPIO_DIMMER 16
#define RXD0 3
#define TXD0 1
#define SETTINGS_ADDRESS 0


struct Settings{
  char ssid[64];
  char password[64];
};
Settings settingsFlash;
const char compileDate[] = __DATE__ " " __TIME__;
byte duty = 127;
int freq = 10000;
byte resolution = 8;
char* splitChar = ";";
BluetoothSerial SerialBT;
char espName[32];
char pinChar[9];

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
     SerialBT.printf("BT LED DRIVER 0.1 - %s - Karol Weyna\r\n", compileDate); 
     SerialBT.println("----------- CONFIG --------------------");
     SerialBT.printf("Name: %s\r\n", espName);
     SerialBT.printf("FLASH SIZE: %d\r\n", EEPROM.length());
     SerialBT.printf("SSID: %s PASSWORD: %s\r\n", settingsFlash.ssid, settingsFlash.password);
     SerialBT.println("---------------------------------------");
  }
}

void setup() {
  uint64_t pin = ((((uint64_t)ESP.getEfuseMac() ^ 0xFFFFFFFFFFFFFFFF) + 2137) % 100000000);
  String(pin, DEC).toCharArray(pinChar, 9);
  String("ESP32-"+String(ESP.getEfuseMac(), HEX)).toCharArray(espName, 32);
  SerialBT.begin(espName);
  SerialBT.setPin(pinChar);
  SerialBT.register_callback(callback);
  ledcSetup(LED_CHANNEL0, freq, resolution);
  ledcAttachPin(GPIO_DIMMER, LED_CHANNEL0);
  ledcWrite(LED_CHANNEL0, duty);
  if(!EEPROM.begin(EEPROM_SIZE)){
    SerialBT.println("failed to initialise EEPROM");
  } else {
    EEPROM.get(SETTINGS_ADDRESS, settingsFlash);
    //WiFi.mode(WIFI_STA);
    //WiFi.begin(settingsFlash.ssid, settingsFlash.password);
    //SerialBT.print("Connecting to WiFi ..");
    //while (WiFi.status() != WL_CONNECTED) {
    //  Serial.print('.');
    //  delay(1000);
    //}
    //Serial.println(WiFi.localIP());
  }
}

void loop() {
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
                SerialBT.printf("Settings Wifi - SSID: %s PASSWORD: %s\r\n", ssid, password);
            }
            if(command.equals("setbright")){
              int x = splittedString[2].toInt();
              byte p = (byte)((x/100.0f) * 255);
              duty = p;
              SerialBT.printf("SETBRIGHT - %d RAW(%d)\r\n", x, p);
              ledcWrite(LED_CHANNEL0, duty);
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
