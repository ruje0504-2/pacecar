// Native controls: fake SoC values, lifecycle markers and backend classification.
// These tests do not read or change physical driver/clock state.
#include "pacecar/metrics/SwitchSocProvider.h"
#include "pacecar/metrics/GraphicsApiProvider.h"
#include "pacecar/metrics/MetricsFingerprint.h"
#include "pacecar/overlay/Layout.h"
#include "u_graphics_api_marker.h"
#include <cassert>
#include <cstdio>
using namespace pacecar::metrics;
class FakeSoc final:public ISwitchSocSource {public:SwitchSocMetrics value;SwitchSocMetrics Read()override{return value;}};
int main(){
    auto source=std::make_unique<FakeSoc>();auto* fake=source.get();SwitchSocProvider provider(std::move(source));
    fake->value.valid=SwitchCpu|SwitchGpu|SwitchRam|SwitchSocTemp|SwitchBoardTemp|SwitchFan|SwitchBattery;
    fake->value.cpuKhz=1963500;fake->value.gpuKhz=998400;fake->value.ramKhz=1600000;
    fake->value.socTempC=42;fake->value.boardTempC=37;fake->value.fanPwm=76;fake->value.batteryPercent=97;
    MetricsSnapshot snap;snap.cpu.totalFrequencyMhz=1000;snap.cpu.cores.resize(4);
    assert(provider.Poll(snap)==S_OK && snap.cpu.totalFrequencyMhz==1963.5 && snap.gpu.coreClockMhz==998.4);
    assert(!snap.gpu.temperatureStatus.available); // SoC != independent GPU junction
    auto h=FingerprintSnapshot(snap);fake->value.batteryPercent=96;provider.Poll(snap);assert(h!=FingerprintSnapshot(snap));
    MetricsSnapshot copy;copy.CopyFrom(snap);assert(copy.soc.batteryPercent==96);
    fake->value={};provider.Poll(snap);assert(!snap.gpu.clockStatus.available&&snap.gpu.coreClockMhz==0&&snap.cpu.totalFrequencyMhz==0);
    assert(ClassifyGraphicsApi({})==0);
    assert(ClassifyGraphicsApi({true,false,true,false,false})==9);
    assert(ClassifyGraphicsApi({true,false,false,true,true})==10);
    assert(ClassifyGraphicsApi({true,false,false,false,true})==11);
    assert(ClassifyGraphicsApi({true,true,false,false,false})==2);
    assert(ClassifyGraphicsApi({true,false,false,false,false})==1);
    assert(ClassifyGraphicsApi({true,false,true,false,true})==0);
    assert(ClassifyGraphicsApi({true,true,false,false,true})==0);
    assert(ClassifyGraphicsApi({true,false,false,false,false,true})==0);
    assert(ClassifyGraphicsApi({false,false,false,false,false,true})==0);
    assert(ClassifyGraphicsApi({true,true,false,false,false,true})==0);
    assert(ClassifyGraphicsApi({true,false,true,false,false,true})==0);
    assert(ClassifyGraphicsApi({true,false,false,false,false,false,false})==0);
    GraphicsApiEvidence tgd3d{};tgd3d.tgd3dModule=true;tgd3d.tgd3dDevice=true;
    assert(ClassifyGraphicsApi(tgd3d)==3);
    tgd3d.vulkanDevice=true;assert(ClassifyGraphicsApi(tgd3d)==0); // simultaneous frontends
    tgd3d.vulkanDevice=false;tgd3d.dxvk11=true;assert(ClassifyGraphicsApi(tgd3d)==0);
    tgd3d.dxvk11=false;tgd3d.zinkScreen=true;assert(ClassifyGraphicsApi(tgd3d)==0);
    tgd3d.zinkScreen=false;tgd3d.modulesKnown=false;assert(ClassifyGraphicsApi(tgd3d)==0);
    util_graphics_api_marker first{},second{};util_graphics_api_marker_open(&first,2);util_graphics_api_marker_open(&second,2);
    assert(first.count&&*first.count==2);util_graphics_api_marker_close(&first);assert(*second.count==1);
    util_graphics_api_marker_close(&second);
    pacecar::Config config;config.general.view=pacecar::ViewMode::SwitchBar;
    auto layout=pacecar::overlay::LayoutSettingsFromConfig(config);
    assert(pacecar::overlay::MeasureLayout(layout).height<=58);
    puts("PASS: live SoC overrides nominal frequency; unavailable values clear; battery redraw; backend proof; Mesa marker lifetime; compact bar");
}
