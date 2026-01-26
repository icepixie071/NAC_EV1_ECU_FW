#include <ArduinoRS485.h>   // ArduinoModbus depends on the ArduinoRS485 library
#include <ArduinoModbus.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <Wire.h>
#include <SoftwareSerial.h>

#define PIN_MAX485_DE         (2)
#define PIN_SPD_PULSE_OUT     (10)
#define PIN_BUZ_PULSE_OUT     (11)
#define PIN_SW_UART_TX        (7)
#define PIN_SW_UART_RX        (4)
#define PIN_SWAN_SPD_PULSE_IN (12)
#define PIN_ACC_SIG_SW_OUT    (3)

#define I2C_ADDR_CPU2         (0x08)
#define MODBUS_SLAVE_ID_ECU6A (2)

#define LPF_TOTAL_NUM (8)
#define KMH_CNV_PARAM (0.00607974)  // 3200[rpm] = 19.4552[km/h]
#define PLS_CNV_PARAM (1.29701)     // 19.4552[Hz] = 15[km/h]
#define PLS_UPDATE_TH (1.0)         // Hz

#define TIMER_1_FRQ   (7813.0f)     // = 16[MHz] -> 1024[div] -> 7813[Hz]
#define TIMER_1_DUTY  (0.5f)        // = 50%
#define TIMER_2_FRQ   (7813.0f)     // = 16[MHz] -> 1024[div] -> 7813[Hz]
#define TIMER_2_DUTY  (0.5f)        // = 50%

SoftwareSerial DebugSerial(PIN_SW_UART_RX, PIN_SW_UART_TX);

void getSpeedDataECU6A(void);
void getBattDataECU6A(void);
void setOutSpeedPulse(float);

float g_swan_spd_pls_hz = 0;
float g_motspd_rpm      = 0;
float g_vehspd_kmh      = 0;
float g_pulse_hz        = 0;
float g_pulse_pre_hz    = 0;
float g_pulse_diff_hz   = 0;
float g_tone_pwm_hz     = 0;
float g_acc_out         = 0;
uint16_t g_motspd_data  = 0;
uint16_t g_comm_status  = 0;
uint8_t g_battlev_data  = 0;
int16_t g_acc_in_ad     = 0;


void setup() 
{
  // Timer Register map https://usicolog.nomaki.jp/engineering/avr/avrPWM.html
  // Timer* out[Hz] = 16[MHz] / (2 * prescaler_ratio * (OCR*A + 1))

  // Timer1 Setting for Mater pulse 
  TCCR1A = 0b00100001;  // D10 PWM(non invert mode)
  TCCR1B = 0b00010101;  // Pre scaler ratio = 1/1024 (sorce 16[MHz], output 7812.5[Hz])
  pinMode(PIN_SPD_PULSE_OUT, OUTPUT);

  // Timer2 Setting for Buzzer pulse
  TCCR2A = 0b01000011;  // D11 PWM(non invert mode) https://forum.arduino.cc/t/pwm-for-a-complete-noob/341473/8
  TCCR2B = 0b00001111;  // Pre scaler ratio = 1/1024 (sorce 16[MHz], output 7812.5[Hz])
  pinMode(PIN_BUZ_PULSE_OUT, OUTPUT);

  // Setup Accsel signal sw relay output
  pinMode(PIN_ACC_SIG_SW_OUT, OUTPUT);
  digitalWrite(PIN_ACC_SIG_SW_OUT, 0);
  delay(5000);
  digitalWrite(PIN_ACC_SIG_SW_OUT, 1);

  // Setup Debug serial (soft serial)
  DebugSerial.begin(19200);
  pinMode(PIN_SW_UART_RX, INPUT);
  pinMode(PIN_SW_UART_TX, OUTPUT);

  // WDT
  wdt_enable(WDTO_4S);

  // Setup serial MODBUS port
  Serial.begin(19200, SERIAL_8N1);  // baud-rate at 19200 for MODBUS
  pinMode(PIN_MAX485_DE, OUTPUT);   // DE_PIN is controled by "ArduinoRS485.h"
  while (!Serial);

  // start the Modbus RTU client
  if (!ModbusRTUClient.begin(19200))
  {
    while (1);
  }
}


void getSpeedDataECU6A() 
{
  // send a Holding registers read request to (slave) id X, for 1 registers
  if (ModbusRTUClient.requestFrom(MODBUS_SLAVE_ID_ECU6A, HOLDING_REGISTERS, 0x00, 1))
  {
    g_motspd_data = ModbusRTUClient.read();
  }

  DebugSerial.println(g_motspd_data);
}


void getBattDataECU6A() 
{

}


void loop() 
{
  static uint8_t tone_state = 0;

  // WDT reset
  wdt_reset();
  
  getSpeedDataECU6A();

  delay(200);

  // update veh speed pulse out
  g_swan_spd_pls_hz = (float) g_motspd_data;
  g_pulse_hz = g_swan_spd_pls_hz / 130;

  if (g_pulse_hz < 1.0)           g_pulse_hz = 1.0;
  if (g_pulse_hz > 10000.0)       g_pulse_hz = 1.0;

    OCR1A = (uint16_t)(TIMER_1_FRQ / g_pulse_hz) - 1;                 // set timer max val
    OCR1B = (uint16_t)(TIMER_1_FRQ / g_pulse_hz * TIMER_1_DUTY) - 1;  // set duty ratio

  // update buzzer pulse out
  if (tone_state == 0) 
  {
    tone_state = 1;
    g_tone_pwm_hz = 1800;  // 120Hz~ 50Hz / 1km/h
  }
  else
  {
    tone_state = 0;
    g_tone_pwm_hz = 450;  // 120Hz~ 50Hz / 1km/h
  }

  OCR2A = (uint8_t)(TIMER_2_FRQ / g_tone_pwm_hz) - 1;                 // set timer max val
  OCR2B = (uint8_t)(TIMER_2_FRQ / g_tone_pwm_hz * TIMER_2_DUTY) - 1;  // set duty ratio 

  // CPU2 Communication I2C
  Wire.beginTransmission(I2C_ADDR_CPU2);
  Wire.write(g_battlev_data);              // send battlev
  Wire.endTransmission();

  // for debug
  DebugSerial.print("spd: ");
  DebugSerial.println(g_motspd_data);
}