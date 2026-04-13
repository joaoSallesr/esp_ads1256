#include "header.h"

spi_device_handle_t spi_bus_handle = NULL;

// MUTEXES
SemaphoreHandle_t xADSMutex = NULL;

// DATA SETS
ads1256_sample_t ads1256_sample_g = {0};