// version 0.1 Hardcode num leds, no configuration through the program, prototype

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager



#define BTN_CHANGE_WIFI_CONFIG 12

#define NUM_LEDS_LEFT 4
#define DATA_PIN_LEFT 14
CRGB leds_LEFT[NUM_LEDS_LEFT];

void setup() {
  // put your setup code here, to run once:
   Serial.begin(115200);
   Serial.println("Inicializacion");

   pinMode(BTN_CHANGE_WIFI_CONFIG, INPUT_PULLUP);
  
}

void loop() {
  // put your main code here, to run repeatedly:

}
