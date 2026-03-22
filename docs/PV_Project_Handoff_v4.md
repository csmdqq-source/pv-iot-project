# PV Monitoring IoT — 项目全景文档
## 新对话快速上手指南

> **最后更新：2026-03-17**
> **学生：Tian Qiyuan | 导师：Prof. Petros Aristidou | 塞浦路斯理工大学**

---

## 1. 项目概述

硕士论文项目：基于物联网的智能光伏监控系统，含 PR 故障检测和远程断路器控制。

**完整数据链路：**
```
PV Panel → Inverter (DC→AC) → Breaker (TOWSMR1-40) → Zigbee (0xEF00) → STM32WB5MM → LPUART1 → A7670E (4G LTE) → MQTT (broker.emqx.io:1883) → Node-RED → InfluxDB → Grafana
```

**下行控制：** Dashboard → MQTT → A7670E → STM32 → Zigbee → Breaker ON/OFF

---

## 2. PR 故障检测模型

```
P_theoretical = k × GHI
PR = P_actual / P_theoretical
```

| PR 范围 | 分类 | 故障编码 |
|---------|------|---------|
| > 1.30 | 传感器异常 (warning) | fault_flag = 1 |
| 0.75–1.30 | 正常 | fault_flag = 0 |
| 0.30–0.70 | 性能偏低 (warning) | fault_flag = 2 |
| < 0.30 | 严重故障 (critical) | fault_flag = 3 |

- k 校准公式：k = Σ(P×GHI) / Σ(GHI²)，仅用 GHI > 200 的数据
- device_001 的 k = 0.24（从 GHI=496, P=118W 校准）
- 低辐照度过滤：GHI ≤ 50 时 PR 标记为 -1，不触发告警
- 告警冷却：同设备同规则 30 分钟内只报一次

---

## 3. Node-RED 流程（3 个 Flow）

### Flow 1: 辐照度采集
```
每10分钟 → Build Open-Meteo URL → HTTP Request → Transform → InfluxDB + global.set('ghi_map')
```
- **网格方案**：8 个固定点覆盖塞浦路斯全岛（nicosia, limassol, larnaca, paphos, famagusta, troodos, polis, protaras）
- 设备就近匹配最近网格点的 GHI，不管设备数量怎么增长 API 请求数恒定为 8
- Build URL 节点从 `global.get('irradiance_points')` 读取坐标列表
- Transform 节点将每个坐标的 GHI 存入 `global.set('ghi_map')`

### Flow 2: 光伏监控主管道
```
MQTT In (pv/+/data) → Validate & Transform → 4 路输出:
  → 合并辐照度+功率 (PR 计算) → InfluxDB
  → Enrich Location → InfluxDB
  → Detect Anomalies → 告警 InfluxDB
  → Error Handler
```

**关键节点版本：**
- Validate & Transform v4.2：输出 [fields, tags] 格式，sim_fault 用数字编码放 fields（不是 tag）
- 合并辐照度+功率 v4.0：从 PostgreSQL 注册表读 k 值和坐标，按就近匹配 GHI
- Detect Anomalies v4.0：PR 阈值检测 + 30min 冷却
- 全岛批量模拟器 v3.3：50 台设备，故障状态机，多坐标 GHI

### Flow 3: 远程控制
```
Dashboard (localhost:1880/ui) → 选设备+开关 → MQTT Out (pv/{device_id}/control) → InfluxDB control_log
```

### Flow 4: 设备注册表（新增）
```
每5分钟 → PostgreSQL 查询 → 同步到 global (device_registry + irradiance_points)
```

---

## 4. InfluxDB 数据模型

**Bucket: pv_data** | **Measurement 名称注意：实际用的是 `breaker_data`（不是 device_metrics）**

| Measurement | Tags | Fields |
|-------------|------|--------|
| breaker_data | device_id, location, data_source, device_type | voltage, current, power, ghi, p_theoretical, performance_ratio, fault_flag, k_factor, temperature, status, latitude, longitude |
| irradiance | source, data_type, location, point_id | ghi, ambient_temp, cloud_cover, latitude, longitude |
| alerts | device_id, level, rule | message, detail, level_code, pr, power, p_theoretical, ghi |
| device_locations | device_id, city, region | latitude, longitude, power, status |
| control_log | device_id, action | command_str, command_code, source |

> **重要**：measurement 名称是 `breaker_data` 不是 `device_metrics`，Grafana 查询时注意。

---

## 5. PostgreSQL 设备注册表

```sql
-- 连接方式
docker exec -it postgres psql -U admin -d pv_system

-- 表结构
CREATE TABLE device_registry (
    device_id VARCHAR(50) PRIMARY KEY,
    latitude DOUBLE PRECISION NOT NULL,
    longitude DOUBLE PRECISION NOT NULL,
    name VARCHAR(100),
    location VARCHAR(100),
    k_factor DOUBLE PRECISION DEFAULT 0.15,
    panel_power_wp INTEGER DEFAULT 0,
    device_type VARCHAR(20) DEFAULT 'real',
    status VARCHAR(20) DEFAULT 'active',
    install_date DATE,
    notes TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 当前数据（device_002 已删除）
-- device_001: lat=34.675698, lon=33.042408, k=0.24, Limassol
```

Node-RED 每 5 分钟同步到：
- `global.device_registry` = { device_id → {lat, lon, k, name, location, ...} }
- `global.irradiance_points` = [8 个网格点]

---

## 6. Docker 部署

```yaml
# docker-compose.yml 包含 4 个服务
influxdb:    localhost:8086  (admin / admin123456 / org: pv-monitoring / bucket: pv_data / token: my-super-secret-token-12345)
nodered:     localhost:1880  (Dashboard: localhost:1880/ui)
grafana:     localhost:3000  (admin / admin123)
postgres:    localhost:5432  (admin / admin123456 / db: pv_system)
```

---

## 7. STM32 远程控制代码（已修改 3 个文件）

| 文件 | 改动 |
|------|------|
| a7670e.h | 新增 A7670E_SetSubscribeTopic(), A7670E_RegisterMsgCallback() |
| a7670e.c | 新增 SUB 状态机 + +CMQTTRXPAYLOAD URC 解析 |
| app_zigbee.c | 新增 APP_ZIGBEE_MqttControlCallback() 解析 on/off JSON |

---

## 8. Grafana 仪表盘

| 面板 | 类型 | 查询 measurement |
|------|------|-----------------|
| 功率/辐照度散点图 | XY Chart | breaker_data (X=ghi, Y=power) |
| 实时功率 | Time Series | breaker_data (power) |
| 太阳辐照度 GHI | Time Series | breaker_data (ghi) |
| 电压&电流 | Time Series 双Y轴 | breaker_data (voltage + current) |
| 开关状态 | Stat | breaker_data (status) |
| 地理图 | GeoMap | breaker_data (latitude, longitude, power) |
| 告警历史表格 | Table | alerts |

---

## 9. 实测延迟

| 指标 | 数值 |
|------|------|
| 电阻变化 → STM32 LCD | 3~10s, 平均 6s |
| 电阻变化 → Grafana | ≈ 10s |
| 云端链路 (MQTT→Node-RED→InfluxDB→Grafana) | ≈ 4s |
| 冷启动 (含 Zigbee 配对) | ≈ 20s |
| 远程控制响应 | 2~5s |

> STM32 断电后需重新配对断路器（Zigbee 状态不持久化）

---

## 10. 已知问题与注意事项

1. **sim_fault 必须放 fields 不能放 tags**，否则同一设备产生多条时间序列
2. **measurement 名称是 breaker_data**，不是 device_metrics（历史原因）
3. **k=0.15 太低**，device_001 已校准为 k=0.24
4. **夜间数据照常记录**，PR=-1 表示不适用，不触发告警
5. **笔记本不能待机**，否则 Docker 暂停数据丢失。设置：电源→从不睡眠，合盖→不采取任何操作
6. **device_002 已从数据库删除**（坐标不在塞浦路斯）
7. **ghi_map 有旧缓存**，清除方法：global.set('ghi_map', {}) 然后等 10 分钟重新采集

---

## 11. 老师的 5 个任务及进度

| # | 任务 | 状态 |
|---|------|------|
| 1 | **更新论文** Chapter 3：MQTT管道 + Dashboard + PR算法 + 端到端验证 | ✅ 已写完 4 节（3.4.1~3.4.4 docx 文件已生成） |
| 2 | **实验室验证**：Fluke 万用表精度测试 + 动态响应测试 | 🔲 需去实验室，接 PV Panel → Inverter → Breaker → STM32 |
| 3 | **GitHub 仓库** + 安装指南 (install-stm32.md, install-dashboard.md, custom-server.md) | 🔲 待做 |
| 4 | **可移植性文档**：换 MQTT Broker、Docker/手动部署到任意电脑 | 🔲 待做 |
| 5 | **老师家实测**：真实 PV 环境几天连续测试 | 🔲 等老师确认光伏面板 |

---

## 12. 已生成的文档清单

| 文件 | 内容 |
|------|------|
| PV_Monitoring_System_v3.0.md | 完整项目文档 v3.0 |
| thesis_section_3x1_mqtt.docx | 论文 3.4.1 MQTT 通信管道 |
| thesis_section_3x2_dashboard.docx | 论文 3.4.2 云端仪表盘实现 |
| thesis_section_3x3_pr.docx | 论文 3.4.3 PR 故障检测算法 |
| thesis_section_3x4_e2e.docx | 论文 3.4.4 端到端验证 |
| MQTT_Pipeline_Architecture.svg | MQTT 架构图 |
| 设备注册表_精确辐照度_修改指南.md | PostgreSQL + 多坐标辐照度改造指南 |
| device-registry-flow.json | Node-RED 设备注册表同步流程 |
| remote-control-flow.json | Node-RED 远程控制流程 |
| stm32_remote_control_modified.zip | STM32 远程控制修改后的 3 个文件 |

---

## 13. 当前待修复

- [x] 合并辐照度+功率节点需要把 latitude/longitude 加入 fields（地图显示位置用）
- [x] location tag 改为优先读 PostgreSQL 数据库（不再硬编码 Nicosia）
- [ ] GHI 面板查询改成 breaker_data.ghi（已提供代码，待用户替换）
- [ ] 辐照度采集流 Build URL 节点改为从 global.irradiance_points 读取（修改指南步骤 5）
- [ ] 辐照度采集流 Transform 节点改为多坐标版本（修改指南步骤 5）
- [ ] Detect Anomalies 节点改为从 device_registry 读 k 值（修改指南步骤 7）

---

## 14. 可扩展性方案（答辩用）

| 规模 | 方案 |
|------|------|
| 当前 100~500 台 | 单机 Docker (Node-RED + InfluxDB + Grafana + PostgreSQL) |
| 1K~10K 台 | 自建 EMQX 集群 + Kafka + TimescaleDB |
| 10K~100K 台 | 云托管 (AWS IoT Core + Flink + ClickHouse) |
| 全托管 | AWS IoT Core + Kinesis + Timestream + Managed Grafana |

---

**使用方法：新对话时上传此文档，说"根据文档了解我的项目，继续帮我做 XXX"。**
