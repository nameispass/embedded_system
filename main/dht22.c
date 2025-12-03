

#include "dht22.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"
#include <math.h>

static const char *TAG = TAG_SENSOR;
static gpio_num_t dht_pin = DHT_PIN;

/**
 * @brief Đợi cho đến khi GPIO đạt trạng thái mong muốn hoặc timeout
 */
static int wait_for_state(uint8_t state, uint32_t timeout_us) {
    int elapsed = 0;
    while (gpio_get_level(dht_pin) != state) {
        if (elapsed > timeout_us) {
            return -1;
        }
        ets_delay_us(1);
        elapsed++;
    }
    return elapsed;
}

/**
 * @brief Khởi tạo DHT22
 */
esp_err_t dht22_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << dht_pin),
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DHT22 GPIO config failed");
        return ret;
    }
    
    gpio_set_level(dht_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "DHT22 initialized on GPIO %d", dht_pin);
    return ESP_OK;
}

/**
 * @brief Kiểm tra dữ liệu hợp lệ
 */
bool dht22_is_valid_data(float temp, float hum) {
    // Kiểm tra NaN
    if (isnan(temp) || isnan(hum)) {
        return false;
    }
    
    // Kiểm tra phạm vi DHT22: -40 to 80°C, 0 to 100%
    if (temp < -40.0f || temp > 80.0f) {
        return false;
    }
    
    if (hum < 0.0f || hum > 100.0f) {
        return false;
    }
    
    return true;
}

/**
 * @brief Đọc dữ liệu từ DHT22
 */
esp_err_t dht22_read(float *temperature, float *humidity) {
    uint8_t data[5] = {0};
    
    // Send start signal
    gpio_set_direction(dht_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(dht_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(DHT22_START_SIGNAL_MS));
    
    gpio_set_level(dht_pin, 1);
    ets_delay_us(30);
    
    // Switch to input mode
    gpio_set_direction(dht_pin, GPIO_MODE_INPUT);
    
    // Wait for sensor response
    if (wait_for_state(0, 100) < 0) {
        ESP_LOGW(TAG, "DHT22 no response (1)");
        gpio_set_direction(dht_pin, GPIO_MODE_OUTPUT_OD);
        gpio_set_level(dht_pin, 1);
        return ESP_ERR_TIMEOUT;
    }
    
    if (wait_for_state(1, 100) < 0) {
        ESP_LOGW(TAG, "DHT22 no response (2)");
        gpio_set_direction(dht_pin, GPIO_MODE_OUTPUT_OD);
        gpio_set_level(dht_pin, 1);
        return ESP_ERR_TIMEOUT;
    }
    
    if (wait_for_state(0, 100) < 0) {
        ESP_LOGW(TAG, "DHT22 no response (3)");
        gpio_set_direction(dht_pin, GPIO_MODE_OUTPUT_OD);
        gpio_set_level(dht_pin, 1);
        return ESP_ERR_TIMEOUT;
    }
    
    // Read 40 bits (5 bytes)
    for (int i = 0; i < 40; i++) {
        // Wait for start of bit
        if (wait_for_state(1, 100) < 0) {
            ESP_LOGW(TAG, "DHT22 timeout waiting for bit %d start", i);
            gpio_set_direction(dht_pin, GPIO_MODE_OUTPUT_OD);
            gpio_set_level(dht_pin, 1);
            return ESP_ERR_TIMEOUT;
        }
        
        // Measure high pulse duration
        int duration = wait_for_state(0, 100);
        if (duration < 0) {
            ESP_LOGW(TAG, "DHT22 timeout reading bit %d", i);
            gpio_set_direction(dht_pin, GPIO_MODE_OUTPUT_OD);
            gpio_set_level(dht_pin, 1);
            return ESP_ERR_TIMEOUT;
        }
        
        // Determine bit value
        int byte_idx = i / 8;
        int bit_idx = 7 - (i % 8);
        
        if (duration > DHT22_THRESHOLD_US) {
            data[byte_idx] |= (1 << bit_idx);  // Bit 1
        }
        // else: Bit 0 (already 0)
    }
    
    // Return to output mode
    gpio_set_direction(dht_pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(dht_pin, 1);
    
    // Verify checksum
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        ESP_LOGW(TAG, "DHT22 checksum error: calculated=%02X, received=%02X", 
                 checksum, data[4]);
        return ESP_ERR_INVALID_CRC;
    }
    
    // Convert to temperature and humidity
    uint16_t hum_raw = (data[0] << 8) | data[1];
    uint16_t temp_raw = (data[2] << 8) | data[3];
    
    *humidity = hum_raw / 10.0f;
    
    if (temp_raw & 0x8000) {
        // Negative temperature
        temp_raw &= 0x7FFF;
        *temperature = -(temp_raw / 10.0f);
    } else {
        *temperature = temp_raw / 10.0f;
    }
    
    // Validate data
    if (!dht22_is_valid_data(*temperature, *humidity)) {
        ESP_LOGW(TAG, "DHT22 invalid data: T=%.1f, H=%.1f", *temperature, *humidity);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    ESP_LOGD(TAG, "DHT22 read: T=%.1f°C, H=%.1f%%", *temperature, *humidity);
    
    return ESP_OK;
}
