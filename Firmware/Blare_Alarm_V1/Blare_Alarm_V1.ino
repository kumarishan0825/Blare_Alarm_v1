#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#define TFT_SCLK 9    // D9  -> display SCL
#define TFT_MOSI 10   // D10 -> display SDA
#define TFT_RST  8    // D8
#define TFT_DC   6    // D4
#define TFT_CS   7    // D5
#define TFT_BL   21   // D6
#define BUZZER_PIN 20   
#define BTN1_PIN   2    
#define BTN2_PIN   3    
#define BTN3_PIN   4    
#define BTN4_PIN   5    
#define BUZZER_PASSIVE 0   // 0 = active buzzer, 1 = passive buzzer (uses tone())
#define START_HOUR 7
#define START_MIN  42

class MyST7789 : public Adafruit_ST7789 {
public:
  MyST7789(int8_t cs, int8_t dc, int8_t mosi, int8_t sclk, int8_t rst)
    : Adafruit_ST7789(cs, dc, mosi, sclk, rst) {}

  void setOffsets(uint8_t col, uint8_t row) {
    _colstart = _colstart2 = col;
    _rowstart = _rowstart2 = row;
  }
};

MyST7789 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

int hourNow = START_HOUR, minNow = START_MIN, secNow = 0;
int alarmH = 8, alarmM = 0;
unsigned long lastTick = 0;

enum Mode { CLOCK_MODE, SET_HOURS, SET_MINUTES, RINGING };
Mode mode = CLOCK_MODE;

bool alarmHandled = false;   
bool needRedraw = true;

const uint8_t btnPins[4] = { BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN };
bool lastReading[4]  = { false, false, false, false };
bool stableState[4]  = { false, false, false, false };
unsigned long lastChange[4] = { 0, 0, 0, 0 };
const unsigned long DEBOUNCE_MS = 30;

bool buttonPressed(int i) {
  bool reading = (digitalRead(btnPins[i]) == LOW);
  if (reading != lastReading[i]) {
    lastReading[i] = reading;
    lastChange[i] = millis();
  }
  if (millis() - lastChange[i] > DEBOUNCE_MS && reading != stableState[i]) {
    stableState[i] = reading;
    if (stableState[i]) return true;
  }
  return false;
}

void setBuzzer(bool on) {
  static bool current = false;
  if (on == current) return;
  current = on;
#if BUZZER_PASSIVE
  if (on) tone(BUZZER_PIN, 2000);
  else noTone(BUZZER_PIN);
#else
  digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
#endif
}

void updateClock() {
  while (millis() - lastTick >= 1000) {
    lastTick += 1000;
    secNow++;
    if (secNow >= 60) {
      secNow = 0;
      minNow++;
      if (minNow >= 60) {
        minNow = 0;
        hourNow = (hourNow + 1) % 24;
      }
      if (mode == CLOCK_MODE) needRedraw = true;  
    }
  }
}

void print2(int n) {
  if (n < 10) tft.print('0');
  tft.print(n);
}
void drawTime(int x, int y, int size, int h, int m, uint16_t hCol, uint16_t mCol) {
  int cw = 6 * size;   
  tft.setTextSize(size);
  tft.setTextColor(hCol);
  tft.setCursor(x, y);
  print2(h);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(x + 2 * cw, y);
  tft.print(':');
  tft.setTextColor(mCol);
  tft.setCursor(x + 3 * cw, y);
  print2(m);
}

void drawScreen() {
  tft.fillScreen(ST77XX_BLACK);

  if (mode == CLOCK_MODE) {
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(2, 2);
    tft.print("BLARE");

    drawTime(67, 6, 5, hourNow, minNow, ST77XX_WHITE, ST77XX_WHITE);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(76, 48);
    tft.print("Alarm ");
    print2(alarmH);
    tft.print(':');
    print2(alarmM);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(76, 67);
    tft.print("[SET] [UP] [DOWN] [OK]");

  } else if (mode == SET_HOURS || mode == SET_MINUTES) {
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(88, 2);
    tft.print("SET ALARM");

    bool editingHours = (mode == SET_HOURS);
    drawTime(82, 22, 4, alarmH, alarmM,
             editingHours ? ST77XX_YELLOW : ST77XX_WHITE,
             editingHours ? ST77XX_WHITE : ST77XX_YELLOW);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(editingHours ? 106 : 100, 58);
    tft.print(editingHours ? "HOURS" : "MINUTES");

  } else if (mode == RINGING) {
    tft.setTextSize(3);
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(88, 0);
    tft.print("ALARM!");

    drawTime(102, 28, 2, alarmH, alarmM, ST77XX_WHITE, ST77XX_WHITE);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(88, 52);
    tft.print("PRESS OK");
  }
}

void handleButtons() {
  bool b1 = buttonPressed(0);  
  bool b2 = buttonPressed(1);  
  bool b3 = buttonPressed(2);  
  bool b4 = buttonPressed(3);  

  if (mode == RINGING) {
    if (b4) {                  
      alarmHandled = true;
      mode = CLOCK_MODE;
      needRedraw = true;
    }
    return;
  }

  if (mode == CLOCK_MODE) {
    if (b1) {
      mode = SET_HOURS;
      needRedraw = true;
    }
    return;
  }

  if (b1) {                    
    mode = (mode == SET_HOURS) ? SET_MINUTES : SET_HOURS;
    needRedraw = true;
  }
  if (b2) {                    
    if (mode == SET_HOURS) alarmH = (alarmH + 1) % 24;
    else alarmM = (alarmM + 1) % 60;
    needRedraw = true;
  }
  if (b3) {                    
    if (mode == SET_HOURS) alarmH = (alarmH + 23) % 24;
    else alarmM = (alarmM + 59) % 60;
    needRedraw = true;
  }
  if (b4) {                   
    alarmHandled = false;
    mode = CLOCK_MODE;
    needRedraw = true;
  }
}

void checkAlarm() {
  if (minNow != alarmM || hourNow != alarmH) alarmHandled = false;

  if (mode == CLOCK_MODE && !alarmHandled &&
      hourNow == alarmH && minNow == alarmM) {
    mode = RINGING;
    needRedraw = true;
  }
}

void setup() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);

  tft.init(76, 284);
  tft.setOffsets(82, 18);
  tft.invertDisplay(false);
  tft.setRotation(1);

  for (int i = 0; i < 4; i++) pinMode(btnPins[i], INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  lastTick = millis();
}

void loop() {
  updateClock();
  handleButtons();
  checkAlarm();

  if (mode == RINGING) setBuzzer((millis() / 250) % 2 == 0);
  else setBuzzer(false);

  if (needRedraw) {
    needRedraw = false;
    drawScreen();
  }
}
