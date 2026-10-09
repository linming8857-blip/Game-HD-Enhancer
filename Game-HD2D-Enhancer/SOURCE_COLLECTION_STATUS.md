# Game-HD2D-Enhancer 源码收集结果

生成时间：2026-10-09T21:54:01.016879+00:00

实际保存 0 个原始文件，共 0 字节。

**这是开发源码资料库，不是已编译或实机验证的高清软件。**

源码与原始许可证一同保存；不自动执行第三方安装脚本、不运行第三方二进制。
collected 表示源码快照已入库，不代表运行依赖/SDK/模型/演示资源全部齐备。
预编译二进制、模型权重、演示视频等排除项逐一记录于 provenance/*/ORIGIN.json。
partial 表示仍有缺少的子模块、LFS对象或超大文件；不得称为齐全。

| 项目 | 状态 | 文件数 | 固定提交 |
|---|---|---:|---|
| NVIDIAGameWorks/NVIDIAImageScaling | not_collected | 0 | - |

> NVIDIAGameWorks/NVIDIAImageScaling: HTTP Error 404: Not Found

| GPUOpen-Effects/FidelityFX-FSR | not_collected | 0 | - |

> GPUOpen-Effects/FidelityFX-FSR: HTTP Error 404: Not Found

| GPUOpen-Effects/FidelityFX-CAS | not_collected | 0 | - |

> GPUOpen-Effects/FidelityFX-CAS: HTTP Error 404: Not Found

| iryoku/smaa | not_collected | 0 | - |

> iryoku/smaa: HTTP Error 404: Not Found

| robmikh/Win32CaptureSample | not_collected | 0 | - |

> robmikh/Win32CaptureSample: HTTP Error 404: Not Found

| CRTYPUBG/NovaScale-Upscaler | not_collected | 0 | - |

> CRTYPUBG/NovaScale-Upscaler: HTTP Error 404: Not Found

| bloc97/Anime4K | not_collected | 0 | - |

> bloc97/Anime4K: HTTP Error 404: Not Found

| Blinue/Magpie | not_collected | 0 | - |

> Blinue/Magpie: HTTP Error 404: Not Found

| mausimus/ShaderGlass | not_collected | 0 | - |

> mausimus/ShaderGlass: HTTP Error 404: Not Found

| libretro/slang-shaders | not_collected | 0 | - |

> libretro/slang-shaders: HTTP Error 404: Not Found

| funnyplanter/CuNNy | not_collected | 0 | - |

> funnyplanter/CuNNy: HTTP Error 404: Not Found

| KhronosGroup/glslang | not_collected | 0 | - |

> KhronosGroup/glslang: HTTP Error 404: Not Found

| KhronosGroup/SPIRV-Cross | not_collected | 0 | - |

> KhronosGroup/SPIRV-Cross: HTTP Error 404: Not Found

| ValveSoftware/gamescope | not_collected | 0 | - |

> ValveSoftware/gamescope: HTTP Error 404: Not Found

| crosire/reshade | not_collected | 0 | - |

> crosire/reshade: HTTP Error 404: Not Found

| SpecialKO/SpecialK | not_collected | 0 | - |

> SpecialKO/SpecialK: HTTP Error 404: Not Found

| furkan-1337/LFG | not_collected | 0 | - |

> furkan-1337/LFG: HTTP Error 404: Not Found

| mausimus/ShaderBeam | not_collected | 0 | - |

> mausimus/ShaderBeam: HTTP Error 404: Not Found


## 未纳入

- ThioJoe/Full-Screen-DLSS5-Wrapper：用户明确要求 DLSS5 先不要；不下载源码、DLL、模型或相关组件
- L65536/RealTimeSuperResolutionScreenUpscalerforLinux：尚未确认完整项目的重分发许可证；当前 LICENSE 路径返回404，先不复制到公开仓库
- martymcmodding/iMMERSE：之前查看的许可为 All rights reserved；没有单独授权不纳入
- martymcmodding/qUINT：之前查看的许可为 All rights reserved；没有单独授权不纳入

## 下一步

先完成独立 WGC/D3D11 捕获与关闭恢复，再接入整数缩放和轻量空间放大。
鼠标缩放坐标映射、焦点、Alt-Tab、窗口最小化、帧队列和掌机功耗必须单独测试。
仅有点击穿透不代表缩放后点击坐标必然正确；仅有着色器不代表已完成应用集成。
