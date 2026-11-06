#include<stdio.h>
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.H"
#include "freertos/task.h"

#define I2C_SDA         GPIO_NUM_6
#define I2C_SCL         GPIO_NUM_7
#define I2C_IRQ         GPIO_NUM_4
#define PN532_addr      0x24

static volatile int fall_count = 0;
static const char* TAG = "I2C_intr";

static void IRAM_ATTR irq_isr(void *arg)
{
    fall_count++;
}


void app_main()
{
    gpio_config_t irq_cfg = {
                        .pin_bit_mask = 1ULL<<I2C_IRQ,
                        .mode = GPIO_MODE_INPUT,
                        .pull_up_en = GPIO_PULLUP_ENABLE,
//                        .pull_down_en = GPIO_PULLDOWN_DISABLE,
                        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&irq_cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(I2C_IRQ,irq_isr,NULL);

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

    i2c_device_config_t dev_cfg  = {
                        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                        .device_address = PN532_addr,
                        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t dev;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus,&dev_cfg,&dev));

    uint8_t ack[7],resp[10];
    uint8_t sam[] = {0x00,0x00,0xff,0x05,0xfb,0xd4,0x14,0x01,0x14,0x01};
    i2c_master_transmit(dev,sam,sizeof(sam),100);
    vTaskDelay(pdMS_TO_TICKS(20));
    i2c_master_receive(dev,ack,sizeof(ack),100);
    vTaskDelay(pdMS_TO_TICKS(20));
    i2c_master_receive(dev,resp,sizeof(resp),100);
    vTaskDelay(pdMS_TO_TICKS(20));


    const uint8_t cmd[] = {0x00,0x00,0xFF,0x02,0xFE,0xD4,0x02,0x2A,0x00};
    uint8_t fw_ack[7],fw_resp[14];

    while(1)
    {
        fall_count = 0;
        ESP_LOGI(TAG,"-----new round------");
        ESP_LOGI(TAG,"idle                  : fall so far = %d",fall_count);

        i2c_master_transmit(dev,cmd,sizeof(cmd),100);
        vTaskDelay(pdMS_TO_TICKS(20));
        ESP_LOGI(TAG,"command send          : fall so far = %d",fall_count);

        i2c_master_receive(dev,fw_ack,sizeof(fw_ack),100);
        vTaskDelay(pdMS_TO_TICKS(20));
        ESP_LOGI(TAG,"ack read              : fall so far= %d",fall_count);

        i2c_master_receive(dev,fw_resp,sizeof(fw_resp),100);
        vTaskDelay(pdMS_TO_TICKS(20));
        ESP_LOGI(TAG,"response read         : fall so far= %d",fall_count);

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}