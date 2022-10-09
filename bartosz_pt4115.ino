#include "EEPROM.h"
#include <WiFi.h>
#include <vector>
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

byte duty = 127;
int freq = 10000;
byte resolution = 8;
char* splitChar = ";";

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

void setup() {
  Serial.begin(115200, SERIAL_8N1, RXD0, TXD0);
  ledcSetup(LED_CHANNEL0, freq, resolution);
  ledcAttachPin(GPIO_DIMMER, LED_CHANNEL0);
  ledcWrite(LED_CHANNEL0, duty);
  if(!EEPROM.begin(EEPROM_SIZE)){
    Serial.println("failed to initialise EEPROM");
  } else {
    Serial.print("FLASH SIZE: ");
    Serial.println(EEPROM.length());
    Settings settingsFlash;
    EEPROM.get(SETTINGS_ADDRESS, settingsFlash);
    Serial.println(settingsFlash.ssid);
    Serial.println(settingsFlash.password);
    WiFi.mode(WIFI_STA);
    WiFi.begin(settingsFlash.ssid, settingsFlash.password);
    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print('.');
      delay(1000);
    }
    Serial.println(WiFi.localIP());
  }
}

void loop() {
  // put your main code here, to run repeatedly:

    if(Serial.available() > 0)
    {
        try {
          String str = Serial.readStringUntil('\r');
          std::vector<String> splittedString = splitString(str, *splitChar);
          Serial.print("REC ");
          Serial.println(str);
          String command = splittedString[0];
          
          if(command.equals("setwifi")){
              char ssid[64];
              splittedString[1].toCharArray(ssid, 64);
              char password[64];
              splittedString[2].toCharArray(password, 64);
              Serial.println("Settings Wifi to: ");
              Serial.print("SSID: ");
              Serial.println(ssid);
              Serial.print("PASSWORD: ");
              Serial.println(password);
              Settings settings;
              strcpy(settings.password, password);
              strcpy(settings.ssid, ssid);
              Serial.println(settings.ssid);
              Serial.println(settings.password);
              EEPROM.put(SETTINGS_ADDRESS, settings);
              EEPROM.commit();
          }
          if(command.equals("setbright")){
            int x = splittedString[1].toInt();
            byte p = (byte)((x/100.0f) * 255);
            duty = p;
            Serial.print("P=");
            Serial.println(p);
            ledcWrite(LED_CHANNEL0, duty);
          }

        } catch (int exc){
          Serial.println("ERROR READ UNTIL");
          Serial.println(exc);
        }
    }
}
