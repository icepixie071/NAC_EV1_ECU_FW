#include <ArduinoRS485.h>     // ArduinoModbus depends on the ArduinoRS485 library
#include <ArduinoModbus.h>
#include <SoftwareSerial.h>
#include <MsTimer2.h>
#include <avr/wdt.h>

#define PIN_MAX485_DE           (2)
#define PIN_CURR_OUT_SIG_IN     (A0)
#define PIN_CURR_CHG_SIG_IN     (A1)
#define PIN_SWAN_SPD_PULSE_IN   (16)
#define PIN_SW_UART_TX          (7)
#define PIN_SW_UART_RX          (4)
#define MODBUS_SLAVE_ID         (2)
#define CURR_SUM_MAX            (504000.0f)   // 200[Ah] = 750,000[A/sec] * 70[%] = 504,000[A/sec]
#define CURR_SUM_MIN            (0.001f)
#define CURR_SENS_VREF          (3.337f)      // 2.5[V] * (R2 / (R1 + R2)) * 8.2(GAIN) + 0.5VCC, (R1=47[kΩ], R2=2[kΩ])
#define PERCENT_MAX             (100.0f)
#define SAMPLING_T_MS           (100)         // [msec]        

const int numCoils            = 10;
const int numDiscreteInputs   = 10;
const int numHoldingRegisters = 10;
const int numInputRegisters   = 10;

SoftwareSerial DebugSerial(PIN_SW_UART_RX, PIN_SW_UART_TX);

void getCurrSensVolt(void);

float g_curr_out_volt           = 0;       
float g_curr_out_amp            = 0;
float g_curr_chg_volt           = 0;
float g_curr_chg_amp            = 0;
float g_curr_sum_amp            = 0;
float g_batt_lev_pct            = 0;
float g_swan_spd_pls_hz_fl      = 0;
uint16_t g_batt_lev_pct_dig     = 0;
uint16_t g_swan_spd_pls_hz_int  = 0;


void setup()
{
  // WDT
  wdt_enable(WDTO_8S);

  // Setup serial MODBUS port
  Serial.begin(19200, SERIAL_8N1);  // baud-rate at 19200 for MODBUS
  pinMode(PIN_MAX485_DE, OUTPUT);   // DE_PIN is controled by "ArduinoRS485.h"
  while (!Serial);

  // start the Modbus RTU server, with (slave) id 42
  if (!ModbusRTUServer.begin(MODBUS_SLAVE_ID, 19200))
  {
    while (1);
  }
  
  // configure ModbusRTUServer
  ModbusRTUServer.configureCoils(0x00, numCoils);                         // configure coils at address 0x00
  ModbusRTUServer.configureDiscreteInputs(0x00, numDiscreteInputs);       // configure discrete inputs at address 0x00
  ModbusRTUServer.configureHoldingRegisters(0x00, numHoldingRegisters);   // configure holding registers at address 0x00
  ModbusRTUServer.configureInputRegisters(0x00, numInputRegisters);       // configure input registers at address 0x00

  // setup SWAN9 speed pulse input
  pinMode(PIN_SWAN_SPD_PULSE_IN, INPUT);

  // setup debug serial (soft serial)
  DebugSerial.begin(9600);
  pinMode(PIN_SW_UART_RX, INPUT);
  pinMode(PIN_SW_UART_TX, OUTPUT);

  // setup timer interrupt for calc battery current 
  MsTimer2::set(SAMPLING_T_MS, getCurrSensVolt); // 1000[msec] period
  MsTimer2::start();
}


void getCurrSensVolt()
{
  // get current sensor voltage ADC
#if 1
  uint16_t curr_out_ad = analogRead(PIN_CURR_OUT_SIG_IN);  // get ADC value
  uint16_t curr_chg_ad = analogRead(PIN_CURR_CHG_SIG_IN);  // get ADC value
#else

#endif
  // calc battery output current
  g_curr_out_volt   = (float) ((curr_out_ad / 1024.0f) * 5.0f) - CURR_SENS_VREF; // ADC value -> voltage -> offseted voltage
  g_curr_out_amp    = (g_curr_out_volt / 0.837f) * 100.0f;                       // 2.5[V] IN -> 0.837[V]

  // calc battery charge current
  g_curr_chg_volt   = (float) ((curr_chg_ad / 1024.0f) * 5.0f) - CURR_SENS_VREF; // ADC value -> voltage -> offseted voltage
  g_curr_chg_amp    = (g_curr_chg_volt / 0.837f) * 100.0f;                       // 2.5[V] IN -> 0.837[V]

  // current dead band & direction limitter
  if (g_curr_out_amp < 1.0f) g_curr_out_amp = 0.0f;
  if (g_curr_chg_amp < 1.0f) g_curr_chg_amp = 0.0f;

  // calc integral current 
  g_curr_sum_amp += g_curr_out_amp * ((float)SAMPLING_T_MS / 1000.0f);  // add batt out current ([A/sec] reference)
  g_curr_sum_amp -= g_curr_chg_amp * ((float)SAMPLING_T_MS / 1000.0f);  // add charge current   ([A/sec] reference)

  if (g_curr_sum_amp > CURR_SUM_MAX) g_curr_sum_amp = CURR_SUM_MAX;
  if (g_curr_sum_amp < CURR_SUM_MIN) g_curr_sum_amp = CURR_SUM_MIN;

  // calc battery level percent
  g_batt_lev_pct      = ((CURR_SUM_MAX - g_curr_sum_amp) / CURR_SUM_MAX) * PERCENT_MAX;
  g_batt_lev_pct_dig  = (int)g_batt_lev_pct;

  DebugSerial.print(g_curr_out_volt);
  DebugSerial.print(",");
  DebugSerial.println(g_curr_out_amp);
}


void loop()
{
  // WDT reset
  wdt_reset();

  // poll for Modbus RTU requests
  int packetReceived = ModbusRTUServer.poll();

  // get SWAN9 speed pulse freqency, and MODBUS transfer
  g_swan_spd_pls_hz_fl  = 1000000.0 / ((float) pulseIn(PIN_SWAN_SPD_PULSE_IN, HIGH));
  g_swan_spd_pls_hz_int = (uint16_t)g_swan_spd_pls_hz_fl;
  ModbusRTUServer.holdingRegisterWrite(0x00, g_swan_spd_pls_hz_int);

  // Judge battery level
  if (g_batt_lev_pct <= 0.0f)
  {
    ModbusRTUServer.holdingRegisterWrite(0x0000, 0x01);
  }
  else if ((g_batt_lev_pct) > 5.0f && (g_batt_lev_pct <= 10.0f))
  {
    ModbusRTUServer.holdingRegisterWrite(0x0001, 0x01);
  }
  else if ((g_batt_lev_pct) > 10.0f && (g_batt_lev_pct <= 20.0f))
  {
    ModbusRTUServer.holdingRegisterWrite(0x0002, 0x01);
  }
  else if ((g_batt_lev_pct) > 20.0f && (g_batt_lev_pct <= 30.0f))
  {
    ModbusRTUServer.holdingRegisterWrite(0x0003, 0x01);
  }
  else if ((g_batt_lev_pct) > 30.0f && (g_batt_lev_pct <= 40.0f))
  {
    ModbusRTUServer.holdingRegisterWrite(0x0004, 0x01);
  }
  else if ((g_batt_lev_pct) > 40.0f && (g_batt_lev_pct <= 50.0f))
  {
    ModbusRTUServer.holdingRegisterWrite(0x0005, 0x01);
  }
  else if ((g_batt_lev_pct) > 50.0f && (g_batt_lev_pct <= 60.0f))
  {
    ModbusRTUServer.holdingRegisterWrite(0x0006, 0x01);
  }
  else if ((g_batt_lev_pct) > 60.0f && (g_batt_lev_pct <= 70.0f))
  {
    ModbusRTUServer.holdingRegisterWrite(0x0007, 0x01);
  }
  else if ((g_batt_lev_pct) > 70.0f && (g_batt_lev_pct <= 80.0f))
  {
    ModbusRTUServer.holdingRegisterWrite(0x0008, 0x01);
  }
  else if ((g_batt_lev_pct) > 80.0f && (g_batt_lev_pct <= 90.0f))
  {
    ModbusRTUServer.holdingRegisterWrite(0x0009, 0x01);
  }
  else if ((g_batt_lev_pct) > 90.0f && (g_batt_lev_pct <= 100.0f))
  {
    ModbusRTUServer.holdingRegisterWrite(0x000A, 0x01);
  }
  else  // Error
  {
    ModbusRTUServer.holdingRegisterWrite(0x00AA, 0x01);
  }
}