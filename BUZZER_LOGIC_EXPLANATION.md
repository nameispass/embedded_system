# 📯 Giải thích Logic Buzzer - Code Analysis

## 🎯 Tóm tắt
Buzzer sẽ kêu **chu kỳ** khi nhiệt độ ≥ 45°C, mỗi chu kỳ kêu 10 giây, sau đó tắ## ⚠️ **Vấn Đề Cũ (ĐÃ SỬA)**

**Code cũ chỉ kêu buzzer khi trạng thái THAY ĐỔI:**

```c
if (new_state != last_state) {  // ← Chỉ xử lý khi có THAY ĐỔI
    last_state = new_state;
    switch (new_state) { ... }
}
```

Điều này gây ra:
- ✅ **Lần 1**: NORMAL → OVERHEAT = **THAY ĐỔI** ✓ Buzzer ON
- ❌ **Lần 2**: OVERHEAT → OVERHEAT = **KHÔNG thay đổi** ✗ Không làm gì
- ❌ **Lần 3**: OVERHEAT → OVERHEAT = **KHÔNG thay đổi** ✗ Không làm gì

**Kết quả**: Buzzer chỉ kêu **1 lần 10 giây** rồi tắt, không kêu lại dù T vẫn ≥ 45°C!
## 📍 Code Chính

### 1️⃣ **File: main.c - Hàm alert_task() (Dòng 240-285)**

```c
switch (new_state) {
    case STATE_OVERHEAT:
        // NGUY HIỂM: Bật buzzer và LED
        gpio_set_level(BUZZER_PIN, 1);              // ← Bật buzzer
        gpio_set_level(LED_PIN, 1);
        
        // Khởi động timer tự động tắt buzzer sau 10s (chỉ nếu chưa chạy)
        if (xTimerIsTimerActive(buzzer_timer) == pdFALSE) {
            xTimerStart(buzzer_timer, 0);           // ← Bắt đầu timer 10s
        }
        
        ESP_LOGW(TAG, "🚨 ALERT: OVERHEAT! Buzzer ON");
        break;
```

**Giải thích:**
- `gpio_set_level(BUZZER_PIN, 1)`: **Bật buzzer ngay lập tức**
- `if (xTimerIsTimerActive(buzzer_timer) == pdFALSE)`: **Kiểm tra timer có chạy không**
  - Nếu `pdFALSE` (chưa chạy): `xTimerStart()` khởi động timer
  - Nếu `pdTRUE` (đang chạy): Không khởi động lại (điều này **ngăn timer reset**)

---

### 2️⃣ **File: main.c - Hàm buzzer_timer_callback() (Dòng 98-101)**

```c
/**
 * @brief Timer callback: Tắt buzzer sau 10 giây
 */
void buzzer_timer_callback(TimerHandle_t xTimer) {
    gpio_set_level(BUZZER_PIN, 0);                  // ← Tắt buzzer
    ESP_LOGI(TAG, "Buzzer auto-off after 10s");
}
```

**Giải thích:**
- Sau 10 giây, timer gọi callback này
- `gpio_set_level(BUZZER_PIN, 0)`: **Tắt buzzer tự động**
- Timer là **one-shot** (không lặp), nên chỉ gọi 1 lần

---

### 3️⃣ **File: main.c - Hàm get_buzzer_status() (Dòng 76-82)**

```c
/**
 * @brief Lấy trạng thái buzzer (ON nếu timer đang chạy, OFF nếu không)
 */
bool get_buzzer_status(void) {
    if (buzzer_timer == NULL) {
        return false;
    }
    return (xTimerIsTimerActive(buzzer_timer) != pdFALSE);
}
```

**Giải thích:**
- Dùng để kiểm tra buzzer **có đang kêu không**
- `xTimerIsTimerActive(buzzer_timer)`: Trả về `pdTRUE` nếu timer đang chạy
- Được gọi bởi **web dashboard** (GET /api/buzzer) để hiển thị trạng thái

---

## 🔄 Chu kỳ Hoạt động - Chi tiết Từng Bước (SAU KHI SỬA)

### **Tình huống: T liên tục ≥ 45°C**

```
Thời gian (s):    0      5      10     15     20     25     30     35     40
                  │      │      │      │      │      │      │      │      │
Nhiệt độ:         ═══════════════════════════════════════════════════════════════════
                  45.5°C (vẫn quá nhiệt)
                  
Trạng thái task:
├─ 0s:   T = 45.5°C → STATE_OVERHEAT (thay đổi từ NORMAL)
│        ├─ gpio_set_level(LED_PIN, 1)
│        ├─ if (xTimerIsTimerActive(...) == pdFALSE)  ← TRUE
│        ├─ gpio_set_level(BUZZER_PIN, 1)  ← BUZZER BẬT ✅
│        ├─ xTimerStart(buzzer_timer, 0)  ← TIMER START (cycle 1)
│        └─ Log: "🚨 ALERT: OVERHEAT! Buzzer ON (cycle 1)"
│
├─ 1s:   T = 45.6°C → STATE_OVERHEAT (KHÔNG thay đổi, vẫn OVERHEAT)
│        ├─ gpio_set_level(LED_PIN, 1)
│        ├─ if (xTimerIsTimerActive(...) == pdFALSE)  ← FALSE (timer đang chạy)
│        ├─ Skip → Buzzer vẫn kêu ✅
│        └─ Log: (không log, vẫn đợi)
│
├─ 10s:  ⏰ TIMER GỌI CALLBACK (10s hết)
│        ├─ gpio_set_level(BUZZER_PIN, 0)  ← BUZZER TẮT
│        └─ Log: "Buzzer auto-off after 10s"
│
├─ 11s:  T = 45.7°C → STATE_OVERHEAT (KHÔNG thay đổi)
│        ├─ gpio_set_level(LED_PIN, 1)
│        ├─ if (xTimerIsTimerActive(...) == pdFALSE)  ← TRUE (timer đã hết)
│        ├─ gpio_set_level(BUZZER_PIN, 1)  ← BUZZER BẬT LẠI ✅
│        ├─ xTimerStart(buzzer_timer, 0)  ← TIMER START (cycle 2)
│        └─ Log: "🔔 OVERHEAT: Buzzer ON (cycle repeat)"
│
├─ 20s:  ⏰ TIMER GỌI CALLBACK (10s hết)
│        ├─ gpio_set_level(BUZZER_PIN, 0)  ← BUZZER TẮT
│        └─ Log: "Buzzer auto-off after 10s"
│
├─ 21s:  T = 45.8°C → STATE_OVERHEAT
│        ├─ if (xTimerIsTimerActive(...) == pdFALSE)  ← TRUE (timer đã hết)
│        ├─ gpio_set_level(BUZZER_PIN, 1)  ← BUZZER BẬT LẠI ✅
│        ├─ xTimerStart(buzzer_timer, 0)  ← TIMER START (cycle 3)
│        └─ Log: "🔔 OVERHEAT: Buzzer ON (cycle repeat)"
│
│         ...tiếp tục chu kỳ mỗi 10 giây...
│
└─ 45s:  T = 44.2°C → STATE_WARNING (THAY ĐỔI!)
         ├─ gpio_set_level(BUZZER_PIN, 0)  ← BUZZER TẮT
         ├─ gpio_set_level(LED_PIN, 1)
         ├─ xTimerStop(buzzer_timer, 0)  ← TIMER STOP
         └─ Log: "⚠ ALERT: WARNING! LED ON"


✅ HOẠT ĐỘNG ĐÚNG:
   - Kêu lần 1: 0-10s
   - Kêu lần 2: 11-20s (nếu T vẫn ≥ 45°C)
   - Kêu lần 3: 21-30s (nếu T vẫn ≥ 45°C)
   - ...tiếp tục...
   - Dừng khi T < 45°C
```

---

## ⚠️ **Vấn đề Hiện Tại Của Logic**

**Code chỉ kêu buzzer khi trạng thái THAY ĐỔI:**

```c
if (new_state != last_state) {  // ← Chỉ xử lý khi có THAY ĐỔI
    last_state = new_state;
    switch (new_state) { ... }
}
```

Điều này có nghĩa:
- ✅ **Lần 1**: NORMAL → OVERHEAT = **THAY ĐỔI** ✓ Buzzer ON
- ❌ **Lần 2**: OVERHEAT → OVERHEAT = **KHÔNG thay đổi** ✗ Không làm gì
- ❌ **Lần 3**: OVERHEAT → OVERHEAT = **KHÔNG thay đổi** ✗ Không làm gì

---

## ✅ **Cách Hoạt Động Đúng (ĐÃ SỬA)**

Code đã được sửa để buzzer kêu **chu kỳ**:

```c
// ✅ ĐÚNG (kêu chu kỳ):
switch (new_state) {
    case STATE_OVERHEAT:
        gpio_set_level(LED_PIN, 1);
        
        // Kêu buzzer chu kỳ: Nếu timer hết → bắt đầu lại
        if (xTimerIsTimerActive(buzzer_timer) == pdFALSE) {
            gpio_set_level(BUZZER_PIN, 1);           // ← BẬT BUZZER
            xTimerStart(buzzer_timer, 0);            // ← START TIMER 10s
            
            if (new_state != last_state) {
                ESP_LOGW(TAG, "🚨 ALERT: OVERHEAT! Buzzer ON (cycle 1)");
            } else {
                ESP_LOGW(TAG, "🔔 OVERHEAT: Buzzer ON (cycle repeat)");
            }
        }
        break;
}
```

**Thay đổi chính:**
- ❌ Cũ: Chỉ xử lý khi `new_state != last_state`
- ✅ Mới: **Luôn** xử lý logic OVERHEAT trong switch, không cần chờ state thay đổi
- ✅ Mới: Kiểm tra `if (xTimerIsTimerActive(...) == pdFALSE)` mỗi lần
  - Nếu timer chưa chạy → bắt đầu (chu kỳ 1)
  - Nếu timer đang chạy → skip (đang kêu)
  - Nếu timer hết chạy → bắt đầu lại (chu kỳ tiếp)

---

## 🔍 **Các File Liên Quan**

| File | Hàm | Dòng | Mục đích |
|------|-----|------|---------|
| `main.c` | `alert_task()` | 240-285 | Xử lý logic OVERHEAT |
| `main.c` | `buzzer_timer_callback()` | 98-101 | Tắt buzzer sau 10s |
| `main.c` | `get_buzzer_status()` | 76-82 | Lấy trạng thái buzzer |
| `config.h` | #define | 60-70 | Cấu hình GPIO, thresholds |
| `webserver.c` | `buzzer_handler()` | ~324 | REST API /api/buzzer |

---

## 📊 **State Machine Diagram**

```
┌─────────────────────────────────────────────────────────┐
│                    ALERT_TASK                           │
└─────────────────────────────────────────────────────────┘

                    ┌─────────────┐
                    │EVENT_NEW_DATA│
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │ Đọc State  │
                    │ từ EventGrp │
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
         NORMAL      WARNING       OVERHEAT
              │            │            │
              │         LED ON      BUZZER ON
              │            │         LED ON
              │            │         Timer Start
              │            │            │
              └────────────┼────────────┘
                           │
                   ┌───────▼───────┐
                   │ last_state=  │
                   │   new_state   │
                   └───────┬───────┘
                           │
                    ┌──────▼───────┐
                    │ Wait Event   │
                    │ (next cycle) │
                    └──────────────┘

        BUZZER TIMER (10s one-shot)
              │
              ├─ Được start từ STATE_OVERHEAT
              ├─ Sau 10s gọi callback
              └─ Callback tắt buzzer
```

---

## 🎓 **Tóm Tắt Học Kỳ**

**Code thể hiện:**
1. ✅ Event Group - Quản lý trạng thái hệ thống
2. ✅ Software Timer - Tắt buzzer tự động sau 10s
3. ✅ GPIO Control - Bật/tắt buzzer
4. ✅ State Machine - Xử lý logic từng trạng thái
5. ⚠️ **Hạn chế**: Chỉ kêu 1 lần nếu T vẫn ≥ 45°C

