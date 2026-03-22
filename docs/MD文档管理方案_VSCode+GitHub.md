# MD文档管理方案：VS Code + GitHub

> **原则：一个仓库管所有，文件夹分类，每天push一次**

---

## 1. 先装两个 VS Code 插件

打开 VS Code → 左侧扩展商店，搜索安装：

| 插件 | 作用 |
|------|------|
| **Markdown Preview Enhanced** | 实时预览md，表格/代码块渲染效果好 |
| **GitLens** | 在VS Code里可视化操作Git，不用背命令 |

装完后，打开任意 `.md` 文件，按 `Ctrl+Shift+V` 就能预览。

---

## 2. 仓库结构（一个仓库管全部）

```
pv-iot-project/
│
├── README.md                    ← 项目介绍（面试官第一眼看这个）
│
├── docs/                        ← 项目文档（你现有的md全放这里）
│   ├── PV_Project_Handoff_v4.md
│   ├── conversation_handoff.md
│   ├── device_management_guide.md
│   └── insert_devices.sql
│
├── thesis/                      ← 论文相关素材
│   ├── figures/                 ← 论文用的图片截图
│   │   ├── parity_plot.pdf
│   │   ├── wiring_diagram_v2.pdf
│   │   └── grafana_screenshots/
│   ├── data/                    ← 实验数据
│   │   └── lab_test_record_final_v2.xlsx
│   └── thesis_notes.md          ← 论文写作笔记/待办
│
├── firmware/                    ← STM32 代码（老师要求的）
│   ├── a7670e.h
│   ├── a7670e.c
│   └── app_zigbee.c
│
├── nodered/                     ← Node-RED 流程导出
│   ├── device-registry-flow.json
│   └── remote-control-flow.json
│
├── learning/                    ← 学习笔记 + 练习代码
│   ├── c-basics/
│   │   ├── week1-pointer/
│   │   │   ├── 01_basic_pointer.c
│   │   │   ├── 02_pointer_swap.c
│   │   │   └── notes.md
│   │   ├── week2-string/
│   │   ├── week3-keywords/
│   │   └── week4-datastructure/
│   ├── linux-system/
│   ├── linux-driver/
│   └── can-autosar/
│
├── study-plan/                  ← 学习计划（我给你的文档）
│   ├── 嵌入式Linux驱动_车企求职学习计划.md
│   └── 阶段一_C语言补基础_详细计划.md
│
├── .gitignore
└── LICENSE
```

---

## 3. 从零开始操作（跟着做一遍）

### 第一步：配置Git（只需要做一次）

打开终端（VS Code里按 Ctrl+` ）：

```bash
# 设置你的名字和邮箱（用你GitHub注册的邮箱）
git config --global user.name "你的英文名"
git config --global user.email "你的邮箱@example.com"
```

### 第二步：GitHub上创建仓库

1. 打开 https://github.com/new
2. Repository name 填：`pv-iot-project`
3. Description 填：`IoT-based PV Monitoring System with PR Fault Detection`
4. 选 **Public**（面试官能看到）
5. 勾选 **Add a README file**
6. 点 **Create repository**

### 第三步：克隆到本地

```bash
# 进入你想放代码的目录
cd D:/Projects   # Windows示例，换成你自己的路径

# 克隆仓库（把链接换成你自己的）
git clone https://github.com/你的用户名/pv-iot-project.git

# 进入仓库目录
cd pv-iot-project
```

### 第四步：创建文件夹结构

```bash
mkdir -p docs thesis/figures thesis/data firmware nodered learning/c-basics study-plan
```

### 第五步：把你现有的文件放进去

把你手里的文件按上面的结构复制进对应文件夹，然后：

```bash
# 查看状态（看有哪些新文件）
git status

# 添加所有文件
git add .

# 提交（写清楚这次干了什么）
git commit -m "初始化项目结构，导入现有文档和代码"

# 推送到GitHub
git push origin main
```

---

## 4. 每天的工作流程（养成习惯）

### 学习时

```bash
# 1. 写代码/笔记（在learning/c-basics/week1-pointer/下面）

# 2. 写完后提交（每天晚上22:00之前做一次）
git add .
git commit -m "Day1: 指针基础练习 + 笔记"
git push origin main
```

### VS Code里更方便的方式（不用敲命令）

1. 左侧栏点 **源代码管理** 图标（分支形状）
2. 改过的文件会自动列出来
3. 点文件旁边的 **+** 号 → 暂存（等于 git add）
4. 上方输入框写提交信息 → 点 **√** 提交（等于 git commit）
5. 点 **...** → **推送**（等于 git push）

### commit message 怎么写

```
好的写法：
  Day3: 二级指针和数组指针练习代码
  fix: 修复模拟器v4.4云层概率bug
  thesis: Ch5插入稳态精度测试表格
  docs: 更新项目handoff文档v5

不好的写法：
  update
  修改了一些东西
  111
```

---

## 5. .gitignore 文件（排除不该上传的东西）

在仓库根目录创建 `.gitignore`：

```
# 编译产物
*.o
*.exe
*.out

# 系统文件
.DS_Store
Thumbs.db
desktop.ini

# VS Code 本地配置
.vscode/

# Node modules
node_modules/

# 大文件（论文docx在本地管理，不上传）
*.docx
*.pptx

# Docker数据卷
docker-data/
```

---

## 6. README.md 怎么写（面试官会看！）

```markdown
# IoT-based PV Monitoring System

A smart photovoltaic monitoring system with PR-based fault detection
and remote breaker control, built for my MSc thesis at CUT.

## System Architecture

```
PV Panel → Breaker(TOWSMR1-40) → STM32WB5MM → A7670E(4G)
  → MQTT → Node-RED → InfluxDB → Grafana
```

## Features

- Real-time power monitoring (voltage, current, power)
- Performance Ratio fault detection (4-level classification)
- Remote breaker ON/OFF control via MQTT
- 50-device simulator for scalability testing
- GHI-based irradiance matching (8-grid-point coverage)

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Edge | STM32WB5MM-DK + A7670E (4G LTE) |
| Protocol | MQTT (broker.emqx.io) + Zigbee |
| Backend | Node-RED + InfluxDB + PostgreSQL |
| Frontend | Grafana Dashboard |
| Deployment | Docker Compose |

## Lab Test Results

| Metric | Voltage | Current | Power |
|--------|---------|---------|-------|
| Mean Error (>50W) | 0.13% | 0.77% | 0.69% |
| Response Time (LCD) | — | — | 10.2s avg |
| Remote Control | — | — | <0.5s |

## Getting Started

See [Installation Guide](docs/install-dashboard.md)

## Author

Tian Qiyuan — MSc Electronic Information, HDU / CUT
```

---

## 7. 日常管理要点

| 规则 | 说明 |
|------|------|
| **每天至少push一次** | 哪怕只改了笔记，也提交。GitHub的绿色格子就是你的学习记录 |
| **一个commit做一件事** | 不要攒一周的改动一起提交 |
| **learning/下面按周建文件夹** | week1-pointer、week2-string...清晰有序 |
| **notes.md每天写** | 哪怕只写3句"今天学了什么"，日积月累就是面试素材 |
| **不传大文件** | docx/pptx/视频不要传GitHub，放本地或网盘 |
| **README保持更新** | 每完成一个阶段更新一下，这是你的门面 |
