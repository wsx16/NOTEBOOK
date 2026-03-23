# 基于 YOLOv11 的移动端实时目标检测系统

## 项目简介

将 YOLOv11n 模型部署到 Android 移动端（Redmi K70 / 骁龙 8 Gen 2），通过 NCNN 推理框架 + Vulkan GPU 加速，实现实时目标检测，推理耗时约 30ms/帧。

---

## 一、简历写法

```
项目名称：基于 YOLOv11 的移动端实时目标检测系统
技术栈：YOLOv11 / NCNN / Vulkan / Android NDK (C++ JNI) / CameraX / Java

- 将 YOLOv11n 模型经 PyTorch → ONNX → NCNN 完成格式转换，并通过精度验证确保转换无损
- 基于 NCNN 推理框架在 Android 端实现完整检测 pipeline：
  相机实时取帧 → letterbox 预处理 → GPU 推理 → NMS 后处理 → 检测框绘制
- 通过 JNI 桥接 C++ 推理层与 Java UI 层，启用 Vulkan GPU 加速，
  在骁龙 8 Gen 2 设备上实现 30ms/帧的实时推理
- 实现 COCO 80 类目标检测，支持置信度过滤、IoU 阈值调节、实时 FPS 显示
```

---

## 二、技术架构

### 2.1 模型转换链路

```
PyTorch (.pt)
  → ONNX (.onnx)              # 通用中间表示，跨框架兼容
    → NCNN (.param + .bin)     # 移动端专用格式
```

| 格式 | 作用 |
|------|------|
| `.pt` | PyTorch 原始模型，包含网络结构 + 权重 |
| `.onnx` | 开放神经网络交换格式，工业标准的模型中间表示 |
| `.param` | NCNN 网络结构文件（文本格式，描述每层的类型、输入输出） |
| `.bin` | NCNN 权重文件（二进制，存放卷积核参数等） |

**为什么中间要经过 ONNX？**

ONNX 是工业标准的模型交换格式，所有主流框架都支持导入导出，相当于模型界的"通用语言"。直接 PyTorch → NCNN 的转换器不够成熟，经过 ONNX 中转更稳定可靠。

### 2.2 推理流程

```
相机帧 (YUV_420_888)
  ↓ ① 格式转换
NV21 → JPEG → Bitmap (RGB)
  ↓ ② 旋转校正
根据传感器方向旋转 90°/180°/270°
  ↓ ③ letterbox 预处理
等比缩放到 320×320，不足部分填充灰色(114)
归一化 [0,255] → [0,1]
  ↓ ④ 神经网络推理 (NCNN + Vulkan GPU)
输入: [1, 3, 320, 320] 张量
输出: [84, 2100] 矩阵
  ↓ ⑤ 后处理解码
84 = 4(坐标) + 80(类别得分)
2100 = 不同尺度的 anchor 数量
  ↓ ⑥ 置信度过滤 (>0.25)
  ↓ ⑦ NMS 非极大值抑制 (IoU>0.45 去重)
  ↓ ⑧ 坐标还原到原图尺寸
  ↓ ⑨ 绘制检测框 + 类别名称
```

### 2.3 项目文件结构

```
YoloDetector/
├── ncnn-20260113-android-vulkan/      # NCNN SDK (Vulkan 版)
│   └── arm64-v8a/
│       ├── include/ncnn/
│       └── lib/
├── app/
│   └── src/main/
│       ├── assets/
│       │   ├── model.ncnn.param       # NCNN 网络结构
│       │   └── model.ncnn.bin         # NCNN 权重
│       ├── cpp/
│       │   ├── CMakeLists.txt         # C++ 构建配置
│       │   └── yolo_ncnn_jni.cpp      # C++ 推理核心 (JNI)
│       ├── java/com/example/yolodetector/
│       │   ├── MainActivity.java      # 相机 + 实时检测
│       │   ├── YoloNcnn.java          # JNI 封装 + 结果解析
│       │   └── OverlayView.java       # 检测框绘制
│       ├── res/layout/
│       │   └── activity_main.xml      # UI 布局
│       └── AndroidManifest.xml        # 权限配置
└── build.gradle.kts                   # Gradle 构建脚本
```

---

## 三、核心原理详解

### 3.1 NMS 非极大值抑制

**为什么需要 NMS？**

一个物体可能被多个 anchor 同时检测到，产生多个重叠的框。NMS 的作用是去掉冗余框，只保留最好的那个。

**算法步骤：**

```
1. 按置信度从高到低排序所有检测框
2. 取置信度最高的框 A，放入结果集
3. 计算 A 与剩余所有框的 IoU（交并比）
4. 删除 IoU > 阈值（0.45）的框（它们检测的是同一个物体）
5. 重复 2-4，直到处理完所有框
```

**IoU 计算公式：**

```
IoU = 两框交集面积 / 两框并集面积

交集面积 = max(0, min(x2_a, x2_b) - max(x1_a, x1_b))
         × max(0, min(y2_a, y2_b) - max(y1_a, y1_b))

并集面积 = 面积_A + 面积_B - 交集面积
```

### 3.2 Vulkan GPU 加速

**Vulkan 是什么？**

Vulkan 是新一代图形和计算 API（Khronos 标准），用于在 GPU 上执行通用计算。

**为什么选 Vulkan 而不是 OpenCL？**

| 对比项 | Vulkan | OpenCL |
|--------|--------|--------|
| Android 支持 | 7.0+ 原生支持 | 需要厂商驱动，部分手机不支持 |
| GPU 兼容性 | 高通/Mali/PowerVR 全平台 | 部分设备不可用 |
| CPU 开销 | 更低 | 较高 |
| 生态 | Android 官方推荐 | 逐渐被淘汰 |

**工作原理：**

NCNN 内部将卷积、池化等神经网络算子编译为 Vulkan Compute Shader，直接在 GPU 上并行计算。GPU 拥有数千个计算核心，适合矩阵乘法等并行度高的运算，比 CPU 快 2-3 倍。

### 3.3 letterbox 预处理

**为什么不直接 resize 到 320×320？**

直接 resize 会改变宽高比，导致目标变形（比如人变胖、车变扁），影响检测精度。

**letterbox 做法：**

```
原图 640×480 (4:3)
  ↓ 等比缩放到 320×240
  ↓ 上下各填充 40 像素灰色 (114)
  → 最终 320×320，目标不变形
```

后处理时需要：
1. 减去 padding 偏移
2. 除以缩放比例
3. 还原到原图坐标系

### 3.4 JNI 桥接机制

Java 层和 C++ 层通过 JNI（Java Native Interface）通信：

```
Java 层                              C++ 层
YoloNcnn.java                        yolo_ncnn_jni.cpp
  │                                    │
  ├─ init(AssetManager, bool) ───→     加载 .param/.bin 到内存
  │                                    初始化 NCNN Net 和 Vulkan
  │                                    │
  ├─ detect(Bitmap, conf, iou) ──→     锁定 Bitmap 像素
  │                                    预处理 + 推理 + NMS
  │                                    返回 float[] 数组
  │                                    │
  └─ parseResults(float[]) ────        纯 Java 解析结果
```

JNI 函数命名规则：`Java_包名_类名_方法名`

例如：`Java_com_example_yolodetector_YoloNcnn_detect`

---

## 四、性能数据

### 4.1 实测数据（Redmi K70 / 骁龙 8 Gen 2）

| 指标 | 数值 |
|------|------|
| 推理耗时 | ~30ms（Vulkan GPU） |
| 总帧间隔 | ~75ms（含预处理） |
| 实际帧率 | ~13 FPS |
| 模型大小 | ~11MB（yolo11n NCNN） |
| 输入尺寸 | 320×320 |
| 检测类别 | COCO 80 类 |

### 4.2 性能瓶颈分析

```
总耗时 ~75ms 分解：
├── YUV → Bitmap 转换    ~20ms   (CPU，主要瓶颈)
├── Bitmap 旋转           ~10ms   (CPU)
├── letterbox 预处理      ~5ms    (CPU)
├── NCNN 推理            ~30ms   (GPU, Vulkan)
├── NMS 后处理           ~2ms    (CPU)
└── UI 绘制              ~8ms    (CPU)
```

### 4.3 优化方向

| 优化方法 | 预期效果 | 难度 |
|---------|---------|------|
| 减小输入尺寸到 224×224 | 推理快 2x，精度略降 | 低 |
| INT8 量化 | 模型减小 4x，推理快 1.5x | 中 |
| RenderScript 做 YUV 转换 | 预处理快 3x | 中 |
| 推理放子线程 | UI 不卡顿，体验提升 | 中 |
| 使用 ImageReader 直接拿 RGB | 跳过 YUV 转换 | 高 |

---

## 五、面试问答准备

### Q1：这个项目解决了什么问题？

> 将深度学习目标检测模型部署到资源受限的移动设备上，实现不依赖服务器的实时检测。应用场景包括：离线环境检测、边缘计算、隐私敏感场景（数据不出设备）。

### Q2：为什么选 NCNN 不选 TFLite？

> NCNN 对高通 GPU 的 Vulkan 计算支持更好，推理速度更快。纯 C++ 实现，无额外依赖，包体积小。TFLite 在 Android 上主要走 CPU 或 NNAPI，对 Vulkan 的支持不如 NCNN 直接。

### Q3：模型转换过程中精度会下降吗？

> 做了 ONNX 精度验证，用相同的随机输入分别跑 PyTorch 和 ONNX Runtime，输出最大误差 < 1e-5，转换基本无损。NCNN 使用 FP32 推理，与 ONNX 精度一致。如果用 FP16 或 INT8 量化，会有微小精度损失，但通常 mAP 下降不超过 1%。

### Q4：30ms 的瓶颈在哪？怎么优化？

> GPU 推理本身已经很快了。瓶颈在 CPU 端的图像预处理：YUV → JPEG → Bitmap 的转换链路涉及 JPEG 编解码，开销大。优化方向：① 用 RenderScript 或 GPU 直接做 YUV→RGB 转换 ② 将推理放到独立线程 ③ 减小输入分辨率。

### Q5：NMS 的时间复杂度是多少？

> O(N²)，N 是过滤后的候选框数量。实际中 N 通常很小（几十个），所以 NMS 耗时可以忽略（<2ms）。如果 N 很大，可以用 Soft-NMS 或 DIoU-NMS 替代。

### Q6：Vulkan 和 OpenGL ES 有什么区别？

> OpenGL ES 主要用于图形渲染（画三角形、纹理），Vulkan 除了渲染外还支持通用计算（Compute Shader）。Vulkan 是更底层的 API，给开发者更多控制权，CPU 开销更低，适合深度学习推理这类计算密集型任务。

### Q7：遇到的最大困难是什么？

> Windows 中文用户名路径导致 Android NDK 的 CMake 崩溃（NTSTATUS 0xC0000409），排查了很久才发现是路径编码问题。解决方案是将 Android SDK 和项目都迁移到纯英文路径下。这让我意识到跨平台开发中编码和路径问题的重要性。

### Q8：如果要部署到 iOS 怎么做？

> iOS 推荐用 CoreML 格式：`yolo export format=coreml`，生成 `.mlpackage` 文件，Xcode 直接集成。也可以用 NCNN 的 iOS 版本，流程和 Android 类似，只是 UI 层换成 Swift + AVFoundation。

---

## 六、部署步骤速查

```bash
# 1. 导出 ONNX
yolo export model=yolo11n.pt format=onnx imgsz=320 simplify=True

# 2. 验证精度
python -c "
import onnxruntime, numpy as np
from ultralytics import YOLO
sess = onnxruntime.InferenceSession('yolo11n.onnx')
img = np.random.randn(1,3,320,320).astype(np.float32)
print('Max diff:', np.abs(sess.run(None,{'images':img})[0] -
      YOLO('yolo11n.pt').model(torch.from_numpy(img))[0].detach().numpy()).max())
"

# 3. 导出 NCNN
yolo export model=yolo11n.pt format=ncnn imgsz=320

# 4. Android 项目集成
# 将 model.ncnn.param 和 model.ncnn.bin 放入 app/src/main/assets/
# 下载 ncnn-android-vulkan SDK 放入项目根目录
# 编写 CMakeLists.txt + JNI 代码 + Java 调用层
# Build → Run 到手机
```

---

## 七、这个项目值不值得写进简历

**结论：值得，但要看岗位方向。**

| 岗位方向 | 含金量 | 原因 |
|---------|--------|------|
| 嵌入式 AI / 边缘部署 | 非常高 | 正好对口，完整链路 |
| 计算机视觉工程师 | 高 | 体现工程落地能力 |
| Android 开发 | 加分项 | 会 NDK + JNI 比纯 Java 开发有竞争力 |
| 算法研究岗 | 一般 | 面试官更看重论文和模型创新，不是部署 |
| 后端/前端开发 | 没用 | 不相关 |

**面试官真正在意的不是你做了什么，而是你理解了多深。**

---

## 八、如何真正消化吸收这些知识

别死背，每个知识点问自己三个问题。

### 第一层：是什么（一句话说清楚）

```
NMS        → 去掉重复检测框，只留最好的
Vulkan     → 让 GPU 跑神经网络计算
letterbox  → 等比缩放不变形
JNI        → Java 调 C++ 的桥
ONNX       → 模型格式的"普通话"
```

如果你连一句话都说不出来，说明没理解。

### 第二层：为什么（不这样做会怎样）

```
为什么要 NMS？
→ 不做的话一个人身上画 20 个框

为什么要 letterbox 不直接 resize？
→ 直接 resize 人会变胖，检测不准

为什么用 NCNN 不用 PyTorch？
→ PyTorch 要装几百 MB 的 Python 环境，手机装不下

为什么经过 ONNX 中转？
→ PyTorch 直接转 NCNN 的工具不成熟，ONNX 是通用中转站

为什么用 Vulkan？
→ 手机 GPU 有几千个核心闲着，不用白不用
```

### 第三层：怎么实现的（白纸上写出来）

**自测方法：** 关掉所有参考资料，拿一张白纸，尝试：

1. 画出从相机帧到检测结果的完整流程图
2. 手写 NMS 的伪代码
3. 手写 IoU 的计算公式
4. 解释模型输出 `[84, 2100]` 中 84 和 2100 分别是什么
5. 画出 letterbox 的缩放 + padding 过程

**写不出来的就是没真正理解的，回去看代码搞明白。**

### 面试官的追问模式

面试官不会问你"你做了什么"，他会一直往深处挖：

```
面试官：你说用了 NMS，时间复杂度多少？
你：O(N²)

面试官：N 大了怎么办？
你：实际中 N 很小。如果真的大了，可以用 Soft-NMS 或按类别分组做

面试官：Soft-NMS 和普通 NMS 区别是什么？
你：普通 NMS 直接删框，Soft-NMS 是降低重叠框的置信度，不直接删

面试官：你的 30ms 瓶颈在哪？
你：GPU 推理约 15ms，预处理约 15ms，瓶颈在 YUV 转 Bitmap 的 JPEG 编解码

面试官：怎么优化？
你：用 RenderScript 或直接操作 YUV 数据跳过 JPEG 编解码
```

**你看，他一直在往深处挖。如果你只是背答案，第三个问题就卡住了。**

### 学习行动计划

1. **今天**：把 `yolo_ncnn_jni.cpp` 逐行看一遍，确保每一行你都知道在干什么
2. **明天**：关掉电脑，白纸手写 NMS 算法和推理流程图
3. **后天**：找个朋友模拟面试，让他随便问，看你能不能自然地回答
4. **持续**：尝试换不同的模型（比如自己训练的 `best.pt` 收费站模型），体会参数变化对结果的影响

**理解了原理的人，面试时说话是连贯的、自信的。背答案的人，一被追问就慌。面试官一眼就能看出区别。**

---

## 九、适合的岗位方向

- 嵌入式 AI / 边缘计算工程师
- 移动端算法部署工程师
- 计算机视觉工程师
- Android 开发工程师（AI 方向）
