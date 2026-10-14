#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "gfx.h"

/**
 * Configure the e-paper GPIOs (DC, RST outputs; BUSY input) and add the panel as an SPI2 device
 * (4 MHz, mode 0, MOSI only). Call once from DisplayInit().
 *
 * Returns the spi_bus_initialize()/spi_bus_add_device() error, or ESP_OK.
 */
esp_err_t Ssd1680BusInit(void);

/**
 * Show a frame. Blocks while the panel updates (up to ~5 s full / ~2 s partial, polled with
 * vTaskDelay). Call from the display task only (driver state is not locked).
 *
 * Wakes the panel (hardware reset + init), writes the frame, triggers the update and
 * deep-sleeps the panel again (the datasheet requires sleep between updates). full=true: both
 * RAMs get pFb (a flashing full refresh that clears ghosting; pPrev is unused). full=false:
 * partial (differential) update; RAM 0x26 gets pPrev (what is on the glass now) and RAM 0x24
 * gets pFb, with no border flash.
 *
 * Returns ESP_OK, ESP_ERR_TIMEOUT (BUSY stuck), or the first SPI error during the refresh.
 */
esp_err_t Ssd1680Show(const GfxFb *pFb, const GfxFb *pPrev, bool full);
