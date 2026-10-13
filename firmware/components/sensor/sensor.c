#include "sensor.h"
#include "sht40_parse.h"
#include "board.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SHT40_ADDR        0x44
#define SHT40_MEAS_HIGH   0xFD
#define SHT40_SOFT_RESET  0x94

static const char *TAG = "sht40";
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;

esp_err_t sensor_init(void)
{
    i2c_master_bus_config_t bc = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BOARD_GPIO_I2C_SDA,
        .scl_io_num = BOARD_GPIO_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false, /* 4.7k on board */
    };
    esp_err_t err = i2c_new_master_bus(&bc, &s_bus);
    if (err != ESP_OK) return err;
    i2c_device_config_t dc = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHT40_ADDR,
        .scl_speed_hz = 100000,
    };
    err = i2c_master_bus_add_device(s_bus, &dc, &s_dev);
    if (err == ESP_OK) {
        uint8_t c = SHT40_SOFT_RESET;
        i2c_master_transmit(s_dev, &c, 1, 50);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return err;
}

sensor_reading_t sensor_read(int16_t offset)
{
    sensor_reading_t r = {0};
    uint8_t cmd = SHT40_MEAS_HIGH, f[6];
    if (!s_dev) return r;
    if (i2c_master_transmit(s_dev, &cmd, 1, 50) != ESP_OK) goto fail;
    vTaskDelay(pdMS_TO_TICKS(10));                /* tMEAS high repeatability 8.3 ms max */
    if (i2c_master_receive(s_dev, f, sizeof f, 50) != ESP_OK) goto fail;
    if (!sht40_parse(f, &r.temp_c100, &r.rh_c100)) {
        ESP_LOGW(TAG, "CRC error");
        return r;
    }
    r.temp_c100 = (int16_t)(r.temp_c100 + offset);
    r.valid = true;
    return r;
fail:
    ESP_LOGW(TAG, "I2C error");
    i2c_master_bus_reset(s_bus);
    cmd = SHT40_SOFT_RESET;
    i2c_master_transmit(s_dev, &cmd, 1, 50);
    return r;
}
