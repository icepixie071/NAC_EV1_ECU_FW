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
  MsTimer2::set(1000, getCurrSensVolt); // 1000[msec] period
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
  g_curr_sum_amp += g_curr_out_amp;   // add batt out current
  g_curr_sum_amp -= g_curr_chg_amp;   // add charge current

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

/*
  // Judge battery level
  if (g_batt_lev_pct <= 0.0f)
  {
    modbus_coil_data[0] = 13;
    modbus_coil_data[1] = 13;
  }
  else if ((g_batt_lev_pct) > 0.0f && (g_batt_lev_pct <= 5.0f))
  {
    modbus_coil_data[0] = 12;
    modbus_coil_data[1] = 12;
  }
  else if ((g_batt_lev_pct) > 5.0f && (g_batt_lev_pct <= 10.0f))
  {
    modbus_coil_data[0] = 1;
    modbus_coil_data[1] = 1;
  }
  else if ((g_batt_lev_pct) > 10.0f && (g_batt_lev_pct <= 20.0f))
  {
    modbus_coil_data[0] = 2;
    modbus_coil_data[1] = 2;
  }
  else if ((g_batt_lev_pct) > 20.0f && (g_batt_lev_pct <= 30.0f))
  {
    modbus_coil_data[0] = 3;
    modbus_coil_data[1] = 3;
  }
  else if ((g_batt_lev_pct) > 30.0f && (g_batt_lev_pct <= 40.0f))
  {
    modbus_coil_data[0] = 4;
    modbus_coil_data[1] = 4;
  }
  else if ((g_batt_lev_pct) > 40.0f && (g_batt_lev_pct <= 50.0f))
  {
    modbus_coil_data[0] = 5;
    modbus_coil_data[1] = 5;
  }
  else if ((g_batt_lev_pct) > 50.0f && (g_batt_lev_pct <= 60.0f))
  {
    modbus_coil_data[0] = 6;
    modbus_coil_data[1] = 6;
  }
  else if ((g_batt_lev_pct) > 60.0f && (g_batt_lev_pct <= 70.0f))
  {
    modbus_coil_data[0] = 7;
    modbus_coil_data[1] = 7;
  }
  else if ((g_batt_lev_pct) > 70.0f && (g_batt_lev_pct <= 80.0f))
  {
    modbus_coil_data[0] = 8;
    modbus_coil_data[1] = 8;
  }
  else if ((g_batt_lev_pct) > 80.0f && (g_batt_lev_pct <= 90.0f))
  {
    modbus_coil_data[0] = 9;
    modbus_coil_data[1] = 9;
  }
  else if ((g_batt_lev_pct) > 90.0f && (g_batt_lev_pct <= 100.0f))
  {
    modbus_coil_data[0] = 10;
    modbus_coil_data[1] = 10;
  }
  else
  {
    modbus_coil_data[0] = 12;  // error
    modbus_coil_data[1] = 12;
  }
*/
}