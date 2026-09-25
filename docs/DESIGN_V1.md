# Game HD Enhancer — V1 设计原则

## 1. 产品概念：实时高清眼镜

Game HD Enhancer 是一个独立于游戏本体之外的实时显示增强层。

可以把它理解为“近视时戴眼镜”：

- 游戏原本仍然照常运行；
- Enhancer 只改变用户最终看到的画面；
- 需要时戴上，不需要时摘下；
- 摘下后游戏本身没有被改变。

## 2. 三条硬性要求

### 2.1 通用兼容

第一阶段优先覆盖：

- 天马G及其中调用的模拟器；
- mGBA；
- FC / SFC / GBA 等复古模拟器场景；
- 《仙剑奇侠传》《天之痕》等老单机游戏；
- 《吞食天地 Online》单机版；
- 其他普通 Windows 游戏窗口。

设计不能绑定某一个游戏、某一种引擎或某一种模拟器。

### 2.2 随开随用

不采用“先建立游戏库再启动”的强制模式。

预期流程：

1. 用户先正常打开游戏；
2. 打开 Game HD Enhancer；
3. 选择当前正在运行的窗口，或用快捷键选择前台窗口；
4. 开启高清增强；
5. 随时关闭增强；
6. 游戏继续运行。

关闭 Enhancer 后：

- 不退出游戏；
- 不改变游戏进程；
- 不影响存档；
- 不影响进度；
- 不修改 EXE / DLL / 数据文件；
- 不要求以后必须通过 Enhancer 启动游戏。

可提供“记住上次参数/预设”，但它只能是便利功能，不能成为使用前提。

### 2.3 纯实时渲染，不保存画面

默认运行流程：

```
Capture Frame
    ↓
GPU Process
    ↓
Present
    ↓
Discard Frame
```

正常模式中：

- 不录屏；
- 不保存渲染帧；
- 不生成视频；
- 不把连续画面写入硬盘；
- 不为了渲染建立永久缓存。

GPU 纹理、短时帧缓冲等运行时内存资源是允许的，但在使用结束后释放。

## 3. V1 推荐架构

### Capture

首选：
- Windows Graphics Capture (WGC)

备用：
- DXGI Desktop Duplication
- 其他兼容捕获后端仅作为补充

目标是尽量保持帧在 GPU 路径中，减少 GPU → CPU → GPU 往返复制。

### Renderer

- C++ / C 核心
- Direct3D 11
- HLSL Compute / Pixel Shader
- DXGI SwapChain
- Borderless output window
- Topmost overlay
- Click-through / input pass-through

### Upscaling

V1 重点：

- Integer Scaling
- Nearest Neighbor
- Bicubic / Lanczos（视性能）
- NVIDIA Image Scaling (NIS)
- AMD FidelityFX Super Resolution 1 (FSR 1)

实验：

- Anime4K / 线稿型实时算法
- 轻量 CNN shader
- 针对 2D 像素游戏的 Pixel HD 管线

### Post-processing

- Sharpen
- SMAA / FXAA（按需要）
- Color / Gamma / Contrast
- Deband
- CRT / scanline
- Retro handheld / TV filters

## 4. 不属于 V1 核心的功能

以下内容放到后续版本研究：

- ReShade 式 Present Hook；
- DirectX / Vulkan / OpenGL 注入；
- Depth Buffer；
- Optical Flow；
- Frame Generation；
- DLSS 5 外置实验模式；
- 复杂 AI 超分；
- 游戏材质替换；
- 资源解包或 MOD 系统。

## 5. 安全边界

V1 的默认理念是：

> **增强用户看到的画面，而不是改变游戏本身。**

因此优先采用不注入、不改文件、不持久依赖的方案。

对于联网游戏或带反作弊程序的应用，即使使用外置捕获方案，也应尊重对应游戏规则；Game HD Enhancer 的主要目标仍是模拟器、复古游戏、老单机游戏和本地单机项目。
