#include <Arduino.h>

#define LOG_OUT 1
#define FHT_N 64
#include <FHT.h>
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

#define ADC_0 A0
#define SOUND_R A2
#define SOUND_R_FREQ A3

#define VU_SAMPLE_COUNT 200

#define VU_MODE 1
#define FREQ_MODE_SYMMETRICAL 2
#define FREQ_MODE_THREE_STRIPES 3
#define FREQ_MODE_ONE_STRIPE 4
#define FREQ_MODE_SPECTRE 5
#define COLOR_MODE 6
#define RANDOM_MODE 9

//Случайный режим
#define RANDOM_MODE_COUNT 6
byte randomModes[RANDOM_MODE_COUNT];
byte currentRandomMode = 1;
uint16_t randomPeriod = 0;
// Should make isModeRandom configurable
boolean isModeRandom = true;
unsigned long randomTimer = 0;

byte EMPTY_BRIGHT = 30;
byte LOW_PASS = 50;
byte FREQ_LOW_PASS = 50;
byte LIGHT_SMOOTH = 2;
byte soundAnalyzeMode = 1;
byte packetMode = 0;
byte FREQ_SMOOTH_STEP = 10;
byte freqBrightness[3];

float SMOOTH = 0.5; // was 0.08
float SMOOTH_FREQ = 0.8;
float MAX_COEF_FREQ = 1.5; // коэффициент порога для "вспышки" цветомузыки, больше - четче (по умолчанию 1.5)
float averK = 0.006;
float RsoundLevel_f = 0;
float averageLevel = 50;
float maxFrequencyValue_f = 0;
float colorMusic_f[3];
float colorMusic_aver[3];

int freq_f[32];

int getStaticSoundVolumeValue() {
  int RsoundLevel = 0;
  int RcurrentLevel = 0;
  int maxLevel = 100;
  byte Rlenght = 0;

  for (int i = 0; i < VU_SAMPLE_COUNT; i++) { // делаем 100 измерений
    RcurrentLevel = analogRead(SOUND_R);      // с правого
    if (RsoundLevel < RcurrentLevel)
      RsoundLevel = RcurrentLevel; // ищем максимальное
  }

  RsoundLevel = map(RsoundLevel, LOW_PASS, 1023, 0, 511);

  RsoundLevel = constrain(RsoundLevel, 0, 511);

  RsoundLevel = pow(RsoundLevel, 2.0);

  RsoundLevel_f = RsoundLevel * SMOOTH + RsoundLevel_f * (1 - SMOOTH);

  // Serial.println("F: " + (String)RsoundLevel_f);

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
  }

  return Rlenght;
}

void fhtAnalyzeAudio() {
  for (int i = 0; i < FHT_N; i++) {
    int sample = analogRead(SOUND_R_FREQ);
    fht_input[i] = sample; // put real data into bins
  }
  fht_window();  // window the data for better frequency response
  fht_reorder(); // reorder the data before doing the fht
  fht_run();     // process the data in the fht
  fht_mag_log(); // take the output of the fht
}

void analyzeFrequencies() {
  int colorMusic[3];

  fhtAnalyzeAudio();

  for (byte i = 0; i < 3; i++) {
    colorMusic[i] = 0;
  }

  // Why FHT_N / 2 ???
  for (int i = 0; i < FHT_N / 2; i++) {
    if (fht_log_out[i] < FREQ_LOW_PASS)
      fht_log_out[i] = 0;
  }

  // низкие частоты, выборка со 2 по 5 тон (0 и 1 зашумленные!)
  for (byte i = 2; i < 6; i++) {
    if (fht_log_out[i] > colorMusic[0])
      colorMusic[0] = fht_log_out[i];
  }

  // средние частоты, выборка с 6 по 10 тон
  for (byte i = 6; i < 11; i++) {
    if (fht_log_out[i] > colorMusic[1])
      colorMusic[1] = fht_log_out[i];
  }

  // высокие частоты, выборка с 11 по 31 тон
  for (byte i = 11; i < 32; i++) {
    if (fht_log_out[i] > colorMusic[2])
      colorMusic[2] = fht_log_out[i];
  }

  // Looking for maximum frequency value in fht_log_out[] within 2..32 index range
  int maxFrequencyValue = 0;
  for (byte i = 0; i < FHT_N / 2 - 2; i++) {
    if (fht_log_out[i + 2] > maxFrequencyValue) {
      maxFrequencyValue = fht_log_out[i + 2];
    }

    // Set minimum if less than 5 (why 5 ???)
    if (maxFrequencyValue < 5) {
      maxFrequencyValue = 5;
    }

    // ДЛЯ СПЕКТРА ЧАСТОТ (5 режим)
    if (freq_f[i] < fht_log_out[i + 2])
      freq_f[i] = fht_log_out[i + 2];
    if (freq_f[i] > 0)
      freq_f[i] -= LIGHT_SMOOTH;
    else
      freq_f[i] = 0;
  }

  // Used in animation
  maxFrequencyValue_f = maxFrequencyValue * averK + maxFrequencyValue_f * (1 - averK);

  for (byte i = 0; i < 3; i++) {
    colorMusic_aver[i] = colorMusic[i] * averK + colorMusic_aver[i] * (1 - averK); // общая фильтрация

    colorMusic_f[i] = colorMusic[i] * SMOOTH_FREQ + colorMusic_f[i] * (1 - SMOOTH_FREQ); // локальная

    if (colorMusic_f[i] > ((float)colorMusic_aver[i] * MAX_COEF_FREQ)) {
      freqBrightness[i] = 255;
      //colorMusicFlash[i] = true;
      // running_flag[i] = true;
    } // else colorMusicFlash[i] = false;

    if (freqBrightness[i] >= 0)
      freqBrightness[i] -= FREQ_SMOOTH_STEP;

    if (freqBrightness[i] < EMPTY_BRIGHT) {
      freqBrightness[i] = EMPTY_BRIGHT;
      // running_flag[i] = false;
    }
  }
}

void sendVolumeUnitData() {
  String soundVolume = (String)getStaticSoundVolumeValue();

  //Serial.println(soundVolume);
  Serial.println("$" + (String)soundAnalyzeMode + "#" + soundVolume + ";");
  Serial1.print("$" + (String)soundAnalyzeMode + "#" + soundVolume + ";");
  //  Serial1.print("$" + soundVolume + ";");
}

void sendFreqBrightnessData() {
  analyzeFrequencies();
  String brightnessString = (String)freqBrightness[0] + "#" + (String)freqBrightness[1] + "#" + (String)freqBrightness[2];

  //Serial.println(brightnessString);
  Serial.println("$" + (String)soundAnalyzeMode + "#" + brightnessString + ";");
  Serial1.print("$" + (String)soundAnalyzeMode + "#" + brightnessString + ";");
  //  Serial1.print("$" + brightnessString + ";");
}

void sendFreqSpectreData() {
  analyzeFrequencies();

  int maxFreq = (int)floor(maxFrequencyValue_f);

  String spectreString =
      (String)freq_f[0] + "#" + (String)freq_f[1] + "#" + (String)freq_f[2] + "#" +
      (String)freq_f[3] + "#" + (String)freq_f[4] + "#" + (String)freq_f[5] + "#" +
      (String)freq_f[6] + "#" + (String)freq_f[7] + "#" + (String)freq_f[8] + "#" +
      (String)freq_f[9] + "#" + (String)freq_f[10] + "#" + (String)freq_f[11] + "#" +
      (String)freq_f[12] + "#" + (String)freq_f[13] + "#" + (String)freq_f[14] + "#" +
      (String)freq_f[15] + "#" + (String)freq_f[16] + "#" + (String)freq_f[17] + "#" +
      (String)freq_f[18] + "#" + (String)freq_f[19] + "#" + (String)freq_f[20] + "#" +
      (String)freq_f[21] + "#" + (String)freq_f[22] + "#" + (String)freq_f[23] + "#" +
      (String)freq_f[24] + "#" + (String)freq_f[25] + "#" + (String)freq_f[26] + "#" +
      (String)freq_f[27] + "#" + (String)freq_f[28] + "#" + (String)freq_f[29] + "#" +
      (String)freq_f[30] + "#" + (String)freq_f[31] + "#" + (String)maxFreq;

  //Serial.println(spectreString);
  //Serial.println("$" + (String)soundAnalyzeMode + "#" + spectreString + ";");
  Serial1.print("$" + (String)soundAnalyzeMode + "#" + spectreString + ";");
  //  Serial1.print("$" + spectreString + ";");
}

void sendColorMode() {
  Serial1.print("$" + (String)soundAnalyzeMode + ";");
}

void getModePacketValue() {
  char header = '<'; // стартовый символ
  char ending = '>'; // завершающий символ
  boolean valueRecieved = false;
  boolean parseStarted = false;
  String resultValue = "";

  while (Serial1.available() > 0) {
    char incomingByte = Serial1.read();

    if (parseStarted && incomingByte != ending) {
      resultValue += incomingByte;
      packetMode = resultValue.toInt();
      resultValue = "";
    }

    if (incomingByte == header) {
      parseStarted = true;
    } else if (incomingByte == ending) {
      parseStarted = false;
      valueRecieved = true;
    }
  }

  if (valueRecieved) {
    //
  }
}

void setupRandomModes() {
  randomModes[0] = 1;
  randomModes[1] = 1;
  randomModes[2] = 2;
  randomModes[3] = 3;
  randomModes[4] = 4;
  randomModes[5] = 5;
  randomSeed(analogRead(ADC_0));
}

void updateRandomPeriod() {
  randomPeriod = random(1500, 6000);
}

void updateRandomTimer() {
  randomTimer = millis();
}

byte getNextRandomMode() {
  return randomModes[random(0, RANDOM_MODE_COUNT)];
}

boolean shouldSelectNextRandomMode() {
  return millis() - randomTimer > randomPeriod;
}

void setup() {
  delay(1000);
  analogReference(INTERNAL1V1);

  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);

  Serial.begin(115200);
  Serial1.begin(115200);

  setupRandomModes();
}

void loop() {
  getModePacketValue();

  if (packetMode != 0) {
    soundAnalyzeMode = packetMode;
  }

  if (soundAnalyzeMode == RANDOM_MODE) {
    if (millis() - randomTimer > randomPeriod) {
      updateRandomTimer();
      updateRandomPeriod();
      currentRandomMode = getNextRandomMode();
    }

    soundAnalyzeMode = currentRandomMode;
  }

  switch (soundAnalyzeMode) {
    case VU_MODE:
      sendVolumeUnitData();
      break;
    case FREQ_MODE_SYMMETRICAL:
    case FREQ_MODE_THREE_STRIPES:
    case FREQ_MODE_ONE_STRIPE:
      sendFreqBrightnessData();
      break;
    case FREQ_MODE_SPECTRE:
      sendFreqSpectreData();
      break;
    case COLOR_MODE:
      sendColorMode();
      break;
    default:
      break;
  }
}
