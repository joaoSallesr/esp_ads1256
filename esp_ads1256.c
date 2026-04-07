#include <stdio.h>

#include "esp_ads1256.h"

static const char *TAG = "ads1256";

esp_err_t ads1256_init(const ads1256_config_t *cfg, ads1256_handle_t *out_handle)
{
    esp_err_t ret;

    // SPI BUS config
    spi_bus_config_t bus_config = {
        .mosi_io_num = cfg->mosi,
        .miso_io_num = cfg->miso,
        .sclk_io_num = cfg->sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = cfg->ads_transfer_size,
    };

    ret = spi_bus_initialize(cfg->spi_host, &bus_config, cfg->spi_dma);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        // ESP_ERR_INVALID_STATE - bus already initialized
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        //free(dev);
        return ret;
    }

    return ESP_OK;
}