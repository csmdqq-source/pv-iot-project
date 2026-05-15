# STM32WB Zigbee智能断路器控制系统 - 实施指南

## 🎯 项目目标

使用 **STM32WB5MM-DK** 作为Zigbee协调器，控制 **通欧TOWSMR1-40 Zigbee智能断路器**，实现：
1. ✅ 创建Zigbee网络
2. ✅ 智能断路器自动加入网络
3. ✅ 控制断路器开关闸
4. ✅ 读取电压、电流、功率等数据

---

## 📊 项目架构对比

### 原示例架构 vs 目标架构

```
【原示例】需要2个STM32WB设备
┌─────────────────┐           ┌─────────────────┐
│  STM32WB #1     │           │  STM32WB #2     │
│  协调器         │           │  路由器         │
│  OnOff Server   │◄──────────│  OnOff Client   │
│  (被控制LED)    │  命令控制  │  (发送命令)     │
└─────────────────┘           └─────────────────┘

【目标架构】只需1个STM32WB
┌─────────────────┐           ┌─────────────────┐
│  STM32WB        │           │  智能断路器     │
│  协调器         │           │  (通欧TOWSMR1)  │
│  OnOff Client   │──────────►│  OnOff Server   │
│  (发送命令)     │  控制断路器 │  (执行开关)     │
│                 │◄──────────│                 │
│  Metering读取   │  上报数据  │  电量数据       │
└─────────────────┘           └─────────────────┘
```

### 关键区别

| 对比项 | 原示例 | 目标项目 |
|--------|--------|----------|
| **STM32角色** | OnOff Server（被控） | OnOff Client（控制）+ Metering Client（读数据） |
| **设备数量** | 2个STM32WB | 1个STM32WB + 1个断路器 |
| **控制方向** | 接收命令控制LED | 发送命令控制断路器 |
| **数据读取** | 无 | 读取电压/电流/功率 |
| **所需集群** | OnOff Server (0x0006) | OnOff Client + Metering/Electrical Measurement |

---

## 📋 完整实施步骤

### 阶段一：环境准备与基础验证（第1-2天）

#### ✅ 任务1.1：准备开发环境

**所需工具：**
```
✓ STM32CubeIDE (≥ 1.13.0) 或 IAR/Keil
✓ STM32CubeProgrammer (用于烧录M0+固件)
✓ 串口终端工具 (PuTTY/Tera Term)
✓ USB数据线
```

**硬件清单：**
```
✓ STM32WB5MM-DK 开发板 × 1
✓ 通欧TOWSMR1-40 Zigbee智能断路器 × 1
✓ 测试用电器（可选，用于验证断路器功能）
```

#### ✅ 任务1.2：烧录Zigbee协处理器固件

**步骤：**

1. **找到固件文件：**
```
路径: STM32Cube_FW_WB_V1.23.0\Projects\STM32_Copro_Wireless_Binaries\
文件: stm32wb5x_Zigbee_FFD_fw.bin
```

2. **使用STM32CubeProgrammer烧录：**
```
① 连接STM32WB5MM-DK到PC
② 打开STM32CubeProgrammer
③ 连接方式：ST-Link
④ 点击"Connect"
⑤ 点击"Open file"选择 stm32wb5x_Zigbee_FFD_fw.bin
⑥ 起始地址：0x080CB000 (重要！不要写错)
⑦ 点击"Download"
⑧ 等待"Download completed"
⑨ 点击"Disconnect"
```

**⚠️ 警告：**
- 地址必须是 `0x080CB000`（M0+固件区）
- 不要擦除整个Flash（会删除M0+固件）
- 确认版本 ≥ 1.18.0

#### ✅ 任务1.3：编译并烧录示例工程

**使用STM32CubeIDE：**
```
① 打开工程
   文件 → 导入 → 现有项目
   选择: STM32CubeIDE/

② 编译
   项目 → 全部构建 (Ctrl+B)

③ 烧录
   运行 → 调试 (F11)
   或 运行 → 运行 (Ctrl+F11)
```

**验证烧录成功：**
```
✓ LCD显示 "Starting..."
✓ 等待3-5秒后显示 "Network Ready"
✓ RGB LED显示蓝色
✓ 串口输出 "Network formed on channel 15"
```

#### ✅ 任务1.4：配置串口调试

**串口参数：**
```
波特率:   115200 bps
数据位:   8 bits
停止位:   1 bit
校验位:   None
流控:     None
```

**连接方式：**
```
① USB连接开发板
② 设备管理器中找到虚拟COM口 (Windows: COMx)
③ 打开串口终端，配置参数
④ 复位开发板，观察输出
```

**预期输出：**
```
[INFO] APP_ZIGBEE_Init: Starting Zigbee application
[INFO] Wireless FW version: 1.18.0
[INFO] Zigbee stack layers initialized
[INFO] Network formation started on channel 15
[INFO] Network formed successfully
[INFO]   PAN ID: 0xXXXX
[INFO]   Ext Addr: 0x0080E10000XXXXXX
[INFO]   Short Addr: 0x0000
```

---

### 阶段二：智能断路器研究（第3天）

#### ✅ 任务2.1：了解通欧TOWSMR1-40规格

**查找产品手册：**

根据通欧（Tongou）TOWSMR1系列产品的一般规格，智能断路器通常支持：

**基本参数：**
```
型号:         TOWSMR1-40
额定电流:     40A
工作电压:     AC 220V
通信协议:     Zigbee 3.0
频段:         2.4GHz
设备类型:     Router（路由器）
```

**支持的Zigbee集群（需确认）：**

| 集群ID | 集群名称 | 作用 | 必需 |
|--------|---------|------|------|
| 0x0006 | OnOff | 开关控制 | ✅ 必需 |
| 0x0702 | Metering | 电量计量 | ✅ 可能支持 |
| 0x0B04 | Electrical Measurement | 电气测量 | ✅ 可能支持 |
| 0x0000 | Basic | 基本信息 | ✅ 必需 |
| 0x0003 | Identify | 设备识别 | ⭕ 可选 |

**关键属性：**

```c
/* OnOff集群 (0x0006) */
- Attribute 0x0000: OnOff (Boolean) - 当前开关状态

/* Metering集群 (0x0702) - 如果支持 */
- Attribute 0x0000: CurrentSummationDelivered (uint48) - 累计用电量
- Attribute 0x0300: InstantaneousDemand (int24) - 瞬时功率
- Attribute 0x0400: CurrentTier1SummationDelivered - 分时电量

/* Electrical Measurement集群 (0x0B04) - 如果支持 */
- Attribute 0x0505: RMSVoltage (uint16) - 电压 (V)
- Attribute 0x0508: RMSCurrent (uint16) - 电流 (A)
- Attribute 0x050B: ActivePower (int16) - 有功功率 (W)
- Attribute 0x050F: ApparentPower (uint16) - 视在功率 (VA)
```

#### ✅ 任务2.2：配对断路器并检查集群

**步骤1：让断路器进入配对模式**

```
⚠️ 参考断路器说明书的配对方法，通常是：
① 长按断路器上的配对按钮3-5秒
② LED指示灯开始闪烁（配对模式）
③ 保持配对模式60秒内完成配网
```

**步骤2：STM32协调器允许加入**

```
① 复位STM32WB开发板
② 等待LCD显示 "Network Ready"
③ 按下开发板B1按钮 → 触发Permit Join
④ LCD显示 "Permit Join: ON"
⑤ 等待断路器加入（180秒窗口）
```

**步骤3：观察设备加入**

**串口输出应显示：**
```
[INFO] Permit Join requested for 180 seconds
[INFO] Device Announce received:
[INFO]   NwkAddr: 0x1234
[INFO]   ExtAddr: 0x0080E1XXXXXXXXXX
[INFO] Active Endpoint Request sent
[INFO] Active Endpoint Response: EP=1, EP=2...
[INFO] Simple Descriptor Request sent for EP=1
[INFO] Simple Descriptor Response:
[INFO]   ProfileID: 0x0104 (Home Automation)
[INFO]   DeviceID: 0x0009 (Mains Power Outlet)
[INFO]   Input Clusters: 0x0000, 0x0003, 0x0006...
[INFO]   Output Clusters: 0x0019...
```

**步骤4：记录关键信息**

创建文档记录：
```
断路器网络地址 (NwkAddr): 0x____
断路器扩展地址 (ExtAddr): 0x________________
端点号 (Endpoint): __
支持的输入集群 (Input Clusters):
  - 0x0000 (Basic)
  - 0x0006 (OnOff)
  - 0x0702 (Metering) ?
  - 0x0B04 (Electrical Measurement) ?
  - 其他: _______
```

#### ✅ 任务2.3：使用Zigbee嗅探器（可选但推荐）

**工具选项：**
```
① Ubiqua Protocol Analyzer (付费，专业)
② Wireshark + nRF52840 Dongle (免费)
③ Silicon Labs Network Analyzer (免费，需Silicon Labs工具)
```

**抓包分析：**
```
① 捕获断路器加入过程
② 分析Simple Descriptor响应
③ 确认支持的集群列表
④ 查看属性报告格式
```

---

### 阶段三：代码修改 - 添加OnOff Client（第4-5天）

现在开始修改代码，让STM32WB能够**控制**断路器，而不仅仅是被控制。

#### ✅ 任务3.1：修改 app_zigbee.c - 添加Client集群

**打开文件：**
```
STM32_WPAN/App/app_zigbee.c
```

**修改1：更新配置宏**

找到文件顶部的宏定义（约48-58行），修改：

```c
/* 原代码 */
#define ONOFF_TARGET_NWK_ADDR    0x0000U
#define ONOFF_TARGET_ENDPOINT    1U

/* 修改为 */
#define BREAKER_TARGET_NWK_ADDR  0x0000U  // 待更新为断路器实际地址
#define BREAKER_TARGET_ENDPOINT  1U        // 待更新为断路器实际端点
#define BREAKER_EXT_ADDR         0x0000000000000000ULL  // 待更新

// 添加功能标志
#define ENABLE_BREAKER_CONTROL   1         // 启用断路器控制
#define ENABLE_METERING          1         // 启用电量读取
```

**修改2：在 zigbee_app_info 结构体中添加Client集群**

找到结构体定义（约100-110行），确认已有：

```c
static struct zigbee_app_info {
  struct ZigBeeT *zb;
  enum ZbStartType startupControl;
  enum ZbStatusCodeT join_status;
  uint32_t join_delay;
  bool init_after_join;
  bool has_init;

  struct ZbZclClusterT *onOff_server;   // 保留：用于接收其他设备命令
  struct ZbZclClusterT *onOff_client;   // ✅ 已存在：用于发送命令给断路器

  // 添加新集群（如果需要电量读取）
  struct ZbZclClusterT *metering_client;      // 新增
  struct ZbZclClusterT *electrical_client;    // 新增

  // 添加断路器信息
  uint16_t breaker_nwkAddr;    // 断路器网络地址
  uint8_t  breaker_endpoint;   // 断路器端点
} zigbee_app_info;
```

**修改3：在 APP_ZIGBEE_ConfigEndpoints() 中配置Client**

找到 `APP_ZIGBEE_ConfigEndpoints` 函数（约300行左右），修改：

```c
static void APP_ZIGBEE_ConfigEndpoints(void)
{
  struct ZbApsmeAddEndpointReqT req;
  struct ZbApsmeAddEndpointConfT conf;

  memset(&req, 0, sizeof(req));

  /* ========== 保留原有Server配置 ========== */
  /* Endpoint: SW1_ENDPOINT (17) */
  req.profileId = ZCL_PROFILE_HOME_AUTOMATION;
  req.deviceId = ZCL_DEVICE_ONOFF_SWITCH;
  req.endpoint = SW1_ENDPOINT;
  ZbZclAddEndpoint(zigbee_app_info.zb, &req, &conf);
  assert(conf.status == ZB_STATUS_SUCCESS);

  /* OnOff Server - 保留用于接收其他设备命令 */
  zigbee_app_info.onOff_server = ZbZclOnOffServerAlloc(
      zigbee_app_info.zb,
      SW1_ENDPOINT,
      &OnOffServerCallbacks,  // 保留原有回调
      NULL);
  assert(zigbee_app_info.onOff_server != NULL);

  ZbZclClusterEndpointRegister(zigbee_app_info.onOff_server);

  /* ========== 新增：OnOff Client配置 ========== */
  /* 用于发送命令给断路器 */
  zigbee_app_info.onOff_client = ZbZclOnOffClientAlloc(
      zigbee_app_info.zb,
      SW1_ENDPOINT);  // 可以与Server共用同一端点
  assert(zigbee_app_info.onOff_client != NULL);

  ZbZclClusterEndpointRegister(zigbee_app_info.onOff_client);

  APP_DBG("OnOff Client cluster allocated on endpoint %d", SW1_ENDPOINT);

  /* ========== 可选：配置Metering Client ========== */
#if ENABLE_METERING
  // 等阶段五再添加
#endif
}
```

#### ✅ 任务3.2：修改按钮功能

**原示例的按钮功能：**
- B1: Permit Join（允许设备加入）
- B2: 发送OnOff命令

**目标按钮功能：**
- B1: Permit Join（保持不变）
- B2: **控制断路器开关**（修改为实际控制断路器）
- B3（如果有）或长按B2: 读取电量数据

**修改 APP_ZIGBEE_SW2_Process 函数：**

找到该函数（约600行左右），修改为：

```c
/**
 * @brief  按钮SW2处理 - 控制断路器开关
 */
static void APP_ZIGBEE_SW2_Process(void)
{
  struct ZbApsAddrT dst;
  enum ZclStatusCodeT status;

  /* 检查断路器是否已加入 */
  if (zigbee_app_info.breaker_nwkAddr == 0x0000 ||
      zigbee_app_info.breaker_nwkAddr == 0xFFFF) {
    APP_DBG("Error: Breaker not joined yet!");
    LCD_Clear();
    LCD_Print("No Breaker!");
    return;
  }

  /* 配置目标地址 */
  memset(&dst, 0, sizeof(dst));
  dst.mode = ZB_APSDE_ADDRMODE_SHORT;
  dst.endpoint = zigbee_app_info.breaker_endpoint;
  dst.nwkAddr = zigbee_app_info.breaker_nwkAddr;

  APP_DBG("Sending OnOff Toggle to breaker 0x%04x:EP%d",
          dst.nwkAddr, dst.endpoint);

  /* 发送Toggle命令给断路器 */
  status = ZbZclOnOffClientToggleReq(
      zigbee_app_info.onOff_client,
      &dst,
      APP_ZIGBEE_OnOffClientCallback,  // 响应回调
      NULL);

  if (status != ZCL_STATUS_SUCCESS) {
    APP_DBG("Error sending Toggle command: 0x%02x", status);
    LCD_Clear();
    LCD_Print("Send Failed!");
  } else {
    APP_DBG("Toggle command sent successfully");
    LCD_Clear();
    LCD_Print("Toggle Sent");
  }
}
```

**添加OnOff Client响应回调：**

在文件中添加新函数：

```c
/**
 * @brief  OnOff Client命令响应回调
 * @param  cluster: 集群指针
 * @param  rsp: 响应消息
 * @param  arg: 用户参数
 */
static void APP_ZIGBEE_OnOffClientCallback(
    struct ZbZclCommandRspT *rsp,
    void *arg)
{
  APP_DBG("OnOff Client Response received:");
  APP_DBG("  Status: 0x%02x", rsp->status);
  APP_DBG("  From: 0x%04x", rsp->src.nwkAddr);

  if (rsp->status == ZCL_STATUS_SUCCESS) {
    APP_DBG("Breaker control SUCCESS");
    LCD_Clear();
    LCD_Print("Breaker OK!");
    LED_Set_rgb(0, 255, 0);  // 绿色表示成功
  } else {
    APP_DBG("Breaker control FAILED");
    LCD_Clear();
    LCD_Print("Breaker Fail");
    LED_Set_rgb(255, 0, 0);  // 红色表示失败
  }
}
```

**在函数声明区添加：**

```c
/* 在文件顶部添加函数声明（约80-90行） */
static void APP_ZIGBEE_OnOffClientCallback(struct ZbZclCommandRspT *rsp, void *arg);
```

#### ✅ 任务3.3：修改设备公告处理 - 自动记录断路器信息

修改 `APP_ZIGBEE_DeviceAnnce_cb` 函数，自动保存断路器地址：

```c
static int APP_ZIGBEE_DeviceAnnce_cb(
    struct ZigBeeT *zb,
    struct ZbZdoDeviceAnnceT *annce,
    uint8_t seqno,
    void *arg)
{
  APP_DBG("New Device Announced:");
  APP_DBG("  NwkAddr: 0x%04x", annce->nwkAddr);
  APP_DBG("  ExtAddr: 0x%016llx", annce->extAddr);

  /* ========== 新增：保存断路器信息 ========== */
  // 假设第一个加入的设备是断路器
  if (zigbee_app_info.breaker_nwkAddr == 0xFFFF ||
      zigbee_app_info.breaker_nwkAddr == 0x0000) {
    zigbee_app_info.breaker_nwkAddr = annce->nwkAddr;

    APP_DBG("Breaker device saved: 0x%04x", annce->nwkAddr);

    LCD_Clear();
    LCD_Print("Breaker Join!");
  }

  /* 发送Active Endpoint请求 */
  struct ZbZdoActiveEpReqT req;
  req.dstNwkAddr = annce->nwkAddr;
  req.nwkAddrOfInterest = annce->nwkAddr;

  ZbZdoActiveEpReq(zb, &req, APP_ZIGBEE_ActiveEp_cb, NULL);

  return 0;
}
```

**修改 APP_ZIGBEE_ActiveEp_cb 保存端点号：**

```c
static void APP_ZIGBEE_ActiveEp_cb(
    struct ZbZdoActiveEpRspT *rsp,
    void *arg)
{
  if (rsp->status != ZB_STATUS_SUCCESS) {
    APP_DBG("Active EP Request failed: 0x%02x", rsp->status);
    return;
  }

  APP_DBG("Active Endpoint Response from 0x%04x:", rsp->nwkAddr);
  APP_DBG("  EP Count: %d", rsp->activeEpCount);

  for (uint8_t i = 0; i < rsp->activeEpCount; i++) {
    APP_DBG("  EP[%d]: %d", i, rsp->activeEpList[i]);
  }

  /* ========== 新增：保存断路器端点 ========== */
  if (rsp->nwkAddr == zigbee_app_info.breaker_nwkAddr &&
      rsp->activeEpCount > 0) {
    zigbee_app_info.breaker_endpoint = rsp->activeEpList[0];
    APP_DBG("Breaker endpoint saved: %d", zigbee_app_info.breaker_endpoint);
  }

  /* 继续请求Simple Descriptor */
  if (rsp->activeEpCount > 0) {
    struct ZbZdoSimpleDescReqT req;
    req.dstNwkAddr = rsp->nwkAddr;
    req.nwkAddrOfInterest = rsp->nwkAddr;
    req.endpoint = rsp->activeEpList[0];

    ZbZdoSimpleDescReq(zigbee_app_info.zb, &req,
                       APP_ZIGBEE_SimpleDesc_cb, NULL);
  }
}
```

#### ✅ 任务3.4：初始化断路器信息变量

在 `APP_ZIGBEE_Init` 函数中初始化：

```c
void APP_ZIGBEE_Init(void)
{
  /* ... 原有代码 ... */

  /* ========== 新增：初始化断路器信息 ========== */
  zigbee_app_info.breaker_nwkAddr = 0xFFFF;   // 无效地址
  zigbee_app_info.breaker_endpoint = 1;        // 默认端点1
  zigbee_app_info.metering_client = NULL;      // 稍后配置
  zigbee_app_info.electrical_client = NULL;    // 稍后配置

  /* ... 继续原有代码 ... */
}
```

#### ✅ 任务3.5：编译测试

```bash
① 保存所有修改
② 项目 → 清理 (Clean)
③ 项目 → 构建 (Build)
④ 检查编译错误并修复
⑤ 烧录到STM32WB
⑥ 观察串口输出
```

**预期结果：**
```
✓ 编译无错误
✓ 程序正常启动
✓ 网络形成成功
✓ 按B1可以Permit Join
✓ 断路器加入后保存地址和端点
✓ 按B2发送Toggle命令
```

---

### 阶段四：实现断路器控制功能（第6天）

#### ✅ 任务4.1：完整测试开关控制

**测试步骤：**

```
① 确保断路器已加入网络
   - 串口显示 "Breaker device saved: 0xXXXX"
   - LCD显示 "Breaker Join!"

② 按B2按钮发送Toggle命令
   - 观察断路器继电器动作（咔嗒声）
   - 串口输出 "Toggle command sent successfully"
   - 等待响应回调 "Breaker control SUCCESS"

③ 重复按B2测试多次
   - 断路器应交替开关
   - 每次响应正常

④ 测试断电重启
   - 复位STM32WB
   - 断路器应自动重新加入（作为Router会保存网络信息）
```

#### ✅ 任务4.2：添加独立的开和关命令

修改代码，添加明确的开/关命令（而不仅是Toggle）：

```c
/**
 * @brief  打开断路器
 */
static void APP_ZIGBEE_BreakerOn(void)
{
  struct ZbApsAddrT dst;

  dst.mode = ZB_APSDE_ADDRMODE_SHORT;
  dst.endpoint = zigbee_app_info.breaker_endpoint;
  dst.nwkAddr = zigbee_app_info.breaker_nwkAddr;

  APP_DBG("Sending ON command to breaker");

  if (ZbZclOnOffClientOnReq(zigbee_app_info.onOff_client, &dst,
                            APP_ZIGBEE_OnOffClientCallback, NULL)
      == ZCL_STATUS_SUCCESS) {
    LCD_Clear();
    LCD_Print("Breaker ON");
  }
}

/**
 * @brief  关闭断路器
 */
static void APP_ZIGBEE_BreakerOff(void)
{
  struct ZbApsAddrT dst;

  dst.mode = ZB_APSDE_ADDRMODE_SHORT;
  dst.endpoint = zigbee_app_info.breaker_endpoint;
  dst.nwkAddr = zigbee_app_info.breaker_nwkAddr;

  APP_DBG("Sending OFF command to breaker");

  if (ZbZclOnOffClientOffReq(zigbee_app_info.onOff_client, &dst,
                             APP_ZIGBEE_OnOffClientCallback, NULL)
      == ZCL_STATUS_SUCCESS) {
    LCD_Clear();
    LCD_Print("Breaker OFF");
  }
}
```

**修改按钮逻辑（可选）：**

```c
// 如果有2个可用按钮，可以这样分配：
// B1: 关闭断路器
// B2: 打开断路器

static void APP_ZIGBEE_SW1_Process(void)
{
  // 原本是Permit Join，改为关闭断路器
  APP_ZIGBEE_BreakerOff();
}

static void APP_ZIGBEE_SW2_Process(void)
{
  // 打开断路器
  APP_ZIGBEE_BreakerOn();
}

// 或者保持B1为Permit Join，使用B2长短按区分：
// B2 短按: 开
// B2 长按: 关
```

#### ✅ 任务4.3：读取断路器当前状态

添加读取OnOff属性的功能：

```c
/**
 * @brief  读取断路器当前开关状态
 */
static void APP_ZIGBEE_ReadBreakerState(void)
{
  struct ZbZclReadReqT req;
  uint16_t attr_list[] = { ZCL_ONOFF_ATTR_ONOFF };

  memset(&req, 0, sizeof(req));
  req.dst.mode = ZB_APSDE_ADDRMODE_SHORT;
  req.dst.endpoint = zigbee_app_info.breaker_endpoint;
  req.dst.nwkAddr = zigbee_app_info.breaker_nwkAddr;
  req.count = 1;
  req.attr = attr_list;

  APP_DBG("Reading breaker OnOff state...");

  if (ZbZclReadReq(zigbee_app_info.onOff_client, &req,
                   APP_ZIGBEE_ReadStateCallback, NULL)
      != ZCL_STATUS_SUCCESS) {
    APP_DBG("Failed to send read request");
  }
}

/**
 * @brief  读取状态响应回调
 */
static void APP_ZIGBEE_ReadStateCallback(
    struct ZbZclReadRspT *rsp,
    void *arg)
{
  if (rsp->status != ZCL_STATUS_SUCCESS) {
    APP_DBG("Read failed: 0x%02x", rsp->status);
    return;
  }

  APP_DBG("Breaker state read success:");
  for (uint8_t i = 0; i < rsp->count; i++) {
    if (rsp->attr[i].attrId == ZCL_ONOFF_ATTR_ONOFF) {
      bool state = rsp->attr[i].value[0];
      APP_DBG("  OnOff State: %s", state ? "ON" : "OFF");

      LCD_Clear();
      LCD_Print(state ? "State: ON" : "State: OFF");

      // LED指示
      if (state) {
        LED_Set_rgb(0, 255, 0);  // 绿色=开
      } else {
        LED_Set_rgb(255, 0, 0);  // 红色=关
      }
    }
  }
}
```

---

### 阶段五：添加电量读取功能（第7-8天）

#### ✅ 任务5.1：确认断路器支持的电量集群

**检查Simple Descriptor响应：**

在 `APP_ZIGBEE_SimpleDesc_cb` 函数中添加日志：

```c
static void APP_ZIGBEE_SimpleDesc_cb(
    struct ZbZdoSimpleDescRspT *rsp,
    void *arg)
{
  if (rsp->status != ZB_STATUS_SUCCESS) {
    APP_DBG("Simple Desc failed: 0x%02x", rsp->status);
    return;
  }

  APP_DBG("Simple Descriptor for EP %d:", rsp->endpoint);
  APP_DBG("  ProfileID: 0x%04x", rsp->profileId);
  APP_DBG("  DeviceID: 0x%04x", rsp->deviceId);

  APP_DBG("  Input Clusters (%d):", rsp->inputClusterCount);
  for (uint8_t i = 0; i < rsp->inputClusterCount; i++) {
    APP_DBG("    0x%04x", rsp->inputClusterList[i]);

    /* ========== 检测电量集群 ========== */
    if (rsp->inputClusterList[i] == ZCL_CLUSTER_METER) {
      APP_DBG("    ✓ Metering cluster supported!");
      // 可以配置Metering Client
    }
    if (rsp->inputClusterList[i] == ZCL_CLUSTER_MEASUREMENT_ELECTRICAL) {
      APP_DBG("    ✓ Electrical Measurement cluster supported!");
      // 可以配置Electrical Measurement Client
    }
  }
}
```

#### ✅ 任务5.2：添加Metering集群（如果支持）

**修改 app_zigbee.c：**

**步骤1：包含头文件**

```c
/* 在文件顶部添加 */
#include "zcl/se/zcl.meter.h"  // Metering集群
```

**步骤2：在 ConfigEndpoints 中分配Metering Client**

```c
static void APP_ZIGBEE_ConfigEndpoints(void)
{
  /* ... OnOff配置 ... */

#if ENABLE_METERING
  /* Metering Client - 用于读取电量数据 */
  zigbee_app_info.metering_client = ZbZclMeterClientAlloc(
      zigbee_app_info.zb,
      SW1_ENDPOINT,
      NULL);  // 回调为NULL，使用主动读取

  if (zigbee_app_info.metering_client != NULL) {
    ZbZclClusterEndpointRegister(zigbee_app_info.metering_client);
    APP_DBG("Metering Client allocated");
  }
#endif
}
```

**步骤3：实现读取电量函数**

```c
/**
 * @brief  读取断路器电量数据
 */
static void APP_ZIGBEE_ReadMeteringData(void)
{
  struct ZbZclReadReqT req;

  /* 要读取的属性列表 */
  uint16_t attr_list[] = {
    ZCL_METER_SVR_ATTR_CURSUM_DLVD,         // 0x0000: 累计用电量
    ZCL_METER_SVR_ATTR_INSTANTANEOUS_DEMAND // 0x0400: 瞬时功率
  };

  memset(&req, 0, sizeof(req));
  req.dst.mode = ZB_APSDE_ADDRMODE_SHORT;
  req.dst.endpoint = zigbee_app_info.breaker_endpoint;
  req.dst.nwkAddr = zigbee_app_info.breaker_nwkAddr;
  req.count = sizeof(attr_list) / sizeof(attr_list[0]);
  req.attr = attr_list;

  APP_DBG("Reading metering data from breaker...");

  if (ZbZclReadReq(zigbee_app_info.metering_client, &req,
                   APP_ZIGBEE_MeteringReadCallback, NULL)
      != ZCL_STATUS_SUCCESS) {
    APP_DBG("Failed to send metering read request");
  }
}

/**
 * @brief  电量读取回调
 */
static void APP_ZIGBEE_MeteringReadCallback(
    struct ZbZclReadRspT *rsp,
    void *arg)
{
  if (rsp->status != ZCL_STATUS_SUCCESS) {
    APP_DBG("Metering read failed: 0x%02x", rsp->status);
    return;
  }

  APP_DBG("Metering data received:");

  for (uint8_t i = 0; i < rsp->count; i++) {
    uint16_t attrId = rsp->attr[i].attrId;
    uint8_t *data = rsp->attr[i].value;

    if (attrId == ZCL_METER_SVR_ATTR_CURSUM_DLVD) {
      // 累计用电量 (uint48，单位通常是Wh或kWh)
      uint64_t energy = 0;
      memcpy(&energy, data, 6);  // 读取6字节
      APP_DBG("  Total Energy: %llu Wh", energy);

      char buf[32];
      snprintf(buf, sizeof(buf), "Energy:%lluWh", energy);
      LCD_Clear();
      LCD_Print(buf);
    }
    else if (attrId == ZCL_METER_SVR_ATTR_INSTANTANEOUS_DEMAND) {
      // 瞬时功率 (int24，单位通常是W)
      int32_t power = 0;
      memcpy(&power, data, 3);  // 读取3字节
      if (power & 0x800000) {    // 符号扩展
        power |= 0xFF000000;
      }
      APP_DBG("  Instantaneous Power: %d W", power);

      char buf[32];
      snprintf(buf, sizeof(buf), "Power:%dW", power);
      LCD_Clear();
      LCD_Print(buf);
    }
  }
}
```

#### ✅ 任务5.3：添加Electrical Measurement集群（如果支持）

**步骤1：包含头文件**

```c
#include "zcl/general/zcl.elec.meas.h"  // Electrical Measurement
```

**步骤2：分配集群**

```c
static void APP_ZIGBEE_ConfigEndpoints(void)
{
  /* ... 其他配置 ... */

  /* Electrical Measurement Client */
  zigbee_app_info.electrical_client = ZbZclElecMeasClientAlloc(
      zigbee_app_info.zb,
      SW1_ENDPOINT);

  if (zigbee_app_info.electrical_client != NULL) {
    ZbZclClusterEndpointRegister(zigbee_app_info.electrical_client);
    APP_DBG("Electrical Measurement Client allocated");
  }
}
```

**步骤3：读取电压电流**

```c
/**
 * @brief  读取电压、电流、功率
 */
static void APP_ZIGBEE_ReadElectricalData(void)
{
  struct ZbZclReadReqT req;

  uint16_t attr_list[] = {
    ZCL_ELEC_MEAS_ATTR_RMSVOLTAGE,    // 0x0505: 电压
    ZCL_ELEC_MEAS_ATTR_RMSCURRENT,    // 0x0508: 电流
    ZCL_ELEC_MEAS_ATTR_ACTIVE_POWER,  // 0x050B: 有功功率
    ZCL_ELEC_MEAS_ATTR_APPARENT_POWER // 0x050F: 视在功率
  };

  memset(&req, 0, sizeof(req));
  req.dst.mode = ZB_APSDE_ADDRMODE_SHORT;
  req.dst.endpoint = zigbee_app_info.breaker_endpoint;
  req.dst.nwkAddr = zigbee_app_info.breaker_nwkAddr;
  req.count = sizeof(attr_list) / sizeof(attr_list[0]);
  req.attr = attr_list;

  APP_DBG("Reading electrical measurements...");

  if (ZbZclReadReq(zigbee_app_info.electrical_client, &req,
                   APP_ZIGBEE_ElectricalReadCallback, NULL)
      != ZCL_STATUS_SUCCESS) {
    APP_DBG("Failed to send electrical read request");
  }
}

/**
 * @brief  电气参数读取回调
 */
static void APP_ZIGBEE_ElectricalReadCallback(
    struct ZbZclReadRspT *rsp,
    void *arg)
{
  if (rsp->status != ZCL_STATUS_SUCCESS) {
    APP_DBG("Electrical read failed: 0x%02x", rsp->status);
    return;
  }

  APP_DBG("Electrical measurements received:");

  uint16_t voltage = 0;
  uint16_t current = 0;
  int16_t activePower = 0;

  for (uint8_t i = 0; i < rsp->count; i++) {
    uint16_t attrId = rsp->attr[i].attrId;

    switch (attrId) {
      case ZCL_ELEC_MEAS_ATTR_RMSVOLTAGE:
        memcpy(&voltage, rsp->attr[i].value, 2);
        APP_DBG("  Voltage: %u V (raw)", voltage);
        // 注意：可能需要除以10或100转换为实际电压
        break;

      case ZCL_ELEC_MEAS_ATTR_RMSCURRENT:
        memcpy(&current, rsp->attr[i].value, 2);
        APP_DBG("  Current: %u A (raw)", current);
        // 注意：可能需要除以1000转换为实际电流
        break;

      case ZCL_ELEC_MEAS_ATTR_ACTIVE_POWER:
        memcpy(&activePower, rsp->attr[i].value, 2);
        APP_DBG("  Active Power: %d W", activePower);
        break;

      case ZCL_ELEC_MEAS_ATTR_APPARENT_POWER:
        // 视在功率
        break;
    }
  }

  /* LCD显示 */
  char buf[64];
  snprintf(buf, sizeof(buf), "V:%uV I:%umA P:%dW",
           voltage / 10, current, activePower);
  LCD_Clear();
  LCD_Print(buf);
}
```

**⚠️ 重要：数据格式转换**

不同厂商的断路器可能使用不同的数据格式和缩放因子，需要根据实际抓包或文档调整：

```c
/* 常见转换示例 */
float actual_voltage = raw_voltage / 10.0;      // 可能是10倍
float actual_current = raw_current / 1000.0;    // 可能是1000倍(mA)
float actual_power = raw_power;                  // 可能直接是W
```

#### ✅ 任务5.4：配置按钮触发读取

修改按钮处理，添加读取功能：

```c
/* 方案1：使用B2长按读取 */
static uint32_t b2_press_time = 0;

void BSP_PB_Callback(Button_TypeDef Button)
{
  if (Button == BUTTON_SW2) {
    if (BSP_PB_GetState(BUTTON_SW2) == GPIO_PIN_SET) {
      // 按下
      b2_press_time = HAL_GetTick();
    } else {
      // 释放
      uint32_t duration = HAL_GetTick() - b2_press_time;

      if (duration < 500) {
        // 短按：Toggle开关
        UTIL_SEQ_SetTask(CFG_TASK_BUTTON_SW2, CFG_SCH_PRIO_1);
      } else {
        // 长按：读取数据
        UTIL_SEQ_SetTask(CFG_TASK_READ_METERING, CFG_SCH_PRIO_1);
      }
    }
  }
}

/* 注册新任务 */
void MX_APPE_Init(void)
{
  /* ... */
  UTIL_SEQ_RegTask(CFG_TASK_READ_METERING, UTIL_SEQ_RFU,
                   APP_ZIGBEE_ReadMeteringData);
  /* ... */
}
```

---

### 阶段六：测试与优化（第9-10天）

#### ✅ 任务6.1：完整功能测试

**测试清单：**

```
□ 协调器网络形成
  ✓ LCD显示 "Network Ready"
  ✓ 串口输出网络信息

□ 断路器配对加入
  ✓ 按B1触发Permit Join
  ✓ 断路器进入配对模式
  ✓ 自动加入并保存地址

□ 开关控制测试
  ✓ 按B2发送Toggle命令
  ✓ 断路器继电器动作
  ✓ 响应回调成功

□ 电量读取测试
  ✓ 长按B2触发读取
  ✓ 串口输出电压/电流/功率
  ✓ LCD显示实时数据

□ 异常处理测试
  ✓ 断路器断电重启后自动重连
  ✓ 协调器断电重启，网络重新形成
  ✓ 超出通信范围后恢复

□ 长时间稳定性测试
  ✓ 连续运行24小时无异常
  ✓ 定时读取数据稳定
```

#### ✅ 任务6.2：添加定时读取功能

使用定时器自动读取电量数据：

```c
/* 定义定时器ID */
#define METERING_TIMER_ID  1

/* 在 APP_ZIGBEE_Init 中启动定时器 */
void APP_ZIGBEE_Init(void)
{
  /* ... */

  /* 启动定时器，每30秒读取一次 */
  HW_TS_Create(CFG_TIM_PROC_ID_ISR,
               &METERING_TIMER_ID,
               hw_ts_Repeated,
               APP_ZIGBEE_MeteringTimerCallback);

  HW_TS_Start(METERING_TIMER_ID, 30000);  // 30秒=30000ms
}

/* 定时器回调 */
static void APP_ZIGBEE_MeteringTimerCallback(void)
{
  /* 触发读取任务 */
  UTIL_SEQ_SetTask(CFG_TASK_READ_METERING, CFG_SCH_PRIO_0);
}
```

#### ✅ 任务6.3：优化LCD显示

实现多页面轮询显示：

```c
typedef enum {
  DISPLAY_PAGE_NETWORK = 0,
  DISPLAY_PAGE_BREAKER_STATE,
  DISPLAY_PAGE_VOLTAGE_CURRENT,
  DISPLAY_PAGE_POWER,
  DISPLAY_PAGE_MAX
} DisplayPage_t;

static DisplayPage_t current_page = DISPLAY_PAGE_NETWORK;

/**
 * @brief  更新LCD显示
 */
void APP_ZIGBEE_UpdateDisplay(void)
{
  char line1[32];
  char line2[32];

  switch (current_page) {
    case DISPLAY_PAGE_NETWORK:
      snprintf(line1, 32, "PAN:0x%04x Ch:%d",
               ZbNwkGet(zigbee_app_info.zb, ZB_NWK_NIB_ID_PanId, NULL, 0),
               CHANNEL);
      snprintf(line2, 32, "Devices:%d", 1);  // +断路器数量
      break;

    case DISPLAY_PAGE_BREAKER_STATE:
      snprintf(line1, 32, "Breaker:0x%04x",
               zigbee_app_info.breaker_nwkAddr);
      snprintf(line2, 32, "State:%s", breaker_on ? "ON" : "OFF");
      break;

    case DISPLAY_PAGE_VOLTAGE_CURRENT:
      snprintf(line1, 32, "V:%.1fV", voltage);
      snprintf(line2, 32, "I:%.2fA", current);
      break;

    case DISPLAY_PAGE_POWER:
      snprintf(line1, 32, "P:%dW", active_power);
      snprintf(line2, 32, "E:%lluWh", total_energy);
      break;
  }

  LCD_Clear();
  LCD_Print(line1);
  // 如果LCD支持多行
  // LCD_Print_Line2(line2);
}

/* 定时切换页面 */
void APP_ZIGBEE_NextPage(void)
{
  current_page = (current_page + 1) % DISPLAY_PAGE_MAX;
  APP_ZIGBEE_UpdateDisplay();
}
```

#### ✅ 任务6.4：添加串口命令接口（可选）

实现通过串口控制断路器：

```c
/**
 * @brief  处理串口接收的命令
 */
void APP_ZIGBEE_ProcessUartCommand(uint8_t *cmd, uint16_t len)
{
  if (strncmp((char*)cmd, "on", 2) == 0) {
    APP_ZIGBEE_BreakerOn();
  }
  else if (strncmp((char*)cmd, "off", 3) == 0) {
    APP_ZIGBEE_BreakerOff();
  }
  else if (strncmp((char*)cmd, "toggle", 6) == 0) {
    APP_ZIGBEE_SW2_Process();
  }
  else if (strncmp((char*)cmd, "read", 4) == 0) {
    APP_ZIGBEE_ReadElectricalData();
  }
  else if (strncmp((char*)cmd, "status", 6) == 0) {
    APP_ZIGBEE_PrintNetworkInfo();
  }
  else {
    APP_DBG("Unknown command: %s", cmd);
    APP_DBG("Available commands: on, off, toggle, read, status");
  }
}
```

---

## 🔍 常见问题排查

### 问题1：断路器无法加入网络

**可能原因：**
- 断路器未进入配对模式
- 协调器未执行Permit Join
- 信道不匹配
- 断路器已绑定到其他网络

**解决方案：**
```
① 检查断路器配对指示灯是否闪烁
② 确认协调器LCD显示 "Permit Join: ON"
③ 尝试重置断路器到出厂设置（参考说明书）
④ 使用Zigbee嗅探器检查Beacon Request/Response
⑤ 尝试更换信道（修改CHANNEL宏）
```

### 问题2：控制命令无响应

**可能原因：**
- 断路器地址或端点错误
- 断路器离线
- 路由失败

**解决方案：**
```c
// 添加调试代码
APP_DBG("Breaker NwkAddr: 0x%04x", zigbee_app_info.breaker_nwkAddr);
APP_DBG("Breaker Endpoint: %d", zigbee_app_info.breaker_endpoint);

// 发送命令前检查设备是否在线
struct ZbZdoNwkAddrReqT req;
req.extAddr = /* 断路器扩展地址 */;
ZbZdoNwkAddrReq(zb, &req, callback, NULL);
```

### 问题3：电量数据读取失败

**可能原因：**
- 断路器不支持该集群
- 属性ID错误
- 数据格式不匹配

**解决方案：**
```
① 检查Simple Descriptor确认支持的集群
② 使用Zigbee嗅探器抓包查看实际属性ID
③ 参考断路器厂商文档
④ 尝试读取Basic集群的型号信息
```

### 问题4：网络不稳定

**可能原因：**
- 2.4GHz干扰（WiFi/蓝牙）
- 距离过远
- 信号遮挡

**解决方案：**
```c
// 更换信道，避开WiFi
#define CHANNEL 25  // WiFi通常占用1-11信道

// 降低发射功率测试范围
ZbNwkSet(zb, ZB_NWK_NIB_ID_TxPower, &tx_power, sizeof(tx_power));

// 增加链路质量监控
uint8_t lqi;
ZbNwkGetLqi(zb, breaker_nwkAddr, &lqi);
APP_DBG("Link Quality: %d", lqi);
```

---

## 📚 参考资料

### 必读文档

1. **STM32WB Zigbee应用笔记**
   - AN5506: Zigbee on STM32WB series
   - 详细说明Zigbee栈API和示例

2. **Zigbee Cluster Library规范**
   - ZCL Specification r7
   - 查阅OnOff、Metering、Electrical Measurement集群定义

3. **通欧断路器产品手册**
   - 确认支持的Zigbee功能
   - 配对方法
   - 属性列表

### 推荐工具

1. **Zigbee嗅探器**
   - Silicon Labs Z-Wave/Zigbee Sniffer
   - Wireshark + nRF52840 Dongle

2. **调试工具**
   - STM32CubeMonitor（实时变量监控）
   - Segger J-Link RTT Viewer（高速日志）

---

## 📝 总结

### 开发周期估算

| 阶段 | 天数 | 关键任务 |
|------|------|----------|
| 环境准备 | 1-2天 | 工具安装、固件烧录、基础验证 |
| 断路器研究 | 1天 | 配对测试、集群分析 |
| 代码修改 | 2-3天 | 添加Client、修改按钮逻辑 |
| 功能实现 | 2天 | 开关控制、电量读取 |
| 测试优化 | 2天 | 完整测试、界面优化 |
| **总计** | **8-10天** | 完整功能实现 |

### 最终功能清单

✅ **基础功能**
- Zigbee网络创建
- 断路器自动加入
- 开关控制（On/Off/Toggle）
- 状态读取

✅ **高级功能**
- 电压/电流实时读取
- 功率/电量监控
- LCD多页面显示
- 定时数据采集

✅ **可选扩展**
- 串口命令接口
- 数据日志记录
- 多断路器管理
- OTA固件升级

---

## ✅ 下一步行动

**立即开始：**

1. ✅ 标记第一个任务为"进行中"
2. ✅ 准备开发环境
3. ✅ 烧录M0+固件
4. ✅ 验证协调器基本功能

**有问题随时询问！**

---

<div align="center">

**🚀 祝您项目顺利！🚀**

*如有问题，请查看调试日志并参考常见问题章节*

</div>
