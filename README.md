# Hệ thống Giám sát Nhiệt độ - Cảnh báo
## Temperature Monitoring System with Alert

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.x-red.svg)](https://docs.espressif.com/projects/esp-idf/)
[![FreeRTOS](https://img.shields.io/badge/FreeRTOS-Enabled-green.svg)](https://www.freertos.org/)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Dự án hệ thống giám sát nhiệt độ và độ ẩm sử dụng **ESP32-C3** và **FreeRTOS**, thể hiện đầy đủ các tính năng của hệ điều hành thời gian thực.

---

## 📋 Mục lục
- [Tổng quan](#-tổng-quan)
- [Tính năng FreeRTOS](#-tính-năng-freertos)
- [Tính năng hệ thống](#-tính-năng-hệ-thống)
- [Linh kiện phần cứng](#-linh-kiện-phần-cứng)
- [Sơ đồ kết nối](#-sơ-đồ-kết-nối)
- [Cài đặt](#-cài-đặt)
- [Cấu hình](#️-cấu-hình)
- [Sử dụng](#-sử-dụng)
- [Kiến trúc hệ thống](#-kiến-trúc-hệ-thống)
- [Mở rộng](#-mở-rộng)

---

## 🎯 Tổng quan

Dự án xây dựng hệ thống giám sát nhiệt độ – độ ẩm sử dụng **FreeRTOS** và **ESP32-C3**. Hệ thống đọc dữ liệu từ cảm biến DHT22 theo chu kỳ 1 giây bằng Software Timer, hiển thị lên màn hình OLED SSD1306, và cảnh báo qua buzzer khi nhiệt độ vượt ngưỡng.

### Đặc điểm nổi bật:
- ✅ Sử dụng **đầy đủ** các tính năng FreeRTOS
- ✅ Kiến trúc **đa nhiệm**, không gian đoạn
- ✅ Bảo vệ tài nguyên dùng chung với **Mutex**
- ✅ Quản lý trạng thái thông minh với **Event Groups**
- ✅ Tiết kiệm năng lượng với **Software Timers**

---

## 🚀 Tính năng FreeRTOS

Dự án sử dụng đầy đủ các thành phần quan trọng của FreeRTOS:

### ✔ Tasks (Nhiệm vụ)
Tách chức năng thành nhiều nhiệm vụ chạy song song:

| Task | Chức năng | Độ ưu tiên |
|------|-----------|------------|
| **SensorTask** | Đọc dữ liệu từ DHT22 | 3 |
| **DisplayTask** | Cập nhật OLED | 2 |
| **AlertTask** | Xử lý cảnh báo và buzzer | 4 |

### ✔ Queues (Hàng đợi)
- `sensor_queue`: Truyền dữ liệu nhiệt độ – độ ẩm từ SensorTask → DisplayTask
- `alert_queue`: Gửi thông báo cảnh báo tới AlertTask
- Giảm coupling, tăng tính module

### ✔ Software Timers (Bộ định thời)
- **sensor_timer**: Định kỳ đọc dữ liệu mỗi 1 giây
- **buzzer_timer**: Tự động tắt cảnh báo sau 5 giây
- Tiết kiệm năng lượng, không cần polling

### ✔ Mutex (Loại trừ tương hỗ)
- `i2c_mutex`: Bảo vệ giao tiếp I2C giữa DHT22 và OLED
- Tránh xung đột khi nhiều task dùng chung bus I2C

### ✔ Semaphores (Tín hiệu)
- `data_ready_semaphore`: Đồng bộ khi có dữ liệu mới
- Binary semaphore để báo hiệu sự kiện

### ✔ Event Groups (Nhóm sự kiện)
Quản lý các trạng thái hệ thống:

| Bit | Trạng thái | Mô tả |
|-----|-----------|-------|
| 0 | `NORMAL` | Nhiệt độ bình thường |
| 1 | `WARNING` | Nhiệt độ cao |
| 2 | `OVERHEAT` | Quá nhiệt |
| 3 | `NEW_DATA` | Có dữ liệu mới |

### ✔ Task Notifications (Thông báo nhiệm vụ)
- Đánh thức DisplayTask khi có dữ liệu mới từ cảm biến
- Hiệu quả hơn semaphores cho notify 1-1

---

## 📦 Tính năng hệ thống

### 🌡️ Đọc nhiệt độ & độ ẩm
- Cảm biến: **DHT22** (AM2302)
- Chu kỳ đọc: **1 giây**
- Lọc nhiễu, kiểm tra tính hợp lệ
- Phạm vi: -40°C đến 80°C, 0-100% RH

### 📺 Hiển thị thông tin
- Màn hình: **OLED SSD1306** 128x64
- Giao diện:
  - Nhiệt độ (số lớn + thanh tiến trình)
  - Độ ẩm (số lớn + thanh tiến trình)
  - Trạng thái hệ thống (NORMAL/WARNING/OVERHEAT)
- Cập nhật realtime

### 🔔 Cảnh báo quá nhiệt
- **Buzzer** kích hoạt khi nhiệt độ > ngưỡng
- Tự động tắt sau **5 giây**
- **LED** nhấp nháy theo mức độ nghiêm trọng:
  - Tắt: Bình thường
  - Chậm (1Hz): Cảnh báo
  - Nhanh (4Hz): Quá nhiệt

### 📊 Ghi log qua UART
- Baudrate: **115200**
- Log chi tiết:
  - Dữ liệu cảm biến
  - Thay đổi trạng thái
  - Thống kê hệ thống
  - Debug information

### 🔄 Chuyển trạng thái tự động

```
T < 35°C  → NORMAL    (Bình thường)
T ≥ 35°C  → WARNING   (Cảnh báo)
T ≥ 45°C  → OVERHEAT  (Quá nhiệt)
```

---

## 🛠️ Linh kiện phần cứng

### Danh sách linh kiện

| STT | Linh kiện | Số lượng | Giá (VNĐ) | Ghi chú |
|-----|-----------|----------|-----------|---------|
| 1 | **ESP32-C3-DevKitM-1** | 1 | ~80,000 | Vi điều khiển chính |
| 2 | **DHT22 (AM2302)** | 1 | ~70,000 | Cảm biến nhiệt độ & độ ẩm |
| 3 | **OLED SSD1306 (I2C)** | 1 | ~50,000 | Màn hình 0.96" 128x64 |
| 4 | **Buzzer 5V** | 1 | ~5,000 | Cảnh báo âm thanh |
| 5 | **LED** | 1 | ~1,000 | Báo trạng thái (tùy chọn) |
| 6 | **Breadboard** | 1 | ~15,000 | Để kết nối |
| 7 | **Dây jumper** | 10+ | ~20,000 | Male-Male, Male-Female |
| 8 | **Điện trở 220Ω** | 1 | ~500 | Cho LED |
| 9 | **USB Cable** | 1 | Có sẵn | Cấp nguồn & lập trình |

**Tổng chi phí:** ~240,000 VNĐ

### Thông số kỹ thuật

#### ESP32-C3
- CPU: RISC-V 32-bit, 160MHz
- RAM: 400KB SRAM
- Flash: 4MB
- WiFi/Bluetooth: Có (không dùng trong project này)
- GPIO: 22 pins
- Điện áp: 3.3V

#### DHT22
- Nhiệt độ: -40°C ~ 80°C (±0.5°C)
- Độ ẩm: 0-100% RH (±2%)
- Thời gian đọc: 2 giây
- Giao tiếp: 1-Wire

#### OLED SSD1306
- Kích thước: 0.96"
- Độ phân giải: 128x64 pixels
- Giao tiếp: I2C (0x3C)
- Điện áp: 3.3V/5V

---

## 🔌 Sơ đồ kết nối

### Bảng kết nối chi tiết

| ESP32-C3 Pin | Linh kiện | Pin/Chân |
|--------------|-----------|----------|
| **GPIO 4** | DHT22 | DATA |
| **GPIO 5** | Buzzer | Signal (+) |
| **GPIO 2** | LED | Anode (+) |
| **GPIO 8** | OLED | SDA (I2C Data) |
| **GPIO 9** | OLED | SCL (I2C Clock) |
| **3V3** | DHT22, OLED | VCC/VDD |
| **5V** | Buzzer | VCC |
| **GND** | All | GND |

### Sơ đồ mạch

```
                        ESP32-C3
                     ┌─────────────┐
                     │             │
        DHT22        │  GPIO 4     │
          │          │             │
          ├──────────┤             │
          │          │             │
                     │  GPIO 8 ────├────── OLED SDA
        OLED         │             │
          │          │  GPIO 9 ────├────── OLED SCL
          │          │             │
                     │  GPIO 5 ────├────── Buzzer (+)
        Buzzer       │             │
          │          │  GPIO 2 ────├────── LED (+) ──[220Ω]── GND
          │          │             │
                     │  3V3   ─────├────── DHT22 VCC, OLED VCC
                     │             │
                     │  5V    ─────├────── Buzzer VCC
                     │             │
                     │  GND   ─────├────── Common GND
                     │             │
                     └─────────────┘
```

### Lưu ý kết nối
1. **DHT22**: Nếu module có điện trở kéo lên (pull-up), không cần thêm. Nếu dùng sensor rời, cần điện trở 10kΩ giữa VCC và DATA.
2. **OLED**: Đảm bảo module hỗ trợ 3.3V. Một số module chỉ dùng 5V.
3. **Buzzer**: Nếu buzzer active (có mạch dao động), chỉ cần cấp nguồn. Nếu passive, cần PWM.
4. **LED**: Nhớ dùng điện trở hạn dòng 220Ω-1kΩ.

---

## 💻 Cài đặt

### 1. Yêu cầu hệ thống

- **VSCode** với extension **ESP-IDF**
- **ESP-IDF v5.x** (khuyến nghị v5.1 trở lên)
- **Python 3.8+**
- **Git**
- Driver **CH340** hoặc **CP2102** (cho USB-UART)

### 2. Cài đặt ESP-IDF

#### Trên Linux/macOS:
```bash
# Cài đặt dependencies
sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

# Clone ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git

# Cài đặt tools
cd esp-idf
./install.sh esp32c3

# Thiết lập môi trường (thêm vào ~/.bashrc)
echo "alias get_idf='. $HOME/esp/esp-idf/export.sh'" >> ~/.bashrc
source ~/.bashrc
```

#### Trên Windows:
1. Tải [ESP-IDF Windows Installer](https://dl.espressif.com/dl/esp-idf/)
2. Chạy installer và chọn ESP32-C3
3. Sử dụng **ESP-IDF Command Prompt** hoặc **ESP-IDF PowerShell**

### 3. Clone hoặc download project

```bash
# Clone với Git
git clone https://github.com/yourusername/temp-monitor-esp32c3.git
cd temp-monitor-esp32c3

# Hoặc download ZIP và giải nén
```

### 4. Thiết lập môi trường ESP-IDF

```bash
# Kích hoạt môi trường ESP-IDF
get_idf
# Hoặc
. ~/esp/esp-idf/export.sh
```

### 5. Cấu hình target và menuconfig

```bash
# Đặt target là ESP32-C3
idf.py set-target esp32c3

# Mở menuconfig để cấu hình (tùy chọn)
idf.py menuconfig
```

### 6. Build project

```bash
# Build toàn bộ project
idf.py build
```

### 7. Kết nối phần cứng

Kết nối theo [sơ đồ trên](#-sơ-đồ-kết-nối).

### 8. Flash code vào ESP32-C3

```bash
# Flash với port mặc định
idf.py flash

# Hoặc chỉ định port cụ thể
idf.py -p /dev/ttyUSB0 flash        # Linux
idf.py -p /dev/tty.usbserial-* flash # macOS
idf.py -p COM3 flash                 # Windows
```

### 9. Mở Serial Monitor

```bash
# Monitor với port mặc định
idf.py monitor

# Hoặc chỉ định port
idf.py -p /dev/ttyUSB0 monitor

# Build, Flash và Monitor cùng lúc
idf.py -p /dev/ttyUSB0 flash monitor
```

**Lưu ý:** Nhấn `Ctrl+]` để thoát khỏi monitor.

---

## ⚙️ Cấu hình

### Sử dụng menuconfig

```bash
idf.py menuconfig
```

Điều hướng đến **Temperature Monitor Configuration** để thay đổi cấu hình.

### Chỉnh sửa ngưỡng nhiệt độ

Trong file `main/config.h`:

```c
// Thay đổi các giá trị sau theo nhu cầu
#define TEMP_NORMAL     35.0    // Ngưỡng bình thường (°C)
#define TEMP_WARNING    35.0    // Ngưỡng cảnh báo (°C)
#define TEMP_OVERHEAT   45.0    // Ngưỡng quá nhiệt (°C)
```

### Thay đổi chu kỳ đọc

```c
#define SENSOR_READ_PERIOD_MS   1000    // Đọc cảm biến (ms)
#define DISPLAY_UPDATE_PERIOD   500     // Cập nhật màn hình (ms)
#define BUZZER_DURATION_MS      5000    // Thời gian buzzer (ms)
```

### Cấu hình GPIO

```c
#define DHT_GPIO        GPIO_NUM_4      // GPIO cho DHT22
#define BUZZER_GPIO     GPIO_NUM_5      // GPIO cho Buzzer
#define LED_GPIO        GPIO_NUM_2      // GPIO cho LED
#define I2C_SDA_GPIO    GPIO_NUM_8      // GPIO cho I2C SDA
#define I2C_SCL_GPIO    GPIO_NUM_9      // GPIO cho I2C SCL
```

### Điều chỉnh độ ưu tiên Tasks

```c
#define PRIORITY_SENSOR_TASK    3
#define PRIORITY_DISPLAY_TASK   2
#define PRIORITY_ALERT_TASK     4
```

---

## 📖 Sử dụng

### Khởi động hệ thống

1. Cấp nguồn cho ESP32-C3
2. Hệ thống tự động:
   - Khởi tạo OLED → Hiển thị màn hình chào
   - Khởi tạo DHT22 → Đọc thử
   - Tạo Tasks, Queues, Timers
   - Bắt đầu đọc dữ liệu

### Quan sát hoạt động

#### Trên OLED:
```
== TEMP MONITOR ==
─────────────────
Status: NORMAL
  
25.3 C  [████████░░]
65.0 %  [██████░░░░]
```

#### Trên Serial Monitor:
```
I (325) MAIN: === Temperature Monitor System ===
I (330) MAIN: Initializing system...
I (335) DHT22: DHT22 initialized on GPIO 4
I (340) SSD1306: OLED initialized (128x64)
I (1345) SENSOR: T: 25.3°C, H: 65.0%
I (1350) DISPLAY: Updated: T=25.3, H=65.0, State=NORMAL
I (2345) SENSOR: T: 25.4°C, H: 64.8%
```

### Khi nhiệt độ tăng

1. **T ≥ 35°C** (WARNING):
   - Màn hình: "Status: WARNING" (đảo màu)
   - LED nhấp nháy chậm (1Hz)
   - Buzzer kêu 5 giây
   
2. **T ≥ 45°C** (OVERHEAT):
   - Màn hình: "Status: OVERHEAT" (đảo màu)
   - LED nhấp nháy nhanh (4Hz)
   - Buzzer kêu liên tục (tắt sau 5s, bật lại nếu vẫn quá nhiệt)

### Debug & Monitoring

Mở Serial Monitor (115200 baud) để xem:
- Thống kê mỗi 10 giây
- Chi tiết từng lần đọc
- Thông báo lỗi (nếu có)

---

## 🏗️ Kiến trúc hệ thống

### Cấu trúc thư mục

```
project/
├── CMakeLists.txt          # CMake chính của project
├── sdkconfig               # Cấu hình ESP-IDF
├── sdkconfig.defaults      # Cấu hình mặc định
├── Kconfig.projbuild       # Menu cấu hình tùy chỉnh
├── main/
│   ├── CMakeLists.txt      # CMake của component main
│   ├── main.c              # Entry point - app_main()
│   ├── config.h            # Cấu hình pins, thresholds
│   ├── dht22.c             # Driver DHT22
│   ├── dht22.h
│   ├── ssd1306.c           # Driver OLED SSD1306
│   └── ssd1306.h
└── docs/
    └── freertos_tutorial.md
```

### Luồng dữ liệu

```
        [Software Timer]
               │
               ↓ (1s period)
        ┌──────────────┐
        │ Sensor Task  │
        │  (Read DHT22)│
        └──────┬───────┘
               │
               ↓ (Queue: sensor_data)
        ┌──────────────┐
        │ Display Task │
        │ (Update OLED)│
        └──────┬───────┘
               │
               ↓ (Check Temperature)
        ┌──────────────┐
        │  Alert Task  │
        │ (Buzzer/LED) │
        └──────────────┘
```

### FreeRTOS Objects Diagram

```
┌─────────────────────────────────────────┐
│         FreeRTOS Scheduler              │
└─────────────────────────────────────────┘
            │     │     │
     ┌──────┘     │     └──────┐
     ↓            ↓            ↓
┌────────┐  ┌────────┐  ┌────────┐
│Sensor  │  │Display │  │ Alert  │
│ Task   │  │  Task  │  │  Task  │
└────┬───┘  └───┬────┘  └───┬────┘
     │          │            │
     └──→ [Queue] ──→────────┘
            │
     ┌──────┴──────┬──────────┐
     │             │          │
[I2C Mutex] [Event Group] [Semaphore]
     │             │          │
     └─────────────┴──────────┘
```

### Task State Machine

```
SensorTask:
  IDLE → [Timer Notify] → READ → [Queue Send] → IDLE

DisplayTask:
  WAITING → [Notification] → [Queue Receive] → UPDATE → WAITING

AlertTask:
  LISTENING → [Queue Receive] → ACTIVATE → [Timer] → DEACTIVATE → LISTENING
```

---

## 🔧 Lệnh ESP-IDF thường dùng

### Build & Flash

```bash
# Build project
idf.py build

# Flash vào board
idf.py flash

# Monitor serial output
idf.py monitor

# Build, flash và monitor cùng lúc
idf.py flash monitor

# Chỉ định port
idf.py -p /dev/ttyUSB0 flash monitor
```

### Cấu hình

```bash
# Mở menu cấu hình
idf.py menuconfig

# Đặt target chip
idf.py set-target esp32c3

# Xem cấu hình hiện tại
idf.py size
idf.py size-components
```

### Dọn dẹp

```bash
# Xóa build files
idf.py clean

# Xóa hoàn toàn (bao gồm cả sdkconfig)
idf.py fullclean
```

### Debug

```bash
# Mở GDB debug
idf.py gdb

# OpenOCD debug
idf.py openocd
```

---

## 🔧 Mở rộng

### 1. Thêm WiFi/MQTT
```c
#include "esp_wifi.h"
#include "mqtt_client.h"

// Gửi dữ liệu lên cloud
void mqtt_publish_task(void *pvParameters) {
    sensor_data_t data;
    while(1) {
        if (xQueueReceive(sensor_queue, &data, portMAX_DELAY)) {
            char payload[64];
            snprintf(payload, sizeof(payload), 
                     "{\"temp\":%.1f,\"hum\":%.1f}", 
                     data.temperature, data.humidity);
            esp_mqtt_client_publish(client, "sensor/data", payload, 0, 1, 0);
        }
    }
}
```

### 2. Lưu log vào SD Card
```c
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

// Ghi log vào file
void sd_log_task(void *pvParameters) {
    FILE *f = fopen("/sdcard/log.txt", "a");
    fprintf(f, "%.1f,%.1f\n", temp, hum);
    fclose(f);
}
```

### 3. Thêm cảm biến khác
```c
// Ví dụ: Cảm biến ánh sáng BH1750
#include "bh1750.h"

float lux;
bh1750_read(&dev, &lux);
ESP_LOGI(TAG, "Light: %.1f lux", lux);
```

### 4. Web Server
```c
#include "esp_http_server.h"

// ESP32 Web Server để xem dữ liệu
esp_err_t root_handler(httpd_req_t *req) {
    char resp[128];
    snprintf(resp, sizeof(resp), 
             "<h1>Temperature: %.1f°C</h1>", last_temp);
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
```

---

## 📊 Hiệu năng

### Tài nguyên FreeRTOS

| Task | Stack Size | Priority | CPU Usage |
|------|------------|----------|-----------|
| Sensor | 2KB | 3 | ~5% |
| Display | 4KB | 2 | ~10% |
| Alert | 2KB | 4 | ~2% |
| **IDLE** | - | 0 | ~83% |

### Memory Usage

```bash
# Xem memory usage
idf.py size
idf.py size-components
```

- **Total RAM**: 400KB
- **Used**: ~60KB
- **Free**: ~340KB
- **Stack Safety**: OK (no overflow)

### Power Consumption

- **Active**: ~80mA @ 3.3V
- **Light Sleep**: ~0.8mA
- **Deep Sleep**: ~5µA

---

## 🐛 Troubleshooting

### Vấn đề thường gặp

#### 1. OLED không hiển thị
```bash
# Chạy I2C scanner để kiểm tra địa chỉ
# Trong main_i2c_scanner.c
```
- Kiểm tra địa chỉ I2C (thường là 0x3C hoặc 0x3D)
- Đảm bảo SDA/SCL đúng pin
- Kiểm tra nguồn 3.3V

#### 2. DHT22 đọc lỗi
- Đợi 2 giây sau khi khởi động
- Kiểm tra pull-up resistor (10kΩ)
- Thử GPIO khác

#### 3. Buzzer không kêu
- Kiểm tra loại buzzer (active/passive)
- Đảm bảo nguồn 5V đủ dòng
- Test với `gpio_set_level()` trực tiếp

#### 4. Lỗi build/compile
```bash
# Xóa cache và build lại
idf.py fullclean
idf.py build
```

#### 5. Upload failed
```bash
# Kiểm tra port
ls /dev/ttyUSB*   # Linux
ls /dev/tty.*     # macOS

# Giữ nút BOOT khi flash
idf.py -p /dev/ttyUSB0 flash
```

#### 6. Monitor không hiển thị
```bash
# Kiểm tra baudrate (mặc định 115200)
idf.py -p /dev/ttyUSB0 monitor -b 115200
```

---

## 📚 Tài liệu tham khảo

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [ESP-IDF FreeRTOS SMP](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/system/freertos.html)
- [ESP32-C3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [DHT22 Datasheet](https://www.sparkfun.com/datasheets/Sensors/Temperature/DHT22.pdf)
- [SSD1306 OLED Datasheet](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)

---

## 👥 Tác giả

- **Nhóm Mephisto**
- Học kỳ 9 - 2025
- Trường: Đại học Bách khoa - Đại học Đà Nẵng


