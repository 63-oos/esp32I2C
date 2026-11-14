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
#define VL532L1X        0x29

static char* TAG = "I2C2";

static i2c_master_dev_handle_t vl53;

static esp_err_t reg_read(uint16_t reg,uint8_t *buf,size_t len)
{
    uint8_t addr[2] = {reg>>8,reg&0xff};
    esp_err_t err = i2c_master_transmit(vl53,addr,sizeof(addr),100);
    if(err==ESP_OK) err = i2c_master_receive(vl53,buf,len,100);
    return err;
}

static esp_err_t reg_write(uint16_t reg,uint64_t value)
{
    uint8_t buf[3]= {reg>>8,reg&0xff,value};
    esp_err_t err = i2c_master_transmit(vl53,buf,sizeof(buf),100);
    return err;
}

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

    i2c_device_config_t vl53_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = VL532L1X,
        .scl_speed_hz = 100000,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus,&vl53_cfg,&vl53));

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

    for(uint8_t addr = 0x08;addr<0x78;addr++)
    {
        if(i2c_master_probe(bus,addr,50)==ESP_OK)
        {
            ESP_LOGI(TAG,"FOUND DEVICE 0x%02x",addr);
        }
    }
    uint8_t original,flipped,restored;
    ESP_ERROR_CHECK(reg_read(0x0030,&original,1));
    ESP_ERROR_CHECK(reg_write(0x0030,original^0x10));
    ESP_ERROR_CHECK(reg_read(0x0030,&flipped,1));
    ESP_ERROR_CHECK(reg_write(0x0030,original));
    ESP_ERROR_CHECK(reg_read(0x0030,&restored,1));

    printf("0x0030: orginal = %02x  |   after flipping bit 4 = %02x |   after restoring = %02x\n",original,flipped,restored);
/*    while(1)
    {
        uint8_t id[2] = {0,0};
        uint8_t boot = 0;
        esp_err_t err = reg_read(0X010F,id,sizeof(id));
        if(err!=ESP_OK) ESP_LOGE(TAG,"REG ->%s",esp_err_to_name(err));
        printf("id = %02x %02x\n",id[0],id[1]);
        err = reg_read(0x03,&boot,sizeof(boot));
        if(err!=ESP_OK) ESP_LOGE(TAG,"boot->%s",esp_err_to_name(err));
        printf("err = %s\n",esp_err_to_name(err));
        printf("boot = %02x\n",boot);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }*/

}