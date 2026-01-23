#include <Arduino.h>

#include "DEV_Config.h"
#include "qspi_pio.h"
#include "AMOLED_1in64.h"

// RGB565 colors
#ifndef RED
#define RED   0xF800
#define GREEN 0x07E0
#define BLUE  0x001F
#define WHITE 0xFFFF
#define BLACK 0x0000
#endif

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("BOOT");
  Serial.println("Starting display init...");

  int rc = DEV_Module_Init();
  Serial.print("DEV_Module_Init rc=");
  Serial.println(rc);

  Serial.println("QSPI_GPIO_Init");
  QSPI_GPIO_Init(qspi);

  Serial.println("QSPI_PIO_Init");
  QSPI_PIO_Init(qspi);

  Serial.println("QSPI_1Wrie_Mode");
  QSPI_1Wrie_Mode(&qspi);

  Serial.println("AMOLED_1IN64_Init");
  AMOLED_1IN64_Init();

  Serial.println("SetBrightness");
  AMOLED_1IN64_SetBrightness(100);

  Serial.println("Color clears");
  AMOLED_1IN64_Clear(RED);   DEV_Delay_ms(300);
  AMOLED_1IN64_Clear(GREEN); DEV_Delay_ms(300);
  AMOLED_1IN64_Clear(BLUE);  DEV_Delay_ms(300);
  AMOLED_1IN64_Clear(WHITE); DEV_Delay_ms(300);
  AMOLED_1IN64_Clear(BLACK); DEV_Delay_ms(300);

  Serial.println("DONE");
}

void loop() {}
