#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace hd2d {
struct Size { int w{}, h{}; };
struct Point { int x{}, y{}; };
struct Rect {
    int x{}, y{}, w{}, h{};
    bool contains(Point p) const noexcept {
        return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h;
    }
};
inline void validate(Size s) {
    if (s.w <= 0 || s.h <= 0 || s.w > 8192 || s.h > 8192)
        throw std::invalid_argument("Image dimensions must be in 1..8192");
}
inline Rect fit(Size source, Size output, bool integer) {
    validate(source); validate(output);
    const double scale = std::min(double(output.w) / source.w, double(output.h) / source.h);
    const double used = integer && scale >= 1.0 ? std::floor(scale) : scale;
    const int w = std::clamp(int(std::floor(source.w * used + 1e-8)), 1, output.w);
    const int h = std::clamp(int(std::floor(source.h * used + 1e-8)), 1, output.h);
    return {(output.w - w) / 2, (output.h - h) / 2, w, h};
}
// Black bars are deliberately not mapped into the game. clamp=true is only for
// finishing an already-started drag so the game receives its button-up message.
inline std::optional<Point> map_input(Point p, Rect content, Size source, bool clamp = false) {
    validate(source); validate({content.w, content.h});
    if (!content.contains(p) && !clamp) return std::nullopt;
    const int x = std::clamp(p.x - content.x, 0, content.w - 1);
    const int y = std::clamp(p.y - content.y, 0, content.h - 1);
    return Point{
        std::min(source.w - 1, int(std::int64_t(x) * source.w / content.w)),
        std::min(source.h - 1, int(std::int64_t(y) * source.h / content.h))};
}
// WGC captures the DWM extended frame. Convert the visible client rectangle
// from physical desktop pixels to captured texture pixels (DPI-aware).
inline Rect crop_client(Rect extended, Rect client, Size texture) {
    validate(texture);
    if (extended.w <= 0 || extended.h <= 0 || client.w <= 0 || client.h <= 0)
        throw std::invalid_argument("Window has no drawable client area");
    const double sx = double(texture.w) / extended.w, sy = double(texture.h) / extended.h;
    const int l = std::clamp(int(std::lround((client.x - extended.x) * sx)), 0, texture.w - 1);
    const int t = std::clamp(int(std::lround((client.y - extended.y) * sy)), 0, texture.h - 1);
    const int r = std::clamp(int(std::lround((client.x + client.w - extended.x) * sx)), l + 1, texture.w);
    const int b = std::clamp(int(std::lround((client.y + client.h - extended.y) * sy)), t + 1, texture.h);
    return {l, t, r - l, b - t};
}
// NIS supports up to 2x per axis. For very small retro frames, prepare an
// integer-nearest intermediate; the final NIS pass remains inside that range.
inline int nis_prescale(Size input, Size output) {
    validate(input); validate(output);
    if (output.w < input.w || output.h < input.h) return 1;
    const double ratio = std::max(double(output.w) / input.w, double(output.h) / input.h);
    const int factor = std::max(1, int(std::ceil(ratio / 2.0)));
    return factor;
}
} // namespace hd2d
