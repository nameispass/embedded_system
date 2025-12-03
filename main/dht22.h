

#ifndef DHT22_H
#define DHT22_H

#include "config.h"

// DHT22 timing constants (microseconds)
#define DHT22_START_SIGNAL_MS   20      // Start signal duration (ms)
#define DHT22_RESPONSE_WAIT_US  40      // Wait for sensor response
#define DHT22_DATA_BITS         40      // Total bits to read

// DHT22 timing thresholds
#define DHT22_BIT_0_US          28      // Bit 0 pulse duration (~26-28us)
#define DHT22_BIT_1_US          70      // Bit 1 pulse duration (~70us)
#define DHT22_THRESHOLD_US      40      // Threshold to distinguish 0 and 1

// Function prototypes
esp_err_t dht22_init(void);
esp_err_t dht22_read(float *temperature, float *humidity);
bool dht22_is_valid_data(float temp, float hum);

#endif // DHT22_H
