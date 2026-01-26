#include <Wire.h>
#include <avr/wdt.h>

#define PIN_LED_S_BRK_WARN  (3)
#define PIN_LED_BATT_WARN   (5)
#define PIN_LED_LIGHT_M_SW  (15)
#define PIN_LED_LW_BEAM     (14)
#define PIN_LED_HI_BEAM     (17)

#define I2C_ADDR_CPU2       (0x08)
#define I2C_BUFF_SIZE       (16)

uint8_t i2c_receive_data[I2C_BUFF_SIZE] = {0};
uint8_t i2c_rx_size = 0;

#define I2C_BUF_SHIFT_STS        (i2c_receive_data[0])
#define I2C_BUF_S_BRK_STS        (i2c_receive_data[1])
#define I2C_BUF_LIGHT_M_SW_STS   (i2c_receive_data[2])
#define I2C_BUF_LIGHT_STS        (i2c_receive_data[3])
#define I2C_BUF_BATT_LOW_STS     (i2c_receive_data[4])

// SHIFT status
#define SHIFT_STS_P         (0x00)
#define SHIFT_STS_N         (0x01)
#define SHIFT_STS_D         (0x02)
#define SHIFT_STS_R         (0x03)
#define SHIFT_STS_L         (0x04)
#define SHIFT_STS_ERR       (0x05)

// SIDE BRAKE status
#define SIDE_BRK_STS_OFF    (0x00)
#define SIDE_BRK_STS_ON     (0x01)

// LIGHT status
#define LIGHT_MSTR_SW_STS_OFF   (0x00)
#define LIGHT_MSTR_SW_STS_ON    (0x01)
#define LIGHT_STS_OFF           (0x00)
#define LIGHT_STS_LW_BEAM       (0x01)
#define LIGHT_STS_HI_BEAM       (0x02)

// BATT LEVEL status
#define VBATT_LVL_STS_NOM   (0x00)
#define VBATT_LVL_STS_LOW   (0x01)


void setup() {
  // put your setup code here, to run once:

  // I2C
  Wire.begin(I2C_ADDR_CPU2);
  Wire.onReceive(i2cReceive);
  pinMode (SDA, INPUT);       // disable pullup
  pinMode (SCL, INPUT);       // disable pullup

  // GPIO digitalWrite settings
  pinMode(PIN_LED_S_BRK_WARN,   OUTPUT);
  pinMode(PIN_LED_BATT_WARN,    OUTPUT);
  pinMode(PIN_LED_LIGHT_M_SW,   OUTPUT);
  pinMode(PIN_LED_LW_BEAM,      OUTPUT);
  pinMode(PIN_LED_HI_BEAM,      OUTPUT);

  digitalWrite(PIN_LED_S_BRK_WARN, HIGH);
  digitalWrite(PIN_LED_BATT_WARN, HIGH);
  digitalWrite(PIN_LED_LIGHT_M_SW, HIGH);
  digitalWrite(PIN_LED_LW_BEAM, HIGH);
  digitalWrite(PIN_LED_HI_BEAM, HIGH);

  delay(1000);

  digitalWrite(PIN_LED_S_BRK_WARN, LOW);
  digitalWrite(PIN_LED_BATT_WARN, LOW);
  digitalWrite(PIN_LED_LIGHT_M_SW, LOW);
  digitalWrite(PIN_LED_LW_BEAM, LOW);
  digitalWrite(PIN_LED_HI_BEAM, LOW);
  
  // UART
  Serial.begin(9600);

  // WDT
  wdt_enable(WDTO_4S);
}

void loop() {
  // put your main code here, to run repeatedly:

  static uint8_t shift_sts_tx = SHIFT_STS_P;
  static uint8_t shift_sts_tx_pre = SHIFT_STS_P;

  // WDT reset
  wdt_reset();

  // check shift status
  switch (I2C_BUF_SHIFT_STS)
  {
    case SHIFT_STS_P:   shift_sts_tx = SHIFT_STS_P;   break;
    case SHIFT_STS_N:   shift_sts_tx = SHIFT_STS_N;   break;
    case SHIFT_STS_D:   shift_sts_tx = SHIFT_STS_D;   break;
    case SHIFT_STS_R:   shift_sts_tx = SHIFT_STS_R;   break;
    case SHIFT_STS_L:   shift_sts_tx = SHIFT_STS_L;   break;
    case SHIFT_STS_ERR: shift_sts_tx = SHIFT_STS_ERR; break;
    default:            shift_sts_tx = SHIFT_STS_ERR; break;
  }

  Serial.write(shift_sts_tx);
  shift_sts_tx_pre = shift_sts_tx;


  // Telltale LED's controll
  // 1. Side brake warnning LED
  if (I2C_BUF_S_BRK_STS == SIDE_BRK_STS_ON)
  {
    digitalWrite(PIN_LED_S_BRK_WARN, HIGH);
  }
  else
  {
    digitalWrite(PIN_LED_S_BRK_WARN, LOW);
  }

  // 2. Light master switch status LED
  if (I2C_BUF_LIGHT_M_SW_STS == LIGHT_MSTR_SW_STS_ON)
  {
    digitalWrite(PIN_LED_LIGHT_M_SW, HIGH);
  }
  else
  {
    digitalWrite(PIN_LED_LIGHT_M_SW, LOW);
  }
  
  // 3. Light beam status LED
  if (I2C_BUF_LIGHT_STS == LIGHT_STS_LW_BEAM)   // High beam
  {
    digitalWrite(PIN_LED_LW_BEAM, HIGH);
  }
  else
  {
    digitalWrite(PIN_LED_LW_BEAM, LOW);
  }

  if (I2C_BUF_LIGHT_STS == LIGHT_STS_HI_BEAM)   // Low beam
  {
    digitalWrite(PIN_LED_HI_BEAM, HIGH);
  }
  else
  {
    digitalWrite(PIN_LED_HI_BEAM, LOW);
  }

  // 4. Battery level warnning LED
  if (I2C_BUF_BATT_LOW_STS == VBATT_LVL_STS_LOW)
  {
    digitalWrite(PIN_LED_BATT_WARN, HIGH);
  }
  else
  {
    digitalWrite(PIN_LED_BATT_WARN, LOW);
  }
  

  // Debug Serial
  /*
  Serial.print("Shift: ");
  Serial.println(i2c_receive_data[0]);

  Serial.print("side_brk: ");
  Serial.println(i2c_receive_data[1]);

  Serial.print("mstr_sw: ");
  Serial.println(i2c_receive_data[2]);

  Serial.print("light_state: ");
  Serial.println(i2c_receive_data[3]);

  Serial.print("vbatt_low: ");
  Serial.println(i2c_receive_data[4]);

  Serial.print("i2c_rx_size: ");
  Serial.println(i2c_rx_size);
  */

  delay(100);
}


void i2cReceive(int num) {
  
  i2c_rx_size = Wire.available();

  for (uint8_t i=0; (i<i2c_rx_size) && (i<I2C_BUFF_SIZE); i++)
  {
    i2c_receive_data[i] = Wire.read();
  }
}
