#include "gpu.hpp"
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
    DemoButton = 108, FpsBox = 109;
constexpr int ToggleHotkey = 1, StopHotkey = 2;
struct Source { HWND hwnd{}; DWORD pid{}; std::wstring title; };
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
    bool running{}, fullscreen{}, hotkeysOK{}, inputFailed{};
    unsigned mouseButtons{}; HWND dragTarget{}; POINT lastMouse{};
    std::uintptr_t generation{};
    int frameLimit{60}, frames{}, totalFrames{};
    float sharpness{0.2f};
    Clock::time_point started{}, statsAt{}, nextFrame{};
    int dpi{96};
    ~App() { stop(false); if (font) DeleteObject(font); }
    void text(const std::wstring& s) { if (IsWindow(status)) SetWindowTextW(status, s.c_str()); }
    void fail() {
        auto message = error_text(); stop(false);
        text(L"增强已停止，原游戏未被关闭。\n" + message);
        MessageBoxW(main, (L"增强未能继续，原游戏没有被本程序关闭。\n\n" + message +
            L"\n\n请先使用窗口化游戏、关闭 HDR，或切换到双线性模式。不要关闭系统安全保护。").c_str(),
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
        move(201,20,16,620,34); move(202,20,54,620,24);
        move(203,20,88,110,24); move(SourceBox,130,84,390,260); move(RefreshButton,535,83,95,30);
        move(204,20,130,110,24); move(AlgorithmBox,130,125,500,180);
        move(205,20,173,110,24); move(DisplayBox,130,168,500,150);
        move(206,20,216,110,24); move(SharpSlider,130,211,285,30);
        move(207,435,216,75,24); move(FpsBox,515,211,115,150);
        move(StartButton,20,260,190,40); move(StopButton,225,260,190,40); move(DemoButton,430,260,200,40);
        move(208,20,315,610,54); move(209,20,380,610,72);
    }
    void initialize() {
        control(L"STATIC", L"Game-HD2D-Enhancer 0.1.0 alpha", 0, 201);
        control(L"STATIC", L"实时高清眼镜  ·  不改游戏文件  ·  不录像  ·  无需游戏库", 0, 202);
        control(L"STATIC", L"游戏窗口", 0, 203);
        sourceBox = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, SourceBox);
        control(L"BUTTON", L"刷新窗口", WS_TABSTOP, RefreshButton);
        control(L"STATIC", L"画面算法", 0, 204);
        algorithmBox = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, AlgorithmBox);
        for (auto s : {L"NIS 轻量高清（含锐化）", L"原味像素：整数倍 / 最近邻", L"双线性（兼容 / 对照）"})
            SendMessageW(algorithmBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));
        SendMessageW(algorithmBox, CB_SETCURSEL, 0, 0);
        control(L"STATIC", L"显示方式", 0, 205);
        displayBox = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, DisplayBox);
        SendMessageW(displayBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"等比例全屏（鼠标坐标映射：实验）"));
        SendMessageW(displayBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"独立预览窗口（仍操作原游戏窗口）"));
        SendMessageW(displayBox, CB_SETCURSEL, 0, 0);
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
        control(L"STATIC", L"Ctrl + Alt + H：增强当前前台窗口 / 停止\nCtrl + Alt + End：紧急停止。切换到其他窗口会停止全屏增强。", 0, 208);
        status = control(L"STATIC", L"就绪。先打开游戏，再选择窗口。首次建议用测试画面检查显示和点击。", 0, 209);
        layout(); refresh();
        const bool toggle = RegisterHotKey(main, ToggleHotkey, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'H') != 0;
        const bool escape = RegisterHotKey(main, StopHotkey, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_END) != 0;
        hotkeysOK = toggle && escape;
        if (!hotkeysOK) text(L"快捷键被占用，暂不允许全屏。可使用独立预览模式并通过停止按钮退出。");
        EnableWindow(GetDlgItem(main, StopButton), FALSE);
        SetTimer(main, 1, 100, nullptr);
    }
    void refresh() {
        if (running) return;
        sources.clear(); SendMessageW(sourceBox, CB_RESETCONTENT, 0, 0);
        EnumWindows([](HWND h, LPARAM l) -> BOOL {
            auto& a = *reinterpret_cast<App*>(l);
            DWORD pid{}; GetWindowThreadProcessId(h, &pid);
            if (pid == GetCurrentProcessId() || !IsWindowVisible(h) || IsIconic(h) || GetWindow(h, GW_OWNER)) return TRUE;
            if (GetWindowLongPtrW(h, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
            DWORD cloaked{}; DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
            if (cloaked) return TRUE;
            int length = GetWindowTextLengthW(h); if (length < 1) return TRUE;
            std::wstring title(std::size_t(length) + 1, L'\0');
            title.resize(std::size_t(GetWindowTextW(h, title.data(), int(title.size()))));
            RECT rect{}; GetClientRect(h, &rect); if (rect.right < 16 || rect.bottom < 16) return TRUE;
            a.sources.push_back({h, pid, title});
            std::wstring label = title + L"  [" + std::to_wstring(rect.right) + L"×" + std::to_wstring(rect.bottom) + L"]";
            SendMessageW(a.sourceBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            return TRUE;
        }, reinterpret_cast<LPARAM>(this));
        if (!sources.empty()) SendMessageW(sourceBox, CB_SETCURSEL, 0, 0);
    }
    void start(Source target) {
        stop(false);
        DWORD pid{}; GetWindowThreadProcessId(target.hwnd, &pid);
        if (!IsWindow(target.hwnd) || pid != target.pid || pid == GetCurrentProcessId() || IsIconic(target.hwnd))
            throw std::runtime_error("Source window is unavailable. Refresh the window list.");
        if (!capture::GraphicsCaptureSession::IsSupported())
            throw std::runtime_error("Windows Graphics Capture is unavailable. Windows 11 is recommended.");
        fullscreen = SendMessageW(displayBox, CB_GETCURSEL, 0, 0) == 0;
        if (fullscreen && !hotkeysOK) throw std::runtime_error("Emergency hotkey unavailable. Use preview mode.");
        selected = target;
        algorithm = Algorithm(int(SendMessageW(algorithmBox, CB_GETCURSEL, 0, 0)));
        sharpness = float(SendMessageW(sharpSlider, TBM_GETPOS, 0, 0)) / 100.f;
        frameLimit = SendMessageW(fpsBox, CB_GETCURSEL, 0, 0) == 0 ? 30 : 60;
        auto client = client_screen(selected.hwnd); sourceClient = {client.w, client.h}; validate(sourceClient);
        gpu = std::make_unique<Gpu>();
        ComPtr<IDXGIDevice> dxgi; winrt::check_hresult(gpu->device()->QueryInterface(IID_PPV_ARGS(&dxgi)));
        winrt::com_ptr<IInspectable> native;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.Get(), native.put()));
        winDevice = native.as<directx::Direct3D11::IDirect3DDevice>();
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
        // Output stays hidden until an actual frame has been captured and processed.
        session.StartCapture();
        EnableWindow(GetDlgItem(main, StartButton), FALSE); EnableWindow(GetDlgItem(main, StopButton), TRUE);
        for (int id : {SourceBox, RefreshButton, AlgorithmBox, DisplayBox, SharpSlider, FpsBox}) EnableWindow(GetDlgItem(main, id), FALSE);
        text(L"正在等待第一帧…\nGPU：" + gpu->adapter_name());
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
        running = false; ++generation;
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
            EnableWindow(GetDlgItem(main, StartButton), TRUE); EnableWindow(GetDlgItem(main, StopButton), FALSE);
            for (int id : {SourceBox, RefreshButton, AlgorithmBox, DisplayBox, SharpSlider, FpsBox}) EnableWindow(GetDlgItem(main, id), TRUE);
            if (notify) text(L"已摘掉眼镜。捕获和显示资源已释放，原游戏继续独立运行。");
        }
    }
    void on_frame() {
        if (!running || !pool) return;
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
            if (HIWORD(w) != BN_CLICKED) return DefWindowProcW(h,m,w,l);
            switch (LOWORD(w)) {
            case RefreshButton: refresh(); break;
            case StartButton: {
                auto index = int(SendMessageW(sourceBox, CB_GETCURSEL, 0, 0));
                if (index < 0 || index >= int(sources.size())) { text(L"请先打开游戏，然后刷新并选择窗口。"); break; }
                start(sources[std::size_t(index)]); break;
            }
            case StopButton: stop(true); break;
            case DemoButton: {
                auto exe = executable_dir() / L"Game-HD2D-Enhancer.exe";
                if (reinterpret_cast<INT_PTR>(ShellExecuteW(main,L"open",exe.c_str(),L"--demo",nullptr,SW_SHOWNORMAL)) <= 32)
                    throw std::runtime_error("Unable to start independent demo process");
                text(L"测试画面已另行打开。切到测试画面按 Ctrl+Alt+H，或刷新后选择它。关闭增强不会关闭测试画面。"); break;
            }
            } return 0;
        }
        if (m == WM_HOTKEY) {
            if (w == StopHotkey || running) { stop(true); return 0; }
            HWND target = GetForegroundWindow(); DWORD pid{}; GetWindowThreadProcessId(target,&pid);
            if (target && pid != GetCurrentProcessId()) start({target,pid,L"前台窗口"});
            else text(L"请先切到游戏，再按 Ctrl+Alt+H。");
            return 0;
        }
        if (m == FrameMessage) { if (w == generation) on_frame(); return 0; }
        if (m == ClosedMessage) { if (w == generation) { stop(false); text(L"捕获对象已关闭，增强已停止。"); } return 0; }
        if (m == WM_TIMER) { tick(); return 0; }
        if (m == WM_DPICHANGED) {
            auto r = reinterpret_cast<RECT*>(l); SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE); layout(); return 0;
        }
        if (m == WM_CLOSE) { stop(false); DestroyWindow(main); return 0; }
        if (m == WM_DESTROY) { KillTimer(h,1); UnregisterHotKey(h,ToggleHotkey); UnregisterHotKey(h,StopHotkey); PostQuitMessage(0); return 0; }
        return DefWindowProcW(h,m,w,l);
    }
};
// Synthetic source is a separate process. It is not a captured game or a saved frame.
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
        if (std::wstring(arguments).find(L"--self-test") != std::wstring::npos) {
            AttachConsole(ATTACH_PARENT_PROCESS); FILE* f{}; freopen_s(&f,"CONOUT$","w",stdout); freopen_s(&f,"CONOUT$","w",stderr);
            return run_self_tests();
        }
        INITCOMMONCONTROLSEX common{sizeof(common),ICC_STANDARD_CLASSES | ICC_BAR_CLASSES}; InitCommonControlsEx(&common);
        WNDCLASSEXW wc{sizeof(wc)}; wc.hInstance=instance; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW); wc.lpfnWndProc=App::MainProc; wc.lpszClassName=L"HD2DMain";
        if (!RegisterClassExW(&wc)) winrt::throw_last_error();
        wc.lpfnWndProc=App::OutputProc; wc.lpszClassName=L"HD2DOutput"; wc.style=CS_DBLCLKS;
        if (!RegisterClassExW(&wc)) winrt::throw_last_error();
        if (std::wstring(arguments).find(L"--demo") != std::wstring::npos) {
            wc.lpfnWndProc=DemoProc; wc.lpszClassName=L"HD2DDemo"; RegisterClassExW(&wc);
            RECT r{0,0,320,240}; AdjustWindowRectEx(&r,WS_OVERLAPPEDWINDOW,FALSE,0);
            HWND demo=CreateWindowExW(0,L"HD2DDemo",L"HD2D 测试画面（独立进程）",WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);
            if (!demo) winrt::throw_last_error(); ShowWindow(demo,SW_SHOW);
            MSG msg{}; while(GetMessageW(&msg,nullptr,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); } return 0;
        }
        App app;
        const int dpi = int(GetDpiForSystem());
        RECT r{0,0,MulDiv(650,dpi,96),MulDiv(470,dpi,96)};
        DWORD style=WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        AdjustWindowRectExForDpi(&r,style,FALSE,0,UINT(dpi));
        auto h=CreateWindowExW(0,L"HD2DMain",L"Game-HD2D-Enhancer · 实时高清眼镜",style,
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
