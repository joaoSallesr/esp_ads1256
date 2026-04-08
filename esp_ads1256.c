#include "esp_ads1256.h"

#define ESP_ARG_CHECK(VAL)                                                                                             \
    do {                                                                                                               \
        if (!(VAL))                                                                                                    \
            return ESP_ERR_INVALID_ARG;                                                                                \
    } while (0)

static const char *TAG = "ads1256";

/* ADS1256 timing constants */
// VERIFICAR ESSAS CONSTANTES NO DATASHEET -----------------------------------------------------------
#define T6_US      7 // CS↓ to first SCLK (min 50 ns, use  1µs to be safe)
#define T11_US     4 // Last SCLK to CS↑ (min t11 = 4 × tCLKIN ≈ 1.6 µs @ 7.68 MHz)
#define TSETTLE_US 4 // After SYNC+WAKEUP, wait 4 × tCLKIN before reading

/* SPI clock: ADS1256 supports up to fCLKIN/4. At default 7.68 MHz CLKIN → 1.92 MHz. */
// DEFINIR SPI CLOCK HZ ------------------------------------------------------------------------------
#define SPI_CLOCK_HZ 1920000

/**
 * @brief CS management (low)
 */
static inline void cs_low(ads1256_handle_t handle) {
    gpio_set_level(handle->dev_config.cs, 0);
    ets_delay_us(T6_US);
}

/**
 * @brief CS management (high)
 */
static inline void cs_high(ads1256_handle_t handle) {
    ets_delay_us(T11_US);
    gpio_set_level(handle->dev_config.cs, 1);
}

/**
 * @brief Raw SPI write
 */
static esp_err_t spi_write_bytes(ads1256_handle_t handle, const uint8_t *data, size_t len) {
    spi_transaction_t t = {
        .length    = len * 8,
        .tx_buffer = data,
    };

    return spi_device_polling_transmit(handle->spi_handle, &t);
}

/**
 * @brief Raw SPI read
 */
static esp_err_t spi_read_bytes(ads1256_handle_t handle, uint8_t *data, size_t len) {
    spi_transaction_t t = {
        .length    = len * 8,
        .rxlength  = len * 8,
        .rx_buffer = data,
    };

    return spi_device_polling_transmit(handle->spi_handle, &t);
}

/* Main Implementation */

esp_err_t ads1256_wait_drdy(ads1256_handle_t handle) {
    int64_t deadline_us = esp_timer_get_time() + (int64_t)handle->dev_config.drdy_timeout_ms * 1000;
    while (gpio_get_level(handle->dev_config.drdy) != 0) {
        if (esp_timer_get_time() > deadline_us) {
            ESP_LOGE(TAG, "DRDY timeout");
            return ESP_ERR_TIMEOUT;
        }
        taskYIELD();
    }
    return ESP_OK;
}

esp_err_t ads1256_read_reg(ads1256_handle_t handle, uint8_t reg, uint8_t *out_val) {
    esp_err_t ret   = ESP_OK;
    uint8_t   tx[2] = {ADS1256_CMD_RREG | (reg & 0x0F), 0x00}; // cmd, count-1

    cs_low(handle);
    ESP_GOTO_ON_ERROR(spi_write_bytes(handle, tx, 2), done, TAG, "Failed to write RREG command");
    ets_delay_us(T6_US); // t6: min 50 × tCLKIN ≈ 6.5 µs
    ESP_GOTO_ON_ERROR(spi_read_bytes(handle, out_val, 1), done, TAG, "Failed to read register");

done:
    cs_high(handle);
    return ret;
}

esp_err_t ads1256_write_reg(ads1256_handle_t handle, uint8_t reg, uint8_t val) {
    uint8_t tx[3] = {
        ADS1256_CMD_WREG | (reg & 0x0F), // command
        0x00,                            // count - 1 (writing 1 register)
        val,
    };

    cs_low(handle);
    esp_err_t ret = spi_write_bytes(handle, tx, 3);
    cs_high(handle);
    return ret;
}

esp_err_t ads1256_send_cmd(ads1256_handle_t handle, uint8_t cmd) {
    cs_low(handle);
    esp_err_t ret = spi_write_bytes(handle, &cmd, 1);
    cs_high(handle);
    return ret;
}

esp_err_t ads1256_set_channel(ads1256_handle_t handle, uint8_t pos, uint8_t neg) {
    uint8_t mux = ((pos & 0x0F) << 4) | (neg & 0x0F);
    return ads1256_write_reg(handle, ADS1256_REG_MUX, mux);
}

esp_err_t ads1256_set_gain(ads1256_handle_t handle) {
    return ads1256_write_reg(handle, ADS1256_REG_ADCON, handle->dev_config.gain);
}

esp_err_t ads1256_set_drate(ads1256_handle_t handle) {
    return ads1256_write_reg(handle, ADS1256_REG_DRATE, handle->dev_config.drate);
}

esp_err_t ads1256_init(const ads1256_config_t *ads1256_config, ads1256_handle_t *ads1256_handle) {
    ESP_ARG_CHECK(ads1256_config);
    esp_err_t ret = ESP_FAIL;

    /* validate memory allocation */
    ads1256_handle_t out_handle;
    out_handle = (ads1256_handle_t)calloc(1, sizeof(*out_handle));
    ESP_RETURN_ON_FALSE(out_handle, ESP_ERR_NO_MEM, TAG, "No memory for ADS1256  device");

    /* copy device config to out_handle */
    out_handle->dev_config = *ads1256_config;

    /* set SPI device configuration */

    // VERIFICAR CONFIGURAÇÃO SPI DO DISPOSITIVO --------------------------------------------------------

    const spi_device_interface_config_t ads_dev_config = {
        .command_bits     = 0,
        .address_bits     = 0,
        .dummy_bits       = 0,
        .mode             = 1, // SPI mode 1 for ADS1256
        .duty_cycle_pos   = 128,
        .cs_ena_pretrans  = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz   = 1920000, // 1.92 MHz
        .input_delay_ns   = 0,
        .spics_io_num     = -1, // disable automatic CS management
        .queue_size       = 1,
    };

    // VERIFICAR CONFIGURAÇÃO SPI DO DISPOSITIVO --------------------------------------------------------

    /* add ADS1256 to SPI bus */
    ESP_GOTO_ON_ERROR(spi_bus_add_device(ads1256_config->spi_host, &ads_dev_config, &out_handle->spi_handle),
                      err_handle, TAG, "Failed to add ADS1256 device to SPI bus");

    /* ADS1256 software reset */
    ESP_GOTO_ON_ERROR(ads1256_send_cmd(out_handle, ADS1256_CMD_RESET), err_handle, TAG, "Failed to soft-reset ADS1256");

    /* short delay after software reset */
    vTaskDelay(pdMS_TO_TICKS(1));

    /* wait for ADS1256 to be ready */
    ESP_GOTO_ON_ERROR(ads1256_wait_drdy(out_handle), err_handle, TAG, "Failed to get DRDY");

    // VERIFICAR STATUS e CHANNELS --------------------------------------------------------

    /* set STATUS register after software reset */
    uint8_t bufen_status = ads1256_config->bufen ? 0x02 : 0x00;
    ESP_GOTO_ON_ERROR(ads1256_write_reg(out_handle, ADS1256_REG_STATUS, bufen_status), err_handle, TAG,
                      "Failed to set STATUS register");

    /* set channels after software reset */
    ESP_GOTO_ON_ERROR(ads1256_set_channel(out_handle, ADS1256_MUX_AIN0, ADS1256_MUX_AINCOM), err_handle, TAG,
                      "Failed to set default channel");

    // VERIFICAR STATUS e CHANNELS --------------------------------------------------------

    /* set GAIN */
    ESP_GOTO_ON_ERROR(ads1256_set_gain(out_handle), err_handle, TAG, "Failed to set GAIN");

    /* set DRATE */
    ESP_GOTO_ON_ERROR(ads1256_set_drate(out_handle), err_handle, TAG, "Failed to set DRATE");

    /* run SELF-CALIBRATION */
    ESP_GOTO_ON_ERROR(ads1256_send_cmd(out_handle, ADS1256_CMD_SELFCAL), err_handle, TAG, "Failed self calibration");

    /* wait for ADS1256 to be ready */
    ESP_GOTO_ON_ERROR(ads1256_wait_drdy(out_handle), err_handle, TAG, "Failed to get DRDY");

    ESP_LOGI(TAG, "ADS1256 initialized");
    *ads1256_handle = out_handle;
    return ESP_OK;

err_handle:
    if (out_handle && out_handle->spi_handle)
        spi_bus_remove_device(out_handle->spi_handle);
    free(out_handle);
    return ret;
}