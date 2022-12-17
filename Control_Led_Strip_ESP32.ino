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

  for (int j = 0; j < 255; j++) {
    for (int i = 0; i < NUM_LEDS_L; i++) {
      leds_L[i] = CHSV(j, 255, 255);
    }
    for (int i = 0; i < NUM_LEDS_R; i++) {
      leds_R[i] = CHSV(0, j, 255);
    }
    FastLED[0].showLeds(10);
    FastLED[1].showLeds(10);
    FastLED.delay(100); 
  }
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
            Serial.println("Failed to connect");
          } else {
            Serial.println("ESP32 : CONNECT ! :)");
            Init_State = UDP_conection;
          }
          break;
        case Change_wifi:

          break;
        case UDP_conection:
          Serial.println("UDP INIT");
          delay(3000);
          if (ConnectUDP())
            Init_State = Inactive;
          break;
        case Inactive:
          //listen UDP
          int packetSize = UDP.parsePacket();
          if (packetSize) {
            // read the packet into packetBufffer
            UDP.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);

            if (Is_config_queue(packetBuffer)) {
              Serial.println((char *)packetBuffer);
              struct LedStripConfig Config_Strip;
              //strcpy(Config_Strip.Mode, "Mono"); // Data hardcode!!!
              strcpy(Config_Strip.Mode, "Stereo"); // Data hardcode!!!
              load_config_in_queue(&Config_Strip);

              memset(packetBuffer, '\0', UDP_TX_PACKET_MAX_SIZE);
              Central_State = Configuration;  //Change state to Configuration
            }


          }

          break;
      }
      break;
    case Configuration:
      Serial.println("State: Configuration");
      struct LedStripConfig Config_Strip_read;
      read_config_queue(&Config_Strip_read);

      if (!strcmp(Config_Strip_read.Mode, "Mono")) {
        Serial.println("Configuracion : Mono");

        xTaskCreate(TaskLedMono, "LED MODE MONO", 1024, NULL, 2, NULL);
        queue_mono = xQueueCreate( SIZE_MONO_QUEUE, sizeof( struct Led_Mono ) );
        if (queue_mono == NULL) {
          Serial.println("Error: Queue mono not created");
        }

        if (queue_stereo != NULL) {
          vQueueDelete(queue_stereo);
          Serial.println("Successfull : Delete Queue Stereo");
        }
        Central_State = Led_Control;
        Strip_State = Mono;
      } else if (!strcmp(Config_Strip_read.Mode, "Stereo")) {
        Serial.println("Configuracion : Stereo");

        queue_stereo = xQueueCreate( SIZE_STEREO_QUEUE, sizeof( struct Led_Stereo ));
        xTaskCreate(TaskLedStereo, "LED MODE STEREO", 1024, NULL, 2, NULL);
        Serial.println("Cola creada debug");
        if (queue_stereo == NULL) {
          Serial.println("Error: Queue stereo not created");
        }

        if (queue_mono != NULL) {
          vQueueDelete(queue_mono);
          Serial.println("Successfull : Delete Queue mono");
        }
        Central_State = Led_Control;
        Strip_State = Stereo;

      } else {
        Serial.println("Configuracion : Desconocida");
      }




      break;
    case Led_Control:
      if (UDP.parsePacket()) {
        UDP.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
        if (Is_config_queue(packetBuffer)) {
          struct LedStripConfig Config_Strip;  // create Structure
          strcpy(Config_Strip.Mode, "Mono"); // Data hardcode!!!
          load_config_in_queue(&Config_Strip);
          memset(packetBuffer, '\0', UDP_TX_PACKET_MAX_SIZE);
          Central_State = Configuration;  //Change state to Configuration
          Strip_State = Default;

        } /*else {
          Serial.println((char*)packetBuffer);


        }*/

        // si es config carga en queue y cambiar a estado de Config.
        // se podria cambiar tambien el stado de Strip_state para que se apagen los leds, mientras se cambia la configuracion
        switch (Strip_State) {

          case Mono:
            //Serial.println(": Mono");
            struct Led_Mono led_mono_data;
            char tmp_brightness[4];

            memcpy(led_mono_data.color, packetBuffer, 7);

            memcpy(tmp_brightness, packetBuffer + 7, 4);
            led_mono_data.brightness = atoi(tmp_brightness);
            if (xQueueSend(queue_mono, &led_mono_data, ( TickType_t ) 10 ) != pdPASS) {
              Serial.println("Error: Color and brightness NO load in queue mono");
            }

            memset(packetBuffer, '\0', UDP_TX_PACKET_MAX_SIZE);

            break;
          case Stereo:
            //Serial.println(": Stereo");
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
            //Serial.println((char*)led_stereo_data.brightness_right);

            //Serial.print("Atoi : ");
            //Serial.println(led_stereo_data.brightness_right);

            if (xQueueSend(queue_stereo, &led_stereo_data, ( TickType_t ) 10 ) != pdPASS) {
              Serial.println("Error: Color and brightness NO load in queue stereo");
            }
            memset(packetBuffer, '\0', UDP_TX_PACKET_MAX_SIZE);


            break;
        }
      }


      // ver la cantidad de datos que hay en la cola, en este caso se tiene 2 colas ?,
      // tener en cuenta que son dos semanafros

      if (queue_stereo != NULL) {
        messagesWaiting = uxQueueMessagesWaiting(queue_stereo);
        Serial.print("datos en la cola stereo: ");
        Serial.println(messagesWaiting);
        if (messagesWaiting > MAX_BUFFER_SAFE) {
          xSemaphoreGive(binSemaphore);
          //Serial.println("Iniciar task.. led");
        } else {
          // no no activate task, task is waiting
        }
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
  }
}

void read_config_queue(struct LedStripConfig * Config) {
  if (xQueueReceive(queue_config, &Config, ( TickType_t ) 10 ) == pdPASS) {
    Serial.println("Successful : Data read from config queue");
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
        /*Serial.print("Task :LED ON!!!  -->");
          Serial.print(millis());
          Serial.print("   :  ");
          Serial.println(led_mono.brightness);*/
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

        int limite_left = int(led_stereo.brightness_left * (NUM_LEDS_L / 255.0));

        int limite_right = int(led_stereo.brightness_right * (NUM_LEDS_R / 255.0));
        /*Serial.print("leds para izquierda: " );
          Serial.println(limite_left);
          Serial.print("leds para derecha " );
          Serial.println(limite_right);*/
        for (int i = 0; i < limite_left; i++) {
          leds_L[i] = CRGB(red_left, green_left, blue_left);

        }

        for (int i = 0; i < limite_right; i++) {
          leds_R[i] = CRGB(red_right, green_right, blue_right);
        }
        FastLED[0].showLeds(led_stereo.brightness_left);
        FastLED[1].showLeds(led_stereo.brightness_right);

        for ( int i = 0 ; i < NUM_LEDS_L; i++) {
          leds_L[i] = CRGB(0, 0, 0);
        }

        for ( int i = 0 ; i < NUM_LEDS_R; i++) {
          leds_R[i] = CRGB(0, 0, 0);
        }


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
