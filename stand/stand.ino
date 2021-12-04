#define FASTLED_INTERRUPT_RETRY_COUNT 0
#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include "reactive_common.h"

#define NUM_LEDS 140

#define DATA_PIN D7
#define CLK_PIN D5
#define COLOR_ORDER BGR
#define LED_TYPE SK9822

byte BRIGHTNESS = 150;

float RAINBOW_STEP  = 5.00;
float RAINBOW_STEP_2 = 0.5;
#define MAIN_LOOP 4

byte EMPTY_BRIGHT = 30;
#define EMPTY_COLOR HUE_BLUE
int RAINBOW_PERIOD = 1;
int hue;
float rainbowPaletteOffsetIndex = (float)255 / NUM_LEDS;

#define STRIPE NUM_LEDS / 5
float freq_to_stripe = NUM_LEDS / 40; // /2 так как симметрия, и /20 так как 20 частот

byte LOW_COLOR = 144;           // цвет низких частот
byte MID_COLOR = 192;           // цвет средних
//byte MID_COLOR = HUE_RED;           // цвет средних
byte HIGH_COLOR = 114;          // цвет высоких
//byte HIGH_COLOR = HUE_ORANGE;          // цвет высоких

byte HUE_START = 42;
byte HUE_STEP = 15;

#define LAMP_ID 1
WiFiUDP UDP;

const char *ssid = "sound_reactive"; // The SSID (name) of the Wi-Fi network you want to connect to
const char *password = "123456789";  // The password of the Wi-Fi network

CRGB leds[NUM_LEDS];

#define PARSE_AMOUNT 34

struct LedCommand {
  uint8_t opmode;
  uint8_t data[PARSE_AMOUNT - 1];
};

DEFINE_GRADIENT_PALETTE(soundlevel_gp) {
  0,    0, 255, 255,  // cyan
  50,  255, 0, 104,   // pink
  100,  255, 35, 0,   // red-orange
  150,  255, 104, 0,  // orange
  255,  255, 124, 0,  // lighter-orange
};
CRGBPalette16 myPal = soundlevel_gp;

unsigned long lastReceived = 0;
unsigned long lastHeartBeatSent;
const int heartBeatInterval = 100;

struct LedCommand cmd;

void connectToWifi();

unsigned long main_timer;
unsigned long rainbow_timer;
float rainbow_steps;
int this_color;

void setup()
{
  FastLED.addLeds<LED_TYPE, DATA_PIN, CLK_PIN, COLOR_ORDER, DATA_RATE_MHZ(12)>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.setBrightness(BRIGHTNESS);

  Serial.begin(115200); // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println('\n');

  WiFi.begin(ssid, password); // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println(" ...");

  connectToWifi();
  sendHeartBeat();
  UDP.begin(7001);
}

void sendHeartBeat() {
  struct heartbeat_message hbm;
  hbm.client_id = LAMP_ID;
  hbm.chk = 77777;
  // Serial.println("Sending heartbeat");
  IPAddress ip(192, 168, 4, 1);
  UDP.beginPacket(ip, 7171);
  int ret = UDP.write((char*)&hbm, sizeof(hbm));
  // printf("Returned: %d, also sizeof hbm: %d \n", ret, sizeof(hbm));
  UDP.endPacket();
  lastHeartBeatSent = millis();
}

void loop()
{
  if (millis() - lastHeartBeatSent > heartBeatInterval) {
    sendHeartBeat();
  }

  int packetSize = UDP.parsePacket();
  if (packetSize)
  {
    UDP.read((char *)&cmd, sizeof(struct LedCommand));
    lastReceived = millis();
  }

  if (millis() - lastReceived >= 5000)
  {
    connectToWifi();
  }

  byte count;

  //  Serial.print("Mode: " + (String)cmd.opmode + "/");
  //  for (byte i = 0; i < PARSE_AMOUNT - 1; i++) {
  //    Serial.print((String)cmd.data[i] + "/");
  //  }
  //  Serial.println();

  if (millis() - main_timer > MAIN_LOOP) {
    switch (cmd.opmode) {
      case 1: {
          if (millis() - rainbow_timer > 30) {
            rainbow_timer = millis();
            hue = floor((float)hue + RAINBOW_STEP);
          }
          count = 0;
          for (byte i = 0; i < cmd.data[0]; i++) {
            leds[i] = ColorFromPalette(RainbowColors_p, (count * rainbowPaletteOffsetIndex) / 2 - hue);  // заливка по палитре радуга
            count++;
          }
          if (EMPTY_BRIGHT > 0) {
            CHSV this_dark = CHSV(EMPTY_COLOR, 255, EMPTY_BRIGHT);
            for (byte i = cmd.data[0]; i < NUM_LEDS; i++)
              leds[i] = this_dark;
          }
          break;
        }

      case 2: {
          for (byte i = 0; i < NUM_LEDS; i++) {
            if (i < STRIPE)          leds[i] = CHSV(HIGH_COLOR, 255, cmd.data[2]);
            else if (i < STRIPE * 2) leds[i] = CHSV(MID_COLOR, 255, cmd.data[1]);
            else if (i < STRIPE * 3) leds[i] = CHSV(LOW_COLOR, 255, cmd.data[0]);
            else if (i < STRIPE * 4) leds[i] = CHSV(MID_COLOR, 255, cmd.data[1]);
            else if (i < STRIPE * 5) leds[i] = CHSV(HIGH_COLOR, 255, cmd.data[2]);
          }
          break;
        }

      case 3: {
          // 47/46/47
          for (byte i = 0; i < NUM_LEDS; i++) {
            if (i < 47)            leds[i] = CHSV(HIGH_COLOR, 255, cmd.data[2]);
            else if (i < 93)       leds[i] = CHSV(MID_COLOR, 255, cmd.data[1]);
            else if (i < NUM_LEDS) leds[i] = CHSV(LOW_COLOR, 255, cmd.data[0]);
          }
          break;
        }

      case 4: {
          if (LAMP_ID == 2) {
            for (byte i = 0; i < NUM_LEDS; i++) leds[i] = CHSV(HUE_RED, 255, cmd.data[1]);
          } else {
            for (byte i = 0; i < NUM_LEDS; i++) leds[i] = CHSV(HUE_BLUE, 255, cmd.data[1]);
          }

          break;
        }

      case 5: {
          byte HUEindex = HUE_START;

          byte maxFreqIndex = PARSE_AMOUNT - 2;

          if (cmd.data[maxFreqIndex] == 0) {
            break;
          }

          for (byte i = 0; i < NUM_LEDS / 2; i++) {
            byte this_bright = map(cmd.data[(int)floor((NUM_LEDS / 2 - i) / freq_to_stripe)], 0, cmd.data[maxFreqIndex], 0, 255);

            this_bright = constrain(this_bright, 0, 255);

            leds[i] = CHSV(HUEindex, 255, this_bright);

            leds[NUM_LEDS - i - 1] = leds[i];

            HUEindex += HUE_STEP;

            if (HUEindex > 255) {
              HUEindex = 0;
            }
          }
          break;
        }

      case 6: {
          for (byte i = 0; i < NUM_LEDS; i++) {
            if (LAMP_ID == 2) {
              leds[i] = CHSV(HUE_RED, 255, 255);
            } else {
              leds[i] = CHSV(HUE_BLUE, 255, 255);
            }
          }
          break;
        }


        //      case 2:
        //        // 2nd implementation
        //        count = 0;
        //        for (int i = 0; i > ledData; i++) {
        //          leds[i] = ColorFromPalette(myPal, (count * rainbowPaletteOffsetIndex));
        //          count++;
        //        }
        //        if (EMPTY_BRIGHT > 0) {
        //          CHSV this_dark = CHSV(EMPTY_COLOR, 255, EMPTY_BRIGHT);
        //          for (int i = ledData; i < NUM_LEDS; i++)
        //            leds[i] = this_dark;
        //        }
        //        break;

        //      case 3:
        //        // 3rd implementation
        //        if (millis() - rainbow_timer > 30) {
        //          rainbow_timer = millis();
        //          this_color += RAINBOW_PERIOD;
        //          if (this_color > 255) this_color = 0;
        //          if (this_color < 0) this_color = 255;
        //        }
        //        rainbow_steps = this_color;
        //        for (int i = 0; i < NUM_LEDS; i++) {
        //          leds[i] = CHSV((int)floor(rainbow_steps), 255, 255);
        //          rainbow_steps += RAINBOW_STEP_2;
        //          if (rainbow_steps > 255) rainbow_steps = 0;
        //          if (rainbow_steps < 0) rainbow_steps = 255;
        //        }
        //        break;
        //
        //      case 4:
        //        for (int i = 0; i < NUM_LEDS; i++) {
        //          leds[i] = CHSV(ledData[0], 255, 255);
        //        }
        //        break;
    }

    FastLED.show();

    FastLED.clear();

    main_timer = millis();
  }
}

void connectToWifi() {
  WiFi.mode(WIFI_STA);

  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CHSV(0, 0, 0);
  }

  leds[0] = CRGB(0, 255, 0);
  FastLED.show();

  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i);
    Serial.print(' ');
  }
  Serial.println('\n');
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP()); // Send the IP address of the ESP8266 to the computer
  leds[0] = CRGB(0, 0, 255);
  FastLED.show();
  lastReceived = millis();
}
