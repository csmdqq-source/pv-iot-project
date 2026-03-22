# PV Monitoring IoT — 对话工作记录
## 新对话快速上手文档

> **生成时间：2026-03-21**
> **配合使用：PV_Project_Handoff_v4.md（项目全景文档）**

---

## 1. 本次对话完成的所有改动

### 1.1 Grafana GHI 面板修复
**问题：** GHI 面板显示全部 8 个网格点辐照度，没有按设备过滤
**解决：** 用 Flux subquery 自动匹配设备 region
```flux
deviceRegion = from(bucket: "pv_data")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r._measurement == "breaker_data")
  |> filter(fn: (r) => r.device_id == "${device_id}")
  |> filter(fn: (r) => r._field == "power")
  |> group()
  |> sort(columns: ["_time"], desc: true)
  |> limit(n: 1)
  |> findRecord(fn: (key) => true, idx: 0)

from(bucket: "pv_data")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r._measurement == "openmeteo_irradiance")
  |> filter(fn: (r) => r._field == "ghi")
  |> filter(fn: (r) => r["data_type"] == "actual")
  |> filter(fn: (r) => r["location"] == deviceRegion.region)
```
**备注：** 创建了 device_location 变量但最终删除，改用 subquery 方案

### 1.2 Region Tag 修复（大小写 + 硬编码）
**问题：** device_001 的 region tag 硬编码为 "Nicosia"，实际应为 "limassol"（小写）
**修改了两个 Node-RED 节点：**

**Validate & Transform 节点** — 添加了从注册表读 region：
```javascript
const registry = global.get('device_registry') || {};
const deviceInfo = registry[String(device_id)];
const tags = {
    device_id: String(device_id),
    region: (deviceInfo && deviceInfo.location) ? deviceInfo.location.toLowerCase() : "unknown"
};
```

**Enrich Location 节点** — 完全重写，从注册表读取替代硬编码：
```javascript
const registry = global.get('device_registry') || {};
const device_id = msg.device_id;
const deviceInfo = registry[String(device_id)];

if (deviceInfo) {
    msg.payload[1].region    = deviceInfo.location ? deviceInfo.location.toLowerCase() : "unknown";
    msg.payload[1].latitude  = String(deviceInfo.lat);
    msg.payload[1].longitude = String(deviceInfo.lon);
} else if (msg.raw && msg.raw.latitude && msg.raw.longitude) {
    msg.payload[1].latitude  = String(msg.raw.latitude);
    msg.payload[1].longitude = String(msg.raw.longitude);
    msg.payload[1].region    = "simulated";
}
return msg;
```

### 1.3 PostgreSQL 设备注册表 — 50 台模拟设备入库
**操作：** 插入 50 台模拟设备，分布在南部沿海 5 个城市
- Limassol: 15 台（device_101~115）
- Larnaca: 12 台（device_116~127）
- Paphos: 10 台（device_128~137）
- Nicosia: 8 台（device_138~145）
- Protaras: 5 台（device_146~150）

**SQL 文件：** insert_devices.sql（已执行）

**手动坐标调整：**
- device_111: 34.702326, 33.020979
- device_114: 34.703196, 33.010767
- device_107: 34.707689, 33.013127
- device_103: 34.710435, 33.015417

**device_151 已添加后删除**（测试用）

### 1.4 模拟器重构 — v4.0 → v4.4
**v4.0：** 从 device_registry 读取设备列表（替代随机生成坐标）
**v4.1：** 加 GHI ±3% 微波动 + 收窄 cloud 概率
**v4.2：** 加云层持续性状态机
**v4.3：** 晴天延长到 60~180 分钟，正常 PR 收窄到 0.90~0.96
**v4.4（当前版本）：** 故障频率降至每类每天约 1 台，持续时间 20~40 分钟，电压改为欧标 230V±2V

**当前模拟器完整代码在对话中标记为 "v4.4"**

**关键参数（v4.4）：**
| 参数 | 值 |
|------|-----|
| orientation_factor | 0.93~0.98 |
| 正常 PR | 0.90~0.96 |
| GHI 微波动 | ±3% |
| 云层-晴天 | 85%概率，持续60~180分钟，factor 0.95~1.00 |
| 云层-薄云 | 12%概率，持续20~40分钟，factor 0.72~0.85 |
| 云层-厚云 | 3%概率，持续30~60分钟，factor 0.45~0.60 |
| 逆变器故障 | 每天约1台/50台，持续20~30分钟 |
| 灰尘积累 | 每天约1台/50台，持续30~40分钟 |
| 遮挡 | 每天约1台/50台，持续20~30分钟 |
| 电压 | 230V ±2V |

**清除故障/云层状态方法：**
```javascript
flow.set('sim_fault_states', {});
flow.set('sim_cloud_states', {});
```

### 1.5 Grafana 查询优化 — aggregateWindow
**问题：** 数据点过多导致 "truncated at 7701 points" 警告
**解决：** 所有面板查询加 aggregateWindow，把 `every: 10s` 改为 `every: v.windowPeriod`

**已优化的面板查询模板：**
```flux
// 功率面板
from(bucket: "pv_data")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "breaker_data")
  |> filter(fn: (r) => r["_field"] == "power")
  |> filter(fn: (r) => r["device_id"] == "${device_id}")
  |> drop(columns: ["latitude", "longitude", "region"])
  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)
  |> yield(name: "mean")

// 散点图（pivot 前先聚合）
from(bucket: "pv_data")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pv_correlation")
  |> filter(fn: (r) => r["device_id"] == "${device_id}")
  |> filter(fn: (r) => r["_field"] == "ghi" or r["_field"] == "power")
  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)
  |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
```

---

## 2. 实验室验证（已完成）

### 2.1 实验设置
- 电源：240V AC 市电
- 负载：3 × FESTO Bank 3 电阻箱（每组 R7=4400Ω, R8=2200Ω, R9=1100Ω）
- 参考仪器：Fluke 数字万用表
- 被测设备：TOWSMR1-40 → STM32WB5MM-DK → A7670E → MQTT → Grafana

### 2.2 稳态精度结果
10 个测试点，每点 3 次重复。

**工作范围（>50W，TP2~TP8）汇总：**
| 指标 | 电压 | 电流 | 功率 |
|------|------|------|------|
| 平均误差 | 0.13% | 0.77% | 0.69% |
| 最大误差 | 0.18% | 2.48% | 1.81% |

**全部测试点（TP1~TP9）汇总：**
| 指标 | 电压 | 电流 | 功率 |
|------|------|------|------|
| 平均误差 | 0.12% | 2.10% | 2.62% |
| 最大误差 | 0.18% | 11.11% | 16.15% |

TP1 大误差原因：电流分辨率 0.01A 在小电流（0.054A）下放大相对误差

### 2.3 动态响应结果
| 指标 | STM32 LCD | Grafana |
|------|-----------|---------|
| 平均响应时间 | 10.2s | 19.2s |
| 最大响应时间 | 14.1s | 22.5s |
| 云端链路延迟 | — | ≈9.0s |

### 2.4 远程控制结果
| 指标 | 空载 | 带负载 |
|------|------|--------|
| 平均分闸响应 | 0.48s | 0.42s |
| 平均合闸响应 | 0.51s | 0.48s |
| 快速连续操作 | 约80%成功率（间隔<1s） |

### 2.5 已生成的文件
| 文件 | 内容 |
|------|------|
| lab_test_record_final_v2.xlsx | 完整实验数据（3个Sheet + 汇总） |
| parity_plot.png / .pdf | 功率测量一致性散点图 |
| wiring_diagram_v2.png / .pdf | 实验室接线图 |
| chapter6_results.docx | 论文第6章完整内容 |
| thesis_update_registry.docx | 设备注册表+模拟器+可扩展性章节 |
| device_management_guide.md | 设备增删改查操作指南 |
| insert_devices.sql | 50台模拟设备SQL |

---

## 3. 论文当前状态

### 3.1 章节结构与完成度
| 章节 | 内容 | 状态 |
|------|------|------|
| Ch1 Introduction | 研究背景、SCADA现状、研究内容 | ✅ 已写 |
| Ch2 Literature Review | Zigbee/GSM/LoRa/WiFi/BT对比 | ✅ 已写 |
| Ch3 Research Methodology | 系统设计、技术选型 | ✅ 已写 |
| Ch4 系统实现中间步骤 | MQTT管道、注册表、仪表盘、PR算法 | ✅ 主体已写，缺截图 |
| Ch5 系统硬件设计 | STM32/A7670E/TOWSMR1-40选型 | ✅ 已写，偏短 |
| Ch6 Results and Discussion | 实验结果与分析 | ✅ docx已生成 |
| Ch7 Conclusions | 总结 | 🔲 待写 |

### 3.2 论文待补充项
1. 🔲 Ch4 各节截图（Node-RED Flow、Grafana面板、MQTT日志）
2. 🔲 Ch4 缺少"远程断路器控制"小节
3. 🔲 Ch4 缺少"设备模拟器架构"小节（docx已生成待插入）
4. 🔲 Ch5 加入 STM32 原理图截图（Sheet 3 标注 PB5/PC0，Sheet 7 电源）
5. 🔲 Ch6 插入 parity_plot.pdf 和 wiring_diagram_v2.pdf
6. 🔲 Ch6 插入 Grafana 功率突变截图
7. 🔲 Ch7 Conclusions 待写
8. 🔲 文献引用未导入（需用 Mendeley）
9. 🔲 Figure/Table 编号未统一

### 3.3 GHI面板更新插入位置
在 4.2.5 Grafana 可视化仪表盘中，第（3）段"太阳辐照度 GHI 曲线"后面追加：
> 在初始实现中，该面板直接查询全部 8 个网格点的辐照度数据，导致同时显示多条 GHI 曲线。为解决此问题，本文采用 Flux 子查询机制实现了设备与辐照度的自动联动...

---

## 4. 老师 5 个任务进度

| # | 任务 | 状态 |
|---|------|------|
| 1 | 更新论文 Ch3/Ch4 | ✅ 主体完成，缺截图和远程控制小节 |
| 2 | 实验室验证 | ✅ 数据完成，Ch6 docx已生成 |
| 3 | GitHub仓库+安装指南 | 🔲 待做 |
| 4 | 可移植性文档 | 🔲 待做 |
| 5 | 老师家实测 | 🔲 等老师确认光伏面板 |

---

## 5. 拍摄清单（去实验室前打印）

**硬件照片：**
1. 🔲 实验室全景（电阻箱+断路器+STM32+Fluke全接好）
2. 🔲 STM32WB5MM-DK LCD显示数据特写
3. 🔲 A7670E模块+4G天线+UART接线特写
4. 🔲 PB5/PC0/GND杜邦线连接细节
5. 🔲 TOWSMR1-40断路器特写
6. 🔲 FESTO Bank 3电阻箱
7. 🔲 Fluke接线方式

**软件截图：**
8. 🔲 Grafana仪表盘全貌（device_001, Last 24h）
9. 🔲 Grafana功率突变截图（空载→满载切换）
10. 🔲 Grafana地图面板（50台设备）
11. 🔲 Grafana告警历史表格
12. 🔲 Node-RED Flow 1~4 全景截图
13. 🔲 Node-RED Dashboard远程控制界面
14. 🔲 InfluxDB Data Explorer查询截图
15. 🔲 PostgreSQL设备注册表查询结果
16. 🔲 远程分闸前后Grafana状态变化

---

## 6. 硬件接线信息

| 连接 | 详情 |
|------|------|
| STM32 → A7670E | PB5(TX)→RXD, PC0(RX)←TXD, GND共地 |
| 供电 | DC电源 USB 5V（两个设备共用） |
| STM32 天线 | 内置PCB天线（STM32WB5MMG模块自带） |
| A7670E 天线 | 外接4G天线 |
| 断路器 | TOWSMR1-40 串联在240V AC线路中 |
| Fluke电压表 | 并联在电阻箱两端 |
| Fluke电流表 | 串联在断路器Load→电阻箱之间 |
| STM32原理图 | MB1292-D01 Board Schematic (Sheet 3: MCU引脚, Sheet 7: 电源) |

---

## 7. 关键注意事项（从全景文档继承+本次新增）

1. measurement 名称是 `breaker_data` 不是 `device_metrics`
2. sim_fault 必须放 fields 不能放 tags
3. location/region 全部小写（与 openmeteo_irradiance 匹配）
4. 旧数据 region="Nicosia"（大写）无法修改，只有新数据是 "limassol"（小写）
5. 模拟器状态存在 flow 变量中：`sim_fault_states`、`sim_cloud_states`
6. 清除方法：`flow.set('sim_fault_states', {})` + `flow.set('sim_cloud_states', {})`
7. 合并辐照度+功率节点**不会覆盖 power**，功率完全来自模拟器
8. Grafana 查询必须加 `aggregateWindow(every: v.windowPeriod, ...)` 防止数据点过多
9. InfluxDB 删除数据需要正确的 token（命令行 401 错误未解决，建议用 UI 操作）

---

**使用方法：新对话时上传此文档 + PV_Project_Handoff_v4.md，说"根据文档继续帮我做 XXX"。**
