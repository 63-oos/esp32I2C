/*
 * This code is to scan the list of i2c device connected to the esp32
 * To Run this code copy this code and past it in the main.c or change the file name in the CMakeLists.txt
*/
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_SDA 6
#define I2C_SCL 7
static const char *TAG = "i2c_scan";

void app_main(void)
{
    i2c_master_bus_handle_t bus;
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &bus));

    while (1) {
        int found = 0;
        for (uint8_t addr = 0x08; addr < 0x78; addr++) {
            if (i2c_master_probe(bus, addr, 50) == ESP_OK) {
                ESP_LOGI(TAG, "Found device at 0x%02X", addr);
                found++;
            }
        }
        ESP_LOGI(TAG, "Scan done, %d device(s)", found);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}