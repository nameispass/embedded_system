/**
 * @file webserver.h
 * @brief HTTP Webserver Module - REST API for Temperature Monitoring System
 * @features GET /api/sensor, POST /api/config, GET /api/history, GET /web
 */

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_http_server.h"
#include "config.h"

// ==================== WEBSERVER CONFIGURATION ====================

#define SERVER_PORT             80
#define MAX_HTTP_REQ_HDR_LEN    512
#define MAX_HISTORY_RECORDS     100

// ==================== DATA STRUCTURES ====================

/**
 * @brief Thông tin cấu hình hệ thống
 */
typedef struct {
    float temp_warning;      // Ngưỡng cảnh báo
    float temp_overheat;     // Ngưỡng quá nhiệt
    uint32_t sensor_interval_ms;  // Khoảng thời gian đọc cảm biến
    bool buzzer_enabled;     // Bật/tắt buzzer
} system_config_t;

/**
 * @brief Lịch sử dữ liệu sensor
 */
typedef struct {
    sensor_data_t data;
    system_state_t state;
} history_record_t;

// ==================== FUNCTION PROTOTYPES ====================

/**
 * @brief Khởi tạo HTTP Server
 * @return ESP_OK nếu thành công
 */
esp_err_t webserver_init(void);

/**
 * @brief Dừng HTTP Server
 * @return ESP_OK nếu thành công
 */
esp_err_t webserver_stop(void);

/**
 * @brief Cập nhật dữ liệu sensor cho webserver (được gọi từ sensor_task)
 */
void webserver_update_sensor_data(const sensor_data_t *data, system_state_t state);

/**
 * @brief Cập nhật cấu hình hệ thống
 */
void webserver_update_config(const system_config_t *config);

/**
 * @brief Lấy cấu hình hiện tại
 */
system_config_t webserver_get_config(void);

/**
 * @brief Lấy số lượng bản ghi lịch sử
 */
uint32_t webserver_get_history_count(void);

/**
 * @brief Lấy bản ghi lịch sử theo index
 */
history_record_t webserver_get_history(uint32_t index);

// ==================== WEBSERVER STATE MANAGEMENT ====================

/**
 * @brief Khóa để bảo vệ dữ liệu webserver
 */
extern SemaphoreHandle_t webserver_data_mutex;

#endif // WEBSERVER_H
