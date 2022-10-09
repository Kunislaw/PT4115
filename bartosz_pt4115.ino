#define LED_CHANNEL0 0
#define GPIO_DIMMER 16
#define RXD0 3
#define TXD0 1


byte duty = 127;
int freq = 10000;
byte resolution = 8;
void setup() {
  Serial.begin(115200, SERIAL_8N1, RXD0, TXD0);
  ledcSetup(LED_CHANNEL0, freq, resolution);
  ledcAttachPin(GPIO_DIMMER, LED_CHANNEL0);
  ledcWrite(LED_CHANNEL0, duty);

}

void loop() {
  // put your main code here, to run repeatedly:

    if(Serial.available() > 0)
    {
        try {
          String str = Serial.readStringUntil('\r');
          Serial.print("REC ");
          Serial.println(str);
          byte x = str.toInt();
          byte p = (byte)((x/100.0f) * 255);
          duty = p;
          Serial.print("P=");
          Serial.println(p);
          ledcWrite(LED_CHANNEL0, duty);
        } catch (int exc){
          Serial.println("ERROR READ UNTIL");
          Serial.println(exc);
        }
    }
}
