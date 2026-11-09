#include<stdio.h>
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.H"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define I2C_SDA         GPIO_NUM_6
#define I2C_SCL         GPIO_NUM_7
#define I2C_IRQ         GPIO_NUM_4
#define PN532_addr      0x24

static SemaphoreHandle_t irq_sem;

static const char* TAG = "I2C_intr";

static void IRAM_ATTR irq_isr(void *arg)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(irq_sem,&woken);
    portYIELD_FROM_ISR(woken);
}


void app_main()
{

    irq_sem = xSemaphoreCreateBinary();

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


    //samconfiguration so the irq pin is enabled 
    uint8_t ack[7],resp[10];
    uint8_t sam[] = {0x00,0x00,0xff,0x05,0xfb,0xd4,0x14,0x01,0x14,0x01,0x02,0x00};
    i2c_master_transmit(dev,sam,sizeof(sam),100);
    vTaskDelay(pdMS_TO_TICKS(20));
    i2c_master_receive(dev,ack,sizeof(ack),100);
    vTaskDelay(pdMS_TO_TICKS(20));
    i2c_master_receive(dev,resp,sizeof(resp),100);
    vTaskDelay(pdMS_TO_TICKS(20));


    

    while(1)
    {
        const uint8_t cmd[] = {0x00,0x00,0xFF,0x02,0xFE,0xD4,0x02,0x2A,0x00};
        uint8_t fw_ack[7],fw_resp[14];
        ESP_LOGI(TAG,"-----new round------");


        //throw away the old bell so only the new bell rang.........................
        xSemaphoreTake(irq_sem,0);

        esp_err_t err = i2c_master_transmit(dev,cmd,sizeof(cmd),100);
        int64_t t0 = esp_timer_get_time();
        ESP_LOGI(TAG,"transmit:%s",esp_err_to_name(err));


        //sleep until ack ready......................................................
        if(xSemaphoreTake(irq_sem,pdMS_TO_TICKS(100))==pdTRUE)
        {
            ESP_LOGI(TAG,"bell 1 (ACK ready)            after %lldus",esp_timer_get_time()-t0);
        }
        else
        {
            ESP_LOGW(TAG,"bell 1 never rang");
        }
        i2c_master_receive(dev,fw_ack,sizeof(fw_ack),100);

        
        // sleep until response ready.................................................
        if(xSemaphoreTake(irq_sem,pdMS_TO_TICKS(100))==pdTRUE)
        {
            ESP_LOGI(TAG,"bell 2 (response ready)       after %lldus",esp_timer_get_time()-t0);
        }
        else
        {
            ESP_LOGW(TAG,"bell 2 never rang");
        }
        i2c_master_receive(dev,fw_resp,sizeof(fw_resp),100);

        ESP_LOGI(TAG,"firmware version %d.%d",fw_resp[9],fw_resp[10]);
        vTaskDelay(pdMS_TO_TICKS(3000));

    }
}