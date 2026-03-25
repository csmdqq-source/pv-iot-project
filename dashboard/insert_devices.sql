-- ============================================================
-- 50 台模拟设备 - 南部沿海分布
-- Limassol: 15台, Larnaca: 12台, Paphos: 10台, 
-- Nicosia: 8台, Protaras/Ayia Napa: 5台
-- ============================================================

-- 先清理旧的模拟设备（保留 device_001 真实设备）
DELETE FROM device_registry WHERE device_type = 'simulated';

-- Limassol 区域 (15台) - 沿海 + 城区
INSERT INTO device_registry (device_id, latitude, longitude, name, location, k_factor, panel_power_wp, device_type, status) VALUES
('device_101', 34.6780, 33.0380, 'LIM-Coastal-01', 'limassol', 0.22, 300, 'simulated', 'active'),
('device_102', 34.6850, 33.0450, 'LIM-Coastal-02', 'limassol', 0.19, 250, 'simulated', 'active'),
('device_103', 34.6720, 33.0520, 'LIM-Marina-01', 'limassol', 0.24, 350, 'simulated', 'active'),
('device_104', 34.6900, 33.0280, 'LIM-Industrial-01', 'limassol', 0.18, 400, 'simulated', 'active'),
('device_105', 34.6950, 33.0600, 'LIM-Agios-01', 'limassol', 0.21, 300, 'simulated', 'active'),
('device_106', 34.6680, 33.0150, 'LIM-Amathus-01', 'limassol', 0.23, 280, 'simulated', 'active'),
('device_107', 34.6830, 33.0700, 'LIM-Mesa-01', 'limassol', 0.20, 320, 'simulated', 'active'),
('device_108', 34.6760, 33.0250, 'LIM-Beach-01', 'limassol', 0.17, 250, 'simulated', 'active'),
('device_109', 34.7000, 33.0350, 'LIM-North-01', 'limassol', 0.25, 350, 'simulated', 'active'),
('device_110', 34.7050, 33.0500, 'LIM-North-02', 'limassol', 0.22, 300, 'simulated', 'active'),
('device_111', 34.6650, 33.0800, 'LIM-East-01', 'limassol', 0.19, 280, 'simulated', 'active'),
('device_112', 34.6700, 33.0000, 'LIM-Kourion-01', 'limassol', 0.24, 400, 'simulated', 'active'),
('device_113', 34.6880, 33.0100, 'LIM-Episkopi-01', 'limassol', 0.21, 300, 'simulated', 'active'),
('device_114', 34.6620, 33.0650, 'LIM-Germasogeia-01', 'limassol', 0.18, 250, 'simulated', 'active'),
('device_115', 34.7100, 33.0420, 'LIM-Polemidia-01', 'limassol', 0.23, 350, 'simulated', 'active'),

-- Larnaca 区域 (12台) - 机场附近 + 沿海
('device_116', 34.9200, 33.6300, 'LAR-Airport-01', 'larnaca', 0.22, 300, 'simulated', 'active'),
('device_117', 34.9100, 33.6400, 'LAR-Coastal-01', 'larnaca', 0.20, 280, 'simulated', 'active'),
('device_118', 34.9250, 33.6200, 'LAR-City-01', 'larnaca', 0.19, 250, 'simulated', 'active'),
('device_119', 34.9050, 33.6500, 'LAR-Marina-01', 'larnaca', 0.24, 350, 'simulated', 'active'),
('device_120', 34.9300, 33.6100, 'LAR-North-01', 'larnaca', 0.21, 300, 'simulated', 'active'),
('device_121', 34.9150, 33.6550, 'LAR-Salt-Lake-01', 'larnaca', 0.18, 280, 'simulated', 'active'),
('device_122', 34.8950, 33.6350, 'LAR-Kiti-01', 'larnaca', 0.23, 320, 'simulated', 'active'),
('device_123', 34.9350, 33.6450, 'LAR-Aradippou-01', 'larnaca', 0.20, 300, 'simulated', 'active'),
('device_124', 34.9000, 33.6150, 'LAR-Pervolia-01', 'larnaca', 0.17, 250, 'simulated', 'active'),
('device_125', 34.9400, 33.6250, 'LAR-Industrial-01', 'larnaca', 0.25, 400, 'simulated', 'active'),
('device_126', 34.9180, 33.6600, 'LAR-Dhekelia-01', 'larnaca', 0.22, 300, 'simulated', 'active'),
('device_127', 34.8900, 33.6450, 'LAR-South-01', 'larnaca', 0.19, 280, 'simulated', 'active'),

-- Paphos 区域 (10台) - 沿海度假区
('device_128', 34.7700, 32.4200, 'PAP-Harbour-01', 'paphos', 0.23, 350, 'simulated', 'active'),
('device_129', 34.7750, 32.4300, 'PAP-Tombs-01', 'paphos', 0.20, 300, 'simulated', 'active'),
('device_130', 34.7600, 32.4100, 'PAP-Coastal-01', 'paphos', 0.18, 250, 'simulated', 'active'),
('device_131', 34.7800, 32.4400, 'PAP-City-01', 'paphos', 0.24, 320, 'simulated', 'active'),
('device_132', 34.7650, 32.4000, 'PAP-Coral-Bay-01', 'paphos', 0.21, 300, 'simulated', 'active'),
('device_133', 34.7850, 32.4500, 'PAP-Geroskipou-01', 'paphos', 0.19, 280, 'simulated', 'active'),
('device_134', 34.7550, 32.4150, 'PAP-Beach-01', 'paphos', 0.22, 350, 'simulated', 'active'),
('device_135', 34.7900, 32.4250, 'PAP-North-01', 'paphos', 0.17, 250, 'simulated', 'active'),
('device_136', 34.7500, 32.4050, 'PAP-Peyia-01', 'paphos', 0.25, 400, 'simulated', 'active'),
('device_137', 34.7950, 32.4350, 'PAP-Emba-01', 'paphos', 0.20, 300, 'simulated', 'active'),

-- Nicosia 区域 (8台) - 城区 + 南郊
('device_138', 35.1600, 33.3600, 'NIC-City-01', 'nicosia', 0.21, 300, 'simulated', 'active'),
('device_139', 35.1500, 33.3700, 'NIC-Strovolos-01', 'nicosia', 0.23, 350, 'simulated', 'active'),
('device_140', 35.1400, 33.3500, 'NIC-Lakatamia-01', 'nicosia', 0.19, 280, 'simulated', 'active'),
('device_141', 35.1550, 33.3800, 'NIC-Latsia-01', 'nicosia', 0.22, 300, 'simulated', 'active'),
('device_142', 35.1300, 33.3650, 'NIC-Tseri-01', 'nicosia', 0.18, 250, 'simulated', 'active'),
('device_143', 35.1650, 33.3450, 'NIC-Engomi-01', 'nicosia', 0.24, 320, 'simulated', 'active'),
('device_144', 35.1350, 33.3400, 'NIC-Deftera-01', 'nicosia', 0.20, 300, 'simulated', 'active'),
('device_145', 35.1450, 33.3550, 'NIC-Aglantzia-01', 'nicosia', 0.21, 280, 'simulated', 'active'),

-- Protaras / Ayia Napa 区域 (5台) - 东南沿海
('device_146', 35.0150, 34.0580, 'PRO-Beach-01', 'protaras', 0.23, 350, 'simulated', 'active'),
('device_147', 34.9820, 33.9980, 'AYN-Resort-01', 'protaras', 0.20, 300, 'simulated', 'active'),
('device_148', 35.0120, 34.0540, 'PRO-Fig-Tree-01', 'protaras', 0.19, 280, 'simulated', 'active'),
('device_149', 35.0370, 33.9550, 'PRO-Paralimni-01', 'protaras', 0.24, 320, 'simulated', 'active'),
('device_150', 34.9850, 33.9750, 'AYN-Marina-01', 'protaras', 0.21, 300, 'simulated', 'active');

-- 验证
SELECT location, COUNT(*) as count FROM device_registry WHERE device_type = 'simulated' GROUP BY location ORDER BY count DESC;
SELECT COUNT(*) as total FROM device_registry;
