/* board.h - Thermostat V3 pin map (see hardware part-plan.md "Pin map"). */
#pragma once
#include "driver/gpio.h"

/* HVAC triac drivers: active-high, 10k pull-down on board. */
#define BOARD_GPIO_DRV_W     GPIO_NUM_0   /* heat  (also XTAL_32K_P - no 32k crystal fitted) */
#define BOARD_GPIO_DRV_Y     GPIO_NUM_1   /* cool  (also XTAL_32K_N) */
#define BOARD_GPIO_DRV_G     GPIO_NUM_2   /* fan */

/* Buttons: active-low, 10k external pull-ups. */
#define BOARD_GPIO_BTN_UP    GPIO_NUM_4   /* MTMS: JTAG + SDIO-timing strap */
#define BOARD_GPIO_BTN_DOWN  GPIO_NUM_5   /* MTDI: JTAG + SDIO-timing strap */
#define BOARD_GPIO_BTN_MODE  GPIO_NUM_3

/* I2C: SHT40 (0x44) + Stemma QT. 4.7k external pull-ups. */
#define BOARD_GPIO_I2C_SDA   GPIO_NUM_6
#define BOARD_GPIO_I2C_SCL   GPIO_NUM_7

#define BOARD_GPIO_LED       GPIO_NUM_8   /* active-low, strap pin (must be high at reset) */
#define BOARD_GPIO_BOOT      GPIO_NUM_9   /* strap, BOOT button */

/* STWD100: WDI kick, ~EN active-low arm (10k pull-up = disarmed). */
#define BOARD_GPIO_WD_WDI    GPIO_NUM_10
#define BOARD_GPIO_WD_EN     GPIO_NUM_11

/* E-paper SSD1680 (GDEY029T94), 4-wire SPI, write-only. */
#define BOARD_GPIO_EPD_MOSI  GPIO_NUM_18
#define BOARD_GPIO_EPD_SCK   GPIO_NUM_19
#define BOARD_GPIO_EPD_CS    GPIO_NUM_20
#define BOARD_GPIO_EPD_DC    GPIO_NUM_21
#define BOARD_GPIO_EPD_RST   GPIO_NUM_22
#define BOARD_GPIO_EPD_BUSY  GPIO_NUM_23

/* GPIO12/13 = native USB-Serial-JTAG D-/D+; never reconfigure. */
