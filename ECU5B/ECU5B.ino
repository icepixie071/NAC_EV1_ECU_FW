#include <Wire.h>
#include <avr/wdt.h>

#define PIN_LED_BATT_0    (0)
#define PIN_LED_BATT_1    (1)
#define PIN_LED_BATT_2    (3)
#define PIN_LED_BATT_3    (5)
#define PIN_LED_BATT_4    (10)
#define PIN_LED_BATT_5    (11)
#define PIN_LED_BATT_6    (13)
#define PIN_LED_BATT_7    (15)
#define PIN_LED_BATT_8    (14)
#define PIN_LED_BATT_9    (17)

#define I2C_ADDR_CPU2     (0x08)
#define I2C_BUFF_SIZE     (16)
#define I2C_TIMEOUT_VAL   (400)

uint8_t i2c_receive_data[I2C_BUFF_SIZE] = {0};
uint8_t i2c_rx_size = 0;

#define I2C_BUF_BATT_STS0 (i2c_receive_data[0])
#define I2C_BUF_BATT_STS1 (i2c_receive_data[1])

// battery status
#define BATT_STS_ERR    (99U)
#define BATT_STS_LV0    (0U)
#define BATT_STS_LV1    (1U)
#define BATT_STS_LV2    (2U)
#define BATT_STS_LV3    (3U)
#define BATT_STS_LV4    (4U)
#define BATT_STS_LV5    (5U)
#define BATT_STS_LV6    (6U)
#define BATT_STS_LV7    (7U)
#define BATT_STS_LV8    (8U)
#define BATT_STS_LV9    (9U)
#define BATT_STS_LV10   (10U)

uint16_t g_batt_status      = BATT_STS_ERR;
uint32_t g_i2c_timeout_cnt  = 0;

void setup() {
  // put your setup code here, to run once:

  // I2C
  Wire.begin(I2C_ADDR_CPU2);
  Wire.onReceive(i2cReceive);
  pinMode (SDA, INPUT);       // disable pullup
  pinMode (SCL, INPUT);       // disable pullup

  // GPIO digitalWrite settings
  pinMode(PIN_LED_BATT_0,   OUTPUT);
  pinMode(PIN_LED_BATT_1,   OUTPUT);
  pinMode(PIN_LED_BATT_2,   OUTPUT); 
  pinMode(PIN_LED_BATT_3,   OUTPUT); 
  pinMode(PIN_LED_BATT_4,   OUTPUT); 
  pinMode(PIN_LED_BATT_5,   OUTPUT); 
  pinMode(PIN_LED_BATT_6,   OUTPUT); 
  pinMode(PIN_LED_BATT_7,   OUTPUT);
  pinMode(PIN_LED_BATT_8,   OUTPUT);
  pinMode(PIN_LED_BATT_9,   OUTPUT);
  
  digitalWrite(PIN_LED_BATT_0, HIGH);
  digitalWrite(PIN_LED_BATT_1, HIGH);
  digitalWrite(PIN_LED_BATT_2, HIGH);
  digitalWrite(PIN_LED_BATT_3, HIGH);
  digitalWrite(PIN_LED_BATT_4, HIGH);
  digitalWrite(PIN_LED_BATT_5, HIGH);
  digitalWrite(PIN_LED_BATT_6, HIGH);
  digitalWrite(PIN_LED_BATT_7, HIGH);
  digitalWrite(PIN_LED_BATT_8, HIGH);
  digitalWrite(PIN_LED_BATT_9, HIGH);
  delay(1000);
  
  // UART
  Serial.begin(9600);

  // WDT
  wdt_enable(WDTO_4S);
}

void loop() {
  // put your main code here, to run repeatedly:

  // WDT reset
  wdt_reset();

  // check value glitch 
  if (I2C_BUF_BATT_STS0 != I2C_BUF_BATT_STS1)
  {
    g_batt_status = BATT_STS_ERR;
  }
  else
  {
    g_batt_status = I2C_BUF_BATT_STS0;    
  }

  // i2c recieve timeout counter
  if (g_i2c_timeout_cnt <= I2C_TIMEOUT_VAL)
  {
    g_i2c_timeout_cnt ++;
  }
  else
  {
    g_i2c_timeout_cnt = I2C_TIMEOUT_VAL;
    g_batt_status     = BATT_STS_ERR;
  }
  
  // check shift status
  switch (g_batt_status)
  {
    case BATT_STS_LV0:  
      
      digitalWrite(PIN_LED_BATT_0, HIGH);
      digitalWrite(PIN_LED_BATT_1, LOW);
      digitalWrite(PIN_LED_BATT_2, LOW);
      digitalWrite(PIN_LED_BATT_3, LOW);
      digitalWrite(PIN_LED_BATT_4, LOW);
      digitalWrite(PIN_LED_BATT_5, LOW);
      digitalWrite(PIN_LED_BATT_6, LOW);
      digitalWrite(PIN_LED_BATT_7, LOW);
      digitalWrite(PIN_LED_BATT_8, LOW);
      digitalWrite(PIN_LED_BATT_9, LOW);
      delay(1000);

      // flashing LED
      digitalWrite(PIN_LED_BATT_0, LOW);
      delay(500);
      break;

    case BATT_STS_LV1:  
      
      digitalWrite(PIN_LED_BATT_0, HIGH);
      digitalWrite(PIN_LED_BATT_1, LOW);
      digitalWrite(PIN_LED_BATT_2, LOW);
      digitalWrite(PIN_LED_BATT_3, LOW);
      digitalWrite(PIN_LED_BATT_4, LOW);
      digitalWrite(PIN_LED_BATT_5, LOW);
      digitalWrite(PIN_LED_BATT_6, LOW);
      digitalWrite(PIN_LED_BATT_7, LOW);
      digitalWrite(PIN_LED_BATT_8, LOW);
      digitalWrite(PIN_LED_BATT_9, LOW);
      break;

    case BATT_STS_LV2:  
      
      digitalWrite(PIN_LED_BATT_0, HIGH);
      digitalWrite(PIN_LED_BATT_1, HIGH);
      digitalWrite(PIN_LED_BATT_2, LOW);
      digitalWrite(PIN_LED_BATT_3, LOW);
      digitalWrite(PIN_LED_BATT_4, LOW);
      digitalWrite(PIN_LED_BATT_5, LOW);
      digitalWrite(PIN_LED_BATT_6, LOW);
      digitalWrite(PIN_LED_BATT_7, LOW);
      digitalWrite(PIN_LED_BATT_8, LOW);
      digitalWrite(PIN_LED_BATT_9, LOW);
      break;
    
    case BATT_STS_LV3:  
      
      digitalWrite(PIN_LED_BATT_0, HIGH);
      digitalWrite(PIN_LED_BATT_1, HIGH);
      digitalWrite(PIN_LED_BATT_2, HIGH);
      digitalWrite(PIN_LED_BATT_3, LOW);
      digitalWrite(PIN_LED_BATT_4, LOW);
      digitalWrite(PIN_LED_BATT_5, LOW);
      digitalWrite(PIN_LED_BATT_6, LOW);
      digitalWrite(PIN_LED_BATT_7, LOW);
      digitalWrite(PIN_LED_BATT_8, LOW);
      digitalWrite(PIN_LED_BATT_9, LOW);
      break;

    case BATT_STS_LV4:  
      
      digitalWrite(PIN_LED_BATT_0, HIGH);
      digitalWrite(PIN_LED_BATT_1, HIGH);
      digitalWrite(PIN_LED_BATT_2, HIGH);
      digitalWrite(PIN_LED_BATT_3, HIGH);
      digitalWrite(PIN_LED_BATT_4, LOW);
      digitalWrite(PIN_LED_BATT_5, LOW);
      digitalWrite(PIN_LED_BATT_6, LOW);
      digitalWrite(PIN_LED_BATT_7, LOW);
      digitalWrite(PIN_LED_BATT_8, LOW);
      digitalWrite(PIN_LED_BATT_9, LOW);
      break;

    case BATT_STS_LV5:  
      
      digitalWrite(PIN_LED_BATT_0, HIGH);
      digitalWrite(PIN_LED_BATT_1, HIGH);
      digitalWrite(PIN_LED_BATT_2, HIGH);
      digitalWrite(PIN_LED_BATT_3, HIGH);
      digitalWrite(PIN_LED_BATT_4, HIGH);
      digitalWrite(PIN_LED_BATT_5, LOW);
      digitalWrite(PIN_LED_BATT_6, LOW);
      digitalWrite(PIN_LED_BATT_7, LOW);
      digitalWrite(PIN_LED_BATT_8, LOW);
      digitalWrite(PIN_LED_BATT_9, LOW);
      break;

    case BATT_STS_LV6:  
      
      digitalWrite(PIN_LED_BATT_0, HIGH);
      digitalWrite(PIN_LED_BATT_1, HIGH);
      digitalWrite(PIN_LED_BATT_2, HIGH);
      digitalWrite(PIN_LED_BATT_3, HIGH);
      digitalWrite(PIN_LED_BATT_4, HIGH);
      digitalWrite(PIN_LED_BATT_5, HIGH);
      digitalWrite(PIN_LED_BATT_6, LOW);
      digitalWrite(PIN_LED_BATT_7, LOW);
      digitalWrite(PIN_LED_BATT_8, LOW);
      digitalWrite(PIN_LED_BATT_9, LOW);
      break;

    case BATT_STS_LV7:  
      
      digitalWrite(PIN_LED_BATT_0, HIGH);
      digitalWrite(PIN_LED_BATT_1, HIGH);
      digitalWrite(PIN_LED_BATT_2, HIGH);
      digitalWrite(PIN_LED_BATT_3, HIGH);
      digitalWrite(PIN_LED_BATT_4, HIGH);
      digitalWrite(PIN_LED_BATT_5, HIGH);
      digitalWrite(PIN_LED_BATT_6, HIGH);
      digitalWrite(PIN_LED_BATT_7, LOW);
      digitalWrite(PIN_LED_BATT_8, LOW);
      digitalWrite(PIN_LED_BATT_9, LOW);
      break;

    case BATT_STS_LV8:  
      
      digitalWrite(PIN_LED_BATT_0, HIGH);
      digitalWrite(PIN_LED_BATT_1, HIGH);
      digitalWrite(PIN_LED_BATT_2, HIGH);
      digitalWrite(PIN_LED_BATT_3, HIGH);
      digitalWrite(PIN_LED_BATT_4, HIGH);
      digitalWrite(PIN_LED_BATT_5, HIGH);
      digitalWrite(PIN_LED_BATT_6, HIGH);
      digitalWrite(PIN_LED_BATT_7, HIGH);
      digitalWrite(PIN_LED_BATT_8, LOW);
      digitalWrite(PIN_LED_BATT_9, LOW);
      break;

    case BATT_STS_LV9:  
      
      digitalWrite(PIN_LED_BATT_0, HIGH);
      digitalWrite(PIN_LED_BATT_1, HIGH);
      digitalWrite(PIN_LED_BATT_2, HIGH);
      digitalWrite(PIN_LED_BATT_3, HIGH);
      digitalWrite(PIN_LED_BATT_4, HIGH);
      digitalWrite(PIN_LED_BATT_5, HIGH);
      digitalWrite(PIN_LED_BATT_6, HIGH);
      digitalWrite(PIN_LED_BATT_7, HIGH);
      digitalWrite(PIN_LED_BATT_8, HIGH);
      digitalWrite(PIN_LED_BATT_9, LOW);
      break;

    case BATT_STS_LV10:  

      digitalWrite(PIN_LED_BATT_0, HIGH);
      digitalWrite(PIN_LED_BATT_1, HIGH);
      digitalWrite(PIN_LED_BATT_2, HIGH);
      digitalWrite(PIN_LED_BATT_3, HIGH);
      digitalWrite(PIN_LED_BATT_4, HIGH);
      digitalWrite(PIN_LED_BATT_5, HIGH);
      digitalWrite(PIN_LED_BATT_6, HIGH);
      digitalWrite(PIN_LED_BATT_7, HIGH);
      digitalWrite(PIN_LED_BATT_8, HIGH);
      digitalWrite(PIN_LED_BATT_9, HIGH);
      break;        

    case BATT_STS_ERR: 
      
      digitalWrite(PIN_LED_BATT_0, HIGH);
      digitalWrite(PIN_LED_BATT_1, HIGH);
      digitalWrite(PIN_LED_BATT_2, HIGH);
      digitalWrite(PIN_LED_BATT_3, HIGH);
      digitalWrite(PIN_LED_BATT_4, HIGH);
      digitalWrite(PIN_LED_BATT_5, HIGH);
      digitalWrite(PIN_LED_BATT_6, HIGH);
      digitalWrite(PIN_LED_BATT_7, HIGH);
      digitalWrite(PIN_LED_BATT_8, HIGH);
      digitalWrite(PIN_LED_BATT_9, HIGH);
      delay(1000);

      // flashing LED
      digitalWrite(PIN_LED_BATT_0, LOW);
      digitalWrite(PIN_LED_BATT_1, LOW);
      digitalWrite(PIN_LED_BATT_2, LOW);
      digitalWrite(PIN_LED_BATT_3, LOW);
      digitalWrite(PIN_LED_BATT_4, LOW);
      digitalWrite(PIN_LED_BATT_5, LOW);
      digitalWrite(PIN_LED_BATT_6, LOW);
      digitalWrite(PIN_LED_BATT_7, LOW);
      digitalWrite(PIN_LED_BATT_8, LOW);
      digitalWrite(PIN_LED_BATT_9, LOW);
      delay(500);      
      break;   
    
    default:

      digitalWrite(PIN_LED_BATT_0, HIGH);
      digitalWrite(PIN_LED_BATT_1, HIGH);
      digitalWrite(PIN_LED_BATT_2, HIGH);
      digitalWrite(PIN_LED_BATT_3, HIGH);
      digitalWrite(PIN_LED_BATT_4, HIGH);
      digitalWrite(PIN_LED_BATT_5, HIGH);
      digitalWrite(PIN_LED_BATT_6, HIGH);
      digitalWrite(PIN_LED_BATT_7, HIGH);
      digitalWrite(PIN_LED_BATT_8, HIGH);
      digitalWrite(PIN_LED_BATT_9, HIGH);
      delay(1000);

      // flashing LED
      digitalWrite(PIN_LED_BATT_0, LOW);
      digitalWrite(PIN_LED_BATT_1, LOW);
      digitalWrite(PIN_LED_BATT_2, LOW);
      digitalWrite(PIN_LED_BATT_3, LOW);
      digitalWrite(PIN_LED_BATT_4, LOW);
      digitalWrite(PIN_LED_BATT_5, LOW);
      digitalWrite(PIN_LED_BATT_6, LOW);
      digitalWrite(PIN_LED_BATT_7, LOW);
      digitalWrite(PIN_LED_BATT_8, LOW);
      digitalWrite(PIN_LED_BATT_9, LOW);
      delay(500);      
      break;
  }

  delay(500);
}


void i2cReceive(int num) {
  
  i2c_rx_size = Wire.available();

  for (uint8_t i=0; (i<i2c_rx_size) && (i<I2C_BUFF_SIZE); i++)
  {
    i2c_receive_data[i] = Wire.read();
  }

  g_i2c_timeout_cnt = 0;
}
