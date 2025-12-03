/**
 * @file ssd1306.h
 * @brief SSD1306 OLED driver cho ESP-IDF
 */

#ifndef SSD1306_H
#define SSD1306_H

#include "config.h"

// SSD1306 Commands
#define SSD1306_CMD_SET_CONTRAST            0x81
#define SSD1306_CMD_DISPLAY_ALL_ON_RESUME   0xA4
#define SSD1306_CMD_DISPLAY_ALL_ON          0xA5
#define SSD1306_CMD_NORMAL_DISPLAY          0xA6
#define SSD1306_CMD_INVERT_DISPLAY          0xA7
#define SSD1306_CMD_DISPLAY_OFF             0xAE
#define SSD1306_CMD_DISPLAY_ON              0xAF
#define SSD1306_CMD_SET_DISPLAY_OFFSET      0xD3
#define SSD1306_CMD_SET_COM_PINS            0xDA
#define SSD1306_CMD_SET_VCOM_DETECT         0xDB
#define SSD1306_CMD_SET_DISPLAY_CLOCK_DIV   0xD5
#define SSD1306_CMD_SET_PRECHARGE           0xD9
#define SSD1306_CMD_SET_MULTIPLEX           0xA8
#define SSD1306_CMD_SET_LOW_COLUMN          0x00
#define SSD1306_CMD_SET_HIGH_COLUMN         0x10
#define SSD1306_CMD_SET_START_LINE          0x40
#define SSD1306_CMD_MEMORY_MODE             0x20
#define SSD1306_CMD_COLUMN_ADDR             0x21
#define SSD1306_CMD_PAGE_ADDR               0x22
#define SSD1306_CMD_COM_SCAN_INC            0xC0
#define SSD1306_CMD_COM_SCAN_DEC            0xC8
#define SSD1306_CMD_SEG_REMAP               0xA0
#define SSD1306_CMD_CHARGE_PUMP             0x8D
#define SSD1306_CMD_EXTERNAL_VCC            0x01
#define SSD1306_CMD_SWITCH_CAP_VCC          0x02

// Function prototypes
esp_err_t ssd1306_init(void);
esp_err_t ssd1306_clear(void);
esp_err_t ssd1306_display(void);
esp_err_t ssd1306_draw_pixel(uint8_t x, uint8_t y, bool color);
esp_err_t ssd1306_draw_char(uint8_t x, uint8_t y, char c, uint8_t size);
esp_err_t ssd1306_draw_string(uint8_t x, uint8_t y, const char *str, uint8_t size);
esp_err_t ssd1306_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
esp_err_t ssd1306_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
esp_err_t ssd1306_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
esp_err_t ssd1306_update_display(sensor_data_t *data, system_state_t state);
esp_err_t ssd1306_show_welcome_screen(void);

#endif // SSD1306_H
