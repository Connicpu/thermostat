#include "core.h"
#include "sensor.h"
#include "sensor_cycle.h"
#include "board.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SHT40_ADDR 0x44

static const char *kTag = "sht40";
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;
static SensorStats s_stats;

/** Transmit one buffer to the SHT40. Returns 0 on success. */
static int Tx(void *pCtx, const U8 *pBuf, size_t n)
{
    return i2c_master_transmit(s_dev, pBuf, n, SENSOR_I2C_TIMEOUT_MS) == ESP_OK ? 0 : -1;
}

/** Receive one buffer from the SHT40. Returns 0 on success. */
static int Rx(void *pCtx, U8 *pBuf, size_t n)
{
    return i2c_master_receive(s_dev, pBuf, n, SENSOR_I2C_TIMEOUT_MS) == ESP_OK ? 0 : -1;
}

/** Reset the I2C master. Returns 0 when the driver accepts the reset. */
static int BusReset(void *pCtx)
{
    return i2c_master_bus_reset(s_bus) == ESP_OK ? 0 : -1;
}

/** Wait `ms` milliseconds. The cycle only asks for the short measure and retry gaps. */
static void Delay(void *pCtx, U32 ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/** The bus seam bound to the device created by SensorInit(). */
static SensorBus Bus(void)
{
    return (SensorBus){.pCtx = NULL, .tx = Tx, .rx = Rx, .busReset = BusReset, .delayMs = Delay};
}

/** See sensor.h. */
void SensorStatsGet(SensorStats *pOut)
{
    *pOut = s_stats;
}

/** Bring up I2C0 and the SHT40 (soft reset, retried). See sensor.h. */
esp_err_t SensorInit(void)
{
    i2c_master_bus_config_t bc = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BOARD_GPIO_I2C_SDA,
        .scl_io_num = BOARD_GPIO_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false, /* 4.7k on board */
    };
    SensorBus bus;
    esp_err_t err = i2c_new_master_bus(&bc, &s_bus);
    if (err != ESP_OK)
        return err;
    i2c_device_config_t dc = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHT40_ADDR,
        .scl_speed_hz = 100000,
    };
    err = i2c_master_bus_add_device(s_bus, &dc, &s_dev);
    if (err != ESP_OK)
        return err;
    bus = Bus();
    if (SensorSoftReset(&bus, &s_stats) != 0)
        ESP_LOGW(kTag, "soft reset not acked");
    return ESP_OK;
}

/** Measure with retries. See sensor.h. */
SensorReading SensorRead(I16 offset)
{
    SensorReading r = {0};
    SensorStats before;
    SensorBus bus;
    if (!s_dev)
    {
        SensorStatsNoteFail(&s_stats);
        return r;
    }
    before = s_stats;
    bus = Bus();
    r = SensorCycle(&bus, &s_stats, offset);
    if (s_stats.measErr != before.measErr || s_stats.resetErr != before.resetErr)
        ESP_LOGW(kTag, "I2C error");
    if (s_stats.crcErr != before.crcErr)
        ESP_LOGW(kTag, "CRC error");
    return r;
}
