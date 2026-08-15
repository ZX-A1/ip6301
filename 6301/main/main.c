#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"
#include "soc/gpio_num.h"

// ------------------- 引脚配置 -------------------
#define UART_NUM           UART_NUM_0
#define UART_BAUD_RATE     115200
#define UART_TXD_PIN       GPIO_NUM_21
#define UART_RXD_PIN       GPIO_NUM_20
#define UART_BUF_SIZE      1024

#define I2C_MASTER_NUM     I2C_NUM_0
#define I2C_MASTER_SCL_IO  GPIO_NUM_2
#define I2C_MASTER_SDA_IO  GPIO_NUM_3
#define I2C_MASTER_FREQ_HZ 100000

#define CMD_WRITE          0x00   // 写操作
#define CMD_READ           0xFF   // 读操作


static const char *TAG = "UART2I2C";

// ------------------- UART 初始化 -------------------
static void uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_driver_install(UART_NUM, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, UART_TXD_PIN, UART_RXD_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

// ------------------- I2C 初始化 -------------------
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

// ------------------- 发送字符串（带换行） -------------------
static void send_string(const char *str)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s\r\n", str);
    uart_write_bytes(UART_NUM, buf, strlen(buf));
}

// ------------------- 主程序 -------------------
void app_main(void)
{
    uart_init();
    i2c_init();
    ESP_LOGI(TAG, "UART->I2C 桥接器启动");

    // 准备完成，发送 OK
    send_string("OK");

    uint8_t *rx_buffer = (uint8_t *)malloc(UART_BUF_SIZE);
    while (1) {
        int len = uart_read_bytes(UART_NUM, rx_buffer, UART_BUF_SIZE,
                                  pdMS_TO_TICKS(100));
        if (len > 0) {
            

            uint8_t cmd = rx_buffer[0];
            if (cmd != CMD_WRITE && cmd != CMD_READ) {
                send_string("ERR_CMD");
                continue;
            }

            if ((cmd == CMD_WRITE && len < 4)){
                send_string("ERR_WRITELEN");
                continue;
            }

            if (cmd == CMD_READ && len < 3){
                send_string("ERR_READLEN");
                continue;
            }

            uint8_t dev_addr = rx_buffer[1];// I2C 设备地址
            uint8_t reg_addr = rx_buffer[2];// I2C 寄存器地址
            if (cmd == CMD_WRITE) {
            uint8_t write_buf[len-2];// 准备写入数据缓冲区   
            // 执行 I2C 写操作
            for(int i = 0; i < len-2; i++) 
            {
                 write_buf[i] = rx_buffer[2 + i];
            }
            esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM,
                                                       dev_addr,
                                                       write_buf, len - 2,
                                                       pdMS_TO_TICKS(1000));

            // 发送 I2C 操作结果（使用 esp_err_to_name 转换）
            send_string(esp_err_to_name(ret));
            continue;
            }


             // ---------------- 读操作 (0xFF) ----------------
             if (cmd == CMD_READ) {
                uint8_t read_data[len - 2]; // 准备读取数据缓冲区
                // 先写寄存器地址，再读一个字节
                esp_err_t ret = i2c_master_write_read_device(I2C_MASTER_NUM,
                                                             dev_addr,
                                                             &reg_addr, 1,          // 写寄存器地址
                                                             read_data, len - 2,    // 读取数据
                                                             pdMS_TO_TICKS(1000));
                if (ret == ESP_OK) {
                    char resp[32];
                    for(int i = 0; i < len - 2; i++) {
                        snprintf(resp, sizeof(resp), "RD:0x%02X", read_data[i]);
                        send_string(resp);
                    }
                
                } else {
                    send_string(esp_err_to_name(ret));
                }
                continue;
            }

            if (cmd == 0x0A) {
                uint8_t read_data[len - 2]; // 准备读取数据缓冲区
                // 先写寄存器地址，再读一个字节
                esp_err_t ret = i2c_master_read_from_device(I2C_MASTER_NUM, dev_addr,
                                      read_data, sizeof(read_data),
                                      pdMS_TO_TICKS(1000));
                if (ret == ESP_OK) {
                    char resp[32];
                    for(int i = 0; i < len - 2; i++) {
                        snprintf(resp, sizeof(resp), "RD:0x%02X", read_data[i]);
                        send_string(resp);
                    }
                
                } else {
                    send_string(esp_err_to_name(ret));
                }
                continue;
            }



        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}