#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace hd2d {
struct Source { HWND hwnd{}; DWORD pid{}; std::wstring title; };

// A visible owned top-level window may be the game's real output. Neither
// GW_OWNER nor WS_EX_TOOLWINDOW is a reason to reject it. Never walk to
// GA_ROOTOWNER: legacy games may be owned by an invisible helper window.
inline HWND source_root(HWND window) noexcept {
    return window ? GetAncestor(window, GA_ROOT) : nullptr;
}
inline std::wstring window_class(HWND window) {
    wchar_t name[256]{};
    const int size = GetClassNameW(window, name, 256);
    return size > 0 ? std::wstring(name, size) : std::wstring{};
}
inline std::optional<Source> inspect_source(HWND window) {
    if (!IsWindow(window) || !IsWindowVisible(window)) return std::nullopt;
    DWORD pid{};
    GetWindowThreadProcessId(window, &pid);
    if (!pid || pid == GetCurrentProcessId()) return std::nullopt;
    if (GetWindowLongPtrW(window, GWL_STYLE) & WS_CHILD) return std::nullopt;
    if (window == GetDesktopWindow() || window == GetShellWindow()) return std::nullopt;
    const auto cls = window_class(window);
    for (const auto* shellClass : {L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd"})
        if (cls == shellClass) return std::nullopt;
    DWORD cloaked{};
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked)
        return std::nullopt;
    RECT client{};
    if (!GetClientRect(window, &client)) return std::nullopt;
    // List minimized candidates with a label; do not restore someone else's
    // window automatically. Starting capture explains that it must be restored.
    if (!IsIconic(window) && (client.right < 16 || client.bottom < 16)) return std::nullopt;
    const int length = std::clamp(GetWindowTextLengthW(window), 0, 4096);
    std::wstring title(static_cast<std::size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(window, title.data(), static_cast<int>(title.size()));
    title.resize(copied > 0 ? static_cast<std::size_t>(copied) : 0);
    if (title.empty()) title = L"（无标题）" + cls;
    return Source{window, pid, title};
}
inline std::vector<Source> enumerate_sources() {
    struct Context { std::vector<Source> windows; bool failed{}; } context;
    EnumWindows([](HWND window, LPARAM arg) -> BOOL {
        auto& c = *reinterpret_cast<Context*>(arg);
        try {
            if (auto source = inspect_source(window)) c.windows.push_back(*source);
        } catch (...) { c.failed = true; return FALSE; }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));
    if (context.failed) throw std::runtime_error("Window enumeration ran out of resources");
    return context.windows;
}
inline std::wstring source_label(const Source& source) {
    RECT client{}; GetClientRect(source.hwnd, &client);
    std::wstring label = source.title + L"  [PID " + std::to_wstring(source.pid) + L" / " +
        std::to_wstring(client.right) + L"×" + std::to_wstring(client.bottom);
    if (GetWindow(source.hwnd, GW_OWNER)) label += L" / 附属窗口";
    if (GetWindowLongPtrW(source.hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) label += L" / 工具样式";
    if (IsIconic(source.hwnd)) label += L" / 已最小化";
    return label + L"]";
}
inline std::wstring source_diagnostics(const Source& source) {
    RECT client{}; GetClientRect(source.hwnd, &client);
    DWORD currentPid{}, cloaked{};
    GetWindowThreadProcessId(source.hwnd, &currentPid);
    const auto hr = DwmGetWindowAttribute(source.hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    std::wostringstream out;
    out << L"Game-HD2D-Enhancer 0.1.1 alpha\r\n"
        << L"仅包含选中窗口的元数据；不包含游戏画面、按键记录、进程内存或文件内容。\r\n"
        << L"标题: " << source.title << L"\r\n窗口类: " << window_class(source.hwnd)
        << L"\r\nPID: " << source.pid << L" 当前PID: " << currentPid
        << L"\r\n有效: " << IsWindow(source.hwnd) << L" 可见: " << IsWindowVisible(source.hwnd)
        << L" 最小化: " << IsIconic(source.hwnd) << L" DWM隐藏: " << (SUCCEEDED(hr) ? int(cloaked) : -1)
        << L"\r\n客户区: " << client.right << L"×" << client.bottom
        << L"\r\nHWND: 0x" << std::hex << reinterpret_cast<std::uintptr_t>(source.hwnd)
        << L" Owner: 0x" << reinterpret_cast<std::uintptr_t>(GetWindow(source.hwnd, GW_OWNER))
        << L"\r\nStyle: 0x" << GetWindowLongPtrW(source.hwnd, GWL_STYLE)
        << L" ExStyle: 0x" << GetWindowLongPtrW(source.hwnd, GWL_EXSTYLE) << L"\r\n";
    return out.str();
}
}
