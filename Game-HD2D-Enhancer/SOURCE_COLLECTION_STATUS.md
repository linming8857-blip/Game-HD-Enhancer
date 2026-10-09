# Game-HD2D-Enhancer 源码收集结果

生成时间：2026-10-09T21:58:14.484876+00:00

实际保存 27,105 个原始文件，共 1,525,827,573 字节。

**这是开发源码资料库，不是已编译或实机验证的高清软件。**

源码与原始许可证一同保存；不自动执行第三方安装脚本、不运行第三方二进制。
collected 表示源码快照已入库，不代表运行依赖/SDK/模型/演示资源全部齐备。
预编译二进制、模型权重、演示视频等排除项逐一记录于 provenance/*/ORIGIN.json。
partial 表示仍有缺少的子模块、LFS对象或超大文件；不得称为齐全。

| 项目 | 状态 | 文件数 | 固定提交 |
|---|---|---:|---|
| NVIDIAGameWorks/NVIDIAImageScaling | not_collected | 0 | - |

> NVIDIAGameWorks/NVIDIAImageScaling: No top-level license file found: not republishing this source

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
| ValveSoftware/gamescope | partial | 3144 | 36848c2f30e4 |
| crosire/reshade | collected | 2314 | 7bf9de8b33bc |
| SpecialKO/SpecialK | collected | 1940 | 2569dd221046 |
| furkan-1337/LFG | collected | 80 | cd1a2fa2b4fb |
| mausimus/ShaderBeam | collected | 73 | 5f9eef6ab59a |

## 未纳入

- ThioJoe/Full-Screen-DLSS5-Wrapper：用户明确要求 DLSS5 先不要；不下载源码、DLL、模型或相关组件
- L65536/RealTimeSuperResolutionScreenUpscalerforLinux：尚未确认完整项目的重分发许可证；当前 LICENSE 路径返回404，先不复制到公开仓库
- martymcmodding/iMMERSE：之前查看的许可为 All rights reserved；没有单独授权不纳入
- martymcmodding/qUINT：之前查看的许可为 All rights reserved；没有单独授权不纳入

## 下一步

先完成独立 WGC/D3D11 捕获与关闭恢复，再接入整数缩放和轻量空间放大。
鼠标缩放坐标映射、焦点、Alt-Tab、窗口最小化、帧队列和掌机功耗必须单独测试。
仅有点击穿透不代表缩放后点击坐标必然正确；仅有着色器不代表已完成应用集成。
