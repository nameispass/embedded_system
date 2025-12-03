/**
 * @file webserver.c
 * @brief HTTP Webserver Module Implementation (No external JSON library)
 * Cung cấp REST API endpoints cho hệ thống giám sát nhiệt độ
 */

#include "webserver.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

static const char *TAG = "WEBSERVER";

// Forward declaration
const char* get_state_string(system_state_t state);

// ==================== GLOBAL STATE ====================

static httpd_handle_t server = NULL;
SemaphoreHandle_t webserver_data_mutex = NULL;

static sensor_data_t current_sensor_data = {0};
static system_state_t current_system_state = STATE_NORMAL;
static system_config_t current_config = {
    .temp_warning = TEMP_WARNING,
    .temp_overheat = TEMP_OVERHEAT,
    .sensor_interval_ms = SENSOR_READ_PERIOD_MS,
    .buzzer_enabled = true
};

// Lịch sử dữ liệu (vòng tròn)
static history_record_t history[MAX_HISTORY_RECORDS];
static uint32_t history_count = 0;
static uint32_t history_index = 0;

// ==================== HELPER FUNCTIONS ====================

/**
 * @brief Thêm bản ghi vào lịch sử
 */
static void add_to_history(const sensor_data_t *data, system_state_t state) {
    if (xSemaphoreTake(webserver_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        history[history_index].data = *data;
        history[history_index].state = state;
        
        history_index = (history_index + 1) % MAX_HISTORY_RECORDS;
        if (history_count < MAX_HISTORY_RECORDS) {
            history_count++;
        }
        
        xSemaphoreGive(webserver_data_mutex);
    }
}

/**
 * @brief Tạo JSON string cho dữ liệu sensor (không dùng cJSON)
 */
static char* create_sensor_json(void) {
    static char json_buffer[512];
    
    if (xSemaphoreTake(webserver_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        snprintf(json_buffer, sizeof(json_buffer),
            "{\"temperature\":%.1f,\"humidity\":%.1f,\"status\":\"%s\",\"is_valid\":%s,\"timestamp\":%lld}",
            current_sensor_data.temperature,
            current_sensor_data.humidity,
            get_state_string(current_system_state),
            current_sensor_data.is_valid ? "true" : "false",
            current_sensor_data.timestamp
        );
        
        ESP_LOGD(TAG, "Sensor JSON: %s", json_buffer);
        xSemaphoreGive(webserver_data_mutex);
    } else {
        // Fallback nếu mutex timeout
        snprintf(json_buffer, sizeof(json_buffer),
            "{\"temperature\":0,\"humidity\":0,\"status\":\"UNKNOWN\",\"is_valid\":false,\"timestamp\":0}");
        ESP_LOGW(TAG, "Mutex timeout, returning fallback JSON");
    }
    
    return json_buffer;
}

/**
 * @brief Tạo JSON string cho cấu hình hệ thống
 */
static char* create_config_json(void) {
    static char json_buffer[256];
    
    if (xSemaphoreTake(webserver_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        snprintf(json_buffer, sizeof(json_buffer),
            "{\"temp_warning\":%.1f,\"temp_overheat\":%.1f,\"sensor_interval_ms\":%" PRIu32 ",\"buzzer_enabled\":%s}",
            current_config.temp_warning,
            current_config.temp_overheat,
            current_config.sensor_interval_ms,
            current_config.buzzer_enabled ? "true" : "false"
        );
        
        xSemaphoreGive(webserver_data_mutex);
    }
    
    return json_buffer;
}

// ==================== PARSE FUNCTION ====================

/**
 * @brief Parse JSON POST data (simple parser)
 */
static void parse_config_from_post(const char *data, int len) {
    // Simple JSON parsing (không dùng cJSON)
    char temp_warning_str[16] = {0};
    char temp_overheat_str[16] = {0};
    char buzzer_str[16] = {0};
    
    // Parse: "temp_warning":32.0
    const char *ptr = strstr(data, "\"temp_warning\"");
    if (ptr) {
        ptr = strchr(ptr, ':');
        if (ptr) sscanf(ptr + 1, "%15s", temp_warning_str);
    }
    
    // Parse: "temp_overheat":40.0
    ptr = strstr(data, "\"temp_overheat\"");
    if (ptr) {
        ptr = strchr(ptr, ':');
        if (ptr) sscanf(ptr + 1, "%15s", temp_overheat_str);
    }
    
    // Parse: "buzzer_enabled":true/false
    ptr = strstr(data, "\"buzzer_enabled\"");
    if (ptr) {
        ptr = strchr(ptr, ':');
        if (ptr) sscanf(ptr + 1, "%15s", buzzer_str);
    }
    
    // Update config if values are valid
    if (xSemaphoreTake(webserver_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (temp_warning_str[0] != 0) {
            float val = atof(temp_warning_str);
            if (val > 0 && val < 100) current_config.temp_warning = val;
        }
        
        if (temp_overheat_str[0] != 0) {
            float val = atof(temp_overheat_str);
            if (val > 0 && val < 100) current_config.temp_overheat = val;
        }
        
        if (buzzer_str[0] != 0) {
            current_config.buzzer_enabled = (strstr(buzzer_str, "true") != NULL);
        }
        
        xSemaphoreGive(webserver_data_mutex);
    }
}

/**
 * @brief GET /api/sensor - Lấy dữ liệu cảm biến hiện tại
 */
static esp_err_t sensor_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /api/sensor");
    
    // Create local buffer to avoid race condition with static buffer
    char json_buffer[512];
    
    if (xSemaphoreTake(webserver_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        snprintf(json_buffer, sizeof(json_buffer),
            "{\"temperature\":%.1f,\"humidity\":%.1f,\"status\":\"%s\",\"is_valid\":%s,\"timestamp\":%lld}",
            current_sensor_data.temperature,
            current_sensor_data.humidity,
            get_state_string(current_system_state),
            current_sensor_data.is_valid ? "true" : "false",
            current_sensor_data.timestamp
        );
        
        xSemaphoreGive(webserver_data_mutex);
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_buffer, strlen(json_buffer));
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Mutex timeout");
    }
    
    return ESP_OK;
}

/**
 * @brief GET /api/config - Lấy cấu hình hệ thống
 */
static esp_err_t config_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /api/config");
    
    char *json_str = create_config_json();
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    return ESP_OK;
}

/**
 * @brief POST /api/config - Cập nhật cấu hình hệ thống
 */
static esp_err_t config_post_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "POST /api/config");
    
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data received");
        return ESP_FAIL;
    }
    
    buf[ret] = '\0';
    parse_config_from_post(buf, ret);
    
    char *response_json = create_config_json();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_json, strlen(response_json));
    
    ESP_LOGI(TAG, "✓ Config updated");
    return ESP_OK;
}

/**
 * @brief GET /api/history - Lấy lịch sử dữ liệu
 */
static esp_err_t history_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /api/history");
    
    int limit = 10;
    int offset = 0;
    
    // Parse query string manually
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len > 0) {
        char *query_str = malloc(query_len + 1);
        if (query_str != NULL && httpd_req_get_url_query_str(req, query_str, query_len + 1) == ESP_OK) {
            char *ptr = strstr(query_str, "limit=");
            if (ptr) {
                limit = atoi(ptr + 6);
            }
            
            ptr = strstr(query_str, "offset=");
            if (ptr) {
                offset = atoi(ptr + 7);
            }
        }
        if (query_str) free(query_str);
    }
    
    if (limit > MAX_HISTORY_RECORDS) limit = MAX_HISTORY_RECORDS;
    if (limit < 1) limit = 1;
    if (offset < 0) offset = 0;
    
    // Build JSON response
    static char response[2048];
    int pos = 0;
    
    pos += snprintf(response + pos, sizeof(response) - pos,
        "{\"total\":%" PRIu32 ",\"limit\":%d,\"offset\":%d,\"records\":[",
        history_count, limit, offset
    );
    
    if (xSemaphoreTake(webserver_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int count = 0;
        for (int i = offset; i < history_count && count < limit && pos < sizeof(response) - 150; i++) {
            if (count > 0) {
                pos += snprintf(response + pos, sizeof(response) - pos, ",");
            }
            
            pos += snprintf(response + pos, sizeof(response) - pos,
                "{\"temperature\":%.1f,\"humidity\":%.1f,\"status\":\"%s\",\"timestamp\":%lld}",
                history[i].data.temperature,
                history[i].data.humidity,
                get_state_string(history[i].state),
                history[i].data.timestamp
            );
            count++;
        }
        
        xSemaphoreGive(webserver_data_mutex);
    }
    
    pos += snprintf(response + pos, sizeof(response) - pos, "]}");
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, pos);
    
    return ESP_OK;
}

/**
 * @brief GET /api/status - Lấy trạng thái hệ thống (ngắn gọn)
 */
static esp_err_t status_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /api/status");
    
    static char json_buffer[256];
    
    if (xSemaphoreTake(webserver_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        snprintf(json_buffer, sizeof(json_buffer),
            "{\"temperature\":%.1f,\"humidity\":%.1f,\"status\":\"%s\"}",
            current_sensor_data.temperature,
            current_sensor_data.humidity,
            get_state_string(current_system_state)
        );
        
        xSemaphoreGive(webserver_data_mutex);
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_buffer, strlen(json_buffer));
    
    return ESP_OK;
}

/**
 * @brief GET /api/buzzer - Lấy trạng thái buzzer (ON/OFF)
 */
static esp_err_t buzzer_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /api/buzzer");
    
    static char json_buffer[128];
    
    bool buzzer_on = get_buzzer_status();
    snprintf(json_buffer, sizeof(json_buffer),
        "{\"buzzer_status\":\"%s\",\"is_active\":%s}",
        buzzer_on ? "ON" : "OFF",
        buzzer_on ? "true" : "false"
    );
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_buffer, strlen(json_buffer));
    
    return ESP_OK;
}

/**
 * @brief GET / - Trang HTML chính (Gửi theo chunks để tránh lỗi socket)
 */
static esp_err_t root_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /");
    
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    
    // Phần 1: DOCTYPE + HEAD + CSS
    const char *html_part1 = 
        "<!DOCTYPE html><html><head><title>Temperature Monitor</title>"
        "<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>"
        "* {margin:0;padding:0;box-sizing:border-box;}"
        "body {font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;padding:20px;}"
        ".container {max-width:1000px;margin:0 auto;}"
        "h1 {color:#00d4ff;margin-bottom:30px;text-align:center;}"
        ".card {background:#16213e;border:1px solid #0f3460;border-radius:8px;padding:20px;margin-bottom:20px;}"
        ".data-row {display:flex;justify-content:space-between;padding:10px 0;border-bottom:1px solid #0f3460;}"
        ".data-row:last-child {border-bottom:none;}"
        ".label {font-weight:bold;color:#00d4ff;}"
        ".value {color:#00ff88;font-size:18px;}"
        ".status.NORMAL {color:#00ff88;}"
        ".status.WARNING {color:#ffaa00;}"
        ".status.DANGER {color:#ff3333;}"
        "button {background:#0f3460;border:2px solid #00d4ff;color:#00d4ff;padding:10px 20px;border-radius:5px;cursor:pointer;margin-right:10px;margin-top:10px;}"
        "button:hover {background:#00d4ff;color:#1a1a2e;}"
        "input {background:#0f3460;border:1px solid #00d4ff;color:#eee;padding:8px;border-radius:4px;width:100px;margin-right:5px;}"
        ".loading {text-align:center;color:#00d4ff;}"
        "</style></head><body>"
        "<div class='container'><h1>🌡️ Temperature Monitoring</h1>";
    
    // Phần 2: HTML body
    const char *html_part2 = 
        "<div class='card'><h2>📊 Sensor Data</h2><div id='sensor-data' class='loading'>Loading...</div></div>"
        "<div class='card'><h2>📯 Buzzer Status</h2><div id='buzzer-data' class='loading'>Loading...</div></div>"
        "<div class='card'><h2>⚙️ Configuration</h2><div id='config-data' class='loading'>Loading...</div>"
        "<div><input type='number' id='temp-warning' placeholder='Warning' step='0.1'>"
        "<input type='number' id='temp-overheat' placeholder='Overheat' step='0.1'>"
        "<button onclick='updateConfig()'>Update Config</button></div></div>"
        "</div>";
    
    // Phần 3: JavaScript (Tối ưu - minified)
    const char *html_part3 = 
        "<script>"
        "function fetchSensorData(){"
        "fetch('/api/sensor').then(r=>r.json()).then(d=>{"
        "let h='<div class=\"data-row\"><span class=\"label\">Temperature:</span><span class=\"value\">'+d.temperature.toFixed(1)+'°C</span></div>';"
        "h+='<div class=\"data-row\"><span class=\"label\">Humidity:</span><span class=\"value\">'+d.humidity.toFixed(1)+'%</span></div>';"
        "h+='<div class=\"data-row\"><span class=\"label\">Status:</span><span class=\"value status '+d.status+'\">'+d.status+'</span></div>';"
        "document.getElementById('sensor-data').innerHTML=h;"
        "}).catch(e=>console.error('Sensor error:',e));}"
        "function fetchBuzzerStatus(){"
        "fetch('/api/buzzer').then(r=>r.json()).then(d=>{"
        "let color=d.is_active?'#ff3333':'#00ff88';"
        "let h='<div class=\"data-row\"><span class=\"label\">Status:</span><span class=\"value\" style=\"color:'+color+'\">'+d.buzzer_status+'</span></div>';"
        "document.getElementById('buzzer-data').innerHTML=h;"
        "}).catch(e=>console.error('Buzzer error:',e));}"
        "function fetchConfig(){"
        "fetch('/api/config').then(r=>r.json()).then(d=>{"
        "let h='<div class=\"data-row\"><span class=\"label\">Warning:</span><span class=\"value\">'+d.temp_warning.toFixed(1)+'°C</span></div>';"
        "h+='<div class=\"data-row\"><span class=\"label\">Overheat:</span><span class=\"value\">'+d.temp_overheat.toFixed(1)+'°C</span></div>';"
        "document.getElementById('config-data').innerHTML=h;"
        "document.getElementById('temp-warning').value=d.temp_warning;"
        "document.getElementById('temp-overheat').value=d.temp_overheat;"
        "}).catch(e=>console.error('Config error:',e));}"
        "function updateConfig(){"
        "let w=parseFloat(document.getElementById('temp-warning').value);"
        "let o=parseFloat(document.getElementById('temp-overheat').value);"
        "fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({temp_warning:w,temp_overheat:o})})"
        ".then(()=>{alert('Updated!');fetchConfig();}).catch(e=>alert('Error:'+e));}"
        "document.addEventListener('DOMContentLoaded',function(){"
        "fetchSensorData();fetchBuzzerStatus();fetchConfig();setInterval(fetchSensorData,2000);setInterval(fetchBuzzerStatus,2000);"
        "});"
        "</script></body></html>";
    
    // Gửi từng phần để tránh lỗi socket
    httpd_resp_send_chunk(req, html_part1, strlen(html_part1));
    httpd_resp_send_chunk(req, html_part2, strlen(html_part2));
    httpd_resp_send_chunk(req, html_part3, strlen(html_part3));
    
    // Kết thúc gửi
    httpd_resp_send_chunk(req, NULL, 0);
    
    return ESP_OK;
}

// ==================== URL ROUTING ====================

static const httpd_uri_t uri_get_root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_get_sensor = {
    .uri = "/api/sensor",
    .method = HTTP_GET,
    .handler = sensor_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_get_status = {
    .uri = "/api/status",
    .method = HTTP_GET,
    .handler = status_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_get_buzzer = {
    .uri = "/api/buzzer",
    .method = HTTP_GET,
    .handler = buzzer_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_get_config = {
    .uri = "/api/config",
    .method = HTTP_GET,
    .handler = config_get_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_post_config = {
    .uri = "/api/config",
    .method = HTTP_POST,
    .handler = config_post_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_get_history = {
    .uri = "/api/history",
    .method = HTTP_GET,
    .handler = history_handler,
    .user_ctx = NULL
};

// ==================== PUBLIC API ====================

esp_err_t webserver_init(void) {
    // Tạo mutex
    if (webserver_data_mutex == NULL) {
        webserver_data_mutex = xSemaphoreCreateMutex();
    }
    
    // Cấu hình server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_SERVER_PORT;
    config.max_open_sockets = 4;  // Reduced to fit within LWIP_MAX_SOCKETS (7)
    
    ESP_LOGI(TAG, "Starting HTTP Server on port %d", config.server_port);
    
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server!");
        return ESP_FAIL;
    }
    
    // Đăng ký các URI handler
    httpd_register_uri_handler(server, &uri_get_root);
    httpd_register_uri_handler(server, &uri_get_sensor);
    httpd_register_uri_handler(server, &uri_get_status);
    httpd_register_uri_handler(server, &uri_get_buzzer);
    httpd_register_uri_handler(server, &uri_get_config);
    httpd_register_uri_handler(server, &uri_post_config);
    httpd_register_uri_handler(server, &uri_get_history);
    
    ESP_LOGI(TAG, "✓ HTTP Server initialized");
    ESP_LOGI(TAG, "  GET  / - HTML Dashboard");
    ESP_LOGI(TAG, "  GET  /api/sensor - Get sensor data");
    ESP_LOGI(TAG, "  GET  /api/status - Get status (short)");
    ESP_LOGI(TAG, "  GET  /api/buzzer - Get buzzer status");
    ESP_LOGI(TAG, "  GET  /api/config - Get configuration");
    ESP_LOGI(TAG, "  POST /api/config - Update configuration");
    ESP_LOGI(TAG, "  GET  /api/history - Get history");
    
    return ESP_OK;
}

esp_err_t webserver_stop(void) {
    if (server != NULL) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "✓ HTTP Server stopped");
    }
    return ESP_OK;
}

void webserver_update_sensor_data(const sensor_data_t *data, system_state_t state) {
    if (xSemaphoreTake(webserver_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        current_sensor_data = *data;
        current_system_state = state;
        
        // Thêm vào lịch sử
        add_to_history(data, state);
        
        xSemaphoreGive(webserver_data_mutex);
    }
}

void webserver_update_config(const system_config_t *config) {
    if (xSemaphoreTake(webserver_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        current_config = *config;
        xSemaphoreGive(webserver_data_mutex);
    }
}

system_config_t webserver_get_config(void) {
    system_config_t config;
    if (xSemaphoreTake(webserver_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        config = current_config;
        xSemaphoreGive(webserver_data_mutex);
    }
    return config;
}

uint32_t webserver_get_history_count(void) {
    uint32_t count = 0;
    if (xSemaphoreTake(webserver_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        count = history_count;
        xSemaphoreGive(webserver_data_mutex);
    }
    return count;
}

history_record_t webserver_get_history(uint32_t index) {
    history_record_t record = {0};
    if (index < MAX_HISTORY_RECORDS && 
        xSemaphoreTake(webserver_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        record = history[index];
        xSemaphoreGive(webserver_data_mutex);
    }
    return record;
}
