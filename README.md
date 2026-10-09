# Game HD Enhancer 👓

> **当前优先项目：[Game-HD2D-Enhancer](Game-HD2D-Enhancer/README.md)**（2D 老游戏 / 模拟器实时高清眼镜）。本阶段排除 DLSS5。源码收集结果见 [实际收集状态](Game-HD2D-Enhancer/SOURCE_COLLECTION_STATUS.md)。
>
> 以下保留原始设计讨论；它们是目标而非已经验证的功能。当前实施范围、许可证规则与完成度以 `Game-HD2D-Enhancer/` 为准。

**定位：给游戏戴一副“实时高清眼镜”。**

Game HD Enhancer 的目标不是修改游戏资源、替换游戏文件或重新制作高清材质，而是在游戏最终画面之上增加一个可随时开启/关闭的 **实时高清滤镜层**。

## 核心原则

- 不修改游戏本体。
- 不要求提前把游戏加入软件。
- 游戏运行后，可直接选择当前窗口并开启增强。
- 随时关闭 Game HD Enhancer，游戏继续正常运行。
- 不影响游戏存档、进度或游戏文件。
- 正常运行时不录屏、不保存渲染帧、不生成视频缓存。
- 每帧仅执行：**捕获 → GPU 实时处理 → 显示 → 丢弃**。
- Game HD Enhancer 不能成为游戏运行依赖；即使软件关闭、崩溃或被删除，游戏仍可正常运行。

## 首批目标程序

- 天马G及其调用的模拟器
- mGBA
- 老 Windows 单机游戏，例如《仙剑奇侠传》《轩辕剑叁外传：天之痕》
- 《吞食天地 Online》单机版
- 其他可捕获的 Windows 游戏 / 模拟器窗口

## V1 技术路线

```
Game / Emulator Window
        ↓
Windows Graphics Capture
        ↓
D3D11 GPU Texture
        ↓
Real-time Enhancement Pipeline
        ├─ Integer Scaling
        ├─ NIS
        ├─ FSR 1
        ├─ Pixel / Retro filters
        ├─ Anime-style upscaling (optional)
        ├─ SMAA / AA
        └─ Sharpen / Color
        ↓
DXGI SwapChain
        ↓
Borderless / Topmost HD Output
        ↓
Mouse & Keyboard Pass-through
```

V1 优先采用外置捕获方案，不做游戏 DLL 注入。

详细设计见：
- [V1 设计原则](docs/DESIGN_V1.md)
- [开源与参考项目清单](docs/OPEN_SOURCE_REFERENCES.md)
