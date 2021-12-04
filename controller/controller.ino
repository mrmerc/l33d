#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include "reactive_common.h"

#define READ_PIN A0

#define NUMBER_OF_CLIENTS 2

#define ADC_SAMPLES 300

#define VOL_THR 10
const int checkDelay = 5000;

unsigned long lastChecked;
WiFiUDP UDP;

bool heartbeats[NUMBER_OF_CLIENTS];

uint8_t opMode = 6;
uint8_t prevOpMode = opMode;

#define PARSE_AMOUNT 34  // максимальное количество значений в массиве, который хотим получить
#define header      '$' // стартовый символ
#define divider     '#' // разделительный символ
#define ending      ';' // завершающий символ

struct LedCommand {
  uint8_t opmode;
  uint8_t data[PARSE_AMOUNT - 1];
};

uint8_t intData[PARSE_AMOUNT];  // массив численных значений после парсинга
byte parse_index;
boolean recievedFlag;
boolean parseStarted;
boolean settingsEnabled = false;
String string_convert = "";

void setup()
{
  delay(1500);

  pinMode(READ_PIN, INPUT);

  /* WiFi Part */
  Serial.begin(115200);
  // Serial.println();
  // Serial.print("Setting soft-AP ... ");
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("sound_reactive", "123456789");
  // Serial.print("Soft-AP IP address = ");
  // Serial.println(WiFi.softAPIP());
  // Serial.println(opMode);
  UDP.begin(7171);
  resetHeartBeats();
  waitForConnections();
  lastChecked = millis();

  for (byte i = 0; i < PARSE_AMOUNT; i++) {
    intData[i] = 0;
  }
}


void loop()
{
  if (millis() - lastChecked > checkDelay) {
    if (!checkHeartBeats()) {
      waitForConnections();
    }
    lastChecked = millis();
  }

  packetParsing();

  if (intData[0] != 0) {
    prevOpMode = intData[0];
  }

  sendLedData(prevOpMode);

  // Should be called by webserver, not in loop. And probably send data via SoftwareSerial
  sendOpModeToArduino();

  // Clear parsed data array. TODO: put it in separate func
  for (byte i = 0; i < PARSE_AMOUNT; i++) {
    intData[i] = 0;
  }

  delay(4);
}

int getStaticSoundVolumeValue() {
  int RsoundLevel = 0;
  int RcurrentLevel = 0;
  float RsoundLevel_f = 0;
  float averageLevel = 50,
        averK = 0.006;
  int maxLevel = 100;
  byte Rlenght;

  for (int i = 0; i < 200; i++) {                                   // делаем 100 измерений
    RcurrentLevel = analogRead(READ_PIN);                            // с правого
    if (RsoundLevel < RcurrentLevel) RsoundLevel = RcurrentLevel;   // ищем максимальное
  }

  if (RsoundLevel < 50) RsoundLevel = 50;
  RsoundLevel = map(RsoundLevel, 50, 1023, 0, 511);

  RsoundLevel = constrain(RsoundLevel, 0, 511);

  RsoundLevel = pow(RsoundLevel, 2.0);

  float SMOOTH = 0.08;

  RsoundLevel_f = RsoundLevel * SMOOTH + RsoundLevel_f * (1 - SMOOTH);

  if (RsoundLevel_f > 10) {

    // расчёт общей средней громкости с обоих каналов, фильтрация.
    // Фильтр очень медленный, сделано специально для автогромкости
    averageLevel = (float)(RsoundLevel_f + RsoundLevel_f) / 2 * averK + averageLevel * (1 - averK);

    // принимаем максимальную громкость шкалы как среднюю, умноженную на некоторый коэффициент MAX_COEF
    maxLevel = (float)averageLevel * 1.8;

    // преобразуем сигнал в длину ленты (где MAX_CH это половина количества светодиодов)
    Rlenght = map(RsoundLevel_f, 0, maxLevel, 0, 140);
    // ограничиваем до макс. числа светодиодов (НАДО, БЕЗ ЭТОГО ЗАВИСАЕТ)
    Rlenght = constrain(Rlenght, 0, 140);

    // Serial.println(Rlenght);

    return Rlenght;
  }
}

int getSoundValue() {
  int adcMaxValue = 0;
  int adcCurrentValue = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    adcCurrentValue = analogRead(READ_PIN);

    if (adcCurrentValue > adcMaxValue) {
      adcMaxValue = adcCurrentValue;
    }
  }

  //if (adcMaxValue < VOL_THR) adcMaxValue = VOL_THR;
  //RsoundLevel = map(RsoundLevel, VOL_THR, 1023, 0, 511);

  /* Sound volume level auto-correction */
  static int highestMaximumValue = 0;
  /* We set maximum ADC value to equalize first iteration values
     e.g. adcMaxVal == 5 => hMV == 5 and lMV == 5
  */
  static int lowestMinimumValue = 1023;
  /* Counter represents how much loop cycles auto-correction will maintain  */
  static byte counter = 0;
  /* Exponential filter values */
  static float filteredMinValue = 1000; static float filteredMaxValue = 1000;
  static float filteredLength = 0;
  const float FILTER_COEFFICIENT = 0.01;
  const float FILTER_LENGTH_COEFFICIENT = 0.2;

  counter++;
  if (counter >= 50) {
    highestMaximumValue = 0;
    lowestMinimumValue = 1023;
    counter = 0;
  }

  if (adcMaxValue > highestMaximumValue) {
    highestMaximumValue = adcMaxValue;
  }
  if (adcMaxValue < lowestMinimumValue) {
    lowestMinimumValue = adcMaxValue;
  }

  filteredMinValue += (float)(lowestMinimumValue - filteredMinValue) * FILTER_COEFFICIENT;
  filteredMaxValue += (float)(highestMaximumValue - filteredMaxValue) * FILTER_COEFFICIENT;

  /* CRIT when filteredMaxValue - filteredMinValue becomes more than VOL_THR

      38.50 50.50 38.68 140
      val   filMin  filMax-filMin length
  */

  int ledLength = 140;

  if ((float)adcMaxValue > filteredMaxValue) ledLength = 140;
  else if ((float)adcMaxValue < filteredMinValue) ledLength = 0;
  else ledLength = map(adcMaxValue - (int)filteredMinValue, 6, (int)filteredMaxValue - (int)filteredMinValue, 0, 140);

  ledLength = constrain(ledLength, 0, 140);

  /*Serial.print(adcMaxValue);
    Serial.print(' ');
    Serial.print(adcMaxValue - filteredMinValue);
    Serial.print(' ');
    Serial.print(filteredMinValue);
    Serial.print(' ');
    Serial.print(filteredMaxValue - filteredMinValue);
    Serial.print(' ');
    Serial.println(ledLength);*/

  int TEST_VAR = 0;

  /*Serial.print(filteredMinValue);
    Serial.print(' ');
    Serial.print(adcMaxValue);
    Serial.print(' ');
    Serial.println(filteredMaxValue);*/

  filteredLength += (float)(ledLength - filteredLength) * FILTER_LENGTH_COEFFICIENT;
  if (adcMaxValue > filteredMaxValue) filteredLength = 140;

  if (filteredMaxValue - filteredMinValue > VOL_THR) TEST_VAR = filteredLength;
  //Serial.println("TEST VAR - " + (String)TEST_VAR);

  return TEST_VAR;
}

void sendLedDataMultiple(uint8_t hue, uint8_t op_mode)
{
  struct LedCommand send_data;
  //send_data.opmode = op_mode;
  //send_data.data = data;
  send_data.opmode = op_mode;

  for (int i = 0; i < NUMBER_OF_CLIENTS; i++)
  {
    if (i == 0) {
      send_data.data[0] = HUE_BLUE;
    } else {
      send_data.data[0] = HUE_RED;
    }
    IPAddress ip(192, 168, 4, 2 + i);
    UDP.beginPacket(ip, 7001);
    UDP.write((char*)&send_data, sizeof(struct LedCommand));
    UDP.endPacket();
  }
}

void sendLedData(uint8_t opMode)
{
  struct LedCommand send_data;
  send_data.opmode = opMode;

  for (byte i = 1; i < PARSE_AMOUNT; i++) {
    send_data.data[i - 1] = intData[i];
  }

  for (int i = 0; i < NUMBER_OF_CLIENTS; i++)
  {
    IPAddress ip(192, 168, 4, 2 + i);
    UDP.beginPacket(ip, 7001);
    UDP.write((char*)&send_data, sizeof(struct LedCommand));
    UDP.endPacket();
  }
}

void waitForConnections() {
  while (true) {
    readHeartBeat();
    if (checkHeartBeats()) {
      return;
    }
    delay(checkDelay);
    resetHeartBeats();
  }
}

void resetHeartBeats() {
  for (int i = 0; i < NUMBER_OF_CLIENTS; i++) {
    heartbeats[i] = false;
  }
}

void readHeartBeat() {
  struct heartbeat_message hbm;
  while (true) {
    int packetSize = UDP.parsePacket();
    if (!packetSize) {
      break;
    }
    UDP.read((char *)&hbm, sizeof(struct heartbeat_message));
    if (hbm.client_id > NUMBER_OF_CLIENTS) {
      // Serial.println("Error: invalid client_id received");
      continue;
    }
    heartbeats[hbm.client_id - 1] = true;
  }
}

bool checkHeartBeats() {
  for (int i = 0; i < NUMBER_OF_CLIENTS; i++) {
    if (!heartbeats[i]) {
      return false;
    }
  }
  resetHeartBeats();
  return true;
}

void sendOpModeToArduino() {
  Serial.print("<" + (String)opMode + ">");
}

void packetParsing() {

  // Serial.println(Serial.available());

  while (Serial.available() > 0) {
    char incomingByte = Serial.read();
    //Serial.print(incomingByte);

    if (parseStarted) {                                         // если приняли начальный символ (парсинг разрешён)
      if (incomingByte != divider && incomingByte != ending) {  // если это не пробел И не конец
        string_convert += incomingByte;                       // складываем в строку
      }
      else {                                                    // если это пробел или ; конец пакета
        intData[parse_index] = string_convert.toInt();          // преобразуем строку в int и кладём в массив
        string_convert = "";                                    // очищаем строку
        parse_index++;                                          // переходим к парсингу следующего элемента массива
      }
    }

    if (incomingByte == header) {                               // если это $
      parseStarted = true;                                      // поднимаем флаг, что можно парсить
      parse_index = 0;                                          // сбрасываем индекс
    } else if (incomingByte == ending) {                        // если таки приняли ; - конец парсинга
      parseStarted = false;                                     // сброс
      recievedFlag = true;                                      // флаг на принятие
    }
  }

  if (recievedFlag) {       // если получены данные
    // Serial.println("Recived: " + String(intData[0]) + "/" + String(intData[1]) + "/" + String(intData[2]));
    recievedFlag = false;
  }
}
