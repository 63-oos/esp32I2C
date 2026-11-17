#include<stdio.h>
#include "string.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "vl53_config.h"

#define I2C_SDA         GPIO_NUM_6
#define I2C_SCL         GPIO_NUM_7
#define VL53_ADDR       0x29
#define CONFIG_START    0x002D // first register of the table (0x2D...0X87  = 91  bytes)

_Static_assert(sizeof(VL51L1X_DEFAULT_CONFIGURATION)==91,"The must be exactly 91 bytes");

static char* TAG = "I2C_vl53l1x";

static i2c_master_dev_handle_t vl53;

static esp_err_t reg_read(uint16_t reg,uint8_t *buf,size_t len)
{
    uint8_t arr[2] = {reg>>8,reg&0xff};
    esp_err_t err = i2c_master_transmit(vl53,arr,sizeof(arr),100);
    if(err==ESP_OK)
    {
        i2c_master_receive(vl53,buf,len,100);
    }
    return err;
}

static esp_err_t reg_write(uint16_t reg ,uint8_t value)
{
    uint8_t buf[3] = {reg>>8,reg&0xff,value};
    esp_err_t err = i2c_master_transmit(vl53,buf,sizeof(buf),100);
    return err;
}

void app_main()
{

    vTaskDelay(pdMS_TO_TICKS(500));

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
                            .device_address = VL53_ADDR,
                            .scl_speed_hz = 100000,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus,&vl53_cfg,&vl53));

    esp_err_t err = i2c_master_probe(bus,0x24,100);
    ESP_LOGI(TAG,"BUS CHECK PROBE 0X24 ->%s",esp_err_to_name(err));
    if(err!=ESP_OK) return ;
    err = i2c_master_probe(bus,VL53_ADDR,100);
    ESP_LOGI(TAG,"BUS CHECK PROBE 0X29 ->%s",esp_err_to_name(err));
    if(err!=ESP_OK) return ;
    

    uint8_t boot =0;
    err = reg_read(0x00E5,&boot,1);
    if(err!=ESP_OK)
    {
        ESP_LOGE(TAG,"err = %s",esp_err_to_name(err));
        return ;
    }
    ESP_LOGI(TAG,"boot status = %02x",boot);
/*    uint8_t boot = 0;
    for(int i=0;i<100&&boot==0;i++)
    {
        reg_read(0x00E5,&boot,sizeof(boot));
        if(boot==0) vTaskDelay(pdMS_TO_TICKS(10));
    }
    printf("boot status = %02x\n",boot);
    if(boot == 0)
    {
        printf("sensor did not boot, stoping.\n");
        return;
    }*/

    uint8_t v;
    reg_read(0x0030,&v,sizeof(v));
    printf("Register 0x0030 before loading the table = 0x%02x\n",v);

    const size_t n= sizeof(VL51L1X_DEFAULT_CONFIGURATION);
    for(size_t i=0;i<n;i++)
    {
        ESP_ERROR_CHECK(reg_write(CONFIG_START+i, VL51L1X_DEFAULT_CONFIGURATION[i]));
    }
    printf("word %d register\n",(int)n);

    int bad= 0;
    for(size_t i =0;i<n;i++)
    {
        ESP_ERROR_CHECK(reg_read(CONFIG_START+i,&v,1));
        if(v!=VL51L1X_DEFAULT_CONFIGURATION[i])
        {
            printf(" reg0x%04x: wrote %02x, read back %02x\n",(unsigned)(CONFIG_START+i),VL51L1X_DEFAULT_CONFIGURATION[i],v);
            bad++;
        }
    }
    printf("%d of %d read back diffrently \n",bad,(int)n);

    reg_read(0x0030,&v,1);
    printf("register 0x0030 AFTER loading  the table  = 0x%02x\n",v);
}