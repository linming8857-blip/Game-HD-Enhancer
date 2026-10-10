#include "gpu.hpp"
#include "window_sources.hpp"
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <utility>

using namespace hd2d;
namespace capture = winrt::Windows::Graphics::Capture;
namespace graphics = winrt::Windows::Graphics;
namespace directx = winrt::Windows::Graphics::DirectX;
namespace interop = ::Windows::Graphics::DirectX::Direct3D11;
using Clock = std::chrono::steady_clock;
constexpr UINT FrameMessage = WM_APP + 11, ClosedMessage = WM_APP + 12;
constexpr int SourceBox = 101, RefreshButton = 102, AlgorithmBox = 103,
    DisplayBox = 104, SharpSlider = 105, StartButton = 106, StopButton = 107,
    DemoButton = 108, FpsBox = 109, PickButton = 110, InfoButton = 111;
constexpr int ToggleHotkey = 1, StopHotkey = 2;
struct Signal {
    std::atomic<HWND> window{};
    std::atomic<bool> pending{};
    std::uintptr_t generation{};
};
static Rect client_screen(HWND h) {
    RECT r{}; if (!GetClientRect(h, &r)) winrt::throw_last_error();
    POINT p{r.left, r.top}; if (!ClientToScreen(h, &p)) winrt::throw_last_error();
    return {p.x, p.y, r.right - r.left, r.bottom - r.top};
}
static std::wstring error_text() {
    try { throw; }
    catch (winrt::hresult_error const& e) {
        std::wostringstream s; s << e.message().c_str() << L"\nHRESULT: 0x" << std::hex << unsigned(e.code()); return s.str();
    } catch (std::exception const& e) { return winrt::to_hstring(e.what()).c_str(); }
    catch (...) { return L"未知错误"; }
}
class App {
public:
    HWND main{}, output{}, sourceBox{}, algorithmBox{}, displayBox{}, sharpSlider{}, fpsBox{}, status{};
    HFONT font{}; std::vector<Source> sources; Source selected{};
    std::unique_ptr<Gpu> gpu;
    capture::GraphicsCaptureItem item{nullptr};
    capture::Direct3D11CaptureFramePool pool{nullptr};
    capture::GraphicsCaptureSession session{nullptr};
    directx::Direct3D11::IDirect3DDevice winDevice{nullptr};
    winrt::event_token frameToken{}, closedToken{};
    std::shared_ptr<Signal> signal;
    graphics::SizeInt32 poolSize{};
    Rect content{}; Size sourceClient{};
    Algorithm algorithm{Algorithm::NIS};
    bool running{}, fullscreen{}, hotkeysOK{}, inputFailed{}, picking{};
    unsigned mouseButtons{}; HWND dragTarget{}; POINT lastMouse{};
    std::uintptr_t generation{};
    int frameLimit{60}, frames{}, totalFrames{}, pickSeconds{-1};
    float sharpness{0.2f};
    Clock::time_point started{}, statsAt{}, nextFrame{}, pickDeadline{};
    std::wstring stage{L"初始化"};
    int dpi{96};
    ~App() { stop(false); if (font) DeleteObject(font); }
    void text(const std::wstring& s) { if (IsWindow(status)) SetWindowTextW(status, s.c_str()); }
    void fail() {
        const auto message = error_text();
        const auto context = stage + (selected.hwnd ? L"\n目标：" + selected.title : L"");
        const bool sourceError = stage == L"检查源窗口" || stage == L"选取前台窗口";
        stop(false);
        text(context + L"\n" + message);
        const auto help = sourceError
            ? L"\n\n请先恢复并点击游戏，再用“3秒选游戏”选取，或刷新列表。无需更改 HDR 或显卡设置。"
            : L"\n\n请保留错误信息。先使用窗口化游戏和独立预览模式；不要关闭系统安全保护。";
        MessageBoxW(main, (L"增强未能继续，本程序没有关闭原游戏。\n\n" + context + L"\n" + message + help).c_str(),
            L"Game-HD2D-Enhancer · 测试版", MB_OK | MB_ICONWARNING);
    }
    static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        App* app = reinterpret_cast<App*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (m == WM_NCCREATE) {
            app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            app->main = h; SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }
        if (!app) return DefWindowProcW(h, m, w, l);
        try { return app->message(h, m, w, l); }
        catch (...) { app->fail(); return 0; }
    }
    HWND control(const wchar_t* cls, const wchar_t* caption, DWORD style, int id) {
        HWND h = CreateWindowExW(0, cls, caption, WS_CHILD | WS_VISIBLE | style,
            0, 0, 10, 10, main, reinterpret_cast<HMENU>(INT_PTR(id)), GetModuleHandleW(nullptr), nullptr);
        if (!h) winrt::throw_last_error();
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE); return h;
    }
    void layout() {
        dpi = int(GetDpiForWindow(main));
        if (font) DeleteObject(font);
        font = CreateFontW(-MulDiv(10, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
        auto move = [&](int id, int x, int y, int width, int height) {
            HWND child = GetDlgItem(main, id);
            MoveWindow(child, MulDiv(x,dpi,96), MulDiv(y,dpi,96), MulDiv(width,dpi,96), MulDiv(height,dpi,96), TRUE);
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        };
        move(201,20,16,610,30); move(202,20,52,610,24);
        move(203,20,88,80,24); move(SourceBox,100,84,430,260); move(RefreshButton,540,83,90,30);
        move(PickButton,100,123,145,32); move(InfoButton,260,123,150,32);
        move(210,20,166,610,40);
        move(204,20,221,100,24); move(AlgorithmBox,120,216,510,180);
        move(205,20,261,100,24); move(DisplayBox,120,256,510,150);
        move(206,20,301,100,24); move(SharpSlider,120,296,285,30);
        move(207,435,301,75,24); move(FpsBox,515,296,115,150);
        move(StartButton,20,342,190,40); move(StopButton,225,342,190,40); move(DemoButton,430,342,200,40);
        move(208,20,396,610,52); move(209,20,458,610,82);
        SendMessageW(sourceBox, CB_SETDROPPEDWIDTH, MulDiv(590,dpi,96), 0);
    }
    void initialize() {
        control(L"STATIC", L"Game-HD2D-Enhancer 0.1.1 alpha · 窗口识别修正版", 0, 201);
        control(L"STATIC", L"实时高清眼镜  ·  不改游戏文件  ·  不录像  ·  无需游戏库", 0, 202);
        control(L"STATIC", L"游戏窗口", 0, 203);
        sourceBox = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, SourceBox);
        control(L"BUTTON", L"刷新窗口", WS_TABSTOP, RefreshButton);
        control(L"BUTTON", L"3秒选游戏", WS_TABSTOP, PickButton);
        control(L"BUTTON", L"复制窗口信息", WS_TABSTOP, InfoButton);
        control(L"STATIC", L"尚未选择游戏。点击“3秒选游戏”后，在3秒内点击原游戏窗口。", 0, 210);
        control(L"STATIC", L"画面算法", 0, 204);
        algorithmBox = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, AlgorithmBox);
        for (auto s : {L"NIS 轻量高清（含锐化）", L"原味像素：整数倍 / 最近邻", L"双线性（兼容 / 对照）"})
            SendMessageW(algorithmBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));
        SendMessageW(algorithmBox, CB_SETCURSEL, 0, 0);
        control(L"STATIC", L"显示方式", 0, 205);
        displayBox = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, DisplayBox);
        SendMessageW(displayBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"等比例全屏（鼠标坐标映射：实验）"));
        SendMessageW(displayBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"独立预览窗口（仍操作原游戏窗口）"));
        SendMessageW(displayBox, CB_SETCURSEL, 1, 0);
        control(L"STATIC", L"NIS 锐化", 0, 206);
        sharpSlider = control(TRACKBAR_CLASSW, L"", TBS_AUTOTICKS | WS_TABSTOP, SharpSlider);
        SendMessageW(sharpSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100)); SendMessageW(sharpSlider, TBM_SETPOS, TRUE, 20);
        control(L"STATIC", L"帧率上限", 0, 207);
        fpsBox = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, FpsBox);
        SendMessageW(fpsBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"30 FPS"));
        SendMessageW(fpsBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"60 FPS")); SendMessageW(fpsBox, CB_SETCURSEL, 1, 0);
        control(L"BUTTON", L"戴上眼镜 / 开启", WS_TABSTOP | BS_DEFPUSHBUTTON, StartButton);
        control(L"BUTTON", L"摘掉眼镜 / 停止", WS_TABSTOP, StopButton);
        control(L"BUTTON", L"打开测试画面", WS_TABSTOP, DemoButton);
        control(L"STATIC", L"Ctrl + Alt + H：直接增强当前前台游戏 / 停止\nCtrl + Alt + End：紧急停止。首次默认预览；全屏鼠标仍为实验功能。", 0, 208);
        status = control(L"STATIC", L"就绪。先选择游戏；不要选择解压软件的文件资源管理器。", 0, 209);
        layout(); refresh();
        const bool toggle = RegisterHotKey(main, ToggleHotkey, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'H') != 0;
        const bool escape = RegisterHotKey(main, StopHotkey, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_END) != 0;
        hotkeysOK = toggle && escape;
        if (!hotkeysOK) text(L"快捷键被占用，暂不允许全屏。可使用“3秒选游戏”与独立预览，通过停止按钮退出。");
        EnableWindow(GetDlgItem(main, StopButton), FALSE);
        SetTimer(main, 1, 100, nullptr);
    }
    std::optional<Source> chosen_source() const {
        auto row = SendMessageW(sourceBox, CB_GETCURSEL, 0, 0);
        if (row == CB_ERR) return std::nullopt;
        auto index = SendMessageW(sourceBox, CB_GETITEMDATA, WPARAM(row), 0);
        if (index < 0 || static_cast<std::size_t>(index) >= sources.size()) return std::nullopt;
        return sources[static_cast<std::size_t>(index)];
    }
    void selection_changed() {
        const auto choice = chosen_source();
        const auto label = choice ? L"已选择：" + source_label(*choice)
            : L"尚未选择游戏。点击“3秒选游戏”后，在3秒内点击原游戏窗口。";
        SetWindowTextW(GetDlgItem(main,210), label.c_str());
        EnableWindow(GetDlgItem(main, StartButton), !running && !picking && choice.has_value());
        EnableWindow(GetDlgItem(main, InfoButton), !running && !picking && choice.has_value());
    }
    void refresh(std::optional<Source> preferred = std::nullopt) {
        if (running || picking) return;
        if (!preferred) preferred = chosen_source();
        // Preserve HWND+PID, never an old list index, and never silently choose
        // Explorer (or another unrelated window) just because it is first.
        auto found = enumerate_sources();
        if (preferred) {
            if (auto current = inspect_source(preferred->hwnd); current && current->pid == preferred->pid) {
                const bool included = std::any_of(found.begin(), found.end(), [&](auto const& s) {
                    return s.hwnd == current->hwnd && s.pid == current->pid;
                });
                if (!included) found.push_back(*current);
            }
        }
        sources = std::move(found);
        SendMessageW(sourceBox, CB_RESETCONTENT, 0, 0);
        LRESULT selectedRow = CB_ERR;
        for (std::size_t i = 0; i < sources.size(); ++i) {
            const auto label = source_label(sources[i]);
            const auto row = SendMessageW(sourceBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            if (row == CB_ERR || row == CB_ERRSPACE) throw std::runtime_error("Unable to populate window list");
            SendMessageW(sourceBox, CB_SETITEMDATA, WPARAM(row), LPARAM(i));
            if (preferred && sources[i].hwnd == preferred->hwnd && sources[i].pid == preferred->pid) selectedRow = row;
        }
        SendMessageW(sourceBox, CB_SETCURSEL, WPARAM(selectedRow), 0);
        selection_changed();
        text(L"找到 " + std::to_wstring(sources.size()) + L" 个候选窗口（包括附属/工具样式窗口）。\n请选择实际游戏，或使用“3秒选游戏”；最小化的窗口请先手动恢复。");
    }
    void cancel_pick() noexcept {
        picking = false; pickSeconds = -1;
        if (IsWindow(main)) {
            KillTimer(main, 2);
            SetWindowTextW(GetDlgItem(main,PickButton), L"3秒选游戏");
            EnableWindow(GetDlgItem(main,PickButton), !running);
        }
    }
    void begin_pick() {
        if (running) return;
        picking = true; pickSeconds = -1; pickDeadline = Clock::now() + std::chrono::seconds(3);
        for (int id : {SourceBox,RefreshButton,PickButton,InfoButton,StartButton}) EnableWindow(GetDlgItem(main,id), FALSE);
        EnableWindow(GetDlgItem(main,StopButton), TRUE);
        if (!SetTimer(main,2,100,nullptr)) { cancel_pick(); winrt::throw_last_error(); }
        text(L"请在3秒内点击原游戏窗口，让游戏处于前台。\n倒计时结束后仅选中窗口，不会自动开启增强。可按紧急停止取消。");
    }
    void pick_tick() {
        if (!picking) return;
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(pickDeadline - Clock::now()).count();
        if (left > 0) {
            const int seconds = int((left + 999) / 1000);
            if (seconds != pickSeconds) {
                pickSeconds = seconds;
                SetWindowTextW(GetDlgItem(main,PickButton), (L"剩余 " + std::to_wstring(seconds) + L" 秒…").c_str());
            }
            return;
        }
        const HWND target = source_root(GetForegroundWindow());
        cancel_pick();
        for (int id : {SourceBox,RefreshButton,PickButton}) EnableWindow(GetDlgItem(main,id), TRUE);
        EnableWindow(GetDlgItem(main,StopButton), FALSE);
        stage = L"选取前台窗口";
        if (auto source = inspect_source(target)) {
            refresh(*source);
            text(L"已选中：" + source->title + L"\n回到本程序，确认窗口名称后点击“戴上眼镜 / 开启”。");
        } else {
            selection_changed();
            text(L"未选中游戏：倒计时结束时前台不是可用的外部窗口。\n请再点“3秒选游戏”，随后点击游戏画面或标题栏；无需最小化游戏。");
        }
    }
    void copy_info() {
        if (running || picking) return;
        auto choice = chosen_source(); if (!choice) { text(L"请先选择游戏窗口。"); return; }
        stage = L"复制窗口信息";
        const auto data = source_diagnostics(*choice);
        const auto bytes = (data.size()+1) * sizeof(wchar_t);
        auto memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!memory) winrt::throw_last_error();
        void* pointer = GlobalLock(memory);
        if (!pointer) { GlobalFree(memory); winrt::throw_last_error(); }
        std::memcpy(pointer, data.c_str(), bytes); GlobalUnlock(memory);
        if (!OpenClipboard(main)) { GlobalFree(memory); winrt::throw_last_error(); }
        const bool ok = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT,memory);
        CloseClipboard();
        if (!ok) { GlobalFree(memory); throw std::runtime_error("Unable to write clipboard"); }
        text(L"已复制选中窗口的信息。需要排查时可粘贴到聊天。\n只包含窗口标题、PID、窗口类型等元数据，不包含游戏画面或文件内容。");
    }
    void start(Source target) {
        stop(false); stage = L"检查源窗口"; selected = target;
        DWORD pid{}; GetWindowThreadProcessId(target.hwnd, &pid);
        if (!IsWindow(target.hwnd) || pid != target.pid)
            throw std::runtime_error("选中窗口已关闭或已被替换。请刷新并重新选择游戏。");
        if (pid == GetCurrentProcessId())
            throw std::runtime_error("不能捕获增强器自身。请先点击原游戏窗口。");
        if (IsIconic(target.hwnd))
            throw std::runtime_error("选中窗口已最小化。请在任务栏手动恢复原游戏，再开启增强。");
        if (!inspect_source(target.hwnd))
            throw std::runtime_error("选中窗口当前不可见或不是可用的游戏窗口。请重新选取。");
        if (!capture::GraphicsCaptureSession::IsSupported())
            throw std::runtime_error("当前系统不支持 Windows Graphics Capture。建议使用 Windows 11。");
        fullscreen = SendMessageW(displayBox, CB_GETCURSEL, 0, 0) == 0;
        if (fullscreen && !hotkeysOK) throw std::runtime_error("紧急停止快捷键不可用，请使用独立预览模式。");
        algorithm = Algorithm(int(SendMessageW(algorithmBox, CB_GETCURSEL, 0, 0)));
        sharpness = float(SendMessageW(sharpSlider, TBM_GETPOS, 0, 0)) / 100.f;
        frameLimit = SendMessageW(fpsBox, CB_GETCURSEL, 0, 0) == 0 ? 30 : 60;
        auto client = client_screen(selected.hwnd); sourceClient = {client.w, client.h}; validate(sourceClient);
        stage = L"创建图形设备 / 编译着色器";
        gpu = std::make_unique<Gpu>();
        ComPtr<IDXGIDevice> dxgi; winrt::check_hresult(gpu->device()->QueryInterface(IID_PPV_ARGS(&dxgi)));
        winrt::com_ptr<IInspectable> native;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.Get(), native.put()));
        winDevice = native.as<directx::Direct3D11::IDirect3DDevice>();
        stage = L"创建窗口捕获";
        auto factory = winrt::get_activation_factory<capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
        winrt::check_hresult(factory->CreateForWindow(selected.hwnd, winrt::guid_of<capture::GraphicsCaptureItem>(), winrt::put_abi(item)));
        poolSize = item.Size(); validate({poolSize.Width, poolSize.Height});
        pool = capture::Direct3D11CaptureFramePool::CreateFreeThreaded(winDevice,
            directx::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, poolSize);
        session = pool.CreateCaptureSession(item);
        session.IsCursorCaptureEnabled(false);
        signal = std::make_shared<Signal>(); signal->window.store(main); signal->generation = ++generation;
        auto s = signal;
        frameToken = pool.FrameArrived([s](auto const&, auto const&) {
            auto h = s->window.load();
            if (h && !s->pending.exchange(true))
                if (!PostMessageW(h, FrameMessage, WPARAM(s->generation), 0)) s->pending.store(false);
        });
        closedToken = item.Closed([s](auto const&, auto const&) {
            if (auto h = s->window.load()) PostMessageW(h, ClosedMessage, WPARAM(s->generation), 0);
        });
        stage = L"创建增强显示窗口";
        MONITORINFO monitor{sizeof(monitor)};
        if (!GetMonitorInfoW(MonitorFromWindow(selected.hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) winrt::throw_last_error();
        RECT bounds = monitor.rcMonitor;
        DWORD style = fullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW;
        if (!fullscreen) bounds = {monitor.rcWork.left + 30, monitor.rcWork.top + 30, monitor.rcWork.left + 990, monitor.rcWork.top + 750};
        output = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | (fullscreen ? WS_EX_TOPMOST : 0),
            L"HD2DOutput", L"HD2D 预览 · 关闭此窗口不会关闭游戏", style,
            bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
            nullptr, nullptr, GetModuleHandleW(nullptr), this);
        if (!output) winrt::throw_last_error();
        // This only applies to our own output, never to the user's game.
        SetWindowDisplayAffinity(output, WDA_EXCLUDEFROMCAPTURE);
        RECT area{}; GetClientRect(output, &area);
        gpu->attach(output, {area.right, area.bottom});
        SetForegroundWindow(selected.hwnd);
        if (fullscreen && GetForegroundWindow() != selected.hwnd)
            throw std::runtime_error("Cannot keep the game in the foreground. Activate it and use Ctrl+Alt+H.");
        frames = totalFrames = 0; inputFailed = false; mouseButtons = 0; dragTarget = nullptr;
        started = statsAt = nextFrame = Clock::now(); running = true;
        stage = L"启动窗口捕获";
        // Output stays hidden until an actual frame has been captured and processed.
        session.StartCapture();
        EnableWindow(GetDlgItem(main, StartButton), FALSE); EnableWindow(GetDlgItem(main, StopButton), TRUE);
        for (int id : {SourceBox, RefreshButton, AlgorithmBox, DisplayBox, SharpSlider, FpsBox, PickButton, InfoButton}) EnableWindow(GetDlgItem(main, id), FALSE);
        text(L"正在等待第一帧：" + selected.title + L"\nGPU：" + gpu->adapter_name());
    }
    void release_mouse() noexcept {
        if (dragTarget && IsWindow(dragTarget)) {
            LPARAM p = MAKELPARAM(lastMouse.x, lastMouse.y);
            if (mouseButtons & 1) PostMessageW(dragTarget, WM_LBUTTONUP, 0, p);
            if (mouseButtons & 2) PostMessageW(dragTarget, WM_RBUTTONUP, 0, p);
            if (mouseButtons & 4) PostMessageW(dragTarget, WM_MBUTTONUP, 0, p);
        }
        mouseButtons = 0; dragTarget = nullptr;
        if (GetCapture() == output) ReleaseCapture();
    }
    void stop(bool notify) noexcept {
        running = false; ++generation; cancel_pick();
        if (signal) signal->window.store(nullptr);
        release_mouse();
        try { if (pool) pool.FrameArrived(frameToken); } catch (...) {}
        try { if (item) item.Closed(closedToken); } catch (...) {}
        try { if (session) session.Close(); } catch (...) {}
        try { if (pool) pool.Close(); } catch (...) {}
        session = nullptr; pool = nullptr; item = nullptr; winDevice = nullptr; signal.reset();
        gpu.reset();
        if (auto h = std::exchange(output, nullptr)) DestroyWindow(h);
        if (IsWindow(main)) {
            EnableWindow(GetDlgItem(main, StopButton), FALSE);
            for (int id : {SourceBox, RefreshButton, AlgorithmBox, DisplayBox, SharpSlider, FpsBox, PickButton, InfoButton}) EnableWindow(GetDlgItem(main, id), TRUE);
            try { selection_changed(); } catch (...) {}
            if (notify) text(L"已摘掉眼镜。捕获和显示资源已释放，原游戏继续独立运行。");
        }
    }
    void on_frame() {
        if (!running || !pool) return;
        stage = L"实时捕获 / 增强 / 呈现";
        signal->pending.store(false);
        capture::Direct3D11CaptureFrame latest{nullptr};
        for (int i = 0; i < 4; ++i) {
            auto frame = pool.TryGetNextFrame(); if (!frame) break;
            if (latest) latest.Close(); latest = frame;
        }
        if (!latest) return;
        auto size = latest.ContentSize();
        if (size.Width <= 0 || size.Height <= 0) { latest.Close(); return; }
        if (size.Width != poolSize.Width || size.Height != poolSize.Height) {
            latest.Close(); validate({size.Width, size.Height}); poolSize = size;
            pool.Recreate(winDevice, directx::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size); return;
        }
        const auto now = Clock::now();
        if (now + std::chrono::milliseconds(1) < nextFrame || !gpu->ready()) { latest.Close(); return; }
        auto access = latest.Surface().as<interop::IDirect3DDxgiInterfaceAccess>();
        ComPtr<ID3D11Texture2D> texture;
        winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(&texture)));
        D3D11_TEXTURE2D_DESC td{}; texture->GetDesc(&td);
        auto client = client_screen(selected.hwnd); sourceClient = {client.w, client.h};
        RECT extended{};
        if (FAILED(DwmGetWindowAttribute(selected.hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &extended, sizeof(extended))))
            if (!GetWindowRect(selected.hwnd, &extended)) winrt::throw_last_error();
        auto crop = crop_client({extended.left, extended.top, extended.right - extended.left, extended.bottom - extended.top},
            client, {int(td.Width), int(td.Height)});
        RECT out{}; GetClientRect(output, &out);
        if (out.right < 1 || out.bottom < 1) { latest.Close(); return; }
        gpu->resize({out.right, out.bottom});
        content = fit({crop.w, crop.h}, {out.right, out.bottom}, algorithm == Algorithm::Integer);
        auto& enhanced = gpu->enhance(texture.Get(), crop, {content.w, content.h}, algorithm, sharpness);
        latest.Close();
        gpu->present(enhanced, content);
        if (totalFrames == 0) ShowWindow(output, SW_SHOWNOACTIVATE);
        ++frames; ++totalFrames;
        const auto period = std::chrono::microseconds(1000000 / frameLimit);
        nextFrame = std::max(nextFrame + period, now);
        const double elapsed = std::chrono::duration<double>(now - statsAt).count();
        if (elapsed >= 1.0) {
            std::wostringstream line;
            line << L"增强中：" << crop.w << L"×" << crop.h << L" → " << content.w << L"×" << content.h
                 << L"，呈现 " << int(frames / elapsed + .5) << L" FPS（不是游戏帧率/延迟测量）\n";
            if (algorithm == Algorithm::NIS && (content.w < crop.w || content.h < crop.h))
                line << L"目标小于源画面，当前使用双线性缩小。";
            else if (algorithm == Algorithm::Integer && (content.w < crop.w || content.h < crop.h))
                line << L"源画面大于输出区域，当前为最近邻非整数缩小。";
            else line << (fullscreen ? L"鼠标映射为实验功能；不兼容时 Ctrl+Alt+End 停止。" : L"预览模式：请继续操作原游戏窗口。");
            text(line.str()); frames = 0; statsAt = now;
        }
    }
    void tick() {
        if (!running) return;
        DWORD pid{}; GetWindowThreadProcessId(selected.hwnd, &pid);
        if (!IsWindow(selected.hwnd) || pid != selected.pid || IsIconic(selected.hwnd)) {
            stop(false); text(L"源窗口关闭或最小化，增强已安全停止。"); return;
        }
        if (inputFailed) {
            stop(false); text(L"目标窗口拒收鼠标消息，增强已停止。可能存在权限差异，请使用预览模式。"); return;
        }
        if (fullscreen && GetForegroundWindow() != selected.hwnd) {
            stop(false); text(L"前台窗口已切换，已摘掉眼镜。回到游戏按 Ctrl+Alt+H 可重新开启。"); return;
        }
        if (!totalFrames && Clock::now() - started > std::chrono::seconds(8)) {
            stop(false); text(L"8 秒内未收到可显示画面，增强已停止。请尝试窗口模式；不支持的捕获不会自动注入修复。");
        }
    }
    void forward_mouse(UINT message, WPARAM w, LPARAM l) {
        if (!running || !fullscreen || !totalFrames) return;
        POINT screen{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) ScreenToClient(output, &screen);
        const bool release = message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP;
        auto mapped = map_input({screen.x, screen.y}, content, sourceClient, mouseButtons != 0 || release);
        if (!mapped) return;
        POINT p{mapped->x, mapped->y}; HWND receiver = dragTarget;
        if (!receiver || !IsWindow(receiver)) {
            receiver = selected.hwnd;
            for (int i = 0; i < 8; ++i) {
                HWND child = ChildWindowFromPointEx(receiver, p, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED | CWP_SKIPTRANSPARENT);
                if (!child || child == receiver) break;
                MapWindowPoints(receiver, child, &p, 1); receiver = child;
            }
        } else MapWindowPoints(selected.hwnd, receiver, &p, 1);
        if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) { mouseButtons |= 1; dragTarget = receiver; SetCapture(output); }
        if (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK) { mouseButtons |= 2; dragTarget = receiver; SetCapture(output); }
        if (message == WM_MBUTTONDOWN || message == WM_MBUTTONDBLCLK) { mouseButtons |= 4; dragTarget = receiver; SetCapture(output); }
        lastMouse = p;
        if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) ClientToScreen(receiver, &p);
        if (!PostMessageW(receiver, message, w, MAKELPARAM(p.x, p.y))) inputFailed = true;
        if (message == WM_LBUTTONUP) mouseButtons &= ~1u;
        if (message == WM_RBUTTONUP) mouseButtons &= ~2u;
        if (message == WM_MBUTTONUP) mouseButtons &= ~4u;
        if (!mouseButtons) { dragTarget = nullptr; if (GetCapture() == output) ReleaseCapture(); }
    }
    static LRESULT CALLBACK OutputProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        App* a = reinterpret_cast<App*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (m == WM_NCCREATE) { a = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams); SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(a)); }
        if (!a) return DefWindowProcW(h, m, w, l);
        try {
            if (m == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
            if (m == WM_ERASEBKGND) return 1;
            if (m == WM_CLOSE) { a->stop(true); return 0; }
            if (m == WM_PAINT) { PAINTSTRUCT p{}; BeginPaint(h, &p); EndPaint(h, &p); return 0; }
            if (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST && a->fullscreen) {
                if (m != WM_XBUTTONDOWN && m != WM_XBUTTONUP && m != WM_XBUTTONDBLCLK) a->forward_mouse(m,w,l);
                return 0;
            }
        } catch (...) { a->fail(); return 0; }
        return DefWindowProcW(h, m, w, l);
    }
    LRESULT message(HWND h, UINT m, WPARAM w, LPARAM l) {
        if (m == WM_CREATE) { initialize(); return 0; }
        if (m == WM_COMMAND) {
            if (LOWORD(w) == SourceBox && HIWORD(w) == CBN_SELCHANGE) { selection_changed(); return 0; }
            if (HIWORD(w) != BN_CLICKED) return DefWindowProcW(h,m,w,l);
            switch (LOWORD(w)) {
            case RefreshButton: refresh(); break;
            case PickButton: begin_pick(); break;
            case InfoButton: copy_info(); break;
            case StartButton: {
                auto choice = chosen_source();
                if (!choice) { text(L"请先用“3秒选游戏”选取，或从下拉列表中手动选择实际游戏。"); break; }
                start(*choice); break;
            }
            case StopButton: stop(true); break;
            case DemoButton: {
                auto exe = executable_dir() / L"Game-HD2D-Enhancer.exe";
                if (reinterpret_cast<INT_PTR>(ShellExecuteW(main,L"open",exe.c_str(),L"--demo",nullptr,SW_SHOWNORMAL)) <= 32)
                    throw std::runtime_error("Unable to start independent demo process");
                text(L"测试画面已另行打开。切到测试画面按 Ctrl+Alt+H，或用“3秒选游戏”选取。关闭增强不会关闭测试画面。"); break;
            }
            } return 0;
        }
        if (m == WM_HOTKEY) {
            if (w == StopHotkey || running || picking) { stop(true); return 0; }
            stage = L"选取前台窗口";
            if (auto target = inspect_source(source_root(GetForegroundWindow()))) { refresh(*target); start(*target); }
            else text(L"请先点击原游戏画面或标题栏，再按 Ctrl+Alt+H。也可用“3秒选游戏”。");
            return 0;
        }
        if (m == FrameMessage) { if (w == generation) on_frame(); return 0; }
        if (m == ClosedMessage) { if (w == generation) { stop(false); text(L"捕获对象已关闭，增强已停止。"); } return 0; }
        if (m == WM_TIMER) { if (w == 2) pick_tick(); else tick(); return 0; }
        if (m == WM_DPICHANGED) {
            auto r = reinterpret_cast<RECT*>(l); SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE); layout(); return 0;
        }
        if (m == WM_CLOSE) { stop(false); DestroyWindow(main); return 0; }
        if (m == WM_DESTROY) { KillTimer(h,1); KillTimer(h,2); UnregisterHotKey(h,ToggleHotkey); UnregisterHotKey(h,StopHotkey); PostQuitMessage(0); return 0; }
        return DefWindowProcW(h,m,w,l);
    }
};
// Synthetic sources are separate processes. Never captured user data.
static LRESULT CALLBACK DemoProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    static int frame{}, clicks{}, keys{};
    if (m == WM_CREATE) { SetTimer(h,1,33,nullptr); return 0; }
    if (m == WM_TIMER) { ++frame; InvalidateRect(h,nullptr,FALSE); return 0; }
    if (m == WM_LBUTTONDOWN) { ++clicks; return 0; }
    if (m == WM_KEYDOWN) { ++keys; return 0; }
    if (m == WM_PAINT) {
        PAINTSTRUCT ps{}; auto dc = BeginPaint(h,&ps); RECT r{}; GetClientRect(h,&r);
        FillRect(dc,&r,GetSysColorBrush(COLOR_WINDOW));
        for (int y=0;y<10;++y) for (int x=0;x<20;++x) {
            RECT tile{x*16,100+y*12,x*16+16,112+y*12};
            auto brush = CreateSolidBrush(((x+y)&1) ? RGB(38,65,90) : RGB(178,216,226)); FillRect(dc,&tile,brush); DeleteObject(brush);
        }
        RECT moving{(frame*3)%260,65,(frame*3)%260+36,91}; auto brush=CreateSolidBrush(RGB(230,130,65)); FillRect(dc,&moving,brush); DeleteObject(brush);
        SetBkMode(dc,TRANSPARENT);
        const auto line=L"高清眼镜测试  点击: "+std::to_wstring(clicks)+L"  按键: "+std::to_wstring(keys);
        TextOutW(dc,8,8,line.c_str(),int(line.size()));
        const wchar_t* hint=L"点击任意位置 / 按键，计数应增加。"; TextOutW(dc,8,35,hint,int(wcslen(hint)));
        EndPaint(h,&ps); return 0;
    }
    if (m == WM_DESTROY) { KillTimer(h,1); PostQuitMessage(0); return 0; }
    return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR arguments,int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    try {
        const std::wstring args(arguments);
        if (args.find(L"--self-test") != std::wstring::npos) {
            AttachConsole(ATTACH_PARENT_PROCESS); FILE* f{}; freopen_s(&f,"CONOUT$","w",stdout); freopen_s(&f,"CONOUT$","w",stderr);
            return run_self_tests();
        }
        INITCOMMONCONTROLSEX common{sizeof(common),ICC_STANDARD_CLASSES | ICC_BAR_CLASSES}; InitCommonControlsEx(&common);
        WNDCLASSEXW wc{sizeof(wc)}; wc.hInstance=instance; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW); wc.lpfnWndProc=App::MainProc; wc.lpszClassName=L"HD2DMain";
        if (!RegisterClassExW(&wc)) winrt::throw_last_error();
        wc.lpfnWndProc=App::OutputProc; wc.lpszClassName=L"HD2DOutput"; wc.style=CS_DBLCLKS;
        if (!RegisterClassExW(&wc)) winrt::throw_last_error();
        if (args.find(L"--demo") != std::wstring::npos) {
            wc.lpfnWndProc=DemoProc; wc.lpszClassName=L"HD2DDemo"; RegisterClassExW(&wc);
            const bool owned = args.find(L"--demo-owned") != std::wstring::npos;
            const bool tool = args.find(L"--demo-tool") != std::wstring::npos;
            const bool untitled = args.find(L"--demo-untitled") != std::wstring::npos;
            HWND owner = owned ? CreateWindowExW(0,L"STATIC",L"HD2D invisible owner",WS_POPUP,0,0,1,1,nullptr,nullptr,instance,nullptr) : nullptr;
            if (owned && !owner) winrt::throw_last_error();
            DWORD ex = tool ? WS_EX_TOOLWINDOW : 0;
            RECT r{0,0,320,240}; AdjustWindowRectEx(&r,WS_OVERLAPPEDWINDOW,FALSE,ex);
            const wchar_t* title = untitled ? L"" : owned ? L"HD2D Owned Fixture" : tool ? L"HD2D Tool Fixture" : L"HD2D 测试画面（独立进程）";
            HWND demo=CreateWindowExW(ex,L"HD2DDemo",title,WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,owner,nullptr,instance,nullptr);
            if (!demo) winrt::throw_last_error(); ShowWindow(demo,SW_SHOW);
            MSG msg{}; while(GetMessageW(&msg,nullptr,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
            if (owner && IsWindow(owner)) DestroyWindow(owner);
            return 0;
        }
        App app;
        const int dpi = int(GetDpiForSystem());
        RECT r{0,0,MulDiv(650,dpi,96),MulDiv(558,dpi,96)};
        DWORD style=WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        AdjustWindowRectExForDpi(&r,style,FALSE,0,UINT(dpi));
        auto h=CreateWindowExW(0,L"HD2DMain",L"Game-HD2D-Enhancer 0.1.1 · 实时高清眼镜",style,
            CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,&app);
        if(!h) winrt::throw_last_error(); ShowWindow(h,show);
        MSG msg{}; BOOL result{};
        while((result=GetMessageW(&msg,nullptr,0,0))>0) {
            if (!IsDialogMessageW(h,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        }
        return result == -1 ? 1 : int(msg.wParam);
    } catch (...) {
        auto text=error_text(); MessageBoxW(nullptr,text.c_str(),L"HD2D 启动失败",MB_OK|MB_ICONERROR); return 1;
    }
}
