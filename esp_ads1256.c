#include "esp_ads1256.h"

#define ESP_ARG_CHECK(VAL)              \
    do                                  \
    {                                   \
        if (!(VAL))                     \
            return ESP_ERR_INVALID_ARG; \
    } while (0)

static const char *TAG = "ads1256";

// Temporary stubs - implement later
esp_err_t ads1256_send_cmd(ads1256_handle_t handle, uint8_t cmd)
{
    // TODO: implement
    return ESP_OK;
}

esp_err_t ads1256_wait_drdy(ads1256_handle_t handle)
{
    // TODO: implement
    return ESP_OK;
}

esp_err_t ads1256_write_reg(ads1256_handle_t handle, uint8_t reg, uint8_t val)
{
    // TODO: implement
    return ESP_OK;
}

esp_err_t ads1256_set_channel(ads1256_handle_t handle, uint8_t pos, uint8_t neg)
{
    // TODO: implement
    return ESP_OK;
}

esp_err_t ads1256_set_gain(ads1256_handle_t handle)
{
    // TODO: implement
    return ESP_OK;
}

esp_err_t ads1256_set_drate(ads1256_handle_t handle)
{
    // TODO: implement
    return ESP_OK;
}

esp_err_t ads1256_init(const ads1256_config_t *ads1256_config, ads1256_handle_t *ads1256_handle)
{
    ESP_ARG_CHECK(ads1256_config);
    esp_err_t ret = ESP_FAIL;

    /* validate memory allocation */
    ads1256_handle_t dev_handle;
    dev_handle = (ads1256_handle_t)calloc(1, sizeof(*dev_handle));
    ESP_RETURN_ON_FALSE(dev_handle, ESP_ERR_NO_MEM,
                        TAG, "No memory for ADS1256  device");

    /* copy device config to dev_handle */
    dev_handle->dev_config = *ads1256_config;

    /* set SPI device configuration */

    // VERIFICAR CONFIGURAÇÃO SPI DO DISPOSITIVO --------------------------------------------------------

    const spi_device_interface_config_t ads_dev_config = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 1, // SPI mode 1 for ADS1256
        .duty_cycle_pos = 128,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = 1920000, // 1.92 MHz
        .input_delay_ns = 0,
        .spics_io_num = ads1256_config->cs,
        .queue_size = 1,
    };

    // VERIFICAR CONFIGURAÇÃO SPI DO DISPOSITIVO --------------------------------------------------------

    /* add ADS1256 to SPI bus */
    ESP_GOTO_ON_ERROR(spi_bus_add_device(ads1256_config->spi_host, &ads_dev_config, &dev_handle->spi_handle),
                      err_handle, TAG, "Failed to add ADS1256 device to SPI bus");

    /* ADS1256 software reset */
    ESP_GOTO_ON_ERROR(ads1256_send_cmd(dev_handle, ADS1256_CMD_RESET),
                      err_handle, TAG, "Failed to soft-reset ADS1256");

    /* short delay after software reset */
    vTaskDelay(pdMS_TO_TICKS(1));

    /* wait for ADS1256 to be ready */
    ESP_GOTO_ON_ERROR(ads1256_wait_drdy(dev_handle),
                      err_handle, TAG, "Failed to get DRDY");

    // VERIFICAR STATUS e CHANNELS --------------------------------------------------------

    /* set STATUS register after software reset */
    uint8_t bufen_status = ads1256_config->bufen ? 0x02 : 0x00;
    ESP_GOTO_ON_ERROR(ads1256_write_reg(dev_handle, ADS1256_REG_STATUS, bufen_status),
                      err_handle, TAG, "Failed to set STATUS register");

    /* set channels after software reset */
    ESP_GOTO_ON_ERROR(ads1256_set_channel(dev_handle, ADS1256_MUX_AIN0, ADS1256_MUX_AINCOM),
                      err_handle, TAG, "Failed to set default channel");

    // VERIFICAR STATUS e CHANNELS --------------------------------------------------------

    /* set GAIN */
    ESP_GOTO_ON_ERROR(ads1256_set_gain(dev_handle),
                      err_handle, TAG, "Failed to set GAIN");

    /* set DRATE */
    ESP_GOTO_ON_ERROR(ads1256_set_drate(dev_handle),
                      err_handle, TAG, "Failed to set DRATE");

    /* run SELF-CALIBRATION */
    ESP_GOTO_ON_ERROR(ads1256_send_cmd(dev_handle, ADS1256_CMD_SELFCAL),
                      err_handle, TAG, "Failed self calibration");

    /* wait for ADS1256 to be ready */
    ESP_GOTO_ON_ERROR(ads1256_wait_drdy(dev_handle),
                      err_handle, TAG, "Failed to get DRDY");

    ESP_LOGI(TAG, "ADS1256 initialized");
    *ads1256_handle = dev_handle;
    return ESP_OK;

err_handle:
    if (dev_handle && dev_handle->spi_handle)
        spi_bus_remove_device(dev_handle->spi_handle);
    free(dev_handle);
    return ret;
}