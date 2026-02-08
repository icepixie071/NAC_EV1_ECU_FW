#include <ArduinoRS485.h>   // ArduinoModbus depends on the ArduinoRS485 library
#include <ArduinoModbus.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <MsTimer2.h>

#define PIN_OUT_MAX485_DE         (2)
#define PIN_OUT_SPD_PULSE         (10)
#define PIN_OUT_BUZZ_PULSE        (11)
#define PIN_OUT_SW_UART_TX        (7)
#define PIN_IN_SW_UART_RX         (4)
#define PIN_IN_REVERSE_SIG        (12)
#define PIN_OUT_ACC_SIG_EN_SW     (3)

#define I2C_ADDR_CPU2             (0x08)
#define MODBUS_SLAVE_ID_ECU6A     (2)

#define LPF_TOTAL_NUM             (8)
#define KMH_CNV_PARAM             (0.00607974)  // 3200[rpm] = 19.4552[km/h]
#define PLS_CNV_PARAM             (1.29701)     // 19.4552[Hz] = 15[km/h]
#define PLS_UPDATE_TH             (1.0)         // Hz

#define TIMER_1_FRQ               (7813.0f)     // = 16[MHz] -> 1024[div] -> 7813[Hz]
#define TIMER_1_DUTY              (0.5f)        // = 50%
#define TIMER_2_FRQ               (7813.0f)     // = 16[MHz] -> 1024[div] -> 7813[Hz]
#define TIMER_2_DUTY              (0.5f)        // = 50%

#define BUZZ_FRQ_SPD_BASE_HZ      (130.0f)    // Hz
#define BUZZ_FRQ_SPD_COEFF_HZ     (6.5f)      // Hz/Kmh
#define BUZZ_FRQ_REV_HZ           (200.0f)    // Hz
#define BUZZ_STATE_ALL_BEEP_OFF   (0)
#define BUZZ_STATE_SPD_BEEP_OUT   (1)
#define BUZZ_STATE_REV_BEEP_OUT   (2)
#define BUZZ_STATE_REV_BEEP_OFF   (3)

SoftwareSerial DebugSerial(PIN_IN_SW_UART_RX, PIN_OUT_SW_UART_TX);

void getSpeedDataECU6A(void);
void getBattDataECU6A(void);
void setOutSpeedPulse(float);
void setReverseBuzzerSound(void);

float g_swan_spd_pulse_hz   = 0;
float g_motspd_rpm          = 0;
float g_vehspd_kmh          = 0;
float g_mater_sig_pulse_hz  = 0;
float g_pulse_pre_hz        = 0;
float g_pulse_diff_hz       = 0;
float g_tone_pwm_hz         = 0;
float g_acc_out             = 0;
uint16_t g_motspd_data      = 0;
uint16_t g_comm_status      = 0;
uint8_t g_battlev_data      = 0;
int16_t g_acc_in_ad         = 0;
uint8_t g_buzzer_state      = 0;


void setup() 
{
  // Timer Register map https://usicolog.nomaki.jp/engineering/avr/avrPWM.html
  // Timer* out[Hz] = 16[MHz] / (2 * prescaler_ratio * (OCR*A + 1))

  // Timer1 Setting for Mater pulse 
  TCCR1A = 0b00100001;  // D10 PWM(non invert mode)
  TCCR1B = 0b00010101;  // Pre scaler ratio = 1/1024 (sorce 16[MHz], output 7812.5[Hz])
  pinMode(PIN_OUT_SPD_PULSE, OUTPUT);

  // Timer2 Setting for Buzzer pulse
  TCCR2A = 0b01000011;  // D11 PWM(non invert mode) https://forum.arduino.cc/t/pwm-for-a-complete-noob/341473/8
  TCCR2B = 0b00001111;  // Pre scaler ratio = 1/1024 (sorce 16[MHz], output 7812.5[Hz])
  pinMode(PIN_OUT_BUZZ_PULSE, INPUT);
  pinMode(PIN_IN_REVERSE_SIG, INPUT);
  digitalWrite(PIN_OUT_BUZZ_PULSE, 0);
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

  // Setup serial MODBUS port
  Serial.begin(19200, SERIAL_8N1);      // baud-rate at 19200 for MODBUS
  pinMode(PIN_OUT_MAX485_DE, OUTPUT);   // DE_PIN is controled by "ArduinoRS485.h"
  while (!Serial);

  // start the Modbus RTU client
  if (!ModbusRTUClient.begin(19200))
  {
    while (1);
  }

  // setup timer interrupt for buzzer
  MsTimer2::set(500, setReverseBuzzerSound); // 500[msec] period
  MsTimer2::stop();
  g_buzzer_state = BUZZ_STATE_SPD_BEEP_OUT;
}


void getSpeedDataECU6A() 
{
  // send a Holding registers read request to (slave) id X, for 1 registers
  if (ModbusRTUClient.requestFrom(MODBUS_SLAVE_ID_ECU6A, HOLDING_REGISTERS, 0x00, 1))
  {
    g_motspd_data = ModbusRTUClient.read();
  }
}


void getBattDataECU6A() 
{
  // send a Holding registers read request to (slave) id X, for 1 registers
  if (ModbusRTUClient.requestFrom(MODBUS_SLAVE_ID_ECU6A, HOLDING_REGISTERS, 0x01, 1))
  {
    g_battlev_data = ModbusRTUClient.read();
  }
}


void setReverseBuzzerSound() 
{
  switch (g_buzzer_state)
  {
    case BUZZ_STATE_REV_BEEP_OFF:
      pinMode(PIN_OUT_BUZZ_PULSE, OUTPUT);
      OCR2A = (uint8_t)(TIMER_2_FRQ / BUZZ_FRQ_REV_HZ) - 1;                 // set timer max val
      OCR2B = (uint8_t)(TIMER_2_FRQ / BUZZ_FRQ_REV_HZ * TIMER_2_DUTY) - 1;  // set duty ratio
      g_buzzer_state = BUZZ_STATE_REV_BEEP_OUT;
      break;

    case BUZZ_STATE_REV_BEEP_OUT:
      pinMode(PIN_OUT_BUZZ_PULSE, INPUT);
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
  
  getSpeedDataECU6A();
  getBattDataECU6A();

  // update veh speed pulse out
  g_swan_spd_pulse_hz = (float) g_motspd_data;
  g_mater_sig_pulse_hz = g_swan_spd_pulse_hz / 130;

  if (g_mater_sig_pulse_hz < 1.0)     g_mater_sig_pulse_hz = 1.0;
  if (g_mater_sig_pulse_hz > 10000.0) g_mater_sig_pulse_hz = 10000.0;
  OCR1A = (uint16_t)(TIMER_1_FRQ / g_mater_sig_pulse_hz) - 1;                 // set timer max val
  OCR1B = (uint16_t)(TIMER_1_FRQ / g_mater_sig_pulse_hz * TIMER_1_DUTY) - 1;  // set duty ratio
  
  // judge buzzer out state
  if (g_buzzer_state == BUZZ_STATE_SPD_BEEP_OUT && (digitalRead(PIN_IN_REVERSE_SIG)))
  {
    g_buzzer_state = BUZZ_STATE_REV_BEEP_OUT;
    MsTimer2::start();
  }
  else if (digitalRead(PIN_IN_REVERSE_SIG) == 0)
  {
    MsTimer2::stop();
    g_tone_pwm_hz   = BUZZ_FRQ_SPD_BASE_HZ + (g_mater_sig_pulse_hz * BUZZ_FRQ_SPD_COEFF_HZ);

    if (g_tone_pwm_hz < 1.0)     g_tone_pwm_hz = 1.0;
    if (g_tone_pwm_hz > 10000.0) g_tone_pwm_hz = 10000.0;
    OCR2A = (uint8_t)(TIMER_2_FRQ / g_tone_pwm_hz) - 1;                 // set timer max val
    OCR2B = (uint8_t)(TIMER_2_FRQ / g_tone_pwm_hz * TIMER_2_DUTY) - 1;  // set duty ratio 

    pinMode(PIN_OUT_BUZZ_PULSE, OUTPUT);
    g_buzzer_state  = BUZZ_STATE_SPD_BEEP_OUT;
  }
  else
  {
    /* Do nothing */
  }

  // CPU2 Communication I2C
  Wire.beginTransmission(I2C_ADDR_CPU2);
  Wire.write(g_battlev_data);              // send battlev
  Wire.endTransmission();

  // for debug
  DebugSerial.print("spd: ");
  DebugSerial.println(g_motspd_data);
  DebugSerial.print("vat: ");
  DebugSerial.println(g_battlev_data);
  DebugSerial.print("tone: ");
  DebugSerial.println(g_tone_pwm_hz);
}