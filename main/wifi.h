/**
 * @file wifi.h
 * @brief WiFi Module - Kết nối WiFi cho ESP32
 */

#ifndef WIFI_H
#define WIFI_H

#include "esp_wifi.h"
#include "esp_event.h"

// ==================== FUNCTION PROTOTYPES ====================

/**
 * @brief Khởi tạo và kết nối WiFi
 * @return ESP_OK nếu thành công
 */
esp_err_t wifi_init_sta(void);

/**
 * @brief Lấy trạng thái kết nối WiFi
 * @return true nếu đã kết nối, false nếu chưa
 */
bool wifi_is_connected(void);

/**
 * @brief Lấy IP address hiện tại
 * @return Con trỏ đến chuỗi IP (hoặc "0.0.0.0" nếu chưa kết nối)
 */
const char* wifi_get_ip_address(void);

/**
 * @brief Dừng WiFi
 */
esp_err_t wifi_stop(void);

#endif // WIFI_H
