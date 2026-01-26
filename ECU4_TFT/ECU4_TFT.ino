#include <Adafruit_GFX.h>     // Adafruit GFXライブラリ
#include <Adafruit_ST7789.h>  // Adafruit ST7789ライブラリ
#include <SPI.h>

// SHIFT status
#define SHIFT_STS_P         (0x00)
#define SHIFT_STS_N         (0x01)
#define SHIFT_STS_D         (0x02)
#define SHIFT_STS_R         (0x03)
#define SHIFT_STS_L         (0x04)
#define SHIFT_STS_ERR       (0x05)

#define PIN_TFT_CS          (10)  // none
#define PIN_TFT_RST         (14)  // blue
#define PIN_TFT_DC          (15)  // green

// ST7789
Adafruit_ST7789 tft = Adafruit_ST7789(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

char displayChar = 'P';             // デフォルトで"P"を表示
static ch
新規スケッチ
ar lastDisplayChar = 'P';  // 最後に表示した文字
uint8_t shift_state = SHIFT_STS_P;


void setup() {
  // put your setup code here, to run once:

  Serial.begin(9600);

  // init display ST7789
  tft.init(240, 240, SPI_MODE2);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(100);
  tft.setTextColor(ST77XX_WHITE);
}

void loop() {
  // put your main code here, to run repeatedly:

  while (Serial.available() > 0)
  {
    shift_state = (uint8_t)Serial.read();
  }

  switch (shift_state)
  {
    case SHIFT_STS_P:   displayChar = 'P'; break;
    case SHIFT_STS_N:   displayChar = 'N'; break;
    case SHIFT_STS_D:   displayChar = 'D'; break;
    case SHIFT_STS_R:   displayChar = 'R'; break;
    case SHIFT_STS_L:   displayChar = 'L'; break;
    case SHIFT_STS_ERR: displayChar = 'P'; break;
  }

  // 文字が変更された場合は画面を黒くする
  if (displayChar != lastDisplayChar) 
  {
    tft.fillScreen(ST77XX_BLACK);   // 画面を黒に
    lastDisplayChar = displayChar;  // 最後の文字を更新
  }

  // 液晶ディスプレイに文字を表示
  tft.setCursor(50, 30);            // 文字を中央に配置
  tft.setRotation(1);

  if (displayChar == 'R')
  {
    tft.setTextColor(ST77XX_RED);
  } 
  else 
  {
    tft.setTextColor(ST77XX_WHITE);
  }

  tft.setTextSize(25);              // テキストサイズを画面いっぱいに
  tft.print(displayChar);

  delay(100);
}
