#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "I2S_DSP_RECEIVER";

#define I2S_BCK_IO              GPIO_NUM_4
#define I2S_WS_IO               GPIO_NUM_5
#define I2S_DATA_IN_IO          GPIO_NUM_6

#define SAMPLE_RATE             44100
#define DMA_BUF_LEN             1024
#define NOISE_THRESHOLD         1000  // Hysteresis threshold to ignore wire noise

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

    ESP_LOGI(TAG, "I2S Slave Receiver Initialized with Hysteresis DSP Filter!");
}

void i2s_dsp_processing_task(void *pvParameters) {
    int32_t *dma_buffer = (int32_t *)malloc(DMA_BUF_LEN * sizeof(int32_t));
    if (dma_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffer!");
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
                int16_t sample = (int16_t)(dma_buffer[i] >> 16);
                int16_t abs_sample = (sample < 0) ? -sample : sample;

                if (abs_sample > max_peak) {
                    max_peak = abs_sample;
                }
                sum_squares += ((int64_t)sample * sample);

                // Hysteresis Zero-Crossing State Machine (Filters out breadboard noise)
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

            // Log telemetry ONLY when a strong, valid signal is present
            if (max_peak > 2000) {
                printf("I2S_TELEMETRY: Samples: %d, Peak: %d, RMS: %u, Freq: %u Hz\n",
                       samples_count, (int)max_peak, (unsigned int)rms, (unsigned int)estimated_freq);
                fflush(stdout);
            }
        }
    }
    free(dma_buffer);
}

void app_main(void) {
    init_i2s_slave();
    xTaskCreate(i2s_dsp_processing_task, "i2s_dsp_task", 4096, NULL, 5, NULL);
}