#include <stdio.h>
#include "esp_ads1256.h"

static const char *TAG = "ADS1256_EXAMPLE";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ADS1256 Example");
    
    // Configure the ADS1256
    ads1256_config_t ads_config = {
        .mosi = GPIO_NUM_11,      // MOSI pin
        .miso = GPIO_NUM_13,      // MISO pin
        .sclk = GPIO_NUM_12,      // CLK pin
        .cs = GPIO_NUM_10,        // CS pin
        .spi_host = SPI2_HOST,    // SPI host (SPI2 for ESP32-S3)
        .spi_dma = 0,             // No DMA
        .ads_transfer_size = 4096,
    };
    
    ads1256_handle_t handle;
    esp_err_t ret = ads1256_init(&ads_config, &handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADS1256 initialized successfully");
    } else {
        ESP_LOGE(TAG, "Failed to initialize ADS1256: %s", esp_err_to_name(ret));
    }
    
    // Your application code here
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
