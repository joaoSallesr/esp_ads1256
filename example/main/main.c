#include <stdio.h>
#include "esp_ads1256.h"

static const char *TAG = "ADS1256_EXAMPLE";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ADS1256 Example");
    
    // Initialize SPI bus
    spi_bus_config_t bus_config = {
        .mosi_io_num = GPIO_NUM_11,
        .miso_io_num = GPIO_NUM_13,
        .sclk_io_num = GPIO_NUM_12,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return;
    }
    
    // Configure the ADS1256
    ads1256_config_t ads_config = {
        .spi_host = SPI2_HOST,
        .cs = GPIO_NUM_10,
        .drdy = GPIO_NUM_NC,
        .reset = GPIO_NUM_NC,
        .gain = ADS1256_GAIN_1,
        .drate = ADS1256_DRATE_1000SPS,
        .drdy_timeout_ms = 1000,
        .ads_transfer_size = 4096,
    };
    
    ads1256_handle_t handle;
    ret = ads1256_init(&ads_config, &handle);
    
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
