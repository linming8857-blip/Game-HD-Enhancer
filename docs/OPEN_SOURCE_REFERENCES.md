# Game HD Enhancer — GitHub 参考项目清单

本文件记录当前阶段最值得研究的公开项目。

> 注意：**“参考”不代表可以直接复制代码。**
> 每个项目必须单独遵守其许可证。GPL 项目主要用于研究架构；All Rights Reserved 项目只能观察公开实现思路，不能直接复用其代码。

## A. 与“实时高清眼镜”概念最接近

### 1. RealTimeSuperResolutionScreenUpscalerforLinux
https://github.com/L65536/RealTimeSuperResolutionScreenUpscalerforLinux

**价值：非常高**

虽然项目名带 Linux，但仓库包含 Windows 实现。

Windows 版本已经体现了非常接近我们的流程：

```
Current Active Window
       ↓
Windows Graphics Capture
       ↓
GPU Compute Shader
       ↓
SwapChain
       ↓
Transparent / Click-through Overlay
```

已见实现特征：

- Windows Graphics Capture
- D3D11 路径
- HLSL / CuNNy shader
- 实时放大
- SwapChain 输出
- Topmost
- Transparent / NoActivate
- 输入穿透
- 快捷键开启 / 停止当前窗口增强

**适合研究：**
- “随开随用”的交互；
- 当前活动窗口捕获；
- 透明覆盖层；
- 输入穿透；
- Capture / Compute / Present 并行流程。

**许可证：**
仓库具体许可证需在复用代码前单独确认；当前阶段以架构研究为主。

---

### 2. NovaScale-Upscaler
https://github.com/CRTYPUBG/NovaScale-Upscaler

**价值：非常高**

项目定位就是 Windows 实时超分，并强调：

- No DLL injection
- No API hooking
- DirectX 11
- GPU spatial upscaling
- HLSL Compute Shader

当前仓库结构包括：

```
core/
  capture_dxgi.c
  present_dx11.c
  upscale_basic.c
  upscale_cnn.c
  temporal.c

shaders/
cnn/
ui/
```

已看到的核心结构：

```
DXGI Capture
   ↓
D3D11 Texture
   ↓
Compute Shader
   ↓
Upscaled Texture
   ↓
DXGI SwapChain
   ↓
Topmost Output Window
```

**许可证：MIT**

因此是目前最值得作为 V1 代码实现参考之一的项目。

---

### 3. Magpie
https://github.com/Blinue/Magpie

**价值：非常高 / 成熟度高**

Windows 通用窗口放大工具。

已确认包含：

- Windows Graphics Capture
- Desktop Duplication
- GDI
- DwmSharedSurface
- 全屏 / 窗口缩放
- Anime4K
- FSR
- CRT
- CAS
- SMAA
- FSRCNNX
- 多显示器支持

**适合研究：**
- 捕获后端抽象；
- Scaling pipeline；
- Shader/effect pipeline；
- Profile 架构；
- 老游戏兼容性；
- 帧同步和流畅度优化。

**许可证：GPLv3**

因此主要用于研究设计和架构。如果 Game HD Enhancer 不准备采用 GPLv3，则不要直接复制其 GPL 代码。

---

### 4. ShaderGlass
https://github.com/mausimus/ShaderGlass

**价值：非常高 / 复古游戏重点**

Windows 桌面/窗口实时 Shader 覆盖工具。

特点：

- Windows Capture API
- DirectX 11
- 支持窗口 clone / fullscreen
- RetroArch shader 后端
- 1200+ 复古 shader
- CRT
- TV / VHS
- Handheld
- Upscaling
- Sharpen / denoise / blur
- .slang / .slangp 导入思路

技术链值得研究：

```
RetroArch Slang
    ↓
glslang
    ↓
SPIR-V
    ↓
SPIRV-Cross
    ↓
HLSL
    ↓
D3D11
```

**适合：**
- mGBA
- 天马G
- FC / SFC / GBA
- 老像素 RPG
- CRT/掌机效果

**许可证：GPLv3**

主要研究架构和 Shader 兼容设计。

---

### 5. Full-Screen-DLSS5-Wrapper
https://github.com/ThioJoe/Full-Screen-DLSS5-Wrapper

**价值：概念验证非常高，但不能直接复制代码**

这是目前找到的与“实时高清眼镜”概念最像的项目之一。

公开 README 描述的结构：

```
Screen / Window Capture
      ↓
DLSS Processing
      ↓
New Borderless Window
      ↓
Topmost Processed Image
      ↓
Click-through Input
```

其理念是：

- 不修改目标程序；
- 不注入目标程序；
- 捕获最终显示结果；
- 实时处理；
- 再以新窗口覆盖到原内容上；
- 鼠标可穿透到下面的程序。

这与 Game HD Enhancer 的“高清眼镜层”非常接近。

**许可证：All Rights Reserved**

因此：

- ✅ 可研究公开架构与交互思想
- ❌ 不直接复用其代码

---

## B. 核心算法与宽松许可参考

### 6. NVIDIA Image Scaling
https://github.com/NVIDIAGameWorks/NVIDIAImageScaling

**用途：V1 核心空间超分**

提供：

- NVScaler
- NVSharpen
- HLSL / GLSL
- Compute Shader
- 不依赖运动矢量
- 可处理当前帧

**许可证：MIT**

非常适合外置实时高清处理器。

---

### 7. AMD FidelityFX FSR
https://github.com/GPUOpen-Effects/FidelityFX-FSR

**用途：V1 核心空间超分**

重点研究 FSR 1：

- EASU
- RCAS
- 当前帧空间放大
- 不要求游戏提供 Motion Vector / Depth

**许可证：MIT**

适合 Game HD Enhancer 的外置模式。

---

### 8. Gamescope
https://github.com/ValveSoftware/gamescope

**价值：显示层架构参考**

支持：

- Integer scaling
- FSR
- NIS
- ReShade effect subset
- Borderless / Fullscreen
- 显示层处理

值得注意的是，它允许在 compositor / display layer 上应用效果，而不要求去 Hook 底层游戏。

**许可证：BSD 2-Clause（主体）**

虽然主要面向 Linux，但架构思想很值得参考。

---

## C. Shader / 后处理生态

### 9. ReShade
https://github.com/crosire/reshade

**用途：V2 Hook Mode 参考**

能力：

- D3D9 / D3D10 / D3D11 / D3D12
- OpenGL
- Vulkan
- Present / SwapChain Hook
- Color Buffer
- 部分场景 Depth Buffer
- ReShade FX

**许可证：BSD 3-Clause（核心仓库）**

V1 不采用注入路线，但 V2 高级模式可以研究它的 API 抽象与 effect compiler。

---

### 10. libretro/slang-shaders
https://github.com/libretro/slang-shaders

**用途：复古 Shader 生态**

适合研究：

- CRT
- LCD / handheld
- xBR 类处理
- scanline
- color simulation
- pixel-art effects
- multi-pass shader pipeline

注意每个 shader 可能有自己独立的许可证声明，不能把整个仓库简单视为一个统一许可。

---

### 11. Anime4K
https://github.com/bloc97/Anime4K

**用途：实验性 2D / 动画 / 线稿增强**

它主打实时动画放大，包含：

- Line reconstruction
- Denoise
- Deblur
- CNN shader variants
- 实时运行思路

对于《仙剑》《天之痕》《吞食天地 Online》这类 2D / 2.5D 游戏可以做实验，但不能假设所有像素素材都适合 Anime4K。

复用前需单独检查具体版本和文件许可证。

---

## D. 后续可研究，但不是 V1 必需

### 12. Special K
https://github.com/SpecialKO/SpecialK

PC 游戏图形注入和性能增强框架。

适合研究：
- 图形 API 包装；
- Late / Global injection；
- D3D 调试；
- Frame pacing。

与“外置高清眼镜”的 V1 哲学不同，因此排在后期。

---

### 13. LFG
https://github.com/furkan-1337/LFG

DirectX 11 的实验性 Frame Generation / Upscaling 项目。

包括：

- Present Hook
- Optical Flow
- Frame Interpolation
- Lanczos / Bicubic / Nearest
- RCAS
- ImGui
- Compute Shader

它需要 DLL 注入，因此不适合作为 V1 主路线。

适合 V2/V3 研究光流、插帧以及实时调试工具。

---

## 当前优先研究顺序

### V1 架构

1. **NovaScale-Upscaler** — MIT、Windows、无注入实时超分
2. **RealTimeSuperResolutionScreenUpscalerforLinux 的 Windows 版本** — WGC + 当前窗口 + transparent overlay
3. **Magpie** — 成熟窗口捕获和 scaling pipeline
4. **ShaderGlass** — RetroArch shader / 复古游戏 Shader 管线
5. **NVIDIA Image Scaling** — NIS
6. **FidelityFX FSR 1** — FSR1

### 仅研究思路

- Full-Screen-DLSS5-Wrapper
- ReShade
- Gamescope
- Special K
- LFG

## 当前结论

Game HD Enhancer V1 不需要发明一整套完全未知的技术。

GitHub 上已有项目分别验证了：

- Windows 实时窗口捕获；
- GPU-only / GPU-first 处理；
- Shader pipeline；
- 空间超分；
- 透明最上层覆盖窗口；
- 输入穿透；
- 随开随关；
- 不修改游戏本体。

Game HD Enhancer 的工作重点将是：

> **把这些已经验证过的技术组合成一个简单、稳定、适合老游戏和模拟器的“实时高清眼镜”。**
