# Game-HD2D-Enhancer 0.1.0 alpha · build 3 交付记录

## 交付物

已实际生成 Windows x64 EXE，不是只提交方案、源码链接或空壳界面。

- 发布版本：`hd2d-v0.1.0-alpha-3`
- 构建源码提交：`c74b6c855fab0a8d8d6e109ff2172f1ff23305f2`
- [正式保留的测试版发布页](https://github.com/linming8857-blip/Game-HD-Enhancer/releases/tag/hd2d-v0.1.0-alpha-3)
- [便携 ZIP](https://github.com/linming8857-blip/Game-HD-Enhancer/releases/download/hd2d-v0.1.0-alpha-3/Game-HD2D-Enhancer-0.1.0-alpha-win64.zip)
- ZIP 体积：303,779 字节。
- ZIP SHA256：`ecd397765384911428ec4adc464dd0a8ad1f8056e6c938b015108fae5c8ebf5e`
- `Game-HD2D-Enhancer.exe`：351,232 字节；已从实际发布构建包读取并核对 PE x64 标识 `0x8664`。
- `HD2D-Tests.exe`：196,608 字节，是自检工具，不是主程序。

已下载构建产物，抽出与 Release 完全相同的便携 ZIP，核对 ZIP 哈希以及包内 SHA256SUMS 的全部文件记录。没有运行 Windows EXE 的 Linux 本地环境不冒称运行过它；实际运行测试在下述 Windows 任务中完成。

便携包包括主程序、自检工具、NIS shader、NVIDIA MIT 授权、中文使用说明、自动测试结果、构建信息及 SHA256SUMS。完整解压使用，不能只拿走 EXE。

## 独立聊天开发路径

程序代码在聊天中编写，使用 GitHub 接口提交；构建任务只执行 CMake、MSVC、CTest、校验和、压缩与发布命令。

没有调用 Codex、Work、Codex CLI、Codex Action 或其他 AI 编程代理；运行时也不调用 AI 服务、不需要联网。当前第三方运行算法为固定版本 NVIDIA NIS；未加入 DLSS5。

## 构建与自检：通过

[Windows 构建任务 38012719328](https://github.com/linming8857-blip/Game-HD-Enhancer/actions/runs/38012719328)

环境：Windows Server 2022；MSVC 19.44.35229.0；Windows SDK 10.0.26100.0；Release x64，静态 C/C++ 运行库。

CMake 配置、MSVC 编译、CTest、打包和 prerelease 发布均成功。只有上游 NISConfig 对齐填充警告，不把它作为编译失败处理。

CTest 1 个测试套件内的 19 项检查均通过：

1. 4:3 比例与黑边布局。
2. 800×600 在 1080p 中不能错误采用 2 倍整数缩放。
3. GBA 240×160 的 6 倍整数布局。
4. 黑边点击拒绝映射。
5. 首末像素鼠标坐标映射。
6. 拖拽释放的边界钳制。
7. 负坐标显示器上的客户区裁切。
8. DPI 缩放后的客户区裁切。
9. 拒绝零尺寸输入。
10. 拒绝超大尺寸输入。
11. NIS 大倍率时的最近邻预处理倍率。
12. 普通倍率 NIS 无额外预放大。
13. 25 组比例边界组合。
14. 最近邻 shader 的 2 倍像素逐项精确一致。
15. GPU 裁切颜色正确。
16. 类似 WGC 输入的 BGRA→RGBA 通道转换。
17. 双线性纯色场与非整齐尺寸边缘。
18. 真正 NIS shader 的 1 倍、非整齐尺寸及大倍率预处理后的输出。
19. 30 次 GPU 输出资源尺寸变更与复用。

GPU 正确性自检使用 D3D11 WARP 软件渲染，并不是用户掌机显卡的性能测试。

## 打包后界面与实时预览：通过限定场景检查

[界面/捕获检查任务 38013303755](https://github.com/linming8857-blip/Game-HD-Enhancer/actions/runs/38013303755)

使用的就是上述 Release ZIP，并先验证了相同的 SHA256。测试过程只操作由测试脚本新启动的本项目主程序和独立测试画面，不操作用户真实游戏。

已确认：

- 独立测试画面进程正常启动并创建窗口。
- 增强器主窗口正常启动，10 个必要控件存在。
- 实际窗口列表可以选到独立测试画面。
- 实际开启 NIS 预览后，WGC→纹理处理→预览呈现进入运行状态。
- 停止增强后，输出窗口被销毁，源测试画面进程仍存在。
- 退出整个增强器后，源测试画面进程仍存在。

原始输出完整保留于 [UI_SMOKE_ALPHA3.txt](UI_SMOKE_ALPHA3.txt)。它记录的运行机观察值为 320×240→908×681、2 FPS；这是托管 Windows 测试机的场景，不应推断为 ROG Xbox Ally X 的实际帧率。

此项检查以真实控件、呈现状态和进程状态验证流程，没有逐帧视觉评分；也没有验证全屏鼠标转发。

## 用户怎么开始测试

先打开主程序 `Game-HD2D-Enhancer.exe`，点击“打开测试画面”。切到测试画面按 `Ctrl+Alt+H` 开启；按 `Ctrl+Alt+End` 停止。检查显示、点击、按键及停止后的源窗口状态，再测试真实游戏。

《吞食天地 Online》先用窗口模式。无需登记 EXE、不需要从增强器启动游戏；可刷新并选择当前窗口。NIS 默认锐化 20%，默认呈现上限 60 FPS。源帧不足不会凭空插帧。

目标是 Windows 11 x64、Direct3D11 feature level 11_0。当前测试版为 SDR；HDR/Auto HDR、独占全屏、受保护捕获不是本版承诺的支持项。未签名程序需核对来源，不建议关闭任何安全保护。

## 仍未验收的内容

**仍不是完成全部要求的正式版本。**

真实《吞食天地 Online》、mGBA、天马G调用的模拟器、《仙剑》《天之痕》，以及 ROG Xbox Ally X 实机都尚未验证。

全屏模式的鼠标映射通过普通 Win32 鼠标消息实现，属于实验功能。Raw Input、DirectInput、GetCursorPos 读取、特殊光标、复杂拖拽、鼠标侧键和触控手势不保证兼容；手柄仍由原游戏接收，尚未实测。若操作不准确，用紧急停止快捷键退出，改用独立预览并操作原游戏窗口。

输入延迟、帧同步、全屏安全退出、100 次捕获启停资源稳定性、多显示器、全部 DPI 情况、掌机功耗和续航还需要单独验收。现有数学边界测试不能代替真实设备输入验证。

FSR1、SMAA、CRT/LCD、Anime4K/CuNNy 尚未接入；没有提供占位菜单假装实现。没有 DLSS5、游戏注入、3D 深度效果或插帧。

## 运行时不落盘与源进程隔离

主程序不写游戏文件、不修改存档、不终止源游戏进程；不对游戏注入 DLL。

实时帧只在帧池和 D3D11 纹理中流转。程序没有录屏、帧保存、自动截图、视频缓存或上传功能。自检在内存中读回的是自行生成的测试像素，不是玩家画面。系统或显卡驱动的缓存行为不等同于应用主动录屏。

只在开发资料库里保存源码和开发测试日志，与实时游戏画面不保存的要求不冲突。
