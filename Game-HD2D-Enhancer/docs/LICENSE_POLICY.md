# 第三方源码保存与集成规则

## 三件事必须分开

1. 保存公开源码快照。
2. 将源码实际并入 Game-HD2D-Enhancer。
3. 发布编译后的程序。

源码入库不自动表示已满足发行程序的全部许可要求。本项目没有把整个第三方资料库重新授权为 MIT；每份第三方文件继续受原许可证约束，文件内的版权、附加条款优先核查。

## GPL 不是禁止使用

Magpie、ShaderGlass 等 GPL 项目可在遵守其许可条件的前提下保存、修改和再分发。之前把 GPL 简化成“不能复制、只能参考”的说法不准确。

这次把它们作为独立的上游源码快照收集，保留原始许可证、提交和文件校验值。未来实际集成时，需要决定衍生程序的许可和对应源码提供方式；单纯分目录不会免除集成后的义务。

参考原始许可：
- https://github.com/Blinue/Magpie/blob/dev/LICENSE
- https://github.com/mausimus/ShaderGlass/blob/master/LICENSE
- https://www.gnu.org/licenses/gpl-3.0.html

## 候选组件也不能删除版权

NIS / FSR1 / CAS / SMAA / Win32CaptureSample / Anime4K / NovaScale 等候选项目以运行时解析的固定版本为准，保留各自版权和许可文件。MIT/BSD 等也不是“无任何条件”；原始 notices 与免责声明须保留。

- https://github.com/NVIDIAGameWorks/NVIDIAImageScaling
- https://github.com/GPUOpen-Effects/FidelityFX-FSR
- https://github.com/GPUOpen-Effects/FidelityFX-CAS
- https://github.com/iryoku/smaa
- https://github.com/robmikh/Win32CaptureSample
- https://github.com/bloc97/Anime4K

第三方依赖、示例素材和模型可能有不同条款；不能只看根目录的 License 徽标。Anime4K 的 results 演示图不是算法源码，本次排除。

## 混合 Shader 库

libretro/slang-shaders 作为独立参考资料库保存，不为全部 shader 作统一的可商用或可闭源使用保证。选择每个 shader 时须检查源文件、包含文件、LUT/纹理、预设依赖及许可兼容性。无法确认许可的文件不得混进最终发行包。

ShaderGlass 的原始说明同样要求核对单个 shader 的版权和许可：
https://github.com/mausimus/ShaderGlass#notices

## 本次明确不收

- DLSS5：用户主动排除；不下载对应 Wrapper、DLL、模型，不把它作为候选依赖。
- L65536/RealTimeSuperResolutionScreenUpscalerforLinux：目前未确认完整仓库重分发许可；LICENSE 路径404仅表示该路径不存在，不等于已经穷尽所有授权证据。在查明前不复制到本公开仓库。
- iMMERSE / qUINT：此前看到 All rights reserved，不在没有明确授权的情况下复制。

## 完整性与来源

每个入库项目记录 source URL、原始 branch/ref、固定 commit SHA、文件 SHA-256、子模块原提交、排除文件、LFS 指针和是否编译/实测。

收集器的许可证检查仅是保守初筛，不是完整的法律审计。发现不明或冲突条款时停止集成，补齐授权证据。

本次不执行上游构建脚本、不运行第三方 EXE、不安装来自仓库的依赖。预编译二进制、发布压缩包、演示视频、模型权重等排除项逐一记录。开发源码资料库存储文件，与运行时“不保存渲染帧”是不同事项。
