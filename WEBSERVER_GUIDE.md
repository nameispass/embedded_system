# 🚀 HTTP Webserver Integration Guide

## Tổng Quan

Dự án của bạn hiện đã được tích hợp hoàn chỉnh với:
- ✅ HTTP REST API Server
- ✅ WiFi Connectivity
- ✅ Web Dashboard (HTML + JavaScript)
- ✅ Real-time Data Sync với FreeRTOS tasks
- ✅ Cấu hình qua HTTP API
- ✅ Lịch sử dữ liệu

---

## 📋 Danh Sách Files Mới

| File | Mục đích |
|------|---------|
| `webserver.h` | Header cho module webserver |
| `webserver.c` | Implementation webserver + REST API |
| `wifi.h` | Header cho module WiFi |
| `wifi.c` | Implementation WiFi connectivity |
| `WEBSERVER_GUIDE.md` | File này - hướng dẫn cấu hình |

---

## 🔧 Cấu Hình Trước Khi Compile

### 1️⃣ **Cấu Hình WiFi**

Mở file `main/config.h` và sửa:

```c
// Line ~50
#define WIFI_SSID               "Your_SSID"          // ← Thay đổi SSID WiFi
#define WIFI_PASSWORD           "Your_Password"      // ← Thay đổi mật khẩu
```

**Ví dụ:**
```c
#define WIFI_SSID               "MyHomeWiFi"
#define WIFI_PASSWORD           "MyPassword123"
```

### 2️⃣ **Bật/Tắt Webserver (Tuỳ chọn)**

```c
// Line ~55
#define ENABLE_WEBSERVER        1                    // 1 = Bật, 0 = Tắt
```

### 3️⃣ **Cấu Hình Ngưỡng Nhiệt Độ (Tuỳ chọn)**

```c
// Line ~68-70
#define TEMP_WARNING            35.0f   // Ngưỡng cảnh báo
#define TEMP_OVERHEAT           45.0f   // Ngưỡng quá nhiệt
```

---

## 🔨 Build và Flashing

### Step 1: Clean build
```bash
idf.py fullclean
```

### Step 2: Build project
```bash
idf.py build
```

### Step 3: Flash to ESP32
```bash
idf.py -p /dev/ttyUSB0 flash
```

### Step 4: Monitor logs
```bash
idf.py -p /dev/ttyUSB0 monitor
```

---

## 📊 REST API Endpoints

### 1. **GET /api/sensor** - Lấy dữ liệu cảm biến hiện tại

**Request:**
```bash
curl http://192.168.1.100/api/sensor
```

**Response:**
```json
{
  "temperature": 28.5,
  "humidity": 65.2,
  "status": "NORMAL",
  "is_valid": true,
  "timestamp": 1234567890
}
```

### 2. **GET /api/status** - Lấy trạng thái (ngắn gọn)

**Request:**
```bash
curl http://192.168.1.100/api/status
```

**Response:**
```json
{
  "temp": 28.5,
  "humi": 65.2,
  "state": "NORMAL"
}
```

### 3. **GET /api/config** - Lấy cấu hình hiện tại

**Request:**
```bash
curl http://192.168.1.100/api/config
```

**Response:**
```json
{
  "temp_warning": 35.0,
  "temp_overheat": 45.0,
  "sensor_interval_ms": 1000,
  "buzzer_enabled": true
}
```

### 4. **POST /api/config** - Cập nhật cấu hình

**Request:**
```bash
curl -X POST http://192.168.1.100/api/config \
  -H "Content-Type: application/json" \
  -d '{
    "temp_warning": 32.0,
    "temp_overheat": 40.0,
    "buzzer_enabled": true
  }'
```

**Response:**
```json
{
  "temp_warning": 32.0,
  "temp_overheat": 40.0,
  "sensor_interval_ms": 1000,
  "buzzer_enabled": true
}
```

### 5. **GET /api/history** - Lấy lịch sử dữ liệu

**Request:**
```bash
curl "http://192.168.1.100/api/history?limit=10&offset=0"
```

**Response:**
```json
{
  "total": 125,
  "limit": 10,
  "offset": 0,
  "records": [
    {
      "temperature": 28.5,
      "humidity": 65.2,
      "status": "NORMAL",
      "timestamp": 1234567890
    },
    ...
  ]
}
```

### 6. **GET /** - Web Dashboard

**Truy cập trình duyệt:**
```
http://192.168.1.100/
```

Bạn sẽ thấy dashboard HTML với:
- 📊 Dữ liệu cảm biến real-time
- ⚙️ Cấu hình hệ thống
- 📈 Lịch sử 5 bản ghi gần nhất

---

## 🌐 Web Dashboard

### Tính năng:
- ✅ Real-time temperature & humidity display
- ✅ Status indicator (NORMAL / WARNING / DANGER)
- ✅ Configuration management UI
- ✅ Recent history viewer
- ✅ Auto-refresh mỗi 2 giây

### Cách sử dụng:
1. Mở trình duyệt (trên điện thoại hoặc máy tính)
2. Truy cập: `http://<IP_ADDRESS>`
3. Thay đổi ngưỡng cảnh báo trong UI
4. Nhấn "Update Config"

---

## 📱 Dùng với Client Khác

### Python Script Example

```python
import requests
import json
import time

# IP của ESP32
BASE_URL = "http://192.168.1.100"

# Lấy dữ liệu sensor
def get_sensor_data():
    response = requests.get(f"{BASE_URL}/api/sensor")
    return response.json()

# Cập nhật config
def update_config(temp_warning, temp_overheat):
    data = {
        "temp_warning": temp_warning,
        "temp_overheat": temp_overheat
    }
    response = requests.post(f"{BASE_URL}/api/config", json=data)
    return response.json()

# Lấy lịch sử
def get_history(limit=10):
    response = requests.get(f"{BASE_URL}/api/history?limit={limit}")
    return response.json()

# Test
if __name__ == "__main__":
    while True:
        data = get_sensor_data()
        print(f"Temp: {data['temperature']}°C, Humidity: {data['humidity']}%")
        print(f"Status: {data['status']}\n")
        time.sleep(2)
```

### Node.js Script Example

```javascript
const http = require('http');

const ESP32_IP = '192.168.1.100';

async function getSensorData() {
  return fetch(`http://${ESP32_IP}/api/sensor`)
    .then(r => r.json());
}

async function updateConfig(config) {
  return fetch(`http://${ESP32_IP}/api/config`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(config)
  }).then(r => r.json());
}

// Test
async function main() {
  const data = await getSensorData();
  console.log(`Temperature: ${data.temperature}°C`);
  console.log(`Status: ${data.status}`);
}

main();
```

---

## 🐛 Troubleshooting

### ❌ "WiFi connection failed"
- **Nguyên nhân:** SSID hoặc password sai
- **Giải pháp:** 
  1. Kiểm tra SSID và password trong config.h
  2. Đảm bảo WiFi router đang hoạt động
  3. Check Serial log: `idf.py monitor`

### ❌ "Cannot connect to http://192.168.1.100"
- **Nguyên nhân:** Sai IP address
- **Giải pháp:**
  1. Mở Serial Monitor
  2. Tìm dòng: `got ip: XXX.XXX.XXX.XXX`
  3. Sử dụng IP đó trong trình duyệt

### ❌ "HTTP server failed to start"
- **Nguyên nhân:** Port 80 bị chiếm
- **Giải pháp:** 
  1. Sử dụng port khác (thay đổi `HTTP_SERVER_PORT` trong config.h)
  2. Rebuild project

### ❌ "cJSON not found error"
- **Nguyên nhân:** Component chưa cài đặt
- **Giải pháp:**
  ```bash
  idf.py fullclean
  idf.py build
  ```

---

## 📈 Kiến Trúc Hệ Thống

```
┌─────────────────────────────────────────────────────────────┐
│                      ESP32-C3                                │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │          FREERTOS KERNEL (4 Cores)                    │ │
│  │                                                        │ │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐            │ │
│  │  │ Sensor   │  │ Display  │  │ Alert    │            │ │
│  │  │ Task     │  │ Task     │  │ Task     │            │ │
│  │  └──────────┘  └──────────┘  └──────────┘            │ │
│  │       │            │              │                  │ │
│  │       └────────────┼──────────────┘                  │ │
│  │            Queue + Event Group                       │ │
│  │                    │                                 │ │
│  │            ┌───────┴────────┐                        │ │
│  │            │                │                        │ │
│  │        Webserver Module      │                       │ │
│  │     (REST API + History)     │                       │ │
│  └────────────────────────────────────────────────────────┘ │
│                       │                                      │
│         ┌─────────────┴─────────────┐                       │
│         │                           │                       │
│      Hardware:                  Network:                    │
│    • DHT22 (I2C)            • WiFi Module                  │
│    • OLED SSD1306 (I2C)     • HTTP Server (Port 80)        │
│    • Buzzer (GPIO)                                         │
│    • LED (GPIO)                                            │
└─────────────────────────────────────────────────────────────┘
```

---

## 🎯 Workflow Thực Tế

1. **Sensor Task** (Priority 5)
   - Đọc DHT22 mỗi 1 giây
   - Cập nhật webserver: `webserver_update_sensor_data()`
   - Gửi data qua Queue

2. **Display Task** (Priority 4)
   - Nhận notification từ Sensor Task
   - Cập nhật OLED display
   - Lấy Mutex I2C

3. **Alert Task** (Priority 3)
   - Đợi sự kiện NEW_DATA từ Event Group
   - Bật/tắt buzzer và LED

4. **HTTP Server** (Priority ≥ 2)
   - Chạy độc lập
   - Respond to API requests
   - Trả về dữ liệu từ webserver module
   - Thread-safe (sử dụng Mutex)

---

## ✅ Checklist Trước Khi Deploy

- [ ] Sửa WIFI_SSID trong config.h
- [ ] Sửa WIFI_PASSWORD trong config.h
- [ ] Build project: `idf.py build`
- [ ] Flash: `idf.py flash`
- [ ] Monitor logs: `idf.py monitor`
- [ ] Tìm IP address từ logs
- [ ] Truy cập `http://<IP>` trong browser
- [ ] Test API endpoints với curl
- [ ] Thay đổi config qua web UI
- [ ] Kiểm tra lịch sử dữ liệu

---

## 📚 Tham Khảo Thêm

- **ESP-IDF Documentation:** https://docs.espressif.com/projects/esp-idf/
- **FreeRTOS Documentation:** https://www.freertos.org/
- **cJSON Library:** https://github.com/DaveGamble/cJSON
- **HTTP Server API:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html

---

## 💡 Mở Rộng Tiếp

- [ ] Thêm HTTPS/SSL support
- [ ] MQTT integration (Cloud)
- [ ] WebSocket for real-time updates
- [ ] Database integration (NVS Flash)
- [ ] OTA firmware update
- [ ] BLE support

---

**Chúc bạn thành công! 🎉**
