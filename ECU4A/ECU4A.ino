#include <Wire.h>
#include <avr/wdt.h>

#define I2C_ADDR_CPU2       (0x08)

#define PIN_IN_SHIFT_R      (12)
#define PIN_IN_SHIFT_D      (8)
#define PIN_IN_SHIFT_L      (7)
#define PIN_IN_SHIFT_N      (4)

#define PIN_IN_SIDE_BRK     (A7)

#define PIN_IN_MASTER_SW    (A6)
#define PIN_IN_HI_BEAM      (17)
#define PIN_IN_LW_BEAM      (16)

#define PIN_IN_VBATT_ADC    (A0)

#define PIN_OUT_BACK_LMP    (6)
#define PIN_OUT_BUZZ_REV    (3)
#define PIN_OUT_BUZZ_SPD    (5)

// SHIFT status 
#define SHIFT_STS_P         (0x00)
#define SHIFT_STS_N         (0x01)
#define SHIFT_STS_D         (0x02)
#define SHIFT_STS_R         (0x03)
#define SHIFT_STS_L         (0x04)
#define SHIFT_STS_ERR       (0x05)

uint8_t shift_state     = SHIFT_STS_ERR;
uint8_t shift_state_pre = SHIFT_STS_ERR;
uint8_t shift_state_out = SHIFT_STS_ERR;

// SIDE BRAKE status
#define SIDE_BRK_STS_OFF    (0x00)
#define SIDE_BRK_STS_ON     (0x01)
#define SIDE_BRK_ON_LEVEL   (400)

uint8_t s_brk_state     = SIDE_BRK_STS_OFF;
uint8_t s_brk_state_pre = SIDE_BRK_STS_OFF;
uint8_t s_brk_state_out = SIDE_BRK_STS_OFF;

// LIGHT status
#define LIGHT_MSTR_SW_STS_OFF   (0x00)
#define LIGHT_MSTR_SW_STS_ON    (0x01)
#define LIGHT_MSTR_SW_ON_LEVEL  (800)

#define LIGHT_STS_OFF           (0x00)
#define LIGHT_STS_LW_BEAM       (0x01)
#define LIGHT_STS_HI_BEAM       (0x02)

uint8_t light_mstr_sw_state     = LIGHT_MSTR_SW_STS_OFF;
uint8_t light_mstr_sw_state_pre = LIGHT_MSTR_SW_STS_OFF;
uint8_t light_mstr_sw_state_out = LIGHT_MSTR_SW_STS_OFF;

uint8_t light_state     = LIGHT_STS_OFF;
uint8_t light_state_pre = LIGHT_STS_OFF;
uint8_t light_state_out = LIGHT_STS_OFF;

// VBATT ADC
#define VBATT_ADC_OFSET     (512)
#define VBATT_LPF_K         (0.8f)
#define VBATT_LOW_TH        (11.0f)
#define VBATT_BIN2VOLT_K    (0.04111f)    // K = 実測値base @ 12.54(V) : ADC=305
#define VBATT_LVL_STS_NOM   (0x00)
#define VBATT_LVL_STS_LOW   (0x01)

float   vbatt_lpf     = 0; 
uint8_t vbatt_low_flg = 0;


void setup() {
  // put your setup code here, to run once:

  // ECU3A
  pinMode(PIN_IN_SHIFT_R, INPUT);
  pinMode(PIN_IN_SHIFT_D, INPUT);
  pinMode(PIN_IN_SHIFT_L, INPUT);
  pinMode(PIN_IN_SHIFT_N, INPUT);
  pinMode(PIN_IN_VBATT_ADC, INPUT);

  // ECU3B
  pinMode(PIN_IN_MASTER_SW, INPUT);
  pinMode(PIN_IN_HI_BEAM, INPUT);
  pinMode(PIN_IN_LW_BEAM, INPUT); 
  pinMode(PIN_OUT_BUZZ_REV, OUTPUT);
  pinMode(PIN_OUT_BUZZ_SPD, OUTPUT);

  // ECU3C
  pinMode(PIN_IN_SIDE_BRK, INPUT);  
  pinMode(PIN_OUT_BACK_LMP, OUTPUT);

  // I2C
  Wire.begin();
  pinMode (SDA, INPUT); // disable pullup
  pinMode (SCL, INPUT); // disable pullup

  // UART
  Serial.begin(9600);

  // WDT
  wdt_enable(WDTO_4S);
}

void loop() {
  // put your main code here, to run repeatedly:

  // WDT reset
  wdt_reset();
  
  // Check shift state
  if (digitalRead(PIN_IN_SHIFT_R)) 
  {
    shift_state = SHIFT_STS_R;
  }
  else if (digitalRead(PIN_IN_SHIFT_D) && !digitalRead(PIN_IN_SHIFT_L))
  {
    shift_state = SHIFT_STS_D;
  }
  else if (digitalRead(PIN_IN_SHIFT_D) && digitalRead(PIN_IN_SHIFT_L))
  {
    shift_state = SHIFT_STS_L;
  }
  else if (digitalRead(PIN_IN_SHIFT_N))
  {
    shift_state = SHIFT_STS_N;
  }
  else
  {
    shift_state = SHIFT_STS_P;
  }

  // Avoid chattering
  if (shift_state == shift_state_pre) shift_state_out = shift_state;
  shift_state_pre = shift_state;

  // Output Back lamp
  if (shift_state_out == SHIFT_STS_R)
  {
    digitalWrite(PIN_OUT_BACK_LMP, HIGH);
  }
  else
  {
    digitalWrite(PIN_OUT_BACK_LMP, LOW);
  }

  // Check Side brake
  if (analogRead(PIN_IN_SIDE_BRK) < SIDE_BRK_ON_LEVEL)
  {
    s_brk_state = SIDE_BRK_STS_ON;
  } 
  else
  {
    s_brk_state = SIDE_BRK_STS_OFF;
  }

  // Avoid chattering
  if (s_brk_state == s_brk_state_pre) s_brk_state_out = s_brk_state;
  s_brk_state_pre = s_brk_state;


  // Check light master switch state
  if (analogRead(PIN_IN_MASTER_SW) > LIGHT_MSTR_SW_ON_LEVEL)
  {
    light_mstr_sw_state = LIGHT_MSTR_SW_STS_ON;
  } 
  else
  {
    light_mstr_sw_state = LIGHT_MSTR_SW_STS_OFF;
  }

  // Avoid chattering
  if (light_mstr_sw_state == light_mstr_sw_state_pre) light_mstr_sw_state_out = light_mstr_sw_state;
  light_mstr_sw_state_pre = light_mstr_sw_state;


  // Check light beam state
  if (digitalRead(PIN_IN_HI_BEAM))
  {
    light_state = LIGHT_STS_HI_BEAM;
  }
  else if (digitalRead(PIN_IN_LW_BEAM))
  {
    light_state = LIGHT_STS_LW_BEAM;
  }
  else
  {
    light_state = LIGHT_STS_OFF;
  }

  // Avoid chattering
  if (light_state == light_state_pre) light_state_out = light_state;
  light_state_pre = light_state;


  // Calc VBATT 
  int16_t vbatt_ad = analogRead(PIN_IN_VBATT_ADC) - VBATT_ADC_OFSET;
  vbatt_lpf = (VBATT_LPF_K * vbatt_lpf) + ((1.0f - VBATT_LPF_K) * (float)vbatt_ad * VBATT_BIN2VOLT_K);
  
  if (vbatt_lpf < VBATT_LOW_TH)
  {
    vbatt_low_flg = VBATT_LVL_STS_LOW;
  }
  else
  {
    vbatt_low_flg = VBATT_LVL_STS_NOM;
  }


  // CPU2 Communication I2C
  Wire.beginTransmission(I2C_ADDR_CPU2);
  Wire.write(shift_state_out);              // send shift state
  Wire.write(s_brk_state_out);              // send side brake state
  Wire.write(light_mstr_sw_state_out);      // send light master sw state
  Wire.write(light_state_out);              // send light beam state
  Wire.write(vbatt_low_flg);                // send vbatt low flag
  Wire.endTransmission();

  // Debug Serial
  Serial.print("Shift: ");
  Serial.println(shift_state_out);

  Serial.print("side_brk: ");
  Serial.println(s_brk_state_out);
  
  Serial.print("mstr_sw: ");
  Serial.println(light_mstr_sw_state_out);

  Serial.print("light_state: ");
  Serial.println(light_state_out);

  Serial.print("vbatt: ");
  Serial.println(vbatt_lpf);

  delay(250); // Delay for Avoid chattering
}
