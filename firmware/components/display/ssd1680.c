/* ssd1680.c - GDEY029T94 (SSD1680, 128x296) driver, sequences per Good
 * Display's GDEY029T94 sample code / datasheet. Panel is deep-slept
 * (cmd 0x10) between updates, which is also what the datasheet asks for. */
#include "ssd1680.h"
#include "board.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "epd";
static spi_device_handle_t s_spi;
static esp_err_t s_err;   /* first SPI error since the start of the current refresh */

static void xfer(spi_transaction_t *t)
{
    esp_err_t e = spi_device_polling_transmit(s_spi, t);
    if (e != ESP_OK && s_err == ESP_OK) s_err = e;
}

static void cmd(uint8_t c)
{
    gpio_set_level(BOARD_GPIO_EPD_DC, 0);
    spi_transaction_t t = {.length = 8, .tx_buffer = &c};
    xfer(&t);
}

static void data(const uint8_t *d, size_t n)
{
    gpio_set_level(BOARD_GPIO_EPD_DC, 1);
    while (n) {
        size_t chunk = n > 2048 ? 2048 : n;
        spi_transaction_t t = {.length = chunk * 8, .tx_buffer = d};
        xfer(&t);
        d += chunk;
        n -= chunk;
    }
}

static void cmd_data(uint8_t c, const uint8_t *d, size_t n) { cmd(c); data(d, n); }
#define CMD(c, ...) do { static const uint8_t _d[] = {__VA_ARGS__}; cmd_data((c), _d, sizeof _d); } while (0)

static esp_err_t wait_busy(int timeout_ms)
{
    /* BUSY high = busy. Poll with vTaskDelay so other tasks keep running. */
    for (int t = 0; gpio_get_level(BOARD_GPIO_EPD_BUSY); t += 10) {
        if (t >= timeout_ms) {
            ESP_LOGE(TAG, "BUSY timeout");
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_OK;
}

esp_err_t ssd1680_bus_init(void)
{
    gpio_config_t out = {
        .pin_bit_mask = (1ULL << BOARD_GPIO_EPD_DC) | (1ULL << BOARD_GPIO_EPD_RST),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&out);
    gpio_set_level(BOARD_GPIO_EPD_RST, 1);
    gpio_config_t in = {.pin_bit_mask = 1ULL << BOARD_GPIO_EPD_BUSY, .mode = GPIO_MODE_INPUT};
    gpio_config(&in);

    spi_bus_config_t bus = {
        .mosi_io_num = BOARD_GPIO_EPD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = BOARD_GPIO_EPD_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 2048,
    };
    esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) return err;
    spi_device_interface_config_t dev = {
        .clock_speed_hz = 4 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = BOARD_GPIO_EPD_CS,
        .queue_size = 1,
    };
    return spi_bus_add_device(SPI2_HOST, &dev, &s_spi);
}

static esp_err_t panel_init(void)
{
    gpio_set_level(BOARD_GPIO_EPD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(BOARD_GPIO_EPD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    if (wait_busy(1000) != ESP_OK) return ESP_ERR_TIMEOUT;
    cmd(0x12);                                 /* SW reset */
    if (wait_busy(1000) != ESP_OK) return ESP_ERR_TIMEOUT;
    CMD(0x01, 0x27, 0x01, 0x00);               /* 296 gate lines */
    CMD(0x11, 0x03);                           /* X+, Y+ */
    CMD(0x44, 0x00, 0x0F);                     /* RAM X 0..15 (x8 = 128) */
    CMD(0x45, 0x00, 0x00, 0x27, 0x01);         /* RAM Y 0..295 */
    CMD(0x3C, 0x05);                           /* border waveform */
    CMD(0x21, 0x00, 0x80);                     /* display update control */
    CMD(0x18, 0x80);                           /* internal temperature sensor */
    return ESP_OK;
}

static void set_cursor(void)
{
    CMD(0x4E, 0x00);
    CMD(0x4F, 0x00, 0x00);
}

esp_err_t ssd1680_show(const gfx_fb_t *fb, const gfx_fb_t *prev, bool full)
{
    s_err = ESP_OK;
    esp_err_t err = panel_init();
    if (err != ESP_OK) return err;
    if (full) {
        set_cursor(); cmd_data(0x24, fb->buf, EPD_FB_SIZE);   /* new image */
        set_cursor(); cmd_data(0x26, fb->buf, EPD_FB_SIZE);   /* base for next partial */
        CMD(0x22, 0xF7);
    } else {
        CMD(0x3C, 0x80);                                      /* border: VCOM (no flash) */
        set_cursor(); cmd_data(0x26, prev->buf, EPD_FB_SIZE); /* what is on glass now */
        set_cursor(); cmd_data(0x24, fb->buf, EPD_FB_SIZE);
        CMD(0x22, 0xFF);                                      /* partial (display mode 2) */
    }
    cmd(0x20);
    err = wait_busy(full ? 5000 : 2000);
    CMD(0x10, 0x01);                                          /* deep sleep mode 1 */
    if (err == ESP_OK && s_err != ESP_OK) {
        ESP_LOGE(TAG, "SPI error during refresh: %s", esp_err_to_name(s_err));
        err = s_err;
    }
    return err;
}
