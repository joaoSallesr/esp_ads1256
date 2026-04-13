#include "esp_ads1256.h"
#include <header.h>
#include <stdio.h>
static const char *TAG = "ADS1256_EXAMPLE";

#define SPI_HOST            SPI2_HOST
#define DMA_CHAN            SPI_DMA_CH_AUTO
#define EXAMPLE_CS          GPIO_NUM_39
#define EXAMPLE_DRDY        GPIO_NUM_21
#define ADS_DRDY_TIMEOUT_MS 1000

static void setup_peripherals(void) {
    ESP_LOGI(TAG, "Starting ADS1256 Example");

    // Initialize SPI bus
    spi_bus_config_t bus_config = {
        .mosi_io_num     = GPIO_NUM_11,
        .miso_io_num     = GPIO_NUM_13,
        .sclk_io_num     = GPIO_NUM_12,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4096,
    };

    spi_host_device_t host = SPI_HOST;

    spi_dma_chan_t dma_chan = DMA_CHAN;

    esp_err_t ret = spi_bus_initialize(host, &bus_config, DMA_CHAN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return;
    }
}

static void example_init(ads1256_handle_t *example_handle) {
    /* ADS1256 struct setup */
    // Configure the ADS1256
    ads1256_config_t example_cfg = {
        .spi_host        = SPI_HOST,
        .cs              = EXAMPLE_CS,
        .drdy            = EXAMPLE_DRDY,
        .gain            = ADS1256_GAIN_1,
        .drate           = ADS1256_DRATE_1000SPS,
        .pos_channel     = ADS1256_MUX_AIN0,
        .neg_channel     = ADS1256_MUX_AIN1,
        .drdy_timeout_ms = ADS_DRDY_TIMEOUT_MS,
        .bufen           = false,
    };

    /* Load Cell initialization */
    ESP_ERROR_CHECK(ads1256_init(&example_cfg, example_handle));

    ESP_LOGI(TAG, "ADS initialized");
}

void ads_task(void *pvParameters) {
    ads1256_handle_t example_handle;
    example_init(&example_handle);

    ads1256_sample_t ads_sample;

    while (true) {

        ads1256_start_conversion(example_handle);

        // ADS1256 raw reading
        ads1256_read_result(example_handle, &ads_sample.raw);

        // Update global sample
        xSemaphoreTake(xADSMutex, portMAX_DELAY);
        ads1256_sample_g = ads_sample;
        xSemaphoreGive(xADSMutex);
    }

    ESP_ERROR_CHECK(ads1256_delete(example_handle));
    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting main application");
    setup_peripherals();
    vTaskDelay(pdMS_TO_TICKS(150)); // Wait for peripherals to stabilize

    xTaskCreatePinnedToCore(ads_task, "ADS", configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL, 1);
}
