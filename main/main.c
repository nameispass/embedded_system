/**
 * @file main.c
 * @brief Hệ thống Giám sát Nhiệt độ - ESP-IDF FreeRTOS (FULL FEATURES + WEBSERVER)
 * @features Tasks, Queues, Software Timers, Mutex, Semaphores, Event Groups, Task Notifications
 * @webserver HTTP REST API, WiFi connectivity, Web Dashboard
 */

#include "config.h"
#include "dht22.h"
#include "ssd1306.h"
#include "webserver.h"
#include "wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "MAIN";

// ==================== FREERTOS HANDLES ====================
// Queue để truyền dữ liệu cảm biến
static QueueHandle_t sensor_queue = NULL;

// Mutex bảo vệ I2C bus (OLED và DHT22 dùng chung GPIO)
SemaphoreHandle_t i2c_mutex = NULL;

// Binary Semaphore để đồng bộ dữ liệu mới
SemaphoreHandle_t data_ready_semaphore = NULL;

// Event Group quản lý trạng thái hệ thống
EventGroupHandle_t system_event_group = NULL;

// Software Timers
static TimerHandle_t sensor_timer = NULL;      // Timer đọc cảm biến mỗi 1s
static TimerHandle_t buzzer_timer = NULL;      // Timer tắt buzzer sau 5s

// Task Handles (để dùng Task Notification)
TaskHandle_t display_task_handle = NULL;
TaskHandle_t alert_task_handle = NULL;

// ==================== HELPER FUNCTIONS ====================

/**
 * @brief Xác định trạng thái hệ thống theo nhiệt độ
 */
system_state_t get_system_state(float temperature) {
    if (temperature >= TEMP_OVERHEAT) {
        return STATE_OVERHEAT;
    } else if (temperature >= TEMP_WARNING) {
        return STATE_WARNING;
    } else {
        return STATE_NORMAL;
    }
}

/**
 * @brief Chuyển đổi state sang chuỗi
 */
const char* get_state_string(system_state_t state) {
    switch (state) {
        case STATE_NORMAL:   return "NORMAL";
        case STATE_WARNING:  return "WARNING";
        case STATE_OVERHEAT: return "DANGER!";
        case STATE_ERROR:    return "ERROR";
        default:             return "UNKNOWN";
    }
}

/**
 * @brief Lấy trạng thái buzzer (ON nếu timer đang chạy, OFF nếu không)
 */
bool get_buzzer_status(void) {
    if (buzzer_timer == NULL) {
        return false;
    }
    return (xTimerIsTimerActive(buzzer_timer) != pdFALSE);
}

// ==================== SOFTWARE TIMER CALLBACKS ====================

/**
 * @brief Timer callback: Đọc cảm biến mỗi 1 giây
 */
void sensor_timer_callback(TimerHandle_t xTimer) {
    // Đánh thức sensor_task bằng Task Notification
    if (xTaskNotifyGive(xTaskGetHandle("sensor_task")) == pdPASS) {
        ESP_LOGD(TAG, "Sensor timer triggered");
    }
}

/**
 * @brief Timer callback: Tắt buzzer sau 10 giây
 */
void buzzer_timer_callback(TimerHandle_t xTimer) {
    gpio_set_level(BUZZER_PIN, 0);
    ESP_LOGI(TAG, "Buzzer auto-off after 10s");
}

// ==================== TASK IMPLEMENTATIONS ====================

/**
 * @brief Task đọc cảm biến DHT22 (dùng Task Notification thay vì delay)
 */
void sensor_task(void *pvParameters) {
    sensor_data_t data;
    
    ESP_LOGI(TAG, "✓ Sensor task started");
    
    while (1) {
        // Đợi notification từ sensor_timer (thay vì vTaskDelay)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Lấy Mutex để bảo vệ I2C (nếu cần đọc qua I2C)
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            
            // Đọc DHT22
            if (dht22_read(&data.temperature, &data.humidity) == ESP_OK) {
                data.is_valid = true;
                data.timestamp = esp_timer_get_time();
                
                ESP_LOGI(TAG, "📊 DHT22: T=%.1f°C, H=%.1f%%", 
                         data.temperature, data.humidity);
                
                // Xác định trạng thái mới
                system_state_t new_state = get_system_state(data.temperature);
                
                // Cập nhật Event Group
                xEventGroupClearBits(system_event_group, 
                                     EVENT_STATE_NORMAL | EVENT_STATE_WARNING | EVENT_STATE_OVERHEAT);
                
                if (new_state == STATE_NORMAL) {
                    xEventGroupSetBits(system_event_group, EVENT_STATE_NORMAL);
                } else if (new_state == STATE_WARNING) {
                    xEventGroupSetBits(system_event_group, EVENT_STATE_WARNING);
                } else if (new_state == STATE_OVERHEAT) {
                    xEventGroupSetBits(system_event_group, EVENT_STATE_OVERHEAT);
                }
                
                // Set bit NEW_DATA
                xEventGroupSetBits(system_event_group, EVENT_NEW_DATA);
                
                // Gửi data qua Queue
                xQueueSend(sensor_queue, &data, 0);
                
                // Đánh thức display_task bằng Task Notification
                xTaskNotifyGive(display_task_handle);
                
                // Signal semaphore báo có dữ liệu mới
                xSemaphoreGive(data_ready_semaphore);
                
                // ========== CẬP NHẬT WEBSERVER ==========
                #if ENABLE_WEBSERVER
                webserver_update_sensor_data(&data, new_state);
                #endif
                
            } else {
                data.is_valid = false;
                ESP_LOGW(TAG, "⚠ Failed to read DHT22");
            }
            
            xSemaphoreGive(i2c_mutex);
        } else {
            ESP_LOGW(TAG, "⚠ I2C Mutex timeout");
        }
    }
}

/**
 * @brief Task hiển thị OLED (dùng Task Notification + Event Group)
 */
void display_task(void *pvParameters) {
    sensor_data_t data;
    char temp_str[32], humi_str[32], status_str[32];
    EventBits_t bits;
    
    ESP_LOGI(TAG, "✓ Display task started");
    
    while (1) {
        // Đợi notification từ sensor_task
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Đợi semaphore báo có dữ liệu mới
        if (xSemaphoreTake(data_ready_semaphore, pdMS_TO_TICKS(200)) == pdTRUE) {
            
            // Nhận dữ liệu từ queue
            if (xQueueReceive(sensor_queue, &data, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (data.is_valid) {
                    
                    // Lấy Mutex I2C trước khi viết OLED
                    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        
                        // Xóa màn hình
                        ssd1306_clear();
                        
                        // Hiển thị tiêu đề
                        ssd1306_draw_string(0, 0, "TEMP MONITOR", 1);
                        
                        // Hiển thị nhiệt độ
                        snprintf(temp_str, sizeof(temp_str), "TEMP: %.1fC", data.temperature);
                        ssd1306_draw_string(0, 16, temp_str, 1);
                        
                        // Hiển thị độ ẩm
                        snprintf(humi_str, sizeof(humi_str), "HUMI: %.1f%%", data.humidity);
                        ssd1306_draw_string(0, 32, humi_str, 1);
                        
                        // Đọc trạng thái từ Event Group
                        bits = xEventGroupGetBits(system_event_group);
                        
                        if (bits & EVENT_STATE_OVERHEAT) {
                            snprintf(status_str, sizeof(status_str), "STATUS: DANGER!");
                        } else if (bits & EVENT_STATE_WARNING) {
                            snprintf(status_str, sizeof(status_str), "STATUS: WARNING");
                        } else if (bits & EVENT_STATE_NORMAL) {
                            snprintf(status_str, sizeof(status_str), "STATUS: NORMAL");
                        } else {
                            snprintf(status_str, sizeof(status_str), "STATUS: ---");
                        }
                        
                        ssd1306_draw_string(0, 48, status_str, 1);
                        
                        xSemaphoreGive(i2c_mutex);
                        
                        ESP_LOGI(TAG, "🖥 Display updated: %s", status_str);
                    }
                }
            }
        }
    }
}

/**
 * @brief Task xử lý cảnh báo (Buzzer & LED) - dùng Event Group
 */
void alert_task(void *pvParameters) {
    EventBits_t bits;
    system_state_t last_state = STATE_NORMAL;
    
    ESP_LOGI(TAG, "✓ Alert task started");
    
    while (1) {
        // Đợi sự kiện NEW_DATA từ Event Group
        bits = xEventGroupWaitBits(
            system_event_group,
            EVENT_NEW_DATA,
            pdTRUE,  // Clear bit sau khi đọc
            pdFALSE, // Chỉ cần 1 bit
            portMAX_DELAY
        );
        
        if (bits & EVENT_NEW_DATA) {
            // Đọc trạng thái hiện tại
            bits = xEventGroupGetBits(system_event_group);
            
            system_state_t new_state;
            if (bits & EVENT_STATE_OVERHEAT) {
                new_state = STATE_OVERHEAT;
            } else if (bits & EVENT_STATE_WARNING) {
                new_state = STATE_WARNING;
            } else {
                new_state = STATE_NORMAL;
            }
            
            // Xử lý từng trạng thái (kể cả khi không thay đổi)
            switch (new_state) {
                case STATE_OVERHEAT:
                    // NGUY HIỂM: Bật buzzer và LED
                    gpio_set_level(LED_PIN, 1);
                    
                    // Kêu buzzer chu kỳ: Nếu timer hết → bắt đầu lại
                    if (xTimerIsTimerActive(buzzer_timer) == pdFALSE) {
                        gpio_set_level(BUZZER_PIN, 1);
                        xTimerStart(buzzer_timer, 0);
                        
                        if (new_state != last_state) {
                            ESP_LOGW(TAG, "🚨 ALERT: OVERHEAT! Buzzer ON (cycle 1)");
                        } else {
                            ESP_LOGW(TAG, "🔔 OVERHEAT: Buzzer ON (cycle repeat)");
                        }
                    }
                    break;
                    
                case STATE_WARNING:
                    // CẢNH BÁO: Tắt buzzer, bật LED
                    gpio_set_level(BUZZER_PIN, 0);
                    gpio_set_level(LED_PIN, 1);
                    
                    // Dừng timer buzzer
                    xTimerStop(buzzer_timer, 0);
                    
                    if (new_state != last_state) {
                        ESP_LOGW(TAG, "⚠ ALERT: WARNING! LED ON");
                    }
                    break;
                    
                case STATE_NORMAL:
                    // BÌNH THƯỜNG: Tắt cả hai
                    gpio_set_level(BUZZER_PIN, 0);
                    gpio_set_level(LED_PIN, 0);
                    
                    // Dừng timer buzzer
                    xTimerStop(buzzer_timer, 0);
                    
                    if (new_state != last_state) {
                        ESP_LOGI(TAG, "✓ ALERT: NORMAL");
                    }
                    break;
                    
                default:
                    break;
            }
            
            // Cập nhật last_state sau khi xử lý
            last_state = new_state;
        }
    }
}

/**
 * @brief App main - ESP-IDF entry point
 */
void app_main(void) {
    ESP_LOGI(TAG, "\n╔════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  TEMPERATURE MONITORING SYSTEM + WEBSERVER           ║");
    ESP_LOGI(TAG, "║  Tasks | Queues | Timers | Mutex | Semaphores       ║");
    ESP_LOGI(TAG, "║  Event Groups | Task Notifications | WiFi + HTTP     ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════════════════════╝\n");
    
    // ==================== KHỞI TẠO PHẦN CỨNG ====================
    
    // Khởi tạo GPIO cho Buzzer và LED
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_PIN) | (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(BUZZER_PIN, 0);
    gpio_set_level(LED_PIN, 0);
    ESP_LOGI(TAG, "✓ GPIO initialized (Buzzer=%d, LED=%d)", BUZZER_PIN, LED_PIN);
    
    // Khởi tạo I2C
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "✗ Failed to initialize I2C!");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Khởi tạo OLED
    if (ssd1306_init() != ESP_OK) {
        ESP_LOGE(TAG, "✗ Failed to initialize OLED!");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "✓ OLED initialized");
    
    // Khởi tạo DHT22
    dht22_init();
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "✓ DHT22 initialized (GPIO=%d)", DHT_PIN);
    
    // ==================== KHỞI TẠO WiFi VÀ WEBSERVER ====================
    
    #if ENABLE_WEBSERVER
    ESP_LOGI(TAG, "\n--- WiFi & Webserver Initialization ---\n");
    
    // Khởi tạo WiFi (blocking - đợi kết nối)
    if (wifi_init_sta() == ESP_OK) {
        ESP_LOGI(TAG, "✓ WiFi connected!");
        ESP_LOGI(TAG, "  IP Address: %s", wifi_get_ip_address());
        
        // Khởi tạo HTTP Server
        if (webserver_init() == ESP_OK) {
            ESP_LOGI(TAG, "✓ Webserver initialized!");
            ESP_LOGI(TAG, "  Open browser: http://%s", wifi_get_ip_address());
        } else {
            ESP_LOGE(TAG, "✗ Failed to initialize webserver!");
        }
    } else {
        ESP_LOGW(TAG, "⚠ WiFi connection failed, but system continues...");
    }
    #endif
    
    // ==================== TẠO FREERTOS OBJECTS ====================
    
    // 1. Queue - Truyền dữ liệu sensor
    sensor_queue = xQueueCreate(5, sizeof(sensor_data_t));
    if (sensor_queue == NULL) {
        ESP_LOGE(TAG, "✗ Failed to create queue!");
        return;
    }
    ESP_LOGI(TAG, "✓ Queue created (size=5)");
    
    // 2. Mutex - Bảo vệ I2C bus
    i2c_mutex = xSemaphoreCreateMutex();
    if (i2c_mutex == NULL) {
        ESP_LOGE(TAG, "✗ Failed to create I2C mutex!");
        return;
    }
    ESP_LOGI(TAG, "✓ I2C Mutex created");
    
    // 3. Binary Semaphore - Đồng bộ dữ liệu mới
    data_ready_semaphore = xSemaphoreCreateBinary();
    if (data_ready_semaphore == NULL) {
        ESP_LOGE(TAG, "✗ Failed to create semaphore!");
        return;
    }
    ESP_LOGI(TAG, "✓ Data Ready Semaphore created");
    
    // 4. Event Group - Quản lý trạng thái hệ thống
    system_event_group = xEventGroupCreate();
    if (system_event_group == NULL) {
        ESP_LOGE(TAG, "✗ Failed to create event group!");
        return;
    }
    // Set trạng thái ban đầu là NORMAL
    xEventGroupSetBits(system_event_group, EVENT_STATE_NORMAL);
    ESP_LOGI(TAG, "✓ Event Group created (initial: NORMAL)");
    
    // 5. Software Timer - Đọc cảm biến mỗi 1 giây
    sensor_timer = xTimerCreate(
        "SensorTimer",                      // Tên timer
        pdMS_TO_TICKS(1000),                // Chu kỳ 1 giây
        pdTRUE,                             // Auto-reload
        (void *)0,                          // Timer ID
        sensor_timer_callback               // Callback
    );
    if (sensor_timer == NULL) {
        ESP_LOGE(TAG, "✗ Failed to create sensor timer!");
        return;
    }
    ESP_LOGI(TAG, "✓ Sensor Timer created (1s period)");
    
    // 6. Software Timer - Tắt buzzer sau 5 giây
    buzzer_timer = xTimerCreate(
        "BuzzerTimer",                      // Tên timer
        pdMS_TO_TICKS(10000),               // 10 giây
        pdFALSE,                            // One-shot (không auto-reload)
        (void *)1,                          // Timer ID
        buzzer_timer_callback               // Callback
    );
    if (buzzer_timer == NULL) {
        ESP_LOGE(TAG, "✗ Failed to create buzzer timer!");
        return;
    }
    ESP_LOGI(TAG, "✓ Buzzer Timer created (10s auto-off)");
    
    // ==================== TẠO TASKS ====================
    
    ESP_LOGI(TAG, "\n--- Starting FreeRTOS Tasks ---\n");
    
    // Task 1: Sensor Task (Priority 5 - cao nhất)
    xTaskCreate(
        sensor_task,
        "sensor_task",
        4096,
        NULL,
        5,
        NULL
    );
    ESP_LOGI(TAG, "✓ Sensor Task created (Priority 5)");
    
    // Task 2: Display Task (Priority 4)
    xTaskCreate(
        display_task,
        "display_task",
        4096,
        NULL,
        4,
        &display_task_handle  // Lưu handle để dùng Task Notification
    );
    ESP_LOGI(TAG, "✓ Display Task created (Priority 4)");
    
    // Task 3: Alert Task (Priority 3)
    xTaskCreate(
        alert_task,
        "alert_task",
        2048,
        NULL,
        3,
        &alert_task_handle    // Lưu handle để dùng Task Notification
    );
    ESP_LOGI(TAG, "✓ Alert Task created (Priority 3)");
    
    // ==================== KHỞI ĐỘNG TIMERS ====================
    
    // Khởi động sensor timer
    if (xTimerStart(sensor_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "✗ Failed to start sensor timer!");
        return;
    }
    ESP_LOGI(TAG, "✓ Sensor Timer started");
    
    // ==================== SYSTEM READY ====================
    
    ESP_LOGI(TAG, "\n╔════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║              🚀 SYSTEM RUNNING!                       ║");
    ESP_LOGI(TAG, "║  📊 Sensor reading every 1s                           ║");
    ESP_LOGI(TAG, "║  🖥  Display updates on new data                      ║");
    ESP_LOGI(TAG, "║  🔔 Alerts via Event Group + Timer                    ║");
    #if ENABLE_WEBSERVER
    ESP_LOGI(TAG, "║  🌐 Webserver: http://%s                              ║", wifi_get_ip_address());
    #endif
    ESP_LOGI(TAG, "╚════════════════════════════════════════════════════════╝\n");
}
