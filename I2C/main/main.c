#include<stdio.h>
#include<string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_SDA         GPIO_NUM_6
#define I2C_SCL         GPIO_NUM_7
#define PN532_Addr      0x24

static const char* TAG = "PN532_POLL";

i2c_master_dev_handle_t dev;

esp_err_t dev_ready(int timeout_ms,int *tries)
{
    uint64_t start = esp_timer_get_time();
    int count = 0;

    while((esp_timer_get_time()-start)<timeout_ms*1000LL)
    {
        uint8_t status = 0;
        count++;
        esp_err_t err = i2c_master_receive(dev,&status,1,100);
//        printf("count = %d\n",count);
//        printf("I2C err = %s, status = 0x%02X\n",esp_err_to_name(err), status);
        if(err==ESP_OK && status == 0x01)
        {
            if(tries)*tries = count;
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if(tries) *tries = count;
    return ESP_ERR_TIMEOUT;

}

static esp_err_t pn532_read(const uint8_t* data,uint8_t len,uint8_t *resp,size_t resp_len,int* tries)
{
    uint8_t frame[32];
    uint8_t sum =0;
    frame[0] = 0x00;
    frame[1] = 0x00;
    frame[2] = 0xFF;
    frame[3] = len;
    frame[4] = (uint8_t)(0x100-len);
    for(int i=0;i<len;i++)
    {
        frame[5+i] = data[i];
        sum += data[i];
    }
    frame[5+len] = (uint8_t)(0x100-sum);
    frame[6+len] = 0x00;
    esp_err_t err = i2c_master_transmit(dev,frame,7+len,100);
    if(err!=ESP_OK)
    {
        printf("I2C err = %s\n",esp_err_to_name(err));
        return err;
    }

    //wait for ack fram to read it...
    err = dev_ready(1000,NULL);
    if(err!=ESP_OK)
    {
        printf("I2C err = %s\n",esp_err_to_name(err));
        return err;
    }
    uint8_t ack[7];
    err = i2c_master_receive(dev,ack,sizeof(ack),100);
    if(err!=ESP_OK)
    {
        printf("I2C err = %s\n",esp_err_to_name(err));
        return err;
    }
//    printf("ack frame...\n");
//    ESP_LOG_BUFFER_HEX(TAG,ack,sizeof(ack));
    
    
    //wait for response frame and read it...
    err = dev_ready(1000,tries);
    if(err!=ESP_OK)
    {
        printf("I2C err = %s\n",esp_err_to_name(err));
        return err;
    }

    err = i2c_master_receive(dev,resp,resp_len,100);
    if(err!=ESP_OK){
        printf("I2C err = %s\n",esp_err_to_name(err));
        return err;
    }

    // A valid reply has D5 and (command +1) after the frame header
    if(resp[6] != 0xD5 || resp[7]!=(uint8_t)(data[1]+1))
    {
        printf("ESP_ERR_INVALID_RESPONSE\n");
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

void app_main(void)
{
    i2c_master_bus_handle_t bus;
    i2c_master_bus_config_t bus_config = {
                            .i2c_port = I2C_NUM_0,
                            .sda_io_num = I2C_SDA,
                            .scl_io_num = I2C_SCL,
                            .clk_source = I2C_CLK_SRC_DEFAULT,
                            .glitch_ignore_cnt = 7,
                            .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config,&bus));

    i2c_device_config_t dev_config = {
                            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                            .device_address = PN532_Addr,
                            .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus,&dev_config,&dev));
    


    int found = 0;
    for(uint8_t addr = 0x08;addr<0x78;addr++)
    {
        if(i2c_master_probe(bus,addr,50)==ESP_OK)
        {
            printf("Found device at 0x%02X\n",addr);
            found++;
        }        
    }
    uint8_t resp[32];
    esp_err_t err;

    const uint8_t cmd[] = {0xD4,0x02};

    err = pn532_read(cmd,sizeof(cmd),resp,sizeof(resp),NULL);
    if(err!=ESP_OK)
    {
        printf("[I2C] err = %s\n",esp_err_to_name(err));
    }
//    printf("Response\n");
    ESP_LOGI(TAG,"command response : %s\n",esp_err_to_name(err));
//    ESP_LOG_BUFFER_HEX(TAG,resp,sizeof(resp));


//  SAMconfiguration 
    const uint8_t sam[] = {0xD4,0x14,0x01,0x14,0x01};
    err = pn532_read(sam,sizeof(sam),resp,sizeof(resp),NULL);
    if(err!=ESP_OK)
    {
        printf("[I2C] err : %s\n",esp_err_to_name(err));
        ESP_LOGE(TAG,"Setup failed, stoping. Check wiring and mode switches.");
        return;
    }
    ESP_LOGI(TAG,"SAMConfiguration : %s",esp_err_to_name(err));
//    printf("samconfig resp\n");

//    ESP_LOG_BUFFER_HEX(TAG,resp,sizeof(resp));

//  rfconfiguration
    const uint8_t rfcg[] = {0xD4,0x32,0x05,0xFF,0X01,0x00};
    err = pn532_read(rfcg,sizeof(rfcg),resp,sizeof(resp),NULL);
    if(err!=ESP_OK)
    {
        printf("[I2C] err : %s\n",esp_err_to_name(err));
    }
    ESP_LOGI(TAG,"RFConfiguration : %s",esp_err_to_name(err));
//    ESP_LOG_BUFFER_HEX(TAG,resp,sizeof(resp));

    const uint8_t scan[] = {0xD4,0x4A,0x01,0x00};
    const uint8_t read_page[] = {0xD4, 0x40, 0x01, 0x30, 0x04};
    bool card_status = true;

    while(1)
    {
        int tries = 0;
        err = pn532_read(scan,sizeof(scan),resp,sizeof(resp),&tries);
        if(err!=ESP_OK)
        {
            printf("[I2C] err: %s\n",esp_err_to_name(err));
        }
//        ESP_LOG_BUFFER_HEX(TAG,resp,sizeof(resp));
        if(err == ESP_OK && resp[8]>=1)
        {
//            printf("card detected....\n");
            uint8_t uid_len = resp[13];
            printf("The lenght of the UID is %d\n",uid_len);
            if(!card_status)
            {
                char uid_str[40] = "";
                for(int i=0;i<uid_len;i++)
                {
                    char part[4];
                    snprintf(part,sizeof(part),"%02x%s",resp[14+i],(i<uid_len-1)?":":"");
                    strcat(uid_str,part);
                }
                printf("[Card Detected]: UID : %s (%d Bytes)  ATQA = %02x%02x  SAK =%02x polls = %d\n",uid_str,uid_len,resp[10],resp[11],resp[12],tries);
                err = pn532_read(read_page,sizeof(read_page),resp,sizeof(resp),&tries);
                if(err!=ESP_OK)
                {
                    printf(TAG,esp_err_to_name(err));
                }
                ESP_LOGI(TAG,"Read page response : %s",esp_err_to_name(err));
                ESP_LOG_BUFFER_HEX(TAG,resp,sizeof(resp));
                card_status = !card_status;
            }
            
        }
        else if(err == ESP_OK)
        {
            if(card_status)
            {
                card_status = !card_status;
                printf("no card detected...\n");
            }
        }
//        printf("tires: %d\n",tries);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}