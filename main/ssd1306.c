
#include "ssd1306.h"
#include <string.h>
#include <math.h>

static const char *TAG = TAG_DISPLAY;

// Font 5x7 (ASCII 32-90 - đầy đủ như code test)
static const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // Space (32)
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
};

/**
 * @brief Gửi command tới SSD1306
 */
static esp_err_t ssd1306_write_command(uint8_t cmd) {
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (OLED_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, 0x00, true); // command mode
    i2c_master_write_byte(handle, cmd, true);
    i2c_master_stop(handle);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, handle, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(handle);
    return ret;
}

/**
 * @brief Gửi data tới SSD1306
 */
static esp_err_t ssd1306_write_data(uint8_t data) {
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (OLED_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, 0x40, true); // data mode
    i2c_master_write_byte(handle, data, true);
    i2c_master_stop(handle);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, handle, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(handle);
    return ret;
}

/**
 * @brief Khởi tạo I2C master
 */
esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C config failed");
        return err;
    }
    
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 
                             I2C_MASTER_RX_BUF_LEN, I2C_MASTER_TX_BUF_LEN, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed");
        return err;
    }
    
    ESP_LOGI(TAG, "I2C initialized (SDA=%d, SCL=%d)", 
             I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    return ESP_OK;
}

/**
 * @brief Khởi tạo SSD1306 - Theo code test thành công
 */
esp_err_t ssd1306_init(void) {
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Initialization sequence (giống code test)
    ssd1306_write_command(0xAE); // display off
    ssd1306_write_command(0x20);
    ssd1306_write_command(0x00); // horizontal mode
    ssd1306_write_command(0x40);
    ssd1306_write_command(0x81);
    ssd1306_write_command(0x7F);
    ssd1306_write_command(0xA1);
    ssd1306_write_command(0xA6);
    ssd1306_write_command(0xA8);
    ssd1306_write_command(0x3F);
    ssd1306_write_command(0xC8);
    ssd1306_write_command(0xD3);
    ssd1306_write_command(0x00);
    ssd1306_write_command(0xD5);
    ssd1306_write_command(0x80);
    ssd1306_write_command(0xD9);
    ssd1306_write_command(0xF1);
    ssd1306_write_command(0xDA);
    ssd1306_write_command(0x12);
    ssd1306_write_command(0xDB);
    ssd1306_write_command(0x40);
    ssd1306_write_command(0x8D);
    ssd1306_write_command(0x14);
    ssd1306_write_command(0xAF); // display on
    
    ssd1306_clear();
    
    ESP_LOGI(TAG, "SSD1306 initialized");
    return ESP_OK;
}

/**
 * @brief Xóa buffer - Theo code test
 */
esp_err_t ssd1306_clear(void) {
    for (int i = 0; i < 8; i++) {
        ssd1306_write_command(0xB0 + i); // page
        ssd1306_write_command(0x00);
        ssd1306_write_command(0x10);
        for (int j = 0; j < 128; j++) {
            ssd1306_write_data(0x00);
        }
    }
    return ESP_OK;
}

/**
 * @brief Set cursor position
 */
static void ssd1306_set_cursor(uint8_t page, uint8_t col) {
    ssd1306_write_command(0xB0 + page);           // Set page address
    ssd1306_write_command(0x00 + (col & 0x0F));   // Set lower column address
    ssd1306_write_command(0x10 + (col >> 4));     // Set higher column address
}

/**
 * @brief Write single character
 */
static void ssd1306_write_char(char c) {
    if (c < 32 || c > 90) c = 32; // Default to space if out of range
    
    for (int i = 0; i < 5; i++) {
        ssd1306_write_data(font5x7[c - 32][i]);
    }
    ssd1306_write_data(0x00); // Add spacing between characters
}

/**
 * @brief Gửi buffer lên màn hình - KHÔNG SỬ DỤNG NỮA
 */
esp_err_t ssd1306_display(void) {
    // Không cần nữa vì đang write trực tiếp
    return ESP_OK;
}





/**
 * @brief Vẽ chuỗi - Write trực tiếp như code test
 */
esp_err_t ssd1306_draw_string(uint8_t x, uint8_t y, const char *str, uint8_t size) {
    // Chuyển đổi y (pixel) sang page (0-7)
    uint8_t page = y / 8;
    
    ssd1306_set_cursor(page, x);
    
    while (*str) {
        ssd1306_write_char(*str);
        str++;
    }
    
    return ESP_OK;
}



/**
 * @brief Hiển thị màn hình chào
 */
esp_err_t ssd1306_show_welcome_screen(void) {
    ssd1306_clear();
    ssd1306_draw_string(40, 10, "PBL5", 2);
    ssd1306_draw_string(10, 30, "Temperature", 1);
    ssd1306_draw_string(10, 42, "Monitor System", 1);
    return ssd1306_display();
}

/**
 * @brief Cập nhật màn hình với dữ liệu
 */
esp_err_t ssd1306_update_display(sensor_data_t *data, system_state_t state) {
    if (!data || !data->is_valid) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ssd1306_clear();
    
    // Header
    ssd1306_draw_string(0, 0, "== TEMP MONITOR ==", 1);
    ssd1306_draw_line(0, 10, OLED_WIDTH - 1, 10);
    
    // Status
    ssd1306_draw_string(0, 14, "Status:", 1);
    const char *state_str = get_state_string(state);
    if (state == STATE_WARNING || state == STATE_OVERHEAT) {
        ssd1306_fill_rect(50, 14, strlen(state_str) * 6 + 2, 9);
    }
    ssd1306_draw_string(52, 15, state_str, 1);
    
    // Temperature
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%.1f C", data->temperature);
    ssd1306_draw_string(0, 28, temp_str, 2);
    
    // Temperature bar
    float temp_percent = (data->temperature / 60.0f);
    if (temp_percent > 1.0f) temp_percent = 1.0f;
    uint8_t bar_width = (uint8_t)(temp_percent * 48);
    ssd1306_draw_rect(75, 30, 50, 6);
    if (bar_width > 0) {
        ssd1306_fill_rect(76, 31, bar_width, 4);
    }
    
    // Humidity
    char hum_str[16];
    snprintf(hum_str, sizeof(hum_str), "%.1f %%", data->humidity);
    ssd1306_draw_string(0, 46, hum_str, 2);
    
    // Humidity bar
    float hum_percent = (data->humidity / 100.0f);
    if (hum_percent > 1.0f) hum_percent = 1.0f;
    bar_width = (uint8_t)(hum_percent * 48);
    ssd1306_draw_rect(75, 48, 50, 6);
    if (bar_width > 0) {
        ssd1306_fill_rect(76, 49, bar_width, 4);
    }
    
    return ssd1306_display();
}
