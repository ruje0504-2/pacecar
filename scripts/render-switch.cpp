// Controlled visual fixture, not a screenshot or a physical GPU acceptance.
#include "../src/Pacecar.Overlay/Widgets/WidgetScene.h"
#include <wincodec.h>
#include <cassert>
#include <cstdio>
using namespace pacecar::overlay;
int wmain(int argc,wchar_t** argv){
    if(argc<2)return 2;
    assert(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)));
    const UINT width=argc>2?static_cast<UINT>(_wtoi(argv[2])):640u;
    const UINT height=argc>3?static_cast<UINT>(_wtoi(argv[3])):58u;
    ComPtr<IWICImagingFactory> wic;ComPtr<IWICBitmap> bitmap;ComPtr<ID2D1Factory> factory;ComPtr<ID2D1RenderTarget> target;
    assert(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic))));
    assert(SUCCEEDED(wic->CreateBitmap(width,height,GUID_WICPixelFormat32bppPBGRA,WICBitmapCacheOnLoad,&bitmap)));
    assert(SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory.GetAddressOf())));
    const auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
    const HRESULT surface=factory->CreateWicBitmapRenderTarget(bitmap.Get(),properties,target.GetAddressOf());
    if(FAILED(surface)){printf("WIC render target HRESULT=0x%08lx\n",static_cast<unsigned long>(surface));return 3;}
    WidgetScene scene;pacecar::Config config;config.general.view=height>150?pacecar::ViewMode::SwitchPanel:pacecar::ViewMode::SwitchBar;
    config.general.stat_text_size=11;scene.SetLayout(LayoutSettingsFromConfig(config));
    auto snapshot=std::make_shared<pacecar::metrics::MetricsSnapshot>();
    snapshot->soc.valid=127;snapshot->soc.cpuKhz=1963500;snapshot->soc.gpuKhz=998400;snapshot->soc.ramKhz=1600000;
    snapshot->soc.fanPwm=76;snapshot->soc.socTempC=42;snapshot->soc.boardTempC=37;snapshot->soc.batteryPercent=97;
    snapshot->soc.externalPower=true;snapshot->soc.charging=true;snapshot->frame.status.available=true;snapshot->frame.status.stale=false;
    snapshot->frame.fps=59.7;snapshot->graphicsApi=11;
    auto frame=std::make_shared<pacecar::metrics::DisplayFrame>();frame->snapshot=snapshot;scene.SetFrame(frame);
    target->BeginDraw();target->Clear(D2D1::ColorF(0.03f,0.035f,0.05f,1.0f));scene.Draw(target.Get(),ResolveTheme({pacecar::Theme::Dark,1.0,false}));
    assert(SUCCEEDED(target->EndDraw()));
    ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> image;
    assert(SUCCEEDED(wic->CreateStream(&stream)));assert(SUCCEEDED(stream->InitializeFromFilename(argv[1],GENERIC_WRITE)));
    assert(SUCCEEDED(wic->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder)));
    assert(SUCCEEDED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache)));
    assert(SUCCEEDED(encoder->CreateNewFrame(&image,nullptr)));assert(SUCCEEDED(image->Initialize(nullptr)));
    assert(SUCCEEDED(image->SetSize(width,height)));WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
    assert(SUCCEEDED(image->SetPixelFormat(&format)));assert(SUCCEEDED(image->WriteSource(bitmap.Get(),nullptr)));
    assert(SUCCEEDED(image->Commit()));assert(SUCCEEDED(encoder->Commit()));
    puts("Rendered controlled Switch monitor preview");return 0;
}
