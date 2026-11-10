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

static int i2c_read = 0;

i2c_master_dev_handle_t dev;

static const char* TAG = "I2C_intr";

static void IRAM_ATTR irq_isr(void *arg)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(irq_sem,&woken);
    portYIELD_FROM_ISR(woken);
}

static esp_err_t dev_ready(int timeout_ms)
{
    uint64_t deadline = esp_timer_get_time()+timeout_ms*1000LL;
    while(gpio_get_level(I2C_IRQ)!=0)
    {
        uint64_t remainning = deadline-esp_timer_get_time();
        if(remainning<=0)
        {
            return ESP_ERR_TIMEOUT;
        }
        xSemaphoreTake(irq_sem,remainning/1000+1);
    }
    return ESP_OK;

}
static esp_err_t read_frame(uint8_t* data,size_t len,int timeout_ms)
{
    for(int attempt = 0;attempt<5;attempt++)
    {
        esp_err_t err = dev_ready(timeout_ms);
        if(err!=ESP_OK) return err;
        err = i2c_master_receive(dev,data,len,100);
        i2c_read++;
        if(err!=ESP_OK) return err;
        if(data[0]==0x01)return ESP_OK;
        vTaskDelay(1);
    }
    return ESP_ERR_TIMEOUT;
}

static esp_err_t send_cmd(const uint8_t *data,uint8_t len)
{
    xSemaphoreTake(irq_sem,0);

    uint8_t frame[32];
    uint8_t sum = 0;
    frame[0] = 0x00;
    frame[1] = 0x00;
    frame[2] = 0xff;
    frame[3] = len;
    frame[4] = (uint8_t)(0x100-len);
    for(int i=0;i<len;i++)
    {
        frame[5+i] = data[i];
        sum +=data[i];
    }
    frame[5+len] = (uint8_t)(0x100-sum);
    frame[6+len] = 0x00;

    esp_err_t err = i2c_master_transmit(dev,frame,7+len,100);
    if (err!=ESP_OK)
    {
        ESP_LOGE(TAG,"transmit : %s",esp_err_to_name(err));
    }
    return err;
}
static esp_err_t pn532_cmd(const uint8_t* data,uint8_t len,uint8_t *resp,size_t resp_len)
{
    xSemaphoreTake(irq_sem,0);
    esp_err_t err = send_cmd(data,len);
    if(err!=ESP_OK) return err;

    uint8_t ack[7];
    err = read_frame(ack,sizeof(ack),1000);
    if(err!=ESP_OK) return err;

    err = read_frame(resp,resp_len,1000);
    if(err!=ESP_OK) return err;
    
    return ESP_OK;
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
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus,&dev_cfg,&dev));

    esp_err_t err = i2c_master_probe(bus,PN532_addr,100);
    ESP_LOGI(TAG,"BUS CHECK PROBE 0X24 ->%s",esp_err_to_name(err));
    if(err!=ESP_OK) return ;


    //samconfiguration so the irq pin is enabled 
    uint8_t resp[32];
    const uint8_t sam[] = {0xd4,0x14,0x01,0x14,0x01};
    err = pn532_cmd(sam,sizeof(sam),resp,sizeof(resp));
    ESP_LOGI(TAG,"SAMConfiguration %s",esp_err_to_name(err));
    if(err!= ESP_OK) return ;
    const uint8_t rf[] = {0xD4, 0x32, 0x05, 0xFF, 0x01, 0xFF};
    err = pn532_cmd(rf,sizeof(rf),resp,sizeof(resp));
    ESP_LOGI(TAG,"RFConfiguration %s",esp_err_to_name(err));
    if(err!= ESP_OK) return ;


    const uint8_t scan[] = {0xD4, 0x4A, 0x01, 0x00};
    

    while(1)
    {
        i2c_read = 0;
        xSemaphoreTake(irq_sem,0);
        uint8_t ack[7];
        err = send_cmd(scan,sizeof(scan));
        if(err == ESP_OK) err = read_frame(ack,sizeof(ack),1000);
        if(err!=ESP_OK)
        {
            ESP_LOGI(TAG,"SCAN STARTED failed: %s",esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        ESP_LOGI(TAG,"scan %s",esp_err_to_name(err));
        do{
            err = read_frame(resp,sizeof(resp),5000);
            if(err== ESP_ERR_TIMEOUT)
            {
                ESP_LOGI(TAG,"still waiting I2C READ SO FAR = %d",i2c_read);
            }
        }while(err == ESP_ERR_TIMEOUT);

        if(err == ESP_OK && resp[6]==0xD5 && resp[7]== 0x4B && resp[8] >=1)
        {
            uint8_t uid_len = resp[13];
            printf("[card] UID:");
            char uid[40] = "";
            for(int i=0;i<uid_len;i++)
            {
                char part[4];
                snprintf(part,sizeof(part),"%02x%s",resp[14+i],(i<uid_len-1)?":":"");
                strcat(uid,part);
            }
            printf("%s\n",uid);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));




/*        xSemaphoreTake(irq_sem,0);
        err = send_cmd(scan,sizeof(scan));
        uint8_t ack[7];
        if (err==ESP_OK) err = read_frame(ack,sizeof(ack),1000);
        if(err!=ESP_OK) {
            ESP_LOGI(TAG,"SCAN START FAILED %s",esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        ESP_LOGI(TAG,"scan %s",esp_err_to_name(err));

        do{
            err = read_frame(resp,sizeof(resp),timeout_ms);
            if(err!=ESP_OK)
            {
                ESP_LOGI(TAG,"scan response:%s",esp_err_to_name(err));
            }
        }while(err==ESP_ERR_TIMEOUT);

        if(err == ESP_OK && resp[6] == 0XD5 && resp[7]==0x4B && resp[8]>=1)
        {
            uint8_t uid_len = resp[13];
            printf("[Card] UID:");
            for (int i = 0; i < uid_len && i < 10; i++) printf(" %02X", resp[14 + i]);
            printf("  ATQA=%02X%02X SAK=%02X\n", resp[10], resp[11], resp[12]);
            ESP_LOGI(TAG, "I2C reads for this whole scan = %d", i2c_reads);
        }
        vTaskDelay(pdMS_TO_TICKS(3000));*/

    }
}