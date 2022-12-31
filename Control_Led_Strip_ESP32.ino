// version 0.1 Hardcode num leds, no configuration through the program, prototype

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WiFiUDP.h>
#include <FastLED.h>



#define BTN_CHANGE_WIFI_CONFIG 12


WiFiManager wm;

WiFiUDP UDP;
unsigned int localPort = 8888;
unsigned int remotePort = 8889;
#define UDP_TX_PACKET_MAX_SIZE 50
#define MAX_BUFFER_SAFE 5
#define SIZE_MONO_QUEUE 50
#define SIZE_STEREO_QUEUE 100
#define TIME_SEND_INFO 46

#define NUM_LEDS_L 20    // change
#define NUM_LEDS_R 20    // change
#define DATA_PIN_L 23   // change
#define DATA_PIN_R 22   // change
CRGB leds_L[NUM_LEDS_L];  // change
CRGB leds_R[NUM_LEDS_R];  // change
char packetBuffer[UDP_TX_PACKET_MAX_SIZE] = {};

struct LedStripConfig {
  char Mode[10];
};

struct Led_Mono {
  char color[8];
  byte brightness;
};

struct Led_Stereo {
  char color_left[8];
  byte brightness_left;
  char color_right[8];
  byte brightness_right;
};


QueueHandle_t queue_config;
QueueHandle_t queue_mono;
QueueHandle_t queue_stereo;

xSemaphoreHandle binSemaphore;
enum state {
  Init = 0,
  Configuration,
  Led_Control
};

enum sub_state_init {
  Wifi_connect = 0,
  Change_wifi,
  UDP_conection,
  Inactive
};

enum sub_state_led_control {
  Mono = 0,
  Stereo,
  Default
};

enum state Central_State = Init;
enum sub_state_init Init_State = Wifi_connect;
enum sub_state_led_control Strip_State = Mono;

int messagesWaiting;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Inicializacion");
  queue_config = xQueueCreate( 5, sizeof( struct LedStripConfig * ) );
  pinMode(BTN_CHANGE_WIFI_CONFIG, INPUT_PULLUP);
  binSemaphore = xSemaphoreCreateBinary();
  FastLED.addLeds<NEOPIXEL, DATA_PIN_L>(leds_L, NUM_LEDS_L);
  FastLED.addLeds<NEOPIXEL, DATA_PIN_R>(leds_R, NUM_LEDS_R);

  for (int i = 0; i < NUM_LEDS_L; i++) {
    leds_L[i] = CHSV(1, 255, 255); //left channel
  }
  for (int i = 0; i < NUM_LEDS_R; i++) {
    leds_R[i] = CHSV(1, 255, 255);
  }

  FastLED[0].showLeds(10);
  FastLED[1].showLeds(10);

}

void loop() {
  // put your main code here, to run repeatedly:

  if (digitalRead(BTN_CHANGE_WIFI_CONFIG) == LOW) {
    Central_State = Init;
    Init_State = Change_wifi;
  }

  switch (Central_State) {
    case Init:
      switch (Init_State) {
        case Wifi_connect:
          Serial.println("Estado: Init --> Wifi connect");// Estado con leds
          bool res;
          res = wm.autoConnect("AutoConnectAP", "password"); // password protected ap
          if (!res) {
            Serial.println("Failed to connect"); // parpadeo de error
          } else {
            Serial.println("ESP32 : CONNECT ! :)");
            Init_State = UDP_conection;
          }
          break;
        case Change_wifi:

          break;
        case UDP_conection:
          Serial.println("UDP INIT");
          if (ConnectUDP())
            Init_State = Inactive;
          break;
        case Inactive: //listen UDP

          if (UDP.parsePacket()) {
            UDP.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE); // read the packet into packetBufffer
            if (init_system(packetBuffer)) {
              memset(packetBuffer, '\0', UDP_TX_PACKET_MAX_SIZE);

              for (int i = 0; i < NUM_LEDS_L; i++) {
                leds_L[i] =  CRGB(0, 0, 0); //left channel
              }
              for (int i = 0; i < NUM_LEDS_R; i++) {
                leds_R[i] = CHSV(0, 0, 0);
              }

              leds_L[0] = CRGB(255, 255, 255); //left channel
              leds_R[0] = CRGB(255, 255, 255);

              FastLED[0].showLeds(10);
              FastLED[1].showLeds(10);

              delay(200);
              leds_L[0] = CRGB(0, 0, 0); //left channel
              leds_R[0] = CRGB(0, 0, 0);

              FastLED[0].showLeds(10);
              FastLED[1].showLeds(10);

              Central_State = Configuration;  //Change state to Configuration
            }
          }
          break;
      }
      break;
    case Configuration:
      Serial.println("State: Configuration");
      queue_stereo = xQueueCreate( SIZE_STEREO_QUEUE, sizeof( struct Led_Stereo ));
      if (queue_stereo == NULL) {
        Serial.println("Error: Queue stereo not created");
      }
      xTaskCreate(TaskLedStereo, "LED MODE STEREO", 1024, NULL, 2, NULL);
      Central_State = Led_Control;
      break;
    case Led_Control:
      if (UDP.parsePacket()) {
        UDP.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
        if (stop_system(packetBuffer)) { // delete task and queue
          Central_State = Init;
          Init_State = Inactive;
          memset(packetBuffer, '\0', UDP_TX_PACKET_MAX_SIZE);
        } else { // read queue
          struct Led_Stereo led_stereo_data;
          char tmp_brightness_left[4] = {};
          char tmp_brightness_right[4] = {};

          char * coincidence;
          int separator = 0;

          coincidence = strchr(packetBuffer, '-');
          separator = (int)(coincidence - packetBuffer);

          //Data channel left
          memcpy(led_stereo_data.color_left, packetBuffer, 7);
          memcpy(tmp_brightness_left, packetBuffer + 7, separator - 7); //obtain brightness
          led_stereo_data.brightness_left = atoi(tmp_brightness_left);

          //Data channel right
          memcpy(led_stereo_data.color_right, packetBuffer + separator + 1, 7);
          memcpy(tmp_brightness_right, packetBuffer + separator + 1 + 7, 4); //obtain brightness
          led_stereo_data.brightness_right = atoi(tmp_brightness_right);

          if (xQueueSend(queue_stereo, &led_stereo_data, ( TickType_t ) 10 ) != pdPASS) {
            Serial.println("Error: Color and brightness NO load in queue stereo");
          }
          memset(packetBuffer, '\0', UDP_TX_PACKET_MAX_SIZE);
        }


      }

      messagesWaiting = uxQueueMessagesWaiting(queue_stereo);
      Serial.print("datos en la cola stereo: ");
      Serial.println(messagesWaiting);
      if (messagesWaiting > MAX_BUFFER_SAFE) {
        xSemaphoreGive(binSemaphore);
        //Serial.println("Iniciar task.. led");
      }
      break;
  }


}



boolean ConnectUDP() { // add contador regresivo 10 times intento de conexion
  Serial.println();
  Serial.println("Starting UDP");
  // in UDP error, block execution
  if (UDP.begin(localPort) != 1)
  {
    Serial.println("Connection failed");
    while (true) {
      delay(1000);
    }
  }
  Serial.println("UDP successful");
  return true;
}

bool init_system(char * queue) {
  if (!strcmp (queue, "#Connect_On")) {
    return true;
  } else {
    return false;
  }
}
bool stop_system(char * queue) {
  if (!strcmp (queue, "#Connect_Off")) {
    return true;
  } else {
    return false;
  }
}

bool Is_config_queue(char * queue) {
  if (!strcmp (strtok(queue, ": "), "#config")) {
    return true;
  } else {
    return false;
  }
}

void load_config_in_queue(struct LedStripConfig * Config) {
  if ( xQueueSend(queue_config, Config, ( TickType_t ) 10 ) != pdPASS) {
    Serial.println("Error: Load data in queue config");
  } else {
    Serial.println("Successful: Load data in queue config");
  }
}

void read_config_queue(struct LedStripConfig * Config) {
  if (xQueueReceive(queue_config, &Config, ( TickType_t ) 10 ) == pdPASS) {
    Serial.println("Successful : Data read from config queue");
  } else {
    Serial.println("Error : Data read from config queue");
  }
}




void TaskLedStereo(void *pvParameters) {
  struct Led_Stereo led_stereo;
  bool read_data = true;
  unsigned int red_left = 0 ;
  unsigned int green_left = 0 ;
  unsigned int blue_left = 0 ;
  unsigned int red_right = 0 ;
  unsigned int green_right = 0 ;
  unsigned int blue_right = 0 ;
  for (;;) {
    xSemaphoreTake(binSemaphore, portMAX_DELAY);
    while (read_data) {
      if (xQueueReceive(queue_stereo, &led_stereo, ( TickType_t ) 10) == pdPASS) {
        //if color is #XXXXXX cada led de un color
        red_left = red_from_hexColor(led_stereo.color_left);
        green_left = green_from_hexColor(led_stereo.color_left);
        blue_left = blue_from_hexColor(led_stereo.color_left);

        red_right = red_from_hexColor(led_stereo.color_right);
        green_right = green_from_hexColor(led_stereo.color_right);
        blue_right = blue_from_hexColor(led_stereo.color_right);


        //int limite_left = int(led_stereo.brightness_left * (NUM_LEDS_L / 255.0));

        //int limite_right = int(led_stereo.brightness_right * (NUM_LEDS_R / 255.0));
        /*Serial.print("leds para izquierda: " );
          Serial.println(limite_left);
          Serial.print("leds para derecha " );
          Serial.println(limite_right);*/
        /*for (int i = 0; i < limite_left; i++) {
          leds_L[i] = CRGB(red_left, green_left, blue_left);

          }

          for (int i = 0; i < limite_right; i++) {
          leds_R[i] = CRGB(red_right, green_right, blue_right);
          }*/
        for ( int i = 0 ; i < NUM_LEDS_L; i++) {
          leds_L[i] = CRGB(red_left, green_left, blue_left);
        }

        for ( int i = 0 ; i < NUM_LEDS_R; i++) {
          leds_R[i] = CRGB(red_right, green_right, blue_right);
        }

        FastLED[0].showLeds(led_stereo.brightness_left);
        FastLED[1].showLeds(led_stereo.brightness_right);

      } else {
        read_data = false;
      }
      vTaskDelay(pdMS_TO_TICKS(TIME_SEND_INFO));
    }
    read_data = true;
  }
}



unsigned int red_from_hexColor(char * Hexcolor) {
  char color[3] = {};
  memcpy(color, Hexcolor + 1, 2);
  return (unsigned int)strtoul(color, NULL, 16);
}

unsigned int green_from_hexColor(char * Hexcolor) {
  char color[3] = {};
  memcpy(color, Hexcolor + 3, 2);
  return (unsigned int)strtoul(color, NULL, 16);
}

unsigned int blue_from_hexColor(char * Hexcolor) {
  char color[3] = {};
  memcpy(color, Hexcolor + 5, 2);
  return (unsigned int)strtoul(color, NULL, 16);
}
