#ifndef ESP_ADS1256_H
#define ESP_ADS1256_H

#include <stdint.h>

#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_err.h>
#include <esp_check.h>
#include <esp_log.h>

#include <driver/spi_master.h>
#include <driver/gpio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

// ─── Register Addresses ───────────────────────────────────────────────────────
#define ADS1256_REG_STATUS 0x00
#define ADS1256_REG_MUX 0x01
#define ADS1256_REG_ADCON 0x02
#define ADS1256_REG_DRATE 0x03
#define ADS1256_REG_IO 0x04
#define ADS1256_REG_OFC0 0x05
#define ADS1256_REG_OFC1 0x06
#define ADS1256_REG_OFC2 0x07
#define ADS1256_REG_FSC0 0x08
#define ADS1256_REG_FSC1 0x09
#define ADS1256_REG_FSC2 0x0A

// ─── Commands ─────────────────────────────────────────────────────────────────
#define ADS1256_CMD_WAKEUP 0x00
#define ADS1256_CMD_RDATA 0x01
#define ADS1256_CMD_RDATAC 0x03
#define ADS1256_CMD_SDATAC 0x0F
#define ADS1256_CMD_RREG 0x10 // | reg_addr
#define ADS1256_CMD_WREG 0x50 // | reg_addr
#define ADS1256_CMD_SELFCAL 0xF0
#define ADS1256_CMD_SELFOCAL 0xF1
#define ADS1256_CMD_SELFGCAL 0xF2
#define ADS1256_CMD_SYSOCAL 0xF3
#define ADS1256_CMD_SYSGCAL 0xF4
#define ADS1256_CMD_SYNC 0xFC
#define ADS1256_CMD_STANDBY 0xFD
#define ADS1256_CMD_RESET 0xFE

// ─── MUX channel definitions ──────────────────────────────────────────────────
// Usage: mux = (ADS1256_MUX_AIN0 << 4) | ADS1256_MUX_AINCOM  → single-ended AIN0
#define ADS1256_MUX_AIN0 0x0
#define ADS1256_MUX_AIN1 0x1
#define ADS1256_MUX_AIN2 0x2
#define ADS1256_MUX_AIN3 0x3
#define ADS1256_MUX_AIN4 0x4
#define ADS1256_MUX_AIN5 0x5
#define ADS1256_MUX_AIN6 0x6
#define ADS1256_MUX_AIN7 0x7
#define ADS1256_MUX_AINCOM 0x8

// ─── PGA gain options (ADCON register bits 2:0) ───────────────────────────────
typedef enum
{
    ADS1256_GAIN_1 = 0,
    ADS1256_GAIN_2 = 1,
    ADS1256_GAIN_4 = 2,
    ADS1256_GAIN_8 = 3,
    ADS1256_GAIN_16 = 4,
    ADS1256_GAIN_32 = 5,
    ADS1256_GAIN_64 = 6,
} ads1256_gain_t;

// ─── Data rate options (DRATE register) ──────────────────────────────────────
typedef enum
{
    ADS1256_DRATE_30000SPS = 0xF0,
    ADS1256_DRATE_15000SPS = 0xE0,
    ADS1256_DRATE_7500SPS = 0xD0,
    ADS1256_DRATE_3750SPS = 0xC0,
    ADS1256_DRATE_2000SPS = 0xB0,
    ADS1256_DRATE_1000SPS = 0xA1,
    ADS1256_DRATE_500SPS = 0x92,
    ADS1256_DRATE_100SPS = 0x82,
    ADS1256_DRATE_60SPS = 0x72,
    ADS1256_DRATE_50SPS = 0x63,
    ADS1256_DRATE_30SPS = 0x53,
    ADS1256_DRATE_25SPS = 0x43,
    ADS1256_DRATE_15SPS = 0x33,
    ADS1256_DRATE_10SPS = 0x23,
    ADS1256_DRATE_5SPS = 0x13,
    ADS1256_DRATE_2_5SPS = 0x03,
} ads1256_drate_t;

// ─── Configuration struct ─────────────────────────────────────────────────────
typedef struct
{
    spi_host_device_t spi_host; // e.g. SPI2_HOST
    spi_common_dma_t spi_dma;
    gpio_num_t miso;
    gpio_num_t mosi;
    gpio_num_t sclk;
    gpio_num_t cs;
    gpio_num_t drdy;  // Data Ready (active low)
    gpio_num_t reset; // Hardware reset (active low), or GPIO_NUM_NC to skip
    ads1256_gain_t gain;
    ads1256_drate_t drate;
    int16_t drdy_timeout_ms;
    int16_t ads_transfer_size;
} ads1256_config_t;

// ─── Device handle ────────────────────────────────────────────────────────────
typedef struct ads1256_dev_t *ads1256_handle_t;

esp_err_t ads1256_init(const ads1256_config_t *cfg, ads1256_handle_t *out_handle);

#endif // ESP_ADS1256_H