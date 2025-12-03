
#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"

// ==================== TAG for logging ====================
#define TAG_MAIN    "MAIN"
#define TAG_SENSOR  "SENSOR"
#define TAG_DISPLAY "DISPLAY"
#define TAG_ALERT   "ALERT"

// ==================== PIN CONFIGURATION ====================
#define DHT_PIN         GPIO_NUM_4      // GPIO4 cho DHT22
#define BUZZER_PIN      GPIO_NUM_5      // GPIO5 cho Buzzer
#define LED_PIN         GPIO_NUM_2      // GPIO2 cho LED

// I2C Configuration cho OLED (4 chân: VCC, GND, SCL, SDA)
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_SDA_IO       GPIO_NUM_6      // SDA - GPIO 6
#define I2C_MASTER_SCL_IO       GPIO_NUM_7      // SCL - GPIO 7
#define I2C_MASTER_FREQ_HZ      400000          // 400kHz cho I2C
#define I2C_MASTER_TX_BUF_LEN   0               // Disable buffer
#define I2C_MASTER_RX_BUF_LEN   0               // Disable buffer
#define I2C_MASTER_TIMEOUT_MS   1000

// OLED Configuration (SSD1306 128x64)
#define OLED_I2C_ADDR           0x3C            // Địa chỉ I2C (0x3C)
#define OLED_WIDTH              128
#define OLED_HEIGHT             64

// ==================== SYSTEM THRESHOLDS ====================
#define TEMP_NORMAL     35.0f   // Ngưỡng nhiệt độ bình thường (°C)
#define TEMP_WARNING    35.0f   // Ngưỡng cảnh báo (°C)
#define TEMP_OVERHEAT   45.0f   // Ngưỡng quá nhiệt (°C)

#define HUMIDITY_MIN    30.0f   // Độ ẩm tối thiểu (%)
#define HUMIDITY_MAX    80.0f   // Độ ẩm tối đa (%)

// ==================== TIMING CONFIGURATION ====================
#define SENSOR_READ_PERIOD_MS   1000    // Đọc cảm biến mỗi 1 giây
#define DISPLAY_UPDATE_DELAY_MS 500     // Delay display task
#define BUZZER_DURATION_MS      5000    // Thời gian buzzer kêu
#define DHT22_READ_DELAY_MS     2000    // Delay giữa các lần đọc DHT22

// ==================== FREERTOS CONFIGURATION ====================
// Task Priorities
#define PRIORITY_SENSOR_TASK    3
#define PRIORITY_DISPLAY_TASK   2
#define PRIORITY_ALERT_TASK     4

// Stack Sizes
#define STACK_SIZE_SENSOR       3072
#define STACK_SIZE_DISPLAY      4096
#define STACK_SIZE_ALERT        2048

// Queue Sizes
#define QUEUE_SIZE_SENSOR_DATA  5
#define QUEUE_SIZE_ALERT        3

// ==================== EVENT GROUP BITS ====================
#define EVENT_STATE_NORMAL      (1 << 0)
#define EVENT_STATE_WARNING     (1 << 1)
#define EVENT_STATE_OVERHEAT    (1 << 2)
#define EVENT_NEW_DATA          (1 << 3)

// ==================== DATA STRUCTURES ====================

/**
 * @brief Cấu trúc dữ liệu cảm biến
 */
typedef struct {
    float temperature;      // Nhiệt độ (°C)
    float humidity;         // Độ ẩm (%)
    int64_t timestamp;      // Thời gian đọc (microseconds)
    bool is_valid;          // Dữ liệu hợp lệ hay không
} sensor_data_t;

/**
 * @brief Trạng thái hệ thống
 */
typedef enum {
    STATE_NORMAL = 0,
    STATE_WARNING,
    STATE_OVERHEAT,
    STATE_ERROR
} system_state_t;

/**
 * @brief Thông điệp cảnh báo
 */
typedef struct {
    system_state_t state;
    float value;
    int64_t timestamp;
} alert_message_t;

// ==================== GLOBAL HANDLES ====================
extern TaskHandle_t sensor_task_handle;
extern TaskHandle_t display_task_handle;
extern TaskHandle_t alert_task_handle;

extern QueueHandle_t sensor_data_queue;
extern QueueHandle_t alert_queue;

extern SemaphoreHandle_t i2c_mutex;
extern SemaphoreHandle_t data_ready_semaphore;

extern EventGroupHandle_t system_event_group;

extern TimerHandle_t sensor_timer_handle;
extern TimerHandle_t buzzer_timer_handle;

// ==================== FUNCTION PROTOTYPES ====================

// Task Functions
void sensor_task(void *pvParameters);
void display_task(void *pvParameters);
void alert_task(void *pvParameters);

// Timer Callbacks
void sensor_timer_callback(TimerHandle_t xTimer);
void buzzer_timer_callback(TimerHandle_t xTimer);

// Helper Functions
system_state_t get_system_state(float temperature);
const char* get_state_string(system_state_t state);
void update_system_state(float temperature);
void activate_buzzer(bool enable);
void update_led(system_state_t state);

// DHT22 Functions
esp_err_t dht22_init(void);
esp_err_t dht22_read(float *temperature, float *humidity);
bool dht22_is_valid_data(float temp, float hum);

// OLED Functions
esp_err_t ssd1306_init(void);
esp_err_t ssd1306_clear(void);
esp_err_t ssd1306_display(void);
esp_err_t ssd1306_draw_string(uint8_t x, uint8_t y, const char *str, uint8_t size);
esp_err_t ssd1306_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
esp_err_t ssd1306_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
esp_err_t ssd1306_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
esp_err_t ssd1306_update_display(sensor_data_t *data, system_state_t state);
esp_err_t ssd1306_show_welcome_screen(void);

// I2C Functions
esp_err_t i2c_master_init(void);

// Debug Functions
void print_system_info(void);
void print_sensor_data(sensor_data_t *data);

#endif // CONFIG_H
