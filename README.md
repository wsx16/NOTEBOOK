
# 🧑‍💻 WSX's Tech Notebook & Digital Garden

![Update](https://img.shields.io/badge/Status-Actively%20Updating-brightgreen)
![Tech Stack](https://img.shields.io/badge/Stack-STM32%20%7C%20Linux%20%7C%20LLM-blue)
![Obsidian](https://img.shields.io/badge/Tools-Obsidian-purple)

欢迎来到我的技术笔记与开源项目仓库。这里记录了我从**嵌入式底层硬件控制**走向**边缘 AI 与大模型部署**的完整探索路径。

---

## 🚀 核心项目：智能终端设备全栈实战

本项目是一个基于 STM32U5 的完整物联网终端解决方案，涵盖了从底层驱动移植、RTOS 调度、GUI 开发到云端 OTA 升级的全链路设计。

### 1. 基础架构与通信 (Infrastructure & Communication)
* **环境配置**：基于最新版 Keil + STM32CubeMX 搭建标准化开发环境。
* **系统调度**：全面支持 FreeRTOS，实现业务逻辑解耦与多任务高效调度。
* **设备间通信**：实现 STM32U5 与指纹识别模块 (STM32 核心) 之间的隔离通信，采用 RS485 硬件接口与自有 Modbus RTU 协议格式。
* **协议与数据流**：通过串口中断进行底层数据接收，在 FreeRTOS 任务中完成 Modbus 数据包解析，并使用消息队列（Message Queue）实现跨任务数据安全传递。
* **物联网接入**：驱动 ESP8266 模块，通过 MQTT 协议直连 EMQX 云端服务器，实现高可靠的物联网数据收发。

### 2. 底层驱动与人机交互 (Drivers & GUI)
* **屏幕驱动移植**：深入阅读 Datasheet，从零完成 LCD 显示屏与 TP 触控屏的工作原理分析与底层驱动代码编写。
* **GUI 框架**：成功移植 LVGL (轻量级 MCU 图形库)，构建流畅的人机交互界面。
* **外设联动控制**：通过 LVGL 界面直接控制底层外设，包括 LED 状态指示、蜂鸣器报警、Wi-Fi 网络动态配置以及指纹模块的深度交互（指纹录入、高精度比对、删除）。
* **持久化存储**：实现 Wi-Fi 账号密码的 Flash 本地安全存储，支持通过串口上位机或本地 GUI 进行灵活配置。

### 3. OTA 升级与硬件测试 (OTA & Hardware Debugging)
* **远程固件升级**：独立完成 IAP (In-Application Programming) 架构设计，结合 YMODEM/XMODEM 协议与 OneNET OTA 云平台，实现设备的平滑远程升级。
* **硬件测量验证**：熟练使用万用表进行电压、电阻、电容及电路连通性测试。
* **信号完整性分析**：熟练操作示波器，精准捕获并分析 UART、I2C、SPI 等核心总线的时序与信号波形。
* **硬件进阶**：开展 PCB 硬件原理图与版图设计的系统性学习。

---

## 🐧 系统移植与底层驱动开发 (System Porting & Drivers)

本模块记录了我向 Linux 操作系统底层的探索与沉淀，致力于打通从 MCU 裸机到复杂操作系统的技术壁垒。

* **Bootloader 与内核**：U-Boot 编译与裁剪、Linux Kernel 移植过程踩坑记录。
* **根文件系统**：基于 BusyBox 构建轻量级 Rootfs。
* **字符设备驱动**：Linux 驱动模型解析、设备树 (Device Tree) 编写规范、平台总线 (Platform Bus) 匹配机制。
* **高级驱动开发**：I2C/SPI 子系统分析、中断下半部机制（Tasklet/Workqueue）及内存映射（mmap）实战。

---

## 🧠 大模型微调量化与 RAG 架构 (LLM Fine-tuning & RAG)

此部分聚焦于人工智能大语言模型在私有化场景与边缘计算环境下的高效落地。

### 1. 大模型微调与量化优化
* **参数高效微调 (PEFT)**：深入实践 LoRA、QLoRA、P-Tuning 等前沿微调技术，极大降低算力门槛。
* **模型量化部署**：掌握 GPTQ、AWQ、GGUF 等主流 4-bit/8-bit 量化方法，实现模型在端侧的轻量化运行。
* **模型压缩算法**：探索网络剪枝 (Pruning)、知识蒸馏 (Distillation) 与层间知识迁移。
* **推理加速技术**：深入剖析 Flash Attention 机制与 Paged Attention 显存优化策略，提升长文本吞吐量。

### 2. RAG (检索增强生成)
* **核心检索技术**：向量数据库搭建与调优、语义搜索原理、稀疏与稠密混合检索 (Hybrid Search) 策略。
* **知识库工程**：高质量文档解析与分块 (Chunking) 策略、高效向量化 (Embedding) 方案及索引结构优化。
* **高级增强策略**：用户查询重写 (Query Rewriting)、多文档信息融合及 Re-rank 相关性重排序算法。
* **系统评估指标**：建立多维度评估体系，涵盖检索召回率、生成准确性及端到端系统响应性能。

---

