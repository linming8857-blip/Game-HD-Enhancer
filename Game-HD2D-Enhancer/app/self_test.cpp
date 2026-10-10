#include "gpu.hpp"
#include <winrt/base.h>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <cmath>

namespace hd2d {
static void require(bool value, const char* description) {
    if (!value) throw std::runtime_error(description);
    std::printf("PASS: %s\n", description);
}
int run_self_tests() {
    try {
        auto r = fit({800,600},{1920,1080},false);
        require(r.x==240 && r.y==0 && r.w==1440 && r.h==1080,"4:3 aspect fit and black bars");
        auto i = fit({800,600},{1920,1080},true);
        require(i.w==800 && i.h==600,"800x600 does not incorrectly claim 2x fits 1080p");
        auto gba = fit({240,160},{1920,1080},true);
        require(gba.w==1440 && gba.h==960 && gba.x==240 && gba.y==60,"GBA exact 6x integer layout");
        require(!map_input({239,200},r,{800,600}).has_value(),"black-bar clicks rejected");
        auto a = map_input({240,0},r,{800,600}); auto b = map_input({1679,1079},r,{800,600});
        require(a && a->x==0 && a->y==0 && b && b->x==799 && b->y==599,"mouse mapping first/last pixels");
        auto c = map_input({2000,-10},r,{800,600},true);
        require(c && c->x==799 && c->y==0,"drag release clamps safely");
        auto crop = crop_client({-100,40,816,638},{-92,70,800,600},{816,638});
        require(crop.x==8 && crop.y==30 && crop.w==800 && crop.h==600,"client crop with negative monitor coordinates");
        auto dpiCrop = crop_client({0,0,408,319},{4,15,400,300},{816,638});
        require(dpiCrop.x==8 && dpiCrop.y==30 && dpiCrop.w==800 && dpiCrop.h==600,"DPI-scaled client crop");
        bool rejected=false; try { fit({0,3},{100,100},false); } catch (std::invalid_argument const&) { rejected=true; }
        require(rejected,"zero-size input rejected");
        rejected=false; try { fit({9000,3},{100,100},false); } catch (std::invalid_argument const&) { rejected=true; }
        require(rejected,"oversized input rejected");
        require(nis_prescale({240,160},{1620,1080})==4,"NIS high-ratio nearest preparation");
        require(nis_prescale({800,600},{1440,1080})==1,"NIS ordinary upscaling stays one pass");
        for(int sw : {1,17,240,800,1920}) for(int sh : {1,11,160,600,1080}) {
            auto bounds=fit({sw,sh},{1920,1080},false);
            if(bounds.x<0 || bounds.y<0 || bounds.x+bounds.w>1920 || bounds.y+bounds.h>1080)
                throw std::runtime_error("Geometry sweep exceeded output bounds");
        }
        require(true,"25 aspect-ratio boundary cases");
        Gpu gpu(true); // Deterministic software D3D11 (WARP), not a hardware performance claim.
        std::printf("D3D11 backend: WARP. These tests do NOT validate WGC capture or real games.\n");
        auto source = gpu.make_surface({2,2});
        const std::uint32_t pixels[]{0xff0000ffu,0xff00ff00u,0xffff0000u,0xffffffffu};
        gpu.context()->UpdateSubresource(source.texture.Get(),0,nullptr,pixels,8,0);
        auto& point = gpu.enhance(source.texture.Get(),{0,0,2,2},{4,4},Algorithm::Integer,0);
        auto result = gpu.readback_for_test(point);
        bool exact=true;
        for(int y=0;y<4;++y) for(int x=0;x<4;++x) exact &= result[std::size_t(y)*4+x]==pixels[(y/2)*2+x/2];
        require(exact,"nearest shader reproduces 2x pixels exactly");
        auto& clipped = gpu.enhance(source.texture.Get(),{1,0,1,2},{2,4},Algorithm::Integer,0);
        auto clippedData = gpu.readback_for_test(clipped);
        require(clippedData[0]==pixels[1] && clippedData[7]==pixels[3],"GPU crop preserves correct channels");
        auto bgra = gpu.make_surface({2,2},DXGI_FORMAT_B8G8R8A8_UNORM,false);
        const std::uint32_t bgraPixels[]{0xffff0000u,0xffff0000u,0xffff0000u,0xffff0000u};
        gpu.context()->UpdateSubresource(bgra.texture.Get(),0,nullptr,bgraPixels,8,0);
        auto& converted = gpu.enhance(bgra.texture.Get(),{0,0,2,2},{4,4},Algorithm::Integer,0);
        require(gpu.readback_for_test(converted)[0]==0xff0000ffu,"WGC BGRA to RGBA color conversion");
        auto flat = gpu.make_surface({17,11});
        std::vector<std::uint32_t> grey(17*11,0xff808080u);
        gpu.context()->UpdateSubresource(flat.texture.Get(),0,nullptr,grey.data(),17*4,0);
        for(auto mode : {Algorithm::Bilinear,Algorithm::NIS}) {
            for(auto size : {Size{17,11},Size{31,19},Size{107,69}}) {
                auto& processed=gpu.enhance(flat.texture.Get(),{0,0,17,11},size,mode,.2f);
                auto values=gpu.readback_for_test(processed);
                for(auto v:values) for(int channel=0;channel<3;++channel)
                    if(std::abs(int((v>>(channel*8))&255u)-128)>4)
                        throw std::runtime_error("Flat-field shader test: incorrect value or unwritten edge pixels");
            }
            require(true,mode==Algorithm::NIS ? "NIS real shader: 1x, odd dimensions, >2x preparation and edges" : "bilinear shader: flat-field and odd edges");
        }
        for(int n=0;n<30;++n) {
            auto& processed=gpu.enhance(flat.texture.Get(),{0,0,17,11},{n%2?34:51,n%2?22:33},Algorithm::NIS,.3f);
            if(gpu.readback_for_test(processed).empty()) throw std::runtime_error("Resize test returned no pixels");
        }
        require(true,"30 GPU resource resize/reuse iterations");
        std::printf("ALL SELF-TESTS PASSED. No gameplay, capture, latency or battery validation is implied.\n");
        return 0;
    } catch(winrt::hresult_error const& e) {
        std::fprintf(stderr,"FAIL HRESULT=0x%08x: %ls\n",unsigned(e.code()),e.message().c_str()); return 1;
    } catch(std::exception const& e) { std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1; }
}
} // namespace hd2d
#ifdef HD2D_TEST_EXECUTABLE
int main() { winrt::init_apartment(winrt::apartment_type::multi_threaded); return hd2d::run_self_tests(); }
#endif
