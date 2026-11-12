#include<stdio.h>
#include "string.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_SDA         GPIO_NUM_6
#define I2C_SCL         GPIO_NUM_7
#define PN532_ADDR      0x24
#define VL532L1X         0x29

static char* TAG = "I2C2";

void app_main()
{
    i2c_master_bus_handle_t bus;
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg,&bus));

    esp_err_t err = i2c_master_probe(bus,PN532_ADDR,100);
    if(err!=ESP_OK)
    {
        ESP_LOGE(TAG,"pn532 probefailed----> %s",esp_err_to_name(err));
    }
    ESP_LOGI(TAG,"pn532 probe-----> %s",esp_err_to_name(err));

    err = i2c_master_probe(bus,VL532L1X,100);
    if(err!=ESP_OK)
    {
        ESP_LOGE(TAG,"VL532L1X probe failed------>%s",esp_err_to_name(err));
    }
    ESP_LOGI(TAG,"vl532l1x probe ----->%s",esp_err_to_name(err));

}