/*
 * This code is to scan the list of i2c device connected to the esp32 and debug the device...
 * To Run this code copy this code and past it in the main.c or change the file name in the CMakeLists.txt
*/
#include <stdio.h>
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_SDA     6
#define I2C_SCL     7
#define PN532_ADDR  0x24

static const char *TAG = "i2c_scan";

/* Read from the PN532, retrying until its status byte says "ready" (0x01) */
static esp_err_t pn532_read(i2c_master_dev_handle_t dev, uint8_t *buf, size_t len)
{
    for (int i = 0; i < 20; i++) {
        esp_err_t err = i2c_master_receive(dev, buf, len, 100);
        if (err == ESP_OK && buf[0] == 0x01) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_ERR_TIMEOUT;
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(500));   // give the PN532 time to start

    /* ---- Step 1: create the I2C bus ---- */
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

    /* ---- Step 2: scan the bus once ---- */
    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(bus, addr, 50) == ESP_OK) {
            ESP_LOGI(TAG, "Found device at 0x%02X", addr);
            found++;
        }
    }
    ESP_LOGI(TAG, "Scan done, %d device(s)", found);

    /* ---- Step 3: add the PN532 as a device ---- */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PN532_ADDR,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t pn532;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &pn532));

    /* ---- Step 4: ask the PN532 for its firmware version, forever ---- */
    while (1) {
        // GetFirmwareVersion command frame
        const uint8_t cmd[] = {0x00, 0x00, 0xFF, 0x02, 0xFE, 0xD4, 0x02, 0x2A, 0x00};
        esp_err_t err = i2c_master_transmit(pn532, cmd, sizeof(cmd), 100);
        ESP_LOGI(TAG, "send command -> %s", esp_err_to_name(err));

        if (err == ESP_OK) {
            // First reply: ACK frame (status byte + 6 bytes)
            uint8_t ack[7];
            err = pn532_read(pn532, ack, sizeof(ack));
            ESP_LOGI(TAG, "ack read -> %s", esp_err_to_name(err));
            ESP_LOG_BUFFER_HEX(TAG, ack, sizeof(ack));

            // Second reply: the real answer (status byte + 13 bytes)
            uint8_t resp[14];
            err = pn532_read(pn532, resp, sizeof(resp));
            ESP_LOGI(TAG, "response read -> %s", esp_err_to_name(err));
            ESP_LOG_BUFFER_HEX(TAG, resp, sizeof(resp));

            if (err == ESP_OK && resp[6] == 0xD5 && resp[7] == 0x03) {
                ESP_LOGI(TAG, "PN532 found! chip=PN5%02X firmware=%d.%d",
                         resp[8], resp[9], resp[10]);
            } else {
                ESP_LOGW(TAG, "Unexpected reply, check wiring and mode");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}