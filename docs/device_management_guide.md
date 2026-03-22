# PV 设备注册表管理指南

> 所有操作只需修改 PostgreSQL，Node-RED 每 5 分钟自动同步，无需改代码。

## 连接数据库

```bash
docker exec -it postgres psql -U admin -d pv_system
```

---

## 添加设备

```sql
-- 添加真实设备
INSERT INTO device_registry (device_id, latitude, longitude, name, location, k_factor, panel_power_wp, device_type, status)
VALUES ('device_002', 34.6850, 33.0450, 'Rooftop-B', 'limassol', 0.22, 300, 'real', 'active');

-- 添加模拟设备
INSERT INTO device_registry (device_id, latitude, longitude, name, location, k_factor, panel_power_wp, device_type, status)
VALUES ('device_151', 34.9200, 33.6300, 'LAR-New-01', 'larnaca', 0.20, 300, 'simulated', 'active');
```

**字段说明：**

| 字段 | 必填 | 说明 |
|------|------|------|
| device_id | ✅ | 唯一标识，格式 `device_XXX` |
| latitude | ✅ | 纬度（塞浦路斯范围 34.6~35.2） |
| longitude | ✅ | 经度（塞浦路斯范围 32.2~34.1） |
| name | 选填 | 可读名称 |
| location | ✅ | 地区，必须小写，与辐照度数据匹配：`limassol` `larnaca` `paphos` `nicosia` `protaras` `famagusta` `troodos` `polis` |
| k_factor | 选填 | 系统系数，默认 0.15，真实设备需校准 |
| panel_power_wp | 选填 | 面板额定功率(W)，默认 0 |
| device_type | ✅ | `real` 或 `simulated` |
| status | ✅ | `active` 或 `inactive` |

---

## 修改设备

```sql
-- 修改坐标
UPDATE device_registry SET latitude = 34.70, longitude = 33.05 WHERE device_id = 'device_101';

-- 修改 k 值（校准后更新）
UPDATE device_registry SET k_factor = 0.24 WHERE device_id = 'device_001';

-- 修改地区
UPDATE device_registry SET location = 'paphos' WHERE device_id = 'device_101';

-- 批量修改（同地区所有设备）
UPDATE device_registry SET k_factor = 0.21 WHERE location = 'paphos';
```

---

## 停用 / 启用设备

```sql
-- 停用（保留历史数据，模拟器不再生成数据）
UPDATE device_registry SET status = 'inactive' WHERE device_id = 'device_101';

-- 重新启用
UPDATE device_registry SET status = 'active' WHERE device_id = 'device_101';
```

---

## 删除设备

```sql
-- 删除单台
DELETE FROM device_registry WHERE device_id = 'device_101';

-- 删除所有模拟设备
DELETE FROM device_registry WHERE device_type = 'simulated';
```

> ⚠️ 删除后 InfluxDB 中的历史数据仍然保留，但不再有新数据写入。

---

## 查询设备

```sql
-- 查看所有设备
SELECT device_id, name, location, k_factor, device_type, status FROM device_registry ORDER BY device_id;

-- 按地区统计
SELECT location, COUNT(*) as count FROM device_registry WHERE status = 'active' GROUP BY location ORDER BY count DESC;

-- 查看总数
SELECT device_type, COUNT(*) as count FROM device_registry GROUP BY device_type;

-- 查找特定设备
SELECT * FROM device_registry WHERE device_id = 'device_001';
```

---

## 生效机制

- Node-RED **每 5 分钟**自动同步 PostgreSQL → `global.device_registry`
- 同步后整条链路自动适配：模拟器、Enrich Location、合并辐照度+功率、Detect Anomalies
- 如需立即生效：在 Node-RED 中手动触发 Flow 4 的注册表同步节点

## k 值校准方法

```
k = Σ(P × GHI) / Σ(GHI²)    仅用 GHI > 200 的数据
```

校准后更新：
```sql
UPDATE device_registry SET k_factor = 0.24 WHERE device_id = 'device_001';
```
