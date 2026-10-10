#include "gpu.hpp"
#include <d3dcompiler.h>
#include <winrt/base.h>
#include <NIS_Config.h>
#include <array>
#include <cstring>
#include <stdexcept>

namespace hd2d {
using winrt::check_hresult;
std::filesystem::path executable_dir() {
    std::wstring path(32768, L'\0');
    DWORD n = GetModuleFileNameW(nullptr, path.data(), DWORD(path.size()));
    if (!n || n >= path.size()) throw std::runtime_error("Cannot resolve executable directory");
    path.resize(n);
    return std::filesystem::path(path).parent_path();
}
static const char* basicCode = R"hlsl(
Texture2D<float4> image : register(t0);
RWTexture2D<float4> target : register(u0);
SamplerState linearClamp : register(s0);
cbuffer Settings : register(b0) { uint2 inputSize; uint2 outputSize; uint pointMode; uint3 unused; };
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (any(id.xy >= outputSize)) return;
    float2 uv = (float2(id.xy) + 0.5) / float2(outputSize);
    float3 rgb;
    if (pointMode != 0) {
        uint2 p = min(uint2(uv * inputSize), inputSize - 1);
        rgb = image.Load(int3(p, 0)).rgb;
    } else rgb = image.SampleLevel(linearClamp, uv, 0).rgb;
    target[id.xy] = float4(rgb, 1.0);
}
)hlsl";
static void compiler_check(HRESULT hr, ID3DBlob* errors) {
    if (FAILED(hr) && errors) {
        throw std::runtime_error(std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()));
    }
    check_hresult(hr);
}
Gpu::Gpu(bool software) {
    const D3D_FEATURE_LEVEL requested[]{D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL level{};
    check_hresult(D3D11CreateDevice(nullptr, software ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
        nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, requested, 1, D3D11_SDK_VERSION,
        &device_, &level, &context_));
    ComPtr<IDXGIDevice> dxgi; check_hresult(device_.As(&dxgi));
    ComPtr<IDXGIAdapter> adapter; check_hresult(dxgi->GetAdapter(&adapter));
    DXGI_ADAPTER_DESC description{}; check_hresult(adapter->GetDesc(&description));
    adapter_ = description.Description;
    ComPtr<ID3DBlob> code, errors;
    auto hr = D3DCompile(basicCode, std::strlen(basicCode), "HD2D-basic", nullptr, nullptr,
        "main", "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    compiler_check(hr, errors.Get());
    check_hresult(device_->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &basicShader_));
    D3D11_BUFFER_DESC cb{}; cb.ByteWidth = 32; cb.Usage = D3D11_USAGE_DEFAULT; cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    check_hresult(device_->CreateBuffer(&cb, nullptr, &basicConstants_));
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
    check_hresult(device_->CreateSamplerState(&sampler, &linear_));
}
Gpu::~Gpu() {
    if (context_) { context_->ClearState(); context_->Flush(); }
    if (latency_) CloseHandle(latency_);
}
Surface Gpu::make_surface(Size size, DXGI_FORMAT format, bool writable) {
    validate(size);
    Surface s; s.size = size; s.format = format;
    D3D11_TEXTURE2D_DESC d{};
    d.Width = UINT(size.w); d.Height = UINT(size.h); d.MipLevels = d.ArraySize = 1;
    d.Format = format; d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE | (writable ? D3D11_BIND_UNORDERED_ACCESS : 0);
    check_hresult(device_->CreateTexture2D(&d, nullptr, &s.texture));
    check_hresult(device_->CreateShaderResourceView(s.texture.Get(), nullptr, &s.srv));
    if (writable) check_hresult(device_->CreateUnorderedAccessView(s.texture.Get(), nullptr, &s.uav));
    return s;
}
void Gpu::ensure(Surface& s, Size size, DXGI_FORMAT format, bool writable) {
    if (!s.texture || s.size.w != size.w || s.size.h != size.h || s.format != format)
        s = make_surface(size, format, writable);
}
void Gpu::basic(ID3D11ShaderResourceView* input, Size size, Surface& target, bool point) {
    std::array<UINT, 8> constants{UINT(size.w), UINT(size.h), UINT(target.size.w), UINT(target.size.h), point ? 1u : 0u, 0, 0, 0};
    context_->UpdateSubresource(basicConstants_.Get(), 0, nullptr, constants.data(), 0, 0);
    context_->CSSetShader(basicShader_.Get(), nullptr, 0);
    context_->CSSetConstantBuffers(0, 1, basicConstants_.GetAddressOf());
    context_->CSSetSamplers(0, 1, linear_.GetAddressOf());
    context_->CSSetShaderResources(0, 1, &input);
    context_->CSSetUnorderedAccessViews(0, 1, target.uav.GetAddressOf(), nullptr);
    context_->Dispatch((UINT(target.size.w) + 7) / 8, (UINT(target.size.h) + 7) / 8, 1);
    ID3D11ShaderResourceView* noneSRV = nullptr; ID3D11UnorderedAccessView* noneUAV = nullptr;
    context_->CSSetShaderResources(0, 1, &noneSRV);
    context_->CSSetUnorderedAccessViews(0, 1, &noneUAV, nullptr);
}
void Gpu::initialize_nis() {
    if (nisShader_) return;
    const D3D_SHADER_MACRO defines[]{
        {"NIS_SCALER", "1"}, {"NIS_HDR_MODE", "0"}, {"NIS_BLOCK_WIDTH", "32"},
        {"NIS_BLOCK_HEIGHT", "24"}, {"NIS_THREAD_GROUP_SIZE", "256"},
        {"NIS_USE_HALF_PRECISION", "0"}, {"NIS_VIEWPORT_SUPPORT", "1"}, {"NIS_CLAMP_OUTPUT", "1"}, {nullptr, nullptr}};
    ComPtr<ID3DBlob> code, errors;
    auto path = executable_dir() / L"shaders" / L"NIS" / L"NIS_Main.hlsl";
    auto hr = D3DCompileFromFile(path.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main", "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    compiler_check(hr, errors.Get());
    check_hresult(device_->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &nisShader_));
    D3D11_BUFFER_DESC cb{}; cb.ByteWidth = sizeof(NISConfig); cb.Usage = D3D11_USAGE_DEFAULT; cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    check_hresult(device_->CreateBuffer(&cb, nullptr, &nisConstants_));
    auto coefficients = [this](const void* values, ComPtr<ID3D11ShaderResourceView>& view) {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = UINT(kFilterSize / 4); td.Height = UINT(kPhaseCount); td.MipLevels = td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA initial{}; initial.pSysMem = values; initial.SysMemPitch = UINT(kFilterSize * sizeof(float));
        ComPtr<ID3D11Texture2D> texture;
        check_hresult(device_->CreateTexture2D(&td, &initial, &texture));
        check_hresult(device_->CreateShaderResourceView(texture.Get(), nullptr, &view));
    };
    coefficients(coef_scale, coefficients_); coefficients(coef_USM, usm_);
}
const Surface& Gpu::enhance(ID3D11Texture2D* captured, Rect crop, Size output, Algorithm mode, float sharpness) {
    validate(output);
    D3D11_TEXTURE2D_DESC sourceDesc{}; captured->GetDesc(&sourceDesc);
    if (crop.x < 0 || crop.y < 0 || crop.w <= 0 || crop.h <= 0 ||
        crop.x + crop.w > int(sourceDesc.Width) || crop.y + crop.h > int(sourceDesc.Height))
        throw std::invalid_argument("Capture crop exceeds texture bounds");
    ensure(cropped_, {crop.w, crop.h}, sourceDesc.Format, false);
    D3D11_BOX box{UINT(crop.x), UINT(crop.y), 0, UINT(crop.x + crop.w), UINT(crop.y + crop.h), 1};
    context_->CopySubresourceRegion(cropped_.texture.Get(), 0, 0, 0, 0, captured, 0, &box);
    ensure(result_, output);
    if (mode != Algorithm::NIS || output.w < crop.w || output.h < crop.h) {
        basic(cropped_.srv.Get(), cropped_.size, result_, mode == Algorithm::Integer);
        return result_;
    }
    initialize_nis();
    auto input = cropped_.srv.Get(); auto size = cropped_.size;
    const int factor = nis_prescale(size, output);
    if (factor > 1) {
        ensure(pre_, {size.w * factor, size.h * factor});
        basic(input, size, pre_, true); input = pre_.srv.Get(); size = pre_.size;
    }
    NISConfig config{};
    if (!NVScalerUpdateConfig(config, std::clamp(sharpness, 0.f, 1.f),
            0, 0, UINT(size.w), UINT(size.h), UINT(size.w), UINT(size.h),
            0, 0, UINT(output.w), UINT(output.h), UINT(output.w), UINT(output.h)))
        throw std::runtime_error("NIS rejected the input/output scale ratio");
    context_->UpdateSubresource(nisConstants_.Get(), 0, nullptr, &config, 0, 0);
    ID3D11ShaderResourceView* views[]{input, coefficients_.Get(), usm_.Get()};
    context_->CSSetShader(nisShader_.Get(), nullptr, 0);
    context_->CSSetConstantBuffers(0, 1, nisConstants_.GetAddressOf());
    context_->CSSetSamplers(0, 1, linear_.GetAddressOf());
    context_->CSSetShaderResources(0, 3, views);
    context_->CSSetUnorderedAccessViews(0, 1, result_.uav.GetAddressOf(), nullptr);
    context_->Dispatch((UINT(output.w) + 31) / 32, (UINT(output.h) + 23) / 24, 1);
    ID3D11ShaderResourceView* noViews[3]{}; ID3D11UnorderedAccessView* noUAV = nullptr;
    context_->CSSetShaderResources(0, 3, noViews);
    context_->CSSetUnorderedAccessViews(0, 1, &noUAV, nullptr);
    return result_;
}
void Gpu::attach(HWND window, Size size) {
    validate(size);
    ComPtr<IDXGIDevice> dxgi; check_hresult(device_.As(&dxgi));
    ComPtr<IDXGIAdapter> adapter; check_hresult(dxgi->GetAdapter(&adapter));
    ComPtr<IDXGIFactory2> factory; check_hresult(adapter->GetParent(IID_PPV_ARGS(&factory)));
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = UINT(size.w); sd.Height = UINT(size.h); sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    ComPtr<IDXGISwapChain1> first;
    check_hresult(factory->CreateSwapChainForHwnd(device_.Get(), window, &sd, nullptr, nullptr, &first));
    check_hresult(first.As(&swap_));
    check_hresult(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));
    check_hresult(swap_->SetMaximumFrameLatency(1));
    latency_ = swap_->GetFrameLatencyWaitableObject();
    display_ = size; bind_backbuffer();
}
void Gpu::release_backbuffer() { backView_.Reset(); back_.Reset(); }
void Gpu::bind_backbuffer() {
    check_hresult(swap_->GetBuffer(0, IID_PPV_ARGS(&back_)));
    check_hresult(device_->CreateRenderTargetView(back_.Get(), nullptr, &backView_));
}
void Gpu::resize(Size size) {
    if (!swap_ || (size.w == display_.w && size.h == display_.h)) return;
    validate(size); release_backbuffer();
    check_hresult(swap_->ResizeBuffers(2, UINT(size.w), UINT(size.h), DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));
    display_ = size; bind_backbuffer();
}
bool Gpu::ready() const noexcept {
    return !latency_ || WaitForSingleObject(latency_, 0) == WAIT_OBJECT_0;
}
void Gpu::present(const Surface& image, Rect content) {
    if (content.x < 0 || content.y < 0 || content.x + image.size.w > display_.w ||
        content.y + image.size.h > display_.h) throw std::invalid_argument("Output rectangle exceeds backbuffer");
    const float black[]{0, 0, 0, 1}; context_->ClearRenderTargetView(backView_.Get(), black);
    context_->CopySubresourceRegion(back_.Get(), 0, UINT(content.x), UINT(content.y), 0, image.texture.Get(), 0, nullptr);
    DXGI_PRESENT_PARAMETERS parameters{};
    check_hresult(swap_->Present1(1, 0, &parameters));
}
std::vector<std::uint32_t> Gpu::readback_for_test(const Surface& surface) {
    D3D11_TEXTURE2D_DESC desc{}; surface.texture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    check_hresult(device_->CreateTexture2D(&desc, nullptr, &staging));
    context_->CopyResource(staging.Get(), surface.texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    check_hresult(context_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    std::vector<std::uint32_t> result(std::size_t(surface.size.w) * surface.size.h);
    for (int y = 0; y < surface.size.h; ++y)
        std::memcpy(result.data() + std::size_t(y) * surface.size.w,
            static_cast<const char*>(mapped.pData) + std::size_t(y) * mapped.RowPitch, std::size_t(surface.size.w) * 4);
    context_->Unmap(staging.Get(), 0); return result;
}
} // namespace hd2d
