#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "driver/spi_slave.h"
#include "driver/uart.h"
#include "driver/gpio.h"

static const char *TAG = "HIL_DUT_FIRMWARE";

// ============================================================================
// 1. PIN DEFINITIONS & CONFIGURATION
// ============================================================================

// I2S Bus Configuration (Slave Mode)
#define I2S_BCK_IO              GPIO_NUM_4
#define I2S_WS_IO               GPIO_NUM_5
#define I2S_DATA_IN_IO          GPIO_NUM_6
#define SAMPLE_RATE             44100
#define DMA_BUF_LEN             1024
#define NOISE_THRESHOLD         1000  // Hysteresis threshold to suppress contact bounce noise

// SPI Slave Bus Configuration (Standard ESP32-S3 Pinout)
#define SPI_HOST_ID             SPI2_HOST
#define GPIO_MOSI               GPIO_NUM_11
#define GPIO_MISO               GPIO_NUM_13
#define GPIO_SCLK               GPIO_NUM_12
#define GPIO_CS                 GPIO_NUM_10
#define SPI_BUF_SIZE            128

// UART Command & Telemetry Interface Configuration
#define UART_PORT_NUM           UART_NUM_1
#define UART_BAUD_RATE          115200
#define UART_BUF_SIZE           1024

// Default ESP32-S3 Console UART GPIOs (GPIO 43 = TX, GPIO 44 = RX)
#define UART_TX_IO              GPIO_NUM_17
#define UART_RX_IO              GPIO_NUM_18

// ============================================================================
// 2. I2S SLAVE & REAL-TIME DSP IMPLEMENTATION
// ============================================================================

static i2s_chan_handle_t rx_handle = NULL;

typedef enum {
    SIGNAL_STATE_LOW,
    SIGNAL_STATE_HIGH
} signal_state_t;

void init_i2s_slave(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = DMA_BUF_LEN;
    
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_DATA_IN_IO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    ESP_LOGI(TAG, "I2S Slave Receiver initialized with Hysteresis DSP Filter.");
}

void i2s_dsp_processing_task(void *pvParameters) {
    int32_t *dma_buffer = (int32_t *)malloc(DMA_BUF_LEN * sizeof(int32_t));
    if (dma_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for DMA buffer!");
        vTaskDelete(NULL);
        return;
    }

    size_t bytes_read = 0;

    while (1) {
        esp_err_t err = i2s_channel_read(rx_handle, dma_buffer, DMA_BUF_LEN * sizeof(int32_t), &bytes_read, pdMS_TO_TICKS(500));
        
        if (err == ESP_OK && bytes_read > 0) {
            int samples_count = (int)(bytes_read / sizeof(int32_t));
            int16_t max_peak = 0;
            int64_t sum_squares = 0;
            int zero_crossings = 0;
            
            signal_state_t state = SIGNAL_STATE_LOW;

            for (int i = 0; i < samples_count; i++) {
                // Extract 16-bit PCM payload from 32-bit MSB-aligned I2S slot
                int16_t sample = (int16_t)(dma_buffer[i] >> 16);
                int16_t abs_sample = (sample < 0) ? -sample : sample;

                if (abs_sample > max_peak) {
                    max_peak = abs_sample;
                }
                sum_squares += ((int64_t)sample * sample);

                // Hysteresis Zero-Crossing State Machine (Schmitt Trigger to filter physical wire noise)
                if (state == SIGNAL_STATE_LOW && sample > NOISE_THRESHOLD) {
                    state = SIGNAL_STATE_HIGH;
                    zero_crossings++;
                } else if (state == SIGNAL_STATE_HIGH && sample < -NOISE_THRESHOLD) {
                    state = SIGNAL_STATE_LOW;
                    zero_crossings++;
                }
            }

            uint32_t rms = (uint32_t)sqrt((double)sum_squares / (double)samples_count);
            double duration_sec = (double)samples_count / (double)SAMPLE_RATE;
            uint32_t estimated_freq = (duration_sec > 0) ? (uint32_t)(zero_crossings / (2.0 * duration_sec)) : 0;

            // Send telemetry over DEDICATED HIL UART1 (not stdout/printf!)
            // Send telemetry over DEDICATED HIL UART
            if (max_peak > 2000) {
                char telem_msg[128]; // Properly sized buffer to hold 60-88 bytes telemetry line
                int len = snprintf(telem_msg, sizeof(telem_msg),
                       "I2S_TELEMETRY: Samples: %d, Peak: %d, RMS: %u, Freq: %u Hz\r\n",
                       samples_count, (int)max_peak, (unsigned int)rms, (unsigned int)estimated_freq);
    
                if (len > 0 && len < sizeof(telem_msg)) {
                    uart_write_bytes(UART_PORT_NUM, telem_msg, len);
                }
            }
        }
    }
    free(dma_buffer);
}

// ============================================================================
// 3. SPI SLAVE LOOPBACK IMPLEMENTATION
// ============================================================================

void spi_slave_task(void *pvParameters) {
    WORD_ALIGNED_ATTR uint8_t sendbuf[SPI_BUF_SIZE] = {0};
    WORD_ALIGNED_ATTR uint8_t recvbuf[SPI_BUF_SIZE] = {0};

    spi_bus_config_t buscfg = {
        .mosi_io_num = GPIO_MOSI,
        .miso_io_num = GPIO_MISO,
        .sclk_io_num = GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    spi_slave_interface_config_t slvcfg = {
        .mode = 0,
        .spics_io_num = GPIO_CS,
        .queue_size = 3,
        .flags = 0,
    };

    // Configure pull-up resistors on SPI bus to eliminate floating line states
    gpio_set_pull_mode(GPIO_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_SCLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_CS, GPIO_PULLUP_ONLY);

    esp_err_t ret = spi_slave_initialize(SPI_HOST_ID, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI Slave driver!");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "SPI Slave Driver initialized successfully.");

    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));

    while (1) {
        memset(recvbuf, 0, SPI_BUF_SIZE);
        t.length = SPI_BUF_SIZE * 8; // Length in bits
        t.tx_buffer = sendbuf;
        t.rx_buffer = recvbuf;

        // Block task until the SPI Master initiates a transaction
        ret = spi_slave_transmit(SPI_HOST_ID, &t, portMAX_DELAY);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "SPI Transaction complete. Transmitted %d bytes.", t.trans_len / 8);
            // Copy MOSI payload to MISO buffer for full-duplex loopback verification
            memcpy(sendbuf, recvbuf, SPI_BUF_SIZE);
        }
    }
}

// ============================================================================
// 4. UART CHALLENGE-RESPONSE TASK
// ============================================================================

void init_uart(void) {
    uart_config_t uart_config = {
        .baud_rate  = UART_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_IO, UART_RX_IO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, UART_BUF_SIZE * 2, 0, NULL, 0));

    ESP_LOGI("UART_HIL", "Dedicated HIL UART1 initialized on TX=%d, RX=%d", UART_TX_IO, UART_RX_IO);
}

void uart_task(void *pvParameters) {
    uint8_t rx_char;
    static char line_buf[1024]; // Increased line buffer to 1024 bytes to support stress matrix payloads
    int line_pos = 0;

    while (1) {
        int len = uart_read_bytes(UART_PORT_NUM, &rx_char, 1, pdMS_TO_TICKS(50));
        if (len > 0) {
            char c = (char)rx_char;

            if (c == '\n' || c == '\r') {
                if (line_pos > 0) {
                    line_buf[line_pos] = '\0';

                    if (strstr(line_buf, "UART_PING") != NULL) {
                        const char *pong = "UART_PONG\r\n";
                        uart_write_bytes(UART_PORT_NUM, pong, strlen(pong));
                    } else {
                        uart_write_bytes(UART_PORT_NUM, line_buf, line_pos);
                        uart_write_bytes(UART_PORT_NUM, "\r\n", 2);
                    }
                    line_pos = 0;
                }
            } else {
                if (line_pos < sizeof(line_buf) - 1) {
                    line_buf[line_pos++] = c;
                } else {
                    // Buffer overflow protection for payloads exceeding 1024 bytes
                    line_pos = 0;
                }
            }
        }
    }
}


// ============================================================================
// 5. MAIN ENTRY POINT
// ============================================================================

void app_main(void) {
    ESP_LOGI(TAG, "Starting HIL DUT Multi-Protocol Target Firmware...");

    // 1. Initialize and launch I2S DSP Receiver Task
    init_i2s_slave();
    xTaskCreate(i2s_dsp_processing_task, "i2s_dsp_task", 4096, NULL, 5, NULL);

    // 2. Launch SPI Slave Loopback Task
    xTaskCreate(spi_slave_task, "spi_slave_task", 4096, NULL, 4, NULL);

    // 3. Launch UART Handler Task
    init_uart();
    xTaskCreate(uart_task, "uart_task", 4096, NULL, 5, NULL);
}
