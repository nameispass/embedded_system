# Hướng dẫn thực hành FreeRTOS
## FreeRTOS Tutorial & Best Practices

## Mục lục
1. [Giới thiệu FreeRTOS](#1-giới-thiệu-freertos)
2. [Tasks (Nhiệm vụ)](#2-tasks-nhiệm-vụ)
3. [Queues (Hàng đợi)](#3-queues-hàng-đợi)
4. [Semaphores](#4-semaphores)
5. [Mutex](#5-mutex)
6. [Event Groups](#6-event-groups)
7. [Software Timers](#7-software-timers)
8. [Task Notifications](#8-task-notifications)
9. [Best Practices](#9-best-practices)

---

## 1. Giới thiệu FreeRTOS

**FreeRTOS** là hệ điều hành thời gian thực (RTOS) mã nguồn mở, phổ biến nhất cho embedded systems.

### Ưu điểm:
- ✅ Đa nhiệm preemptive
- ✅ Nhỏ gọn, hiệu năng cao
- ✅ Portable (hỗ trợ nhiều MCU)
- ✅ Miễn phí, MIT license

### Khái niệm cơ bản:

| Khái niệm | Mô tả |
|-----------|-------|
| **Task** | Một "luồng" thực thi độc lập |
| **Scheduler** | Bộ điều phối quyết định task nào chạy |
| **Priority** | Độ ưu tiên (0 = thấp nhất) |
| **Tick** | Đơn vị thời gian cơ bản (thường 1ms) |
| **Context Switch** | Chuyển giữa các tasks |

---

## 2. Tasks (Nhiệm vụ)

### Tạo Task

```cpp
void vTaskFunction(void *pvParameters) {
    // Task code
    while(1) {
        // Do something
        vTaskDelay(pdMS_TO_TICKS(1000));  // Delay 1 giây
    }
}

void setup() {
    xTaskCreate(
        vTaskFunction,      // Hàm task
        "TaskName",         // Tên (để debug)
        2048,               // Stack size (bytes)
        NULL,               // Parameters
        2,                  // Priority (0-configMAX_PRIORITIES)
        NULL                // Task handle
    );
}
```

### Ví dụ trong project

```cpp
// Sensor Task - đọc DHT22
void vSensorTask(void *pvParameters) {
    SensorData_t data;
    while(1) {
        // Đợi notify từ timer
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Đọc sensor
        if (sensor.readSensor(&data)) {
            // Gửi data vào queue
            xQueueSend(xSensorDataQueue, &data, 0);
        }
    }
}

// Tạo task trong setup()
xTaskCreate(
    vSensorTask,
    "SensorTask",
    STACK_SIZE_SENSOR,      // 2048 bytes
    NULL,
    PRIORITY_SENSOR_TASK,   // Priority = 3
    &xSensorTaskHandle
);
```

### Task States

```
┌─────────┐   Create    ┌─────────┐
│ Dormant │─────────────►│  Ready  │
└─────────┘              └────┬────┘
                              │ ▲
                   Scheduler  │ │ Preempt/Yield
                              ▼ │
                         ┌─────────┐
                         │ Running │
                         └────┬────┘
                              │
                   Delay/Wait │
                              ▼
                         ┌─────────┐
                         │ Blocked │
                         └─────────┘
```

### Task Priority

- **0**: Idle Task (lowest)
- **1-3**: Low priority (background tasks)
- **4-7**: Medium priority (normal tasks)
- **8+**: High priority (critical tasks)

**Quy tắc**:
- Task ưu tiên cao hơn sẽ chạy trước
- Task cùng priority → Round-robin
- Alert Task > Sensor Task > Display Task

---

## 3. Queues (Hàng đợi)

**Queue** là cách an toàn để truyền dữ liệu giữa các tasks.

### Tạo Queue

```cpp
// Định nghĩa kiểu dữ liệu
typedef struct {
    float temperature;
    float humidity;
    uint32_t timestamp;
} SensorData_t;

// Tạo queue
QueueHandle_t xSensorDataQueue;
xSensorDataQueue = xQueueCreate(5, sizeof(SensorData_t));
//                               ^   ^
//                            Length  Item size
```

### Gửi dữ liệu (Producer)

```cpp
void vSensorTask(void *pvParameters) {
    SensorData_t data;
    while(1) {
        // Đọc sensor
        data.temperature = readTemp();
        data.humidity = readHumidity();
        data.timestamp = millis();
        
        // Gửi vào queue (chờ tối đa 100ms)
        if (xQueueSend(xSensorDataQueue, &data, pdMS_TO_TICKS(100)) == pdTRUE) {
            Serial.println("Data sent!");
        } else {
            Serial.println("Queue full!");
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### Nhận dữ liệu (Consumer)

```cpp
void vDisplayTask(void *pvParameters) {
    SensorData_t receivedData;
    while(1) {
        // Nhận từ queue (chờ vô hạn)
        if (xQueueReceive(xSensorDataQueue, &receivedData, portMAX_DELAY) == pdTRUE) {
            // Xử lý dữ liệu
            Serial.printf("Received: T=%.1f, H=%.1f\n", 
                         receivedData.temperature, 
                         receivedData.humidity);
            updateDisplay(&receivedData);
        }
    }
}
```

### Queue Behaviors

| Function | Blocking | Use Case |
|----------|----------|----------|
| `xQueueSend()` | Yes | Producer |
| `xQueueSendFromISR()` | No | ISR |
| `xQueueReceive()` | Yes | Consumer |
| `xQueuePeek()` | Yes | Xem không lấy ra |
| `uxQueueMessagesWaiting()` | No | Kiểm tra số lượng |

---

## 4. Semaphores

**Semaphore** dùng để đồng bộ giữa các tasks.

### Binary Semaphore

```cpp
// Tạo
SemaphoreHandle_t xDataReadySemaphore;
xDataReadySemaphore = xSemaphoreCreateBinary();

// Task A: Báo hiệu có dữ liệu
void vSensorTask(void *pvParameters) {
    while(1) {
        readSensorData();
        xSemaphoreGive(xDataReadySemaphore);  // Give = signal
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Task B: Đợi dữ liệu
void vProcessTask(void *pvParameters) {
    while(1) {
        // Chờ semaphore (timeout 5s)
        if (xSemaphoreTake(xDataReadySemaphore, pdMS_TO_TICKS(5000)) == pdTRUE) {
            processData();
        } else {
            Serial.println("Timeout waiting for data!");
        }
    }
}
```

### Counting Semaphore

```cpp
// Tạo với giá trị ban đầu = 3, max = 10
SemaphoreHandle_t xCountingSem = xSemaphoreCreateCounting(10, 3);

// Producer (tăng count)
xSemaphoreGive(xCountingSem);

// Consumer (giảm count)
if (xSemaphoreTake(xCountingSem, 0) == pdTRUE) {
    // Got one resource
}
```

**Use cases**:
- Binary: Synchronization (sensor ready, data available)
- Counting: Resource pool (buffer slots, connections)

---

## 5. Mutex

**Mutex** (Mutual Exclusion) bảo vệ tài nguyên dùng chung.

### Tạo Mutex

```cpp
SemaphoreHandle_t xI2CMutex;
xI2CMutex = xSemaphoreCreateMutex();
```

### Sử dụng Mutex

```cpp
void vSensorTask(void *pvParameters) {
    while(1) {
        // Lock I2C bus
        if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // === CRITICAL SECTION ===
            readDHT22();  // Dùng I2C
            // === END CRITICAL SECTION ===
            
            // Unlock
            xSemaphoreGive(xI2CMutex);
        } else {
            Serial.println("Failed to lock I2C!");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void vDisplayTask(void *pvParameters) {
    while(1) {
        // Lock I2C bus
        if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // === CRITICAL SECTION ===
            updateOLED();  // Dùng I2C
            // === END CRITICAL SECTION ===
            
            xSemaphoreGive(xI2CMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

### Priority Inversion Problem

```
Tình huống:
1. Task Low (priority 1) lock mutex
2. Task High (priority 10) đợi mutex → BLOCKED
3. Task Medium (priority 5) preempt Task Low
   → Task High phải đợi Task Medium xong!

Giải pháp: Priority Inheritance
→ FreeRTOS tự động tăng priority của Task Low lên 10 tạm thời
```

---

## 6. Event Groups

**Event Groups** quản lý nhiều flags/events (lên đến 24 bits).

### Tạo Event Group

```cpp
EventGroupHandle_t xSystemEventGroup;
xSystemEventGroup = xEventGroupCreate();

// Định nghĩa các bits
#define EVENT_STATE_NORMAL      (1 << 0)  // Bit 0
#define EVENT_STATE_WARNING     (1 << 1)  // Bit 1
#define EVENT_STATE_OVERHEAT    (1 << 2)  // Bit 2
#define EVENT_NEW_DATA          (1 << 3)  // Bit 3
```

### Set Bits (Báo hiệu sự kiện)

```cpp
void updateSystemState(float temperature) {
    // Clear old state bits
    xEventGroupClearBits(xSystemEventGroup, 
                        EVENT_STATE_NORMAL | EVENT_STATE_WARNING | EVENT_STATE_OVERHEAT);
    
    // Set new state
    if (temperature < 35.0) {
        xEventGroupSetBits(xSystemEventGroup, EVENT_STATE_NORMAL);
    } else if (temperature < 45.0) {
        xEventGroupSetBits(xSystemEventGroup, EVENT_STATE_WARNING);
    } else {
        xEventGroupSetBits(xSystemEventGroup, EVENT_STATE_OVERHEAT);
    }
}
```

### Wait for Bits (Đợi sự kiện)

```cpp
void vAlertTask(void *pvParameters) {
    while(1) {
        // Đợi bit WARNING hoặc OVERHEAT
        EventBits_t uxBits = xEventGroupWaitBits(
            xSystemEventGroup,
            EVENT_STATE_WARNING | EVENT_STATE_OVERHEAT,  // Bits to wait
            pdFALSE,    // Don't clear on exit
            pdFALSE,    // Wait for ANY bit (not ALL)
            portMAX_DELAY
        );
        
        if (uxBits & EVENT_STATE_WARNING) {
            activateBuzzer(true);
        }
        if (uxBits & EVENT_STATE_OVERHEAT) {
            activateEmergencyShutdown();
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### Check Bits (Không block)

```cpp
EventBits_t uxBits = xEventGroupGetBits(xSystemEventGroup);
if (uxBits & EVENT_NEW_DATA) {
    Serial.println("New data available!");
}
```

---

## 7. Software Timers

**Software Timer** thực thi callback sau một khoảng thời gian.

### Tạo Timer

```cpp
TimerHandle_t xSensorTimerHandle;

// Callback function
void vSensorTimerCallback(TimerHandle_t xTimer) {
    // Notify sensor task to read
    xTaskNotifyGive(xSensorTaskHandle);
}

void setup() {
    // Tạo timer
    xSensorTimerHandle = xTimerCreate(
        "SensorTimer",              // Tên
        pdMS_TO_TICKS(1000),        // Period (1 giây)
        pdTRUE,                     // Auto-reload (lặp lại)
        (void *)0,                  // Timer ID
        vSensorTimerCallback        // Callback
    );
    
    // Start timer
    xTimerStart(xSensorTimerHandle, 0);
}
```

### One-shot Timer

```cpp
// Timer tắt buzzer sau 5 giây
TimerHandle_t xBuzzerTimerHandle;

void vBuzzerTimerCallback(TimerHandle_t xTimer) {
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("Buzzer auto-off");
}

xBuzzerTimerHandle = xTimerCreate(
    "BuzzerTimer",
    pdMS_TO_TICKS(5000),    // 5 seconds
    pdFALSE,                // One-shot (không lặp)
    (void *)0,
    vBuzzerTimerCallback
);

// Kích hoạt buzzer
digitalWrite(BUZZER_PIN, HIGH);
xTimerStart(xBuzzerTimerHandle, 0);  // Sẽ tắt sau 5s
```

### Timer Control

```cpp
// Start/Stop
xTimerStart(xTimer, 0);
xTimerStop(xTimer, 0);

// Reset (restart lại từ đầu)
xTimerReset(xTimer, 0);

// Đổi period
xTimerChangePeriod(xTimer, pdMS_TO_TICKS(2000), 0);

// Kiểm tra đang chạy
if (xTimerIsTimerActive(xTimer)) {
    Serial.println("Timer is running");
}
```

**Lưu ý**: Timer callbacks chạy trong Timer Daemon Task, nên giữ ngắn gọn!

---

## 8. Task Notifications

**Task Notification** là cách nhẹ nhất để báo hiệu giữa 2 tasks (thay semaphore).

### Direct-to-Task Notification

```cpp
// Sender (từ Task hoặc Timer)
void vSensorTimerCallback(TimerHandle_t xTimer) {
    // Notify Sensor Task
    xTaskNotifyGive(xSensorTaskHandle);  // Tăng counter lên 1
}

// Receiver
void vSensorTask(void *pvParameters) {
    while(1) {
        // Đợi notification (counter giảm 1)
        uint32_t ulNotificationValue = ulTaskNotifyTake(
            pdTRUE,         // Clear on exit
            portMAX_DELAY   // Wait forever
        );
        
        if (ulNotificationValue > 0) {
            Serial.printf("Received %u notifications\n", ulNotificationValue);
            readSensor();
        }
    }
}
```

### Notify with Value

```cpp
// Sender
uint32_t alertLevel = 2;  // WARNING level
xTaskNotify(
    xAlertTaskHandle,       // Target task
    alertLevel,             // Value to send
    eSetValueWithOverwrite  // Action
);

// Receiver
void vAlertTask(void *pvParameters) {
    uint32_t ulNotifiedValue;
    while(1) {
        if (xTaskNotifyWait(
            0,              // Don't clear bits on entry
            0xFFFFFFFF,     // Clear all bits on exit
            &ulNotifiedValue,
            portMAX_DELAY
        ) == pdTRUE) {
            Serial.printf("Alert level: %u\n", ulNotifiedValue);
            handleAlert(ulNotifiedValue);
        }
    }
}
```

### Notify from ISR

```cpp
void IRAM_ATTR buttonISR() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Notify task
    vTaskNotifyGiveFromISR(xButtonTaskHandle, &xHigherPriorityTaskWoken);
    
    // Yield if needed
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
```

**So sánh**:

| Feature | Binary Semaphore | Task Notification |
|---------|------------------|-------------------|
| RAM | ~80 bytes | 8 bytes |
| Speed | Slower | Faster (3x) |
| Multiple receivers | Yes | No (1-to-1 only) |
| ISR safe | Yes | Yes |

---

## 9. Best Practices

### ✅ DO's

#### 1. Dùng vTaskDelay() thay vì busy-wait
```cpp
// ❌ BAD: Waste CPU
void badTask(void *pvParameters) {
    while(1) {
        readSensor();
        // Busy waiting
        for(int i = 0; i < 1000000; i++);
    }
}

// ✅ GOOD: Release CPU to other tasks
void goodTask(void *pvParameters) {
    while(1) {
        readSensor();
        vTaskDelay(pdMS_TO_TICKS(1000));  // Yield CPU
    }
}
```

#### 2. Luôn kiểm tra return value
```cpp
if (xQueueSend(xQueue, &data, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("Queue send failed!");
    // Handle error
}
```

#### 3. Giới hạn thời gian trong Critical Section
```cpp
if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Keep this SHORT!
    protectedResource++;
    xSemaphoreGive(xMutex);
}
```

#### 4. Dùng `pdMS_TO_TICKS()` cho portable code
```cpp
// ✅ GOOD: Tự động convert ms → ticks
vTaskDelay(pdMS_TO_TICKS(1000));  // 1 second

// ❌ BAD: Hard-coded ticks (depends on configTICK_RATE_HZ)
vTaskDelay(1000);  // Có thể sai!
```

#### 5. Đặt tên rõ ràng cho tasks
```cpp
xTaskCreate(vSensorTask, "SensorTask", ...);  // Good for debugging
```

### ❌ DON'T's

#### 1. Không dùng delay() trong tasks
```cpp
// ❌ BAD: Block toàn bộ scheduler
void badTask(void *pvParameters) {
    while(1) {
        readSensor();
        delay(1000);  // Arduino delay, blocking!
    }
}
```

#### 2. Không dùng functions "FromISR" trong tasks
```cpp
// ❌ BAD: Wrong context
void task(void *pvParameters) {
    xQueueSendFromISR(...);  // Only use in ISR!
}
```

#### 3. Không quên xSemaphoreGive() sau xSemaphoreTake()
```cpp
// ❌ BAD: Deadlock risk
if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
    if (errorCondition) {
        return;  // Forgot to Give!
    }
    xSemaphoreGive(xMutex);
}

// ✅ GOOD: Always give, even on error
if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
    if (errorCondition) {
        xSemaphoreGive(xMutex);
        return;
    }
    doWork();
    xSemaphoreGive(xMutex);
}
```

#### 4. Không malloc() trong tasks lặp
```cpp
// ❌ BAD: Memory leak
void badTask(void *pvParameters) {
    while(1) {
        char *buffer = (char*)malloc(100);  // Leak!
        processData(buffer);
        // Forgot to free()
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ✅ GOOD: Static allocation
void goodTask(void *pvParameters) {
    static char buffer[100];  // Allocated once
    while(1) {
        processData(buffer);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### 🛡️ Stack Overflow Protection

```cpp
// Enable in FreeRTOSConfig.h
#define configCHECK_FOR_STACK_OVERFLOW 2

// Hook function
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    Serial.printf("STACK OVERFLOW in task: %s\n", pcTaskName);
    while(1);  // Halt
}
```

### 📊 Task Statistics

```cpp
void printTaskStats() {
    char buffer[512];
    vTaskList(buffer);
    Serial.println("Task\t\tState\tPrio\tStack\tNum");
    Serial.println(buffer);
    
    vTaskGetRunTimeStats(buffer);
    Serial.println("\nTask\t\tAbs Time\t% Time");
    Serial.println(buffer);
}
```

### 🔍 Debugging Tips

```cpp
// 1. Watchdog timer
esp_task_wdt_init(5, true);  // 5s timeout
esp_task_wdt_add(NULL);      // Add current task

// 2. Heap monitoring
Serial.printf("Free heap: %u\n", ESP.getFreeHeap());
Serial.printf("Min free heap: %u\n", ESP.getMinFreeHeap());

// 3. Assert macros
configASSERT(xQueue != NULL);  // Halt if NULL
```

---

## Tổng kết

| Feature | Use Case | Complexity |
|---------|----------|------------|
| **Tasks** | Chạy song song nhiều công việc | ⭐⭐ |
| **Queues** | Truyền dữ liệu giữa tasks | ⭐⭐ |
| **Semaphores** | Đồng bộ sự kiện | ⭐⭐ |
| **Mutex** | Bảo vệ tài nguyên dùng chung | ⭐⭐⭐ |
| **Event Groups** | Quản lý nhiều flags | ⭐⭐⭐ |
| **Timers** | Định kỳ thực thi | ⭐⭐ |
| **Notifications** | Báo hiệu nhanh 1-1 | ⭐ |

**Khuyến nghị học tập**:
1. Bắt đầu với Tasks và Queues
2. Thêm Semaphores/Mutex khi cần đồng bộ
3. Nâng cao: Event Groups, Timers, Notifications

---

Chúc bạn thành công với FreeRTOS! 🚀
