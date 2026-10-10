# Game-HD2D-Enhancer 👓

**给 2D 老游戏和模拟器戴一副随时可摘下的实时高清眼镜。**

本项目位于 `linming8857-blip/Game-HD-Enhancer` 仓库内；原来的设计资料和第三方源码档案继续保留。

## 当前测试版：0.1.1 alpha · 窗口识别修正版

**0.1.1 alpha · build 9 已编译成功，通过原有图像/几何自检和新增的窗口选择/捕获回归。它仍是待用户真实游戏与掌机验收的测试版。**

- [下载便携测试包](https://github.com/linming8857-blip/Game-HD-Enhancer/releases/download/hd2d-v0.1.1-alpha-9/Game-HD2D-Enhancer-0.1.1-alpha-win64.zip)
- [发布页面](https://github.com/linming8857-blip/Game-HD-Enhancer/releases/tag/hd2d-v0.1.1-alpha-9)
- [成功的构建和回归任务](https://github.com/linming8857-blip/Game-HD-Enhancer/actions/runs/38015378224)
- [本次优化、验证范围和哈希](docs/WINDOW_SELECTION_V0_1_1_REPORT.md)
- [新版中文使用说明](docs/RUN_V0_1_1.txt)
- [保留的0.1.0交付记录](docs/BUILD_V0_1_REPORT.md)

构建提交：`ce903fde0ed48703d9817a41aaea497fb85cbd7c`。
ZIP：313,512字节。SHA256：`61cb7a3b6196cf3dd3af5c292760da2184bb8980fefa3c9311836584f2ccf254`。

## 0.1.1重点优化

- 放宽旧版过严的窗口筛选：可见附属窗口、工具样式窗口和无标题窗口可以被选择。
- 新增“3秒选游戏”：点击按钮后，在3秒内点击原游戏窗口；结束只选中，不自动增强。
- 刷新按窗口句柄和PID保留选择；不默认选择第一项资源管理器。
- 新增“已选择”信息和用户主动触发的“复制窗口信息”。
- 最小化窗口明确标记，要求用户手动恢复，不自动操作游戏。
- 默认独立预览；源窗口失效与GPU/捕获失败分阶段提示。

**用户截图只证明列表缺少游戏；不能仅凭截图断定其实际Owner或样式。此次修复过严筛选并增加前台选取，实际《吞食天地》兼容性仍需用户反馈。**

## 第一次使用

关闭旧增强器，保持游戏窗口正常运行。将新ZIP完整解压到独立目录，运行`Game-HD2D-Enhancer.exe`，保留`shaders`和`licenses`；不要把文件覆盖到游戏目录。

推荐流程：**3秒选游戏 → 在3秒内点击游戏 → 回到增强器确认名称 → 戴上眼镜 / 开启**。默认预览模式，继续操作原游戏窗口。

`Ctrl+Alt+H`直接增强当前前台游戏或停止；`Ctrl+Alt+End`紧急停止/取消选取。也可先用“打开测试画面”验证。

遇到问题保留完整错误，按需点“复制窗口信息”粘贴到聊天。复制只含所选窗口元数据，不含游戏帧、进程内存或磁盘文档；发送前可检查窗口标题是否包含个人信息。未签名测试版请核对来源与哈希，不关闭安全保护。

## 已接入能力与边界

Windows Graphics Capture指定窗口捕获、客户区裁切、D3D11纹理处理与独立显示；NIS v1.0.3（默认锐化20%）、整数/最近邻、双线性；保持比例和黑边；30/60 FPS呈现上限，不插帧。

全屏与独立预览均保留。**全屏鼠标映射仍是实验功能**，普通Win32消息转发不保证兼容Raw Input、DirectInput、GetCursorPos、特殊光标、触控手势等。全屏操作异常时紧急停止，改用预览并操作原游戏。手柄、真实游戏、掌机帧率、帧同步、延迟、功耗和续航仍需单独验收。

目标输出小于源图时，NIS回退到双线性缩小；整数模式可能使用非整数最近邻缩小，状态栏会提示。超过2倍的NIS放大使用最近邻整数预放大后再执行NIS。

FSR1、SMAA、CRT/LCD、Anime4K/CuNNy、HDR、注入、深度效果、插帧和BFI未接入。**DLSS5不纳入。** 不以占位菜单假装实现。

运行优先Windows 11 x64与Direct3D11 feature level 11_0 GPU；最低运行目标Windows 10 2004，SDR。运行不需要Python、Visual Studio或网络。

## 不改变游戏本体

不修改游戏EXE/DLL/资源/存档，不注入游戏，不结束游戏进程，也不要求通过增强器启动游戏。

捕获帧只在有界RAM/VRAM中实时处理，不录屏、不自动截图、不保存渲染帧/视频缓存、不上传画面。渲染路径不读回CPU；WARP自检用自行生成的测试像素读回验证，不使用玩家画面。系统或驱动缓存行为不等于应用主动录屏。

源游戏自身失焦暂停或特殊输入行为仍需实测，不承诺所有应用绝无影响。

## 源码和构建

`app/`保存本项目原生程序，`CMakeLists.txt`是MSVC构建入口；`third_party/`、`references/`、`provenance/`继续保存18个上游项目的源码、版本、许可和校验记录。

开发命令：`cmake -S Game-HD2D-Enhancer -B build -A x64`，`cmake --build build --config Release`，`ctest --test-dir build -C Release -V`。

新增Windows回归脚本：`tools/test_window_selection.ps1`。它只操作自行启动的模拟窗口进程，不接触用户游戏。构建/修改在聊天内完成，使用普通MSVC/CMake/GitHub Actions，不调用Codex、Work、Codex CLI或AI编程代理。

发布包不带约1.56GB的参考资料库，只带当前用到的程序与NIS。NIS原始许可保留在`licenses/NVIDIAImageScaling.txt`和shader头部。

[源码收集报告](SOURCE_COLLECTION_STATUS.md)描述的是资料收集范围，不等于产品全部功能完成，也不等于所有SDK或二进制依赖齐备。

## 许可证

第三方文件保持原始许可，不将整个第三方资料库重授MIT。GPL项目及混合shader的实际集成、修改和分发条件需分别检查，详见[LICENSE_POLICY.md](docs/LICENSE_POLICY.md)。
