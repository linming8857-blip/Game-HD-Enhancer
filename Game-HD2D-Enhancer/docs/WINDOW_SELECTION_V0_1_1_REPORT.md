# Game-HD2D-Enhancer 0.1.1 alpha · build 9

## 本次针对的问题

用户截图显示《吞食天地 Online》正在运行，但增强器下拉列表只有资源管理器与物品管理工具。旧版源码对 GW_OWNER、WS_EX_TOOLWINDOW、空标题等条件直接过滤；这会漏掉符合这些条件的实际游戏窗口。

截图不能证明用户具体游戏窗口拥有哪一种样式，因此本次修复的是已确认存在的过严筛选逻辑，并增加独立前台选取路径；不声称已远程检测或实测了用户的游戏窗口。

## 修复内容

1. 允许可见附属窗口、工具样式窗口和无标题窗口进入候选列表；仍排除本程序、隐藏窗口、桌面壳等。
2. 最小化的候选窗口显示明确标记，不自动恢复或改变游戏窗口。
3. 刷新按 HWND + PID 保留选择；目标关闭后清空选择，不偷偷改选第一项。
4. 新增“3秒选游戏”：点击后切到游戏，倒计时结束仅选择，不自动捕获；可停止取消。
5. 前台选择取 GA_ROOT，而不是跳到可能不可见的 GA_ROOTOWNER。
6. 新增明确的已选择提示与“复制窗口信息”按钮。复制是用户主动操作，仅包含所选窗口元数据，不包含帧、磁盘文档或游戏内存。
7. 默认独立预览，保留全屏实验模式；未选择目标时禁用开启按钮。
8. 错误按窗口检查、图形设备、捕获、显示等阶段提示；源窗口失效不再统一建议改 HDR。

## 实际交付

- 构建提交：`ce903fde0ed48703d9817a41aaea497fb85cbd7c`
- Windows 构建/回归任务：https://github.com/linming8857-blip/Game-HD-Enhancer/actions/runs/38015378224
- 发布版本：`hd2d-v0.1.1-alpha-9`
- 发布页面：https://github.com/linming8857-blip/Game-HD-Enhancer/releases/tag/hd2d-v0.1.1-alpha-9
- 便携包：`Game-HD2D-Enhancer-0.1.1-alpha-win64.zip`
- ZIP大小：313,512 字节。
- ZIP SHA256：`61cb7a3b6196cf3dd3af5c292760da2184bb8980fefa3c9311836584f2ccf254`
- 主程序大小：371,712 字节。
- 主程序 SHA256：`d1f14a352266e4cc9bbd19a5cf38a66f6cec89bf2d18bc9ffadc36ef6e59a836`

已取得真实构建产物，核对 ZIP 与 GitHub Release 的相同哈希、Windows x64 PE 标识及包内全部9条文件校验记录。所有非目录文件由这9条记录和SHA256SUMS.txt自身组成，没有未列入清单的附带文件。

开发方式仍是聊天内代码修改、GitHub提交、普通MSVC/CMake/CTest构建。没有调用Codex、Work或AI编程代理。原始第三方源码快照未修改。DLSS5未加入。

## 构建与测试结果

MSVC Release x64 编译通过。原有几何/图像测试套件通过，涵盖19项PASS检查，包括NIS真实shader的D3D11 WARP运行；WARP是软件渲染正确性检查，不能当作掌机性能测试。

新窗口回归使用本程序独立启动的模拟窗口，不操作用户游戏。测试最初发现模拟窗口缺少WS_POPUP样式，不能证明GA_ROOTOWNER区别；已修正测试夹具并保留严格断言，不把失败当成功。

最终回归输出：

```text
PASS: fixture reproduces visible legacy-style popup with invisible owner; old owner filter would reject it.
PASS: no automatic first-window selection, Start initially disabled, preview default.
PASS: owned, tool-style and untitled fixtures all appear in the real window list.
PASS: refresh preserves the explicitly selected window by identity.
PASS: minimized candidate is retained and labelled; user window not automatically restored.
PASS: three-second picker can be cancelled without starting capture.
PASS: timed foreground picker selects the visible owned window, not its invisible owner.
PASS: owned-window WGC/NIS preview cycle 1 reached running state.
PASS: owned-window WGC/NIS preview cycle 2 reached running state.
PASS: owned-window WGC/NIS preview cycle 3 reached running state.
PASS: three capture/stop cycles release output while independent owned source stays alive.
PASS: a closed source clears selection instead of selecting another program.
PASS: enhancer exits cleanly; independent source processes continue.
```

这些检查验证了托管Windows上的窗口选择、捕获运行状态和退出隔离，不代表像素级画面质量评分、所有老游戏兼容、全屏输入、掌机功耗或性能已经验证。用户真实《吞食天地 Online》仍需下一轮反馈。

## 用户操作

关闭旧增强器，游戏可以保留。新包完整解压到独立目录，保留shaders和licenses。运行主程序，点击“3秒选游戏”，随后在3秒内点击游戏窗口；倒计时结束回到增强器，确认已选择游戏，再开启默认的独立预览。

Ctrl+Alt+End紧急停止；有问题保留完整错误并按需复制所选窗口信息。不要将任何文件覆盖到游戏目录，也不要关闭系统安全保护。

## 保持不变的范围

NIS、整数/最近邻、双线性继续使用；本次不宣称帧率提升。没有游戏DLL注入、录像、自动截图、保存渲染画面、画面上传、DLSS5或插帧。全屏鼠标消息映射仍是实验功能。
