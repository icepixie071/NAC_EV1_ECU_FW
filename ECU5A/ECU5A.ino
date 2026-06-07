#include <avr/io.h>
#include <avr/wdt.h>
#include <Wire.h>
#include <SoftwareSerial.h>

#define PIN_OUT_MAX485_DE         (2)
#define PIN_OUT_SPD_PULSE         (10)
#define PIN_OUT_BUZZ_PULSE        (11)
#define PIN_OUT_SW_UART_TX        (7)
#define PIN_IN_SW_UART_RX         (4)
#define PIN_IN_REVERSE_SIG        (12)
#define PIN_OUT_ACC_SIG_EN_SW     (3)

#define RX_PACKET_SIZE            (5)
#define RX_BUFF_SIZE              (5)
#define UART_VAL_HEADER           (0xAAU)
#define UART_VAL_FOOTER           (0xCCU)
#define UART_RX_TIMEOUT_VAL       (200)         // about 5[sec]

#define I2C_ADDR_CPU2             (0x08)

#define VEHSPD2MATER_PLS_CONV_K   (6.0)

#define TIMER_1_FRQ               (7813.0f)     // = 16[MHz] -> 1024[div] -> 7813[Hz]
#define TIMER_1_DUTY              (0.5f)        // = 50%

#define BUZZ_FRQ_SPD_BASE_HZ      (2000.0f)     // Hz
#define BUZZ_FRQ_SPD_COEFF_HZ     (120.0f)      // Hz/Kmh
#define BUZZ_FRQ_REV_HZ           (200.0f)      // Hz
#define BUZZ_BEEP_OUT_SPD_TH      (1.0f)        // kmh
#define BUZZ_STATE_ALL_BEEP_OFF   (0)
#define BUZZ_STATE_SPD_BEEP_OUT   (1)
#define BUZZ_STATE_REV_BEEP_OUT   (2)
#define BUZZ_STATE_REV_BEEP_OFF   (3)
#define BUZZ_TASK_PERIOD_MS       (500)

#define BATT_STS_ERR              (99U)

SoftwareSerial DebugSerial(PIN_IN_SW_UART_RX, PIN_OUT_SW_UART_TX);

void getSpeedDataECU6A(void);
void getBattDataECU6A(void);
void setOutSpeedPulse(float);
void setReverseBuzzerSound(void);

float g_veh_spd_kmh             = 0;
float g_mater_sig_pulse_hz      = 0;
float g_tone_pwm_hz             = 0;
uint8_t g_battlev_data          = 0;
uint8_t g_buzzer_state          = 0;
uint8_t g_uart_rx_buff[RX_BUFF_SIZE] = {};
uint32_t g_uart_rx_timeout_cnt  = 0;
uint32_t g_sw_timer_now         = 0;
uint32_t g_sw_timer_pre         = 0;

void setup() 
{
  // I2C
  Wire.begin();
  pinMode (SDA, INPUT); // disable pullup
  pinMode (SCL, INPUT); // disable pullup

  // Timer Register map https://usicolog.nomaki.jp/engineering/avr/avrPWM.html
  // Timer* out[Hz] = 16[MHz] / (2 * prescaler_ratio * (OCR*A + 1))

  // Timer1 Setting for Mater pulse 
  TCCR1A = 0b00100001;  // D10 PWM(Fast PWM, Non invert mode)
  TCCR1B = 0b00010101;  // Pre scaler ratio = 1/1024 (sorce 16[MHz], output 7812.5[Hz])
  pinMode(PIN_OUT_SPD_PULSE, OUTPUT);

  // Timer2 Setting for Buzzer pulse
  pinMode(PIN_IN_REVERSE_SIG, INPUT);
  pinMode(PIN_OUT_BUZZ_PULSE, OUTPUT);
  noTone(PIN_OUT_BUZZ_PULSE);
  g_tone_pwm_hz = BUZZ_FRQ_SPD_BASE_HZ;

  // Setup Accsel signal sw relay output
  pinMode(PIN_OUT_ACC_SIG_EN_SW, OUTPUT);
  digitalWrite(PIN_OUT_ACC_SIG_EN_SW, 0);
  delay(5000);
  digitalWrite(PIN_OUT_ACC_SIG_EN_SW, 1);

  // Setup Debug serial (soft serial)
  DebugSerial.begin(19200);
  pinMode(PIN_IN_SW_UART_RX, INPUT);
  pinMode(PIN_OUT_SW_UART_TX, OUTPUT);

  // WDT
  wdt_enable(WDTO_4S);

  // Setup serial port
  Serial.begin(19200, SERIAL_8E1);      // even parity and 1 stop bit
  pinMode(PIN_OUT_MAX485_DE, OUTPUT);   // DE_PIN is enable
  digitalWrite(PIN_OUT_MAX485_DE, 0);   // disable tx
  while (!Serial);

  // setup timer interrupt for buzzer
  g_buzzer_state = BUZZ_STATE_SPD_BEEP_OUT;
}


void setReverseBuzzerSound() 
{
  switch (g_buzzer_state)
  {
    case BUZZ_STATE_REV_BEEP_OFF:
      tone(PIN_OUT_BUZZ_PULSE, (unsigned int)BUZZ_FRQ_REV_HZ);
      g_buzzer_state = BUZZ_STATE_REV_BEEP_OUT;
      break;

    case BUZZ_STATE_REV_BEEP_OUT:
      noTone(PIN_OUT_BUZZ_PULSE);
      g_buzzer_state = BUZZ_STATE_REV_BEEP_OFF;
      break;
    
    default:
      /* Do nothing */
      break;    
  }
}


void loop() 
{
  // WDT reset
  wdt_reset();

  // check UART recieve buffer
  uint8_t trash_buff = 0;

  if (Serial.available() >= RX_PACKET_SIZE)
  {
    g_uart_rx_buff[0] = Serial.read();
    g_uart_rx_buff[1] = Serial.read();
    g_uart_rx_buff[2] = Serial.read();
    g_uart_rx_buff[3] = Serial.read();
    g_uart_rx_buff[4] = Serial.read();

    // verify packet header and footer value 
    if (g_uart_rx_buff[0] == UART_VAL_HEADER && g_uart_rx_buff[4] == UART_VAL_FOOTER)
    {
      // verify check sum 
      if (g_uart_rx_buff[3] == ((g_uart_rx_buff[1] + g_uart_rx_buff[2]) & 0xFF))
      {
        g_veh_spd_kmh         = g_uart_rx_buff[1];
        g_battlev_data        = g_uart_rx_buff[2];
        g_uart_rx_timeout_cnt = 0;
      }
    }
    
    // init UART recieve buffer
    while (Serial.available() > 0) trash_buff = Serial.read();
  }

  // update veh speed pulse out
  g_mater_sig_pulse_hz = g_veh_spd_kmh * VEHSPD2MATER_PLS_CONV_K;

  if (g_mater_sig_pulse_hz < 1.0)     g_mater_sig_pulse_hz = 1.0;
  if (g_mater_sig_pulse_hz > 10000.0) g_mater_sig_pulse_hz = 10000.0;

  OCR1A = (uint16_t)(TIMER_1_FRQ / g_mater_sig_pulse_hz) - 1;                 // Timer1(16bit) set timer max val
  OCR1B = (uint16_t)(TIMER_1_FRQ / g_mater_sig_pulse_hz * TIMER_1_DUTY) - 1;  // Timer1(16bit) set duty ratio
  
  // Poling timer, Act reverse sound task
  g_sw_timer_now = millis();

  if (g_sw_timer_now - g_sw_timer_pre >= BUZZ_TASK_PERIOD_MS) 
  {
    g_sw_timer_pre = g_sw_timer_now;

    if (g_buzzer_state == BUZZ_STATE_REV_BEEP_OFF || g_buzzer_state == BUZZ_STATE_REV_BEEP_OUT)
    {
      setReverseBuzzerSound();
    }
  }

  // judge buzzer out state
  if (g_buzzer_state == BUZZ_STATE_SPD_BEEP_OUT && (digitalRead(PIN_IN_REVERSE_SIG)))
  {
    g_buzzer_state = BUZZ_STATE_REV_BEEP_OUT;
  }
  else if (digitalRead(PIN_IN_REVERSE_SIG) == 0)
  {
    g_tone_pwm_hz = BUZZ_FRQ_SPD_BASE_HZ + (g_veh_spd_kmh * BUZZ_FRQ_SPD_COEFF_HZ);

    if (g_tone_pwm_hz < 1.0)     g_tone_pwm_hz = 1.0;
    if (g_tone_pwm_hz > 10000.0) g_tone_pwm_hz = 10000.0;

    g_buzzer_state = BUZZ_STATE_SPD_BEEP_OUT;

    // compare buzzer out speed 
    if (g_veh_spd_kmh > BUZZ_BEEP_OUT_SPD_TH)
    {
      tone(PIN_OUT_BUZZ_PULSE, (unsigned int)g_tone_pwm_hz);
    }
    else
    {
      noTone(PIN_OUT_BUZZ_PULSE);
    }
  }
  else
  {
    /* Do nothing */
  }

  // CPU2 Communication I2C
  Wire.beginTransmission(I2C_ADDR_CPU2);
  Wire.write(g_battlev_data);              // send battlev
  Wire.write(g_battlev_data);              // send battlev for dat glitch check
  Wire.endTransmission();

  // UART recieve timeout counter
  if (g_uart_rx_timeout_cnt <= UART_RX_TIMEOUT_VAL)
  {
    g_uart_rx_timeout_cnt ++;
  }
  else
  {
    g_uart_rx_timeout_cnt = UART_RX_TIMEOUT_VAL;
    g_battlev_data        = BATT_STS_ERR;
  }

  // for debug
  Serial.print("spd: ");
  Serial.println(g_veh_spd_kmh);
  Serial.print("batt: ");
  Serial.println(g_battlev_data);
  Serial.print("tone: ");
  Serial.println(g_tone_pwm_hz);
  Serial.print("rx_sts: ");
  Serial.println(g_uart_rx_timeout_cnt);
}