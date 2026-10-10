# Game-HD2D-Enhancer 👓

**给 2D 老游戏和模拟器戴一副随时可摘下的实时高清眼镜。**

本项目位于 `linming8857-blip/Game-HD-Enhancer` 仓库内；原来的设计资料和第三方源码档案继续保留。

## 当前状态：已生成首个 Windows x64 测试版

**0.1.0 alpha · build 3 已编译成功，并通过 CPU 几何与真实 NIS 着色器的 Direct3D11 WARP 软件渲染自检。它是可供实机验证的早期测试版，不是已经完成全部兼容性验收的正式版。**

- [下载测试版 ZIP](https://github.com/linming8857-blip/Game-HD-Enhancer/releases/download/hd2d-v0.1.0-alpha-3/Game-HD2D-Enhancer-0.1.0-alpha-win64.zip)
- [发布页面与 SHA256](https://github.com/linming8857-blip/Game-HD-Enhancer/releases/tag/hd2d-v0.1.0-alpha-3)
- [成功的 Windows 构建与测试日志](https://github.com/linming8857-blip/Game-HD-Enhancer/actions/runs/38012719328)
- [中文使用说明](docs/RUN_V0_1.txt)

构建源码提交：`c74b6c855fab0a8d8d6e109ff2172f1ff23305f2`。
ZIP：303,779 字节。SHA256：`ecd397765384911428ec4adc464dd0a8ad1f8056e6c938b015108fae5c8ebf5e`。

编写、修改和编译诊断在聊天中完成，GitHub Actions 仅执行普通 CMake/MSVC/CTest/打包命令；没有调用 Codex、Work、Codex Action、Codex CLI 或其他 AI 编程服务。

**不要把编译、自检通过等同于你的游戏和掌机已测试通过。** 《吞食天地 Online》、mGBA、其他游戏、ROG Xbox Ally X、全屏实际输入、帧同步、延迟、功耗和续航仍待实机验收。

## 本版实际接入的功能

- 直接列出正在运行的窗口，不要求提前登记 EXE、不要求建立游戏库。
- `Ctrl+Alt+H` 选择前台窗口并开启增强，再按停止；`Ctrl+Alt+End` 紧急停止。
- Windows Graphics Capture 捕获指定窗口；裁切客户区；D3D11 纹理处理与独立窗口显示。
- 真正接入 NVIDIA NIS v1.0.3，默认锐化 20%；不串联第二个锐化器。
- 原味整数/最近邻缩放；双线性兼容模式；保持比例与黑边。
- 全屏增强与独立预览两种显示方式；30/60 FPS 呈现上限，不做插帧。
- 独立测试画面进程：色块、移动方块、鼠标/按键计数，用于本地验证。
- 停止时释放自己的捕获/显示资源；源窗口最小化或关闭时停止；全屏下切换前台应用会停止增强。

**全屏鼠标映射是实验功能。** 它通过普通 Win32 鼠标消息转发，不保证兼容 Raw Input、DirectInput、GetCursorPos、特殊光标、复杂拖拽、鼠标侧键、触控手势或反作弊游戏。键盘/手柄仍由前台游戏接收；实际手柄和触摸尚待验证。鼠标不正确时用 `Ctrl+Alt+End` 停止，再选独立预览并操作原游戏。

目标输出小于源图时，NIS 模式明确回退到双线性缩小；整数模式可能使用非整数最近邻缩小，并在状态栏提示。源图很小、放大超过 2 倍时，先用最近邻整数预放大，再执行 NIS。

**未接入本版：** FSR1、SMAA、CRT/LCD 滤镜、Anime4K/CuNNy、AI 生成、HDR、注入、深度效果、插帧、BFI。虽然有参考源码，但没有以占位菜单冒充已实现功能。**DLSS5 不纳入。**

## 第一次运行

完整解压 ZIP，双击 `Game-HD2D-Enhancer.exe`，不要只移动 EXE，保留 `shaders` 和 `licenses`。

先点击“打开测试画面”，切到测试画面，按 `Ctrl+Alt+H`；检查显示、点击、按键，再按 `Ctrl+Alt+End` 停止。确认后再测试窗口化的真实游戏。

运行优先 Windows 11 x64、支持 Direct3D11 feature level 11_0 的 GPU；最低目标 Windows 10 2004。本版仅 SDR。运行不需要 Python、Visual Studio、Codex、Work 或网络。未签名测试版可能显示未知发布者；核对来源与哈希，不关闭安全保护。

## 不改变游戏本体

不修改游戏 EXE/DLL/资源/存档、不注入游戏、不结束游戏进程。游戏不依赖本程序启动。

捕获帧只在有界 RAM/VRAM 缓冲区实时处理，不录屏、不自动截图、不保存渲染帧/视频缓存、不上传画面。运行时画面路径不读回 CPU；软件渲染自检使用自行生成的测试像素作读回校验，不使用玩家画面。

源游戏自身的失焦暂停或特殊输入行为仍要逐游戏验证，不作“所有应用绝无影响”的承诺。

## 源码与构建

```text
Game-HD2D-Enhancer/
  app/                          我们自己的原生 Windows 程序与自检
  CMakeLists.txt                MSVC/CMake 构建入口
  docs/RUN_V0_1.txt              测试版说明
  docs/IMPLEMENTATION_PLAN.md    长期实施方案（其中待办以本页实际状态为准）
  docs/LICENSE_POLICY.md        第三方许可规则
  third_party/                  候选组件真实源码
  references/                   独立保存的研究项目源码
  provenance/                   来源、固定提交、文件 SHA256 与排除项
  sources.json / sources.lock.json
  SOURCE_COLLECTION_STATUS.md   上游资料收集结果，不是产品完成度
  tools/                        收集器和独立诊断脚本
```

开发构建：`cmake -S Game-HD2D-Enhancer -B build -A x64`，然后 `cmake --build build --config Release`，再 `ctest --test-dir build -C Release -V`。

发布包不会带上约 1.56GB 的参考资料库；本版只依赖自己编写的程序与固定版本 NIS。NIS 原始 MIT 许可在发布包 `licenses/NVIDIAImageScaling.txt` 和 shader 源码里保留。

18 个上游项目的资料收集情况见 [SOURCE_COLLECTION_STATUS.md](SOURCE_COLLECTION_STATUS.md)。排除的二进制、模型、演示资源、符号链接信息在对应 `provenance/` 中；源码收集不等于所有 SDK/运行依赖齐备。

## 许可证

第三方文件维持各自原始许可，不将整个第三方库重新授权为 MIT。GPL 不是禁止使用，但实际集成、修改、分发需遵守对应条件；分目录不能规避许可证义务。混合 shader 库必须逐文件核对。详见 [LICENSE_POLICY.md](docs/LICENSE_POLICY.md)。
