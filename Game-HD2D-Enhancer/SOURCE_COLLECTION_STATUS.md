# Game-HD2D-Enhancer 源码收集结果

更新时间：2026-10-09T22:03:45.309354+00:00

实际保存 28,135 个上游文件，共 1,557,494,801 字节（含源码、构建文件、许可证、必要资源及文档）。

**这是开发源码资料库，不是已编译或实机验证的 Game-HD2D-Enhancer 软件。**

collected：源码快照已入库；partial：仍有明确缺失项；not_collected：未保存该项目。
预编译程序/库、模型权重、演示视频等不是本次源码交付，逐项排除记录在 provenance/*/ORIGIN.json。
符号链接目标记录在 ORIGIN.json；构建前按需恢复。不可声称所有 SDK/二进制依赖都已齐备。
所有上游代码仅作为文件保存，没有运行。每个文件都有SHA-256；上游提交和子模块提交已记录。

| 项目 | 状态 | 文件数 | 固定提交 |
|---|---|---:|---|
| NVIDIAGameWorks/NVIDIAImageScaling | collected | 328 | 35e13ba316c9 |
| GPUOpen-Effects/FidelityFX-FSR | collected | 669 | a21ffb8f6c13 |
| GPUOpen-Effects/FidelityFX-CAS | collected | 476 | 9fabcc9a2c45 |
| iryoku/smaa | collected | 100 | 71c806a838bd |
| robmikh/Win32CaptureSample | collected | 28 | 49fefe79fd9b |
| CRTYPUBG/NovaScale-Upscaler | collected | 36 | 3888cd0cb722 |
| bloc97/Anime4K | collected | 95 | 7684e9586f8d |
| Blinue/Magpie | collected | 626 | 1c06186ca176 |
| mausimus/ShaderGlass | collected | 3633 | 88b9a75d4d72 |
| libretro/slang-shaders | collected | 5211 | e1d75632a205 |
| funnyplanter/CuNNy | collected | 81 | 906031bb00c1 |
| KhronosGroup/glslang | collected | 3751 | c97e126b09d1 |
| KhronosGroup/SPIRV-Cross | collected | 4848 | aa217aeb6c9f |
| ValveSoftware/gamescope | collected | 3846 | 36848c2f30e4 |
| crosire/reshade | collected | 2314 | 7bf9de8b33bc |
| SpecialKO/SpecialK | collected | 1940 | 2569dd221046 |
| furkan-1337/LFG | collected | 80 | cd1a2fa2b4fb |
| mausimus/ShaderBeam | collected | 73 | 5f9eef6ab59a |

## 明确缺失项

无未说明的源代码下载缺失；仍需单独完成构建和实际运行测试。

## 按要求或许可排除

- ThioJoe/Full-Screen-DLSS5-Wrapper：用户明确要求 DLSS5 先不要；不下载源码、DLL、模型或相关组件
- L65536/RealTimeSuperResolutionScreenUpscalerforLinux：尚未确认完整项目的重分发许可证；当前 LICENSE 路径返回404，先不复制到公开仓库
- martymcmodding/iMMERSE：之前查看的许可为 All rights reserved；没有单独授权不纳入
- martymcmodding/qUINT：之前查看的许可为 All rights reserved；没有单独授权不纳入

## 2D版本下一步

先完成WGC/D3D11独立启停闭环，再接入整数缩放、NIS/FSR1和输入坐标映射。
相关Linux/注入/插帧项目只作资料存档，不参与Windows 2D V1。DLSS5不纳入。
