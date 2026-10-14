/* HOOTL fake SPI master: the SSD1680 model decodes commands and captures the framebuffer. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "driver/gpio.h"
typedef struct SimSpiDev *spi_device_handle_t;
typedef enum { SPI1_HOST = 0, SPI2_HOST = 1 } spi_host_device_t;
typedef enum { SPI_DMA_DISABLED = 0, SPI_DMA_CH_AUTO = 3 } spi_dma_chan_t;
typedef struct { int mosi_io_num, miso_io_num, sclk_io_num, quadwp_io_num, quadhd_io_num; int max_transfer_sz; } spi_bus_config_t;
typedef struct { int clock_speed_hz; uint8_t mode; int spics_io_num; int queue_size; } spi_device_interface_config_t;
typedef struct { size_t length; const void *tx_buffer; void *rx_buffer; } spi_transaction_t;
esp_err_t spi_bus_initialize(spi_host_device_t h, const spi_bus_config_t *c, spi_dma_chan_t dma);
esp_err_t spi_bus_add_device(spi_host_device_t h, const spi_device_interface_config_t *c, spi_device_handle_t *out);
esp_err_t spi_device_polling_transmit(spi_device_handle_t d, spi_transaction_t *t);
