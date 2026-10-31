#include <stdio.h>
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_SDA          6
#define I2C_SCL          7
#define PN532_ADDR       0x24
#define POLL_DELAY_MS    0     // <-- change this in the experiments

static const char *TAG = "pn532_poll";

/* Poll the status byte until the PN532 says ready (0x01).
   Reports how many tries and how many microseconds it took. */
static esp_err_t pn532_wait_ready(i2c_master_dev_handle_t dev, int timeout_ms,
                                  int *tries, int64_t *elapsed_us)
{
    int64_t start = esp_timer_get_time();
    *tries = 0;

    while ((esp_timer_get_time() - start) < timeout_ms * 1000LL) {
        uint8_t status = 0;
        (*tries)++;
        esp_err_t err = i2c_master_receive(dev, &status, 1, 100);
        if (err == ESP_OK && status == 0x01) {
            *elapsed_us = esp_timer_get_time() - start;
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_DELAY_MS));
    }
    *elapsed_us = esp_timer_get_time() - start;
    return ESP_ERR_TIMEOUT;
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(500));   // let the PN532 start up

    /* ---- create the bus ---- */
    i2c_master_bus_handle_t bus;
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    /* ---- add the PN532 ---- */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PN532_ADDR,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t pn532;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &pn532));

    while (1) {
        int tries;
        int64_t us;

        /* 1) send GetFirmwareVersion */
        const uint8_t cmd[] = {0x00, 0x00, 0xFF, 0x02, 0xFE, 0xD4, 0x02, 0x2A, 0x00};
        esp_err_t err = i2c_master_transmit(pn532, cmd, sizeof(cmd), 100);
        ESP_LOGI(TAG, "send command -> %s", esp_err_to_name(err));

        /* 2) poll until the ACK frame is ready, then read it */
        err = pn532_wait_ready(pn532, 1000, &tries, &us);
        ESP_LOGI(TAG, "ACK ready: %s after %d tries, %lld us",
                 esp_err_to_name(err), tries, us);
        uint8_t ack[7];
        i2c_master_receive(pn532, ack, sizeof(ack), 100);
        ESP_LOG_BUFFER_HEX(TAG, ack, sizeof(ack));

        /* 3) poll until the response is ready, then read it */
        err = pn532_wait_ready(pn532, 1000, &tries, &us);
        ESP_LOGI(TAG, "RESPONSE ready: %s after %d tries, %lld us",
                 esp_err_to_name(err), tries, us);
        uint8_t resp[14];
        i2c_master_receive(pn532, resp, sizeof(resp), 100);
        ESP_LOG_BUFFER_HEX(TAG, resp, sizeof(resp));

        if (resp[6] == 0xD5 && resp[7] == 0x03) {
            ESP_LOGI(TAG, "PN532 firmware=%d.%d", resp[9], resp[10]);
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}