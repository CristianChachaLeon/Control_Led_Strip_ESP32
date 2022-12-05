// Test TCP case use to perform comunication

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
//#include <WiFi.h>
#include <FastLED.h>

#define BTN_CHANGE_WIFI_CONFIG 12

WiFiManager wm;
WiFiServer wifiServer(8090);

#define MAX_BUFFER_SAFE 10
#define SIZE_MONO_QUEUE 50
#define SIZE_STEREO_QUEUE 40
#define TIME_SEND_INFO 22

#define NUM_LEDS_L 4    // change
#define NUM_LEDS_R 4    // change
#define DATA_PIN_L 22   // change
#define DATA_PIN_R 23   // change

CRGB leds_L[NUM_LEDS_L];  // change
CRGB leds_R[NUM_LEDS_R];  // change

char data_TCPIP[14]; //#FFEECC255\0



struct LedStripConfig {
  char Mode[10];
};

struct Led_Mono {
  char color[8];
  byte brightness;
};

struct Led_Stereo {
  char color_left[7];
  byte brightness_left;
  char color_right[7];
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
  TCP_conection,
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

  Serial.begin(115200);
  Serial.println("CONTROL LED TCP IP protocol");

  queue_config = xQueueCreate( 5, sizeof( struct LedStripConfig * ) );

  pinMode(BTN_CHANGE_WIFI_CONFIG, INPUT_PULLUP);
  binSemaphore = xSemaphoreCreateBinary();

  FastLED.addLeds<NEOPIXEL, DATA_PIN_L>(leds_L, NUM_LEDS_L);
  FastLED.addLeds<NEOPIXEL, DATA_PIN_R>(leds_R, NUM_LEDS_R);

  for (int i = 0; i < NUM_LEDS_L; i++) {
    leds_L[i] = CRGB(120, 20, 100);
  }
  for (int i = 0; i < NUM_LEDS_R; i++) {
    leds_R[i] = CRGB(255, 0, 0);
  }

  FastLED[0].showLeds(10);
  FastLED[1].showLeds(10);

  memset(data_TCPIP, '\0', sizeof(data_TCPIP));



}

void loop() {
  // put your main code here, to run repeatedly:
  //
  if (digitalRead(BTN_CHANGE_WIFI_CONFIG) == LOW) {
    Central_State = Init;
    Init_State = Change_wifi;
  }

  switch (Central_State) {
    case Init:  // State Init
      switch (Init_State) {
        case Wifi_connect:

          if (Conect_Wifi())
            Init_State = TCP_conection;
          break;
        case Change_wifi:

          break;
        case TCP_conection:
          wifiServer.begin();
          Serial.print("Servidor iniciado\nEsperando cliente, ESP32-IP: ");
          Serial.println(WiFi.localIP());
          Init_State = Inactive;

          break;
        case Inactive:
          WiFiClient client = wifiServer.available();
          ListenClient(&client, &queue_config);
          break;
      }
      break;

    case Configuration:// State Config
      struct LedStripConfig Config_Strip_read;
      read_config_queue(&Config_Strip_read);
      setConfig(&Config_Strip_read);
      Central_State = Led_Control;
      Strip_State = Mono;
      break;
    case Led_Control: // State Led Control
    
      break;


  }
}

bool Conect_Wifi() {
  bool res;
  res = wm.autoConnect("AutoConnectAP", "password"); // password protected ap
  if (!res) {
    Serial.println("Failed to connect");
    return false;
  } else {
    Serial.println("ESP32 : CONNECT ! :)");
    return true;
  }
}

void setConfig(struct LedStripConfig * Config) {
  if (!strcmp(Config->Mode, "Mono")) {
    xTaskCreate(TaskLedMono, "LED MODE MONO", 1024, NULL, 2, NULL);
    queue_mono = xQueueCreate( SIZE_MONO_QUEUE, sizeof( struct Led_Mono ) );
    if (queue_mono == NULL) {
      Serial.println("Error: Queue mono not created");
    }

    if (queue_stereo != NULL) {
      vQueueDelete(queue_stereo);
    }
  }else if(!strcmp(Config->Mode, "Stereo")){
    //todo create task for stereo mode
  }else{
    Serial.println("Configuracion : Desconocida");
  }

}

void ListenClient(WiFiClient* guest, QueueHandle_t* queue) {
  if (guest) {
    while (guest->connected()) {
      int i = 0;
      while (guest->available() > 0) {
        char c = guest->read();
        if ( c == '\n') {
          Serial.println((char*) data_TCPIP);// data_TCPIP=#FFEECC255\n

          if (Is_config_queue(data_TCPIP)) {
            Serial.println("Load configuration");
            struct LedStripConfig Config_Strip;
            strcpy(Config_Strip.Mode, "Mono"); // Data hardcode!!!
            load_config_in_queue(&Config_Strip);

            Central_State = Configuration;

          }
          memset(data_TCPIP, '\0', sizeof(data_TCPIP));
          i = 0;
        } else {
          data_TCPIP[i] = c;
          i++;
        }
      }
      delay(1);
    }
    guest->stop();
    //Serial.println("Client disconnected");
  }
}


bool Is_config_queue(char * queue) {
  if (!strcmp (strtok(queue, ":"), "#config")) {
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
    Serial.println((char*) Config->Mode);
  }
}

void read_config_queue(struct LedStripConfig * Config) {
  if (xQueueReceive(queue_config, &Config, ( TickType_t ) 10 ) == pdPASS) {
    Serial.println("Successful : Data read from config queue");
    Serial.println((char*) Config->Mode);
  } else {
    Serial.println("Error : Data read from config queue");
  }
}


void TaskLedMono(void *pvParameters) {
  struct Led_Mono led_mono;
  bool read_data = true;
  unsigned int red ;
  unsigned int green ;
  unsigned int blue ;
  for (;;) {
    xSemaphoreTake(binSemaphore, portMAX_DELAY);
    while (read_data) {
      if (xQueueReceive(queue_mono, &led_mono, ( TickType_t ) 10) == pdPASS) {
        Serial.print("Task :LED ON!!!  -->");
        Serial.print(millis());
        Serial.print("   :  ");
        Serial.println(led_mono.brightness);
        red = red_from_hexColor(led_mono.color);
        green = green_from_hexColor(led_mono.color);
        blue = blue_from_hexColor(led_mono.color);

        for (int i = 0; i < NUM_LEDS_L; i++) {
          leds_L[i] = CRGB(red, green, blue);

        }

        for (int i = 0; i < NUM_LEDS_R; i++) {
          leds_R[i] = CRGB(red, green, blue);
        }
        FastLED[0].showLeds(led_mono.brightness);
        FastLED[1].showLeds(led_mono.brightness);

      } else {
        read_data = false;
      }
      vTaskDelay(pdMS_TO_TICKS(TIME_SEND_INFO));
    }
    read_data = true;
  }



}

//funcion pendiente de probar

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
