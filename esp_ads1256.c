#include "esp_ads1256.h"

#define ESP_ARG_CHECK(VAL)                                                                                             \
    do {                                                                                                               \
        if (!(VAL))                                                                                                    \
            return ESP_ERR_INVALID_ARG;                                                                                \
    } while (0)

static const char *TAG = "ads1256";

/* ADS1256 timing constants — based on τCLKIN = 130.2ns @ 7.68MHz CLKIN */
#define ADS1256_T6_US       7 // 50 × τCLKIN = 6.51µs — after last DIN bit before first DOUT SCLK (RDATA/RREG)
#define ADS1256_T10_US      2 // 8 × τCLKIN = 1.04µs worst case
#define ADS1256_T11_SYNC_US 4 // 24 × τCLKIN = 3.12µs — delay after SYNC before WAKEUP

/* SPI clock: ADS1256 supports up to fCLKIN/4. At default 7.68 MHz CLKIN → 1.92 MHz. */
#define SPI_CLOCK_HZ 1920000

/**
 * @brief CS management (low)
 */
static inline void cs_low(ads1256_handle_t handle) { gpio_set_level(handle->dev_config.cs, 0); }

/**
 * @brief CS management (high) + t10 delay
 */
static inline void cs_high(ads1256_handle_t handle) {
    ets_delay_us(ADS1256_T10_US);
    gpio_set_level(handle->dev_config.cs, 1);
}

/**
 * @brief Raw SPI write
 */
static esp_err_t spi_write_bytes(ads1256_handle_t handle, const uint8_t *data, size_t len_bytes) {
    spi_transaction_t t = {
        .length    = len_bytes * 8,
        .tx_buffer = data,
    };

    ESP_RETURN_ON_ERROR(spi_device_polling_transmit(handle->spi_handle, &t), TAG, "SPI write failed");
    return ESP_OK;
}

/**
 * @brief Raw SPI read
 */
static esp_err_t spi_read_bytes(ads1256_handle_t handle, uint8_t *data, size_t len_bytes) {
    spi_transaction_t t = {
        .length    = len_bytes * 8,
        .rxlength  = len_bytes * 8,
        .rx_buffer = data,
    };

    ESP_RETURN_ON_ERROR(spi_device_polling_transmit(handle->spi_handle, &t), TAG, "SPI read failed");
    return ESP_OK;
}

/* Main Implementation */
esp_err_t ads1256_wait_drdy(ads1256_handle_t handle) {
    int64_t deadline_us = esp_timer_get_time() + (int64_t)handle->dev_config.drdy_timeout_ms * 1000;

    while (gpio_get_level(handle->dev_config.drdy) != 0) {
        if (esp_timer_get_time() > deadline_us) {
            ESP_LOGE(TAG, "DRDY timeout after %ldms", handle->dev_config.drdy_timeout_ms);
            return ESP_ERR_TIMEOUT;
        }
        taskYIELD();
    }
    return ESP_OK;
}

esp_err_t ads1256_send_cmd(ads1256_handle_t handle, uint8_t cmd) {
    esp_err_t ret = ESP_OK;

    cs_low(handle);
    ESP_GOTO_ON_ERROR(spi_write_bytes(handle, &cmd, 1), done, TAG, "Failed to send command 0x%02X", cmd);

done:
    cs_high(handle);
    return ret;
}

esp_err_t ads1256_read_reg(ads1256_handle_t handle, uint8_t reg, uint8_t *out_val) {
    esp_err_t ret   = ESP_OK;
    uint8_t   tx[2] = {ADS1256_CMD_RREG | (reg & 0x0F), // Byte 0: Read Register cmd : Register
                       0x00};                           // Byte 1: Read 1 byte
    cs_low(handle);
    ESP_GOTO_ON_ERROR(spi_write_bytes(handle, tx, 2), done, TAG, "Failed to write RREG command for reg 0x%02X", reg);
    ets_delay_us(ADS1256_T6_US); // t6: 50 × tCLKIN before clocking out DOUT
    ESP_GOTO_ON_ERROR(spi_read_bytes(handle, out_val, 1), done, TAG, "Failed to read register 0x%02X", reg);

done:
    cs_high(handle);
    return ret;
}

esp_err_t ads1256_write_reg(ads1256_handle_t handle, uint8_t reg, uint8_t val) {
    esp_err_t ret   = ESP_OK;
    uint8_t   tx[3] = {
        ADS1256_CMD_WREG | (reg & 0x0F), // Byte 0: Write Register cmd : Register
        0x00,                            // Byte 1: Write 1 byte
        val,                             // Byte 2: Write value
    };

    cs_low(handle);
    ESP_GOTO_ON_ERROR(spi_write_bytes(handle, tx, 3), done, TAG, "Failed to write register 0x%02X", reg);

done:
    cs_high(handle);
    return ret;
}

esp_err_t ads1256_set_channel(ads1256_handle_t handle, uint8_t pos, uint8_t neg) {
    uint8_t mux = ((pos & 0x0F) << 4) | (neg & 0x0F); // Combine channels into 1 byte
    ESP_RETURN_ON_ERROR(ads1256_write_reg(handle, ADS1256_REG_MUX, mux), TAG,
                        "Failed to set channel MUX (pos=0x%X neg=0x%X)", pos, neg);
    return ESP_OK;
}

esp_err_t ads1256_set_gain(ads1256_handle_t handle) {
    // ADCON:
    // CLKOUT = off (bits 4:3 = 00),
    // sensor detect = off (bits 6:5 = 00),
    // PGA = gain (bits 2:0)
    ESP_RETURN_ON_ERROR(ads1256_write_reg(handle, ADS1256_REG_ADCON, handle->dev_config.gain & 0x07), TAG,
                        "Failed to set gain");
    return ESP_OK;
}

esp_err_t ads1256_set_drate(ads1256_handle_t handle) {
    ESP_RETURN_ON_ERROR(ads1256_write_reg(handle, ADS1256_REG_DRATE, handle->dev_config.drate), TAG,
                        "Failed to set drate");
    return ESP_OK;
}

esp_err_t ads1256_start_conversion(ads1256_handle_t handle) {
    ESP_ARG_CHECK(handle);

    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_ERROR(spi_device_acquire_bus(handle->spi_handle, portMAX_DELAY), TAG,
                        "Failed to acquire bus for conversion");

    /* SYNC */
    uint8_t cmd = ADS1256_CMD_SYNC;
    cs_low(handle);
    ret = spi_write_bytes(handle, &cmd, 1);
    cs_high(handle); // always raised
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send SYNC");
        goto done;
    }

    ets_delay_us(ADS1256_T11_SYNC_US);

    /* WAKEUP */
    cmd = ADS1256_CMD_WAKEUP;
    cs_low(handle);
    ret = spi_write_bytes(handle, &cmd, 1);
    cs_high(handle); // always raised
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send WAKEUP");
    }

done:
    spi_device_release_bus(handle->spi_handle);
    return ret;
}

esp_err_t ads1256_read_result(ads1256_handle_t handle, int32_t *out_raw) {
    ESP_ARG_CHECK(handle);
    ESP_ARG_CHECK(out_raw);

    esp_err_t ret   = ESP_OK;
    uint8_t   rx[3] = {0};

    /* wait for conversion to complete — outside bus acquire, no SPI needed */
    /* DRDY CHECKING -> USER OWN IMPLEMENTATION */
    // ESP_RETURN_ON_ERROR(ads1256_wait_drdy(handle), TAG, "DRDY timeout before read");

    ESP_RETURN_ON_ERROR(spi_device_acquire_bus(handle->spi_handle, portMAX_DELAY), TAG,
                        "Failed to acquire bus for read");

    /* send RDATA command */
    uint8_t cmd = ADS1256_CMD_RDATA;
    cs_low(handle);
    ESP_GOTO_ON_ERROR(spi_write_bytes(handle, &cmd, 1), done, TAG, "Failed to send RDATA");

    /* t6: 50 × tCLKIN = 6.51µs — must wait before clocking out data */
    ets_delay_us(ADS1256_T6_US);

    /* read 24-bit result */
    ESP_GOTO_ON_ERROR(spi_read_bytes(handle, rx, 3), done, TAG, "Failed to read conversion result");

done:
    cs_high(handle);
    spi_device_release_bus(handle->spi_handle);

    if (ret != ESP_OK)
        return ret;

    /* combine bytes and sign-extend from 24-bit to 32-bit */
    int32_t raw = ((int32_t)rx[0] << 16) | ((int32_t)rx[1] << 8) | rx[2];
    if (raw & 0x800000)
        raw |= 0xFF000000;
    *out_raw = raw;

    return ESP_OK;
}

esp_err_t ads1256_init(const ads1256_config_t *ads1256_config, ads1256_handle_t *ads1256_handle) {
    ESP_ARG_CHECK(ads1256_config);
    esp_err_t ret = ESP_FAIL;

    /* validate memory allocation */
    ads1256_handle_t out_handle = (ads1256_handle_t)calloc(1, sizeof(*out_handle));
    ESP_RETURN_ON_FALSE(out_handle, ESP_ERR_NO_MEM, TAG, "No memory for ADS1256 device");

    /* copy device config to out_handle */
    out_handle->dev_config = *ads1256_config;

    /* set SPI device configuration */

    // VERIFICAR CONFIGURAÇÃO DO DISPOSITIVO --------------------------------------------------------
    /* configure CS pin as output, idle high */
    gpio_config_t cs_conf = {
        .pin_bit_mask = 1ULL << ads1256_config->cs,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_GOTO_ON_ERROR(gpio_config(&cs_conf), err_handle, TAG, "Failed to configure CS pin");
    gpio_set_level(ads1256_config->cs, 1); // idle high

    /* configure DRDY pin as input with pull-up (active low signal) */
    gpio_config_t drdy_conf = {
        .pin_bit_mask = 1ULL << ads1256_config->drdy,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    ESP_GOTO_ON_ERROR(gpio_config(&drdy_conf), err_handle, TAG, "Failed to configure DRDY pin");

    /* SPI device configuration */
    const spi_device_interface_config_t ads_dev_config = {
        .command_bits     = 0,   // raw data
        .address_bits     = 0,   // raw data
        .dummy_bits       = 0,   // no extra delay
        .mode             = 1,   // CPOL=0 (idle low), CPHA=1 (sample on falling edge)
        .duty_cycle_pos   = 128, // 50% duty cycle
        .cs_ena_pretrans  = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz   = SPI_CLOCK_HZ,
        .input_delay_ns   = 0,
        .spics_io_num     = -1, // CS managed manually with delays
        .queue_size       = 1,  // queue not used
    };
    // VERIFICAR CONFIGURAÇÃO DO DISPOSITIVO --------------------------------------------------------

    /* add ADS1256 to SPI bus */
    ESP_GOTO_ON_ERROR(spi_bus_add_device(ads1256_config->spi_host, &ads_dev_config, &out_handle->spi_handle),
                      err_handle, TAG, "Failed to add ADS1256 device to SPI bus");

    /* ADS1256 software reset */
    ESP_GOTO_ON_ERROR(ads1256_send_cmd(out_handle, ADS1256_CMD_RESET), err_handle, TAG, "Failed to soft-reset ADS1256");

    /* wait for ADS1256 to be ready */
    ESP_GOTO_ON_ERROR(ads1256_wait_drdy(out_handle), err_handle, TAG, "Failed to get DRDY");

    // VERIFICAR STATUS --------------------------------------------------------
    /* STATUS register: ORDER=0 (MSB first), ACAL=0 (manual cal), BUFEN=config
     * bits 7:4 = read-only chip ID, bit 0 = read-only DRDY                    */
    uint8_t status = ads1256_config->bufen ? 0x02 : 0x00;
    ESP_GOTO_ON_ERROR(ads1256_write_reg(out_handle, ADS1256_REG_STATUS, status), err_handle, TAG,
                      "Failed to set STATUS register");
    // VERIFICAR STATUS --------------------------------------------------------

    /* set channels after software reset */
    ESP_GOTO_ON_ERROR(ads1256_set_channel(out_handle, ads1256_config->pos_channel, ads1256_config->neg_channel),
                      err_handle, TAG, "Failed to set default channel");

    /* set GAIN */
    ESP_GOTO_ON_ERROR(ads1256_set_gain(out_handle), err_handle, TAG, "Failed to set GAIN");

    /* set DRATE */
    ESP_GOTO_ON_ERROR(ads1256_set_drate(out_handle), err_handle, TAG, "Failed to set DRATE");

    /* run SELF-CALIBRATION */
    ESP_GOTO_ON_ERROR(ads1256_send_cmd(out_handle, ADS1256_CMD_SELFCAL), err_handle, TAG, "Failed self calibration");

    /* wait for ADS1256 to be ready */
    ESP_GOTO_ON_ERROR(ads1256_wait_drdy(out_handle), err_handle, TAG, "Failed to get DRDY");

    ESP_LOGI(TAG, "ADS1256 initialized (gain=%d drate=0x%02X bufen=%d)", ads1256_config->gain, ads1256_config->drate,
             ads1256_config->bufen);

    *ads1256_handle = out_handle;
    return ESP_OK;

err_handle:
    if (out_handle->spi_handle)
        spi_bus_remove_device(out_handle->spi_handle);
    free(out_handle);
    return ret;
}

esp_err_t ads1256_delete(ads1256_handle_t handle) {
    ESP_ARG_CHECK(handle);

    if (handle->spi_handle) {
        ESP_RETURN_ON_ERROR(spi_bus_remove_device(handle->spi_handle), TAG, "Failed to remove SPI device");
    }
    free(handle);
    return ESP_OK;
}