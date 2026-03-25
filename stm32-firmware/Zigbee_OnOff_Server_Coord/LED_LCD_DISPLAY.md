# 📺 LED 和 LCD 状态显示功能

## ✅ 已添加功能

### 1️⃣ LED 状态指示

**RGB LED 显示规则：**

| 断路器状态 | LED 颜色 | 说明 |
|-----------|---------|------|
| **闭合（ON）** | 🔴 **红色** | 断路器闭合，负载通电 |
| **断开（OFF）** | 🔵 **蓝色** | 断路器断开，负载断电 |
| **初始化** | 🔵 **蓝色** | 系统启动，默认断开 |

---

### 2️⃣ LCD 屏幕显示

#### 显示布局

```
┌──────────────────────┐
│  Zigbee Breaker      │  ← 第 1 行：标题
│  Status: ON          │  ← 第 2 行：断路器状态
│  LED: RED            │  ← 第 3 行：LED 颜色
│  Addr:0x544d         │  ← 第 4 行：设备地址
└──────────────────────┘
```

#### 不同阶段的显示

**阶段 1：系统初始化**
```
┌──────────────────────┐
│  Zigbee Breaker      │
│  Initializing...     │
│                      │
│                      │
└──────────────────────┘
```

**阶段 2：网络就绪**
```
┌──────────────────────┐
│  Zigbee Breaker      │
│  Network Ready       │
│  Waiting device      │
│                      │
└──────────────────────┘
```

**阶段 3：断路器控制（闭合）**
```
┌──────────────────────┐
│  Zigbee Breaker      │
│  Status: ON          │
│  LED: RED            │
│  Addr:0x544d         │
└──────────────────────┘
```

**阶段 4：断路器控制（断开）**
```
┌──────────────────────┐
│  Zigbee Breaker      │
│  Status: OFF         │
│  LED: BLUE           │
│  Addr:0x544d         │
└──────────────────────┘
```

---

## 🔧 实现细节

### 代码修改位置

**文件：** `STM32_WPAN/App/app_zigbee.c`

#### 1. 添加头文件
```c
#include "stm32wb5mm_dk_lcd.h"
#include "stm32_lcd.h"
```

#### 2. 添加状态变量
```c
struct zigbee_app_info {
  // ... 其他成员
  bool breaker_on;  // 断路器当前状态
};
```

#### 3. 初始化显示
```c
static void APP_ZIGBEE_StackLayersInit(void)
{
  // 初始化变量
  zigbee_app_info.breaker_on = false;

  // 显示初始状态
  BSP_LCD_Clear(LCD_Inst, SSD1315_COLOR_BLACK);
  UTIL_LCD_DisplayStringAt(0, 0, (uint8_t*)"Zigbee Breaker", CENTER_MODE);
  UTIL_LCD_DisplayStringAt(0, 16, (uint8_t*)"Initializing...", CENTER_MODE);
  BSP_LCD_Refresh(LCD_Inst);

  // 设置初始 LED（蓝色）
  LED_Set_rgb(PWM_LED_GSDATA_OFF, PWM_LED_GSDATA_OFF, PWM_LED_GSDATA_47_0);
}
```

#### 4. 更新状态函数
```c
static void APP_ZIGBEE_UpdateBreakerStatus(bool on)
{
  // 更新状态变量
  zigbee_app_info.breaker_on = on;

  // 更新 LED
  if (on) {
    LED_Set_rgb(PWM_LED_GSDATA_47_0, PWM_LED_GSDATA_OFF, PWM_LED_GSDATA_OFF);  // 红色
  } else {
    LED_Set_rgb(PWM_LED_GSDATA_OFF, PWM_LED_GSDATA_OFF, PWM_LED_GSDATA_47_0);  // 蓝色
  }

  // 更新 LCD
  BSP_LCD_Clear(LCD_Inst, SSD1315_COLOR_BLACK);
  UTIL_LCD_DisplayStringAt(0, 0, (uint8_t*)"Zigbee Breaker", CENTER_MODE);

  if (on) {
    UTIL_LCD_DisplayStringAt(0, 16, (uint8_t*)"Status: ON", CENTER_MODE);
    UTIL_LCD_DisplayStringAt(0, 32, (uint8_t*)"LED: RED", CENTER_MODE);
  } else {
    UTIL_LCD_DisplayStringAt(0, 16, (uint8_t*)"Status: OFF", CENTER_MODE);
    UTIL_LCD_DisplayStringAt(0, 32, (uint8_t*)"LED: BLUE", CENTER_MODE);
  }

  char addr[20];
  snprintf(addr, sizeof(addr), "Addr:0x%04X", zigbee_app_info.last_joined_nwkAddr);
  UTIL_LCD_DisplayStringAt(0, 48, (uint8_t*)addr, CENTER_MODE);

  BSP_LCD_Refresh(LCD_Inst);
}
```

#### 5. 控制函数集成
```c
static void APP_ZIGBEE_TuyaControlBreaker(bool on)
{
  uint8_t dp_data[1];
  const uint8_t BREAKER_DP_ID = 16;

  dp_data[0] = on ? 0x01 : 0x00;

  APP_DBG("Control breaker: %s", on ? "ON" : "OFF");
  APP_ZIGBEE_TuyaSendDpCommand(BREAKER_DP_ID, 0x01, dp_data, sizeof(dp_data));

  // 更新显示
  APP_ZIGBEE_UpdateBreakerStatus(on);
}
```

---

## 🎨 LED API 说明

### 可用颜色

```c
// LED 亮度定义
#define PWM_LED_GSDATA_OFF      0      // LED 关闭
#define PWM_LED_GSDATA_47_0     255    // LED 全亮

// 常用颜色
LED_Set_rgb(255, 0, 0);      // 红色
LED_Set_rgb(0, 255, 0);      // 绿色
LED_Set_rgb(0, 0, 255);      // 蓝色
LED_Set_rgb(255, 255, 0);    // 黄色
LED_Set_rgb(255, 0, 255);    // 紫色
LED_Set_rgb(0, 255, 255);    // 青色
LED_Set_rgb(255, 255, 255);  // 白色
LED_Off();                   // 关闭
```

### 示例代码

```c
// 闪烁红色
LED_Set_rgb(255, 0, 0);
HAL_Delay(500);
LED_Off();

// 显示绿色
LED_Set_rgb(0, 255, 0);
```

---

## 📺 LCD API 说明

### 基本操作

```c
// 清屏
BSP_LCD_Clear(LCD_Inst, SSD1315_COLOR_BLACK);

// 显示文本（居中）
UTIL_LCD_DisplayStringAt(0, y, (uint8_t*)"Text", CENTER_MODE);

// 显示文本（左对齐）
UTIL_LCD_DisplayStringAt(x, y, (uint8_t*)"Text", LEFT_MODE);

// 刷新显示
BSP_LCD_Refresh(LCD_Inst);
```

### 显示位置

| Y 坐标 | 行号 | 用途 |
|--------|------|------|
| 0 | 1 | 标题 |
| 16 | 2 | 状态信息 |
| 32 | 3 | LED 颜色 |
| 48 | 4 | 设备地址 |

### 示例代码

```c
// 显示自定义信息
BSP_LCD_Clear(LCD_Inst, SSD1315_COLOR_BLACK);
UTIL_LCD_DisplayStringAt(0, 0, (uint8_t*)"My Title", CENTER_MODE);
UTIL_LCD_DisplayStringAt(0, 16, (uint8_t*)"Line 2", CENTER_MODE);
UTIL_LCD_DisplayStringAt(0, 32, (uint8_t*)"Line 3", CENTER_MODE);
BSP_LCD_Refresh(LCD_Inst);
```

---

## 🚀 使用效果

### 完整流程演示

**1. 系统启动**
```
LED: 🔵 蓝色
LCD: "Initializing..."
```

**2. 网络形成**
```
LED: 闪烁绿色（成功指示）
LCD: "Network Ready"
```

**3. 设备入网**
```
LED: 保持蓝色（断开状态）
LCD: "Waiting device"
```

**4. 按 B2 开闸**
```
LED: 🔵 蓝色 → 🔴 红色
LCD: "Status: OFF" → "Status: ON"
     "LED: BLUE"   → "LED: RED"
串口: "Control breaker: ON"
      "DP command sent: ID=16, Data=0x01"
```

**5. 再按 B2 关闸**
```
LED: 🔴 红色 → 🔵 蓝色
LCD: "Status: ON"  → "Status: OFF"
     "LED: RED"    → "LED: BLUE"
串口: "Control breaker: OFF"
      "DP command sent: ID=16, Data=0x00"
```

---

## 🎯 测试步骤

### 1️⃣ 编译烧录

```
1. 保存所有文件
2. 项目 → 清理
3. 项目 → 构建
4. 烧录到 STM32WB
```

### 2️⃣ 观察启动

**预期：**
- ✅ LED 显示蓝色
- ✅ LCD 显示 "Initializing..."
- ✅ 数秒后显示 "Network Ready"

### 3️⃣ 设备入网

**操作：**
```
1. 按 B1 触发 Permit Join
2. 断路器进入配对模式
3. 等待入网成功
```

**预期：**
- ✅ LCD 显示 "Waiting device"
- ✅ 入网成功后无显示变化（等待控制）

### 4️⃣ 控制测试

**操作：按 B2**

**预期：**
- ✅ LED 从蓝色变红色
- ✅ LCD 显示 "Status: ON"
- ✅ LCD 显示 "LED: RED"
- ✅ LCD 显示设备地址
- ✅ 断路器实际闭合

**操作：再按 B2**

**预期：**
- ✅ LED 从红色变蓝色
- ✅ LCD 显示 "Status: OFF"
- ✅ LCD 显示 "LED: BLUE"
- ✅ 断路器实际断开

---

## 💡 扩展功能建议

### 1. 添加状态历史

```c
// 显示最后操作时间
char time_str[20];
uint32_t minutes = HAL_GetTick() / 60000;
snprintf(time_str, sizeof(time_str), "Last: %lu min", minutes);
UTIL_LCD_DisplayStringAt(0, 48, (uint8_t*)time_str, CENTER_MODE);
```

### 2. 添加错误提示

```c
// 显示错误信息
if (error) {
  BSP_LCD_Clear(LCD_Inst, SSD1315_COLOR_BLACK);
  UTIL_LCD_DisplayStringAt(0, 16, (uint8_t*)"ERROR!", CENTER_MODE);
  UTIL_LCD_DisplayStringAt(0, 32, (uint8_t*)"Check device", CENTER_MODE);
  BSP_LCD_Refresh(LCD_Inst);

  // LED 闪烁红色
  for (int i = 0; i < 5; i++) {
    LED_Set_rgb(255, 0, 0);
    HAL_Delay(200);
    LED_Off();
    HAL_Delay(200);
  }
}
```

### 3. 添加动画效果

```c
// LCD 滚动显示
void ShowScrollingText(const char* text) {
  for (int x = 128; x > -100; x -= 2) {
    BSP_LCD_Clear(LCD_Inst, SSD1315_COLOR_BLACK);
    UTIL_LCD_DisplayStringAt(x, 16, (uint8_t*)text, LEFT_MODE);
    BSP_LCD_Refresh(LCD_Inst);
    HAL_Delay(20);
  }
}
```

---

## 📊 功能对比

| 功能 | 修改前 | 修改后 |
|------|--------|--------|
| LED 指示 | ❌ 无 | ✅ 红/蓝色状态指示 |
| LCD 显示 | ❌ 仅初始 Logo | ✅ 实时状态显示 |
| 状态反馈 | ❌ 仅串口日志 | ✅ 多重反馈 |
| 用户体验 | 😐 需要看日志 | 😊 一目了然 |

---

## 🔧 故障排查

### 问题 1：LCD 无显示

**检查：**
```c
// 确认 LCD 已初始化
extern uint32_t LCD_Inst;
```

**解决：**
- LCD 在 app_entry.c 中初始化
- 确保 app_entry.c 的 LCD_DisplayInit() 已执行

### 问题 2：LED 不亮

**检查：**
```c
// 确认 LED 函数可用
LED_Set_rgb(255, 0, 0);  // 测试红色
HAL_Delay(1000);
LED_Off();
```

**解决：**
- 确保 BSP_PWM_LED 驱动已初始化
- 检查硬件连接

### 问题 3：显示内容不完整

**原因：** snprintf 缓冲区太小

**解决：**
```c
char buffer[30];  // 增大缓冲区
snprintf(buffer, sizeof(buffer), "Addr:0x%04X", addr);
```

---

## 📝 总结

✅ **LED 状态指示**：红色=闭合，蓝色=断开
✅ **LCD 实时显示**：状态、地址、颜色
✅ **多重反馈**：LED + LCD + 串口
✅ **用户友好**：不看日志也能知道状态

**下一步建议：**
- ✅ 测试稳定性
- ⏭️ 添加定时功能
- ⏭️ 添加串口命令控制

---

**现在编译测试，观察 LED 和 LCD 的实时显示！** 🎉
