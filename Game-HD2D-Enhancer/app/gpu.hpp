#pragma once
#include "geometry.hpp"
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <wrl/client.h>
#include <filesystem>
#include <vector>
#include <string>

namespace hd2d {
using Microsoft::WRL::ComPtr;
enum class Algorithm { NIS = 0, Integer = 1, Bilinear = 2 };
struct Surface {
    Size size{};
    DXGI_FORMAT format{DXGI_FORMAT_UNKNOWN};
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11UnorderedAccessView> uav;
};
std::filesystem::path executable_dir();
class Gpu {
public:
    explicit Gpu(bool software = false);
    ~Gpu();
    Gpu(const Gpu&) = delete;
    Gpu& operator=(const Gpu&) = delete;
    ID3D11Device* device() const noexcept { return device_.Get(); }
    ID3D11DeviceContext* context() const noexcept { return context_.Get(); }
    const std::wstring& adapter_name() const noexcept { return adapter_; }
    Surface make_surface(Size, DXGI_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM, bool writable = true);
    const Surface& enhance(ID3D11Texture2D* captured, Rect crop, Size output, Algorithm, float sharpness);
    void attach(HWND output, Size);
    void resize(Size);
    bool ready() const noexcept;
    void present(const Surface&, Rect content);
    // Test-only readback. The normal capture/enhance/present path never uses it.
    std::vector<std::uint32_t> readback_for_test(const Surface&);
private:
    void ensure(Surface&, Size, DXGI_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM, bool writable = true);
    void basic(ID3D11ShaderResourceView*, Size, Surface&, bool point);
    void initialize_nis();
    void release_backbuffer();
    void bind_backbuffer();
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11ComputeShader> basicShader_, nisShader_;
    ComPtr<ID3D11Buffer> basicConstants_, nisConstants_;
    ComPtr<ID3D11SamplerState> linear_;
    ComPtr<ID3D11ShaderResourceView> coefficients_, usm_;
    Surface cropped_, pre_, result_;
    ComPtr<IDXGISwapChain2> swap_;
    ComPtr<ID3D11Texture2D> back_;
    ComPtr<ID3D11RenderTargetView> backView_;
    HANDLE latency_{};
    Size display_{};
    std::wstring adapter_;
};
int run_self_tests();
} // namespace hd2d
