#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_slave.h"
#include "esp_log.h"

/* GPIO Pin Definitions (Aligned with RPi Wiring Map) */
#define GPIO_MOSI 11
#define GPIO_MISO 13
#define GPIO_SCLK 12
#define GPIO_CS   10

static const char *TAG = "ADI_SPI_SLAVE";

void app_main(void) {
    esp_err_t ret;

    /* 1. SPI Bus Configuration */
    spi_bus_config_t buscfg = {
        .mosi_io_num = GPIO_MOSI,
        .miso_io_num = GPIO_MISO,
        .sclk_io_num = GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    /* 2. SPI Slave Interface Configuration */
    spi_slave_interface_config_t slvcfg = {
        .mode = 0,
        .spics_io_num = GPIO_CS,
        .queue_size = 3,
        .flags = 0,
    };

    /* 3. Driver Initialization */
    ret = spi_slave_initialize(SPI2_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
    assert(ret == ESP_OK);

    /* 
     * Static Data Buffers (WORD Aligned for DMA) 
     * Buffer size set to 128 bytes to handle all test patterns (including 10-byte Pattern 2).
     */
    static WORD_ALIGNED_ATTR uint8_t sendbuf[10] = {0};
    static WORD_ALIGNED_ATTR uint8_t recvbuf[10] = {0};
    
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));

    ESP_LOGI(TAG, "SPI Slave ready! Waiting for Master...");

    while (1) {
        memset(recvbuf, 0, sizeof(recvbuf));
    
        /* Use full buffer size for transaction length */
        t.length = sizeof(sendbuf) * 8; 
        t.tx_buffer = sendbuf;
        t.rx_buffer = recvbuf;
        
        ret = spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
        
        if (ret == ESP_OK) {
            /* Calculate received bytes from actual transaction length */
            uint32_t received_bytes = t.trans_len / 8;

            ESP_LOGI(TAG, "Hardware detected: %lu bits", t.trans_len); 
            
            if (received_bytes > 0 && received_bytes <= sizeof(sendbuf)) {
                /* Copy received data back to send buffer for next Echo cycle */
                memcpy(sendbuf, recvbuf, received_bytes);
            }
        }
    }
}