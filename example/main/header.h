#ifndef HEADER_H
#define HEADER_H

#include "esp_ads1256.h"
#include <stdio.h>

#define SPI_HOST            SPI2_HOST
#define DMA_CHAN            SPI_DMA_CH_AUTO
#define EXAMPLE_CS          GPIO_NUM_39
#define EXAMPLE_DRDY        GPIO_NUM_21
#define ADS_DRDY_TIMEOUT_MS 1000

// DATA STRUCTURES - SENSORS
typedef struct {
    int32_t raw;
} ads1256_sample_t;

typedef struct {
    uint32_t timestamp;

    ads1256_sample_t ads1256;
} data_t;

// DATA STRUCTURES - STORAGE
typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    int32_t  ads1256;
} save_t;

typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    int32_t  ads1256;
} send_t;

extern ads1256_sample_t ads1256_sample_g;

// MUTEXES
extern SemaphoreHandle_t xADSMutex;

// TASKS
void ads_task(void *pvParameters);

#endif