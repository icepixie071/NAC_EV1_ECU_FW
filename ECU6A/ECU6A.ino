#include <SoftwareSerial.h>
#include <MsTimer2.h>
#include <avr/wdt.h>

#define PIN_MAX485_DE           (2)
#define PIN_CURR_OUT_SIG_IN     (A0)
#define PIN_CURR_CHG_SIG_IN     (A1)
#define PIN_SWAN_SPD_PULSE_IN   (16)
#define PIN_SW_UART_TX          (7)
#define PIN_SW_UART_RX          (4)

#define PULSELN_TIMEOUT_US      (500000L)     // [μsec]
#define VEH_SPD_CONV_K          (0.002f)
#define VEH_SPD_OUT_MAX         (120)         // [km/h]

#define CURR1_SENS_V_OFFSET     (0.1f)        // [V]
#define CURR2_SENS_V_OFFSET     (0.1f)        // [V]
#define CURR1_SENS_VOLT2CURR_K  (1.6f)        // "LA37S100S05K" Vout_k (0.00625[V/A] = 160[A/V]) / OPAMP_Gain (100)   = 1.6[A/V], and + tunning_gain
#define CURR2_SENS_VOLT2CURR_K  (1.6f)        // "LA37S100S05K" Vout_k (0.00625[V/A] = 160[A/V]) / OPAMP_Gain (100)   = 1.6[A/V], and + tunning_gain
#define CURR1_SENS_RANGE_MAX    (8.0f)        // 5.0[V] * 1.6[A/V]  = 8.0[A]
#define CURR2_SENS_RANGE_MAX    (8.0f)        // 5.0[V] * 1.6[A/V]  = 8.0[A]
#define CURR1_SENS_RANGE_MIN    (0.5f)        // sens dead band [A]
#define CURR2_SENS_RANGE_MIN    (0.5f)        // sens dead band [A]  

#define CURR_SUM_MAX            (504000.0f)   // 200[Ah] = 750,000[A/sec] * 70[%] = 504,000[A/sec]
#define CURR_SUM_MIN            (0.0f)        // [A/sec]
#define PERCENT_MAX             (100.0f)      // [%]
#define SAMPLING_T_MS           (100)         // [msec]
#define ECU_CONSUMP_CURR        (0.1f)        // ECU6A consumption current [A]

#define UART_VAL_HEADER         (0xAAU)
#define UART_VAL_FOOTER         (0xCCU)

// battery status
#define BATT_STS_ERR            (99U)
#define BATT_STS_LV0            (0U)
#define BATT_STS_LV1            (1U)
#define BATT_STS_LV2            (2U)
#define BATT_STS_LV3            (3U)
#define BATT_STS_LV4            (4U)
#define BATT_STS_LV5            (5U)
#define BATT_STS_LV6            (6U)
#define BATT_STS_LV7            (7U)
#define BATT_STS_LV8            (8U)
#define BATT_STS_LV9            (9U)
#define BATT_STS_LV10           (10U)

SoftwareSerial DebugSerial(PIN_SW_UART_RX, PIN_SW_UART_TX);

void IRQ_getCurrSensVolt(void);

float g_curr_sum_amp            = CURR_SUM_MAX;
float g_curr_out_volt           = 0;       
float g_curr_out_amp            = 0;
float g_curr_chg_volt           = 0;
float g_curr_chg_amp            = 0;
float g_batt_lev_pct            = 0;
float g_swan_spd_pls_hz_fl      = 0;
uint8_t g_batt_lev              = 0;
uint8_t g_veh_spd_kmh           = 0;


void setup()
{
  // WDT
  wdt_enable(WDTO_8S);

  // Setup serial UART port
  Serial.begin(19200, SERIAL_8E1);  // even parity and 1 stop bit
  pinMode(PIN_MAX485_DE, OUTPUT);   // DE_PIN is enable
  while (!Serial);
  digitalWrite(PIN_MAX485_DE, 1);   // enable tx

  // setup SWAN9 speed pulse input
  pinMode(PIN_SWAN_SPD_PULSE_IN, INPUT);

  // setup debug serial (soft serial)
  DebugSerial.begin(19200);
  pinMode(PIN_SW_UART_RX, INPUT);
  pinMode(PIN_SW_UART_TX, OUTPUT);

  // setup timer interrupt for calc battery current 
  MsTimer2::set(SAMPLING_T_MS, IRQ_getCurrSensVolt); // (period_ms, call faunc())
  MsTimer2::start();
}


void IRQ_getCurrSensVolt()
{
  // get current sensor voltage ADC
#if 1
  uint16_t curr_out_ad = analogRead(PIN_CURR_OUT_SIG_IN);  // get ADC value
  uint16_t curr_chg_ad = analogRead(PIN_CURR_CHG_SIG_IN);  // get ADC value
#else

#endif
  // calc battery output current
  g_curr_out_volt   = (float) ((curr_out_ad / 1024.0f) * 5.0f) - CURR1_SENS_V_OFFSET;  // ADC value -> voltage -> offseted voltage
  g_curr_out_amp    = (g_curr_out_volt * CURR1_SENS_VOLT2CURR_K);

  // calc battery charge current
  g_curr_chg_volt   = (float) ((curr_chg_ad / 1024.0f) * 5.0f) - CURR2_SENS_V_OFFSET;  // ADC value -> voltage -> offseted voltage
  g_curr_chg_amp    = (g_curr_chg_volt * CURR2_SENS_VOLT2CURR_K);

  // current dead band & direction limitter
  if (g_curr_out_amp > CURR1_SENS_RANGE_MAX) g_curr_out_amp = CURR1_SENS_RANGE_MAX;
  if (g_curr_chg_amp > CURR2_SENS_RANGE_MAX) g_curr_chg_amp = CURR2_SENS_RANGE_MAX;
  if (g_curr_out_amp < CURR1_SENS_RANGE_MIN) g_curr_out_amp = 0.0f;
  if (g_curr_chg_amp < CURR2_SENS_RANGE_MIN) g_curr_chg_amp = 0.0f;

  // calc integral current 
  g_curr_sum_amp -= (g_curr_out_amp + ECU_CONSUMP_CURR) * ((float)SAMPLING_T_MS / 1000.0f);   // add batt out current ([A/sec] reference)
  g_curr_sum_amp += (g_curr_chg_amp) * ((float)SAMPLING_T_MS / 1000.0f);                      // add charge current   ([A/sec] reference)

  if (g_curr_sum_amp > CURR_SUM_MAX) g_curr_sum_amp = CURR_SUM_MAX;
  if (g_curr_sum_amp < CURR_SUM_MIN) g_curr_sum_amp = CURR_SUM_MIN;

  // calc battery level percent
  g_batt_lev_pct = (g_curr_sum_amp / CURR_SUM_MAX) * PERCENT_MAX;
}


void loop()
{
  // WDT reset
  wdt_reset();

  // get SWAN9 speed pulse freqency
  g_swan_spd_pls_hz_fl  = 1000000.0 / ((float) pulseIn(PIN_SWAN_SPD_PULSE_IN, HIGH, PULSELN_TIMEOUT_US));
  g_veh_spd_kmh         = (uint8_t) (g_swan_spd_pls_hz_fl * VEH_SPD_CONV_K);
  
  DebugSerial.println(g_veh_spd_kmh);

  if (g_veh_spd_kmh >= VEH_SPD_OUT_MAX) g_veh_spd_kmh = VEH_SPD_OUT_MAX;

  // Judge battery level
  if (g_batt_lev_pct <= 0.0f)
  {
    g_batt_lev = BATT_STS_LV0;
  }
  else if ((g_batt_lev_pct) > 10.0f && (g_batt_lev_pct <= 20.0f))
  {
    g_batt_lev = BATT_STS_LV1;
  }
  else if ((g_batt_lev_pct) > 20.0f && (g_batt_lev_pct <= 30.0f))
  {
    g_batt_lev = BATT_STS_LV2;
  }
  else if ((g_batt_lev_pct) > 30.0f && (g_batt_lev_pct <= 40.0f))
  {
    g_batt_lev = BATT_STS_LV3;
  }
  else if ((g_batt_lev_pct) > 40.0f && (g_batt_lev_pct <= 50.0f))
  {
    g_batt_lev = BATT_STS_LV4;
  }
  else if ((g_batt_lev_pct) > 50.0f && (g_batt_lev_pct <= 60.0f))
  {
    g_batt_lev = BATT_STS_LV5;
  }
  else if ((g_batt_lev_pct) > 60.0f && (g_batt_lev_pct <= 70.0f))
  {
    g_batt_lev = BATT_STS_LV6;
  }
  else if ((g_batt_lev_pct) > 70.0f && (g_batt_lev_pct <= 80.0f))
  {
    g_batt_lev = BATT_STS_LV7;
  }
  else if ((g_batt_lev_pct) > 80.0f && (g_batt_lev_pct <= 90.0f))
  {
    g_batt_lev = BATT_STS_LV8;
  }
  else if ((g_batt_lev_pct) > 90.0f && (g_batt_lev_pct <= 100.0f))
  {
    g_batt_lev = BATT_STS_LV9;
  }
  else if ((g_batt_lev_pct) > 99.0f && (g_batt_lev_pct <= 100.0f))
  {
    g_batt_lev = BATT_STS_LV10;
  }
  else  // Error
  {
    g_batt_lev = BATT_STS_ERR;
  }

#if (1)
  // UART transfer pucket
  uint8_t check_sum = ((g_veh_spd_kmh + g_batt_lev) & 0xFF);
  
  Serial.write(UART_VAL_HEADER);    // tx_buff 0
  Serial.write(g_veh_spd_kmh);      // tx_buff 1
  Serial.write(g_batt_lev);         // tx_buff 2
  Serial.write(check_sum);          // tx_buff 3
  Serial.write(UART_VAL_FOOTER);    // tx_buff 4
  Serial.flush();                   // wait for complete transfer (about, 45bit / 19200bps = 2.4ms.)

  delay(50);                        // delay ms
#endif

#if (0)
  // for debug
  Serial.print("i_out: ");
  Serial.println(g_curr_out_amp);
  Serial.print("i_chg: ");
  Serial.println(g_curr_chg_amp);
  Serial.print("i_sum: ");
  Serial.println(g_curr_sum_amp);
  Serial.print("b_pct: ");
  Serial.println(g_batt_lev_pct);
#endif

}