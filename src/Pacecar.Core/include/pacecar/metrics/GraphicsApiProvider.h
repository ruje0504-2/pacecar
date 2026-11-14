#pragma once
#include "pacecar/metrics/IMetricProvider.h"
#include <functional>
#include <unordered_map>
#include <string>
namespace pacecar::metrics {
// Concrete backend telemetry wins over module identities. Plain D3D/OpenGL
// modules deliberately produce Unknown. A multi-driver Mesa DLL is not Zink proof.
struct GraphicsApiEvidence {bool vulkanDevice=false,zinkScreen=false,dxvk9=false,dxvk10=false,dxvk11=false;};
inline std::uint32_t ClassifyGraphicsApi(GraphicsApiEvidence e) noexcept {
    if(e.zinkScreen && (e.dxvk9||e.dxvk10||e.dxvk11))return 0;
    if(e.zinkScreen)return 2;
    if(e.dxvk10)return 10;
    if(e.dxvk9&&!e.dxvk11)return 9;
    if(e.dxvk11&&!e.dxvk9)return 11;
    if(e.dxvk9&&e.dxvk11)return 0; // more than one frontend, do not guess
    return e.vulkanDevice?1u:0u;
}
class GraphicsApiProvider final:public IMetricProvider {
public:
    explicit GraphicsApiProvider(std::function<std::uint32_t()> target):target_(std::move(target)){}
    const char* Name()const noexcept override{return "Graphics backend evidence";}
    std::uint32_t Domains()const noexcept override{return 0;}
    std::chrono::milliseconds Cadence()const noexcept override{return std::chrono::milliseconds(2000);}
    HRESULT Poll(MetricsSnapshot& snapshot)override;
private:
    std::function<std::uint32_t()> target_;
    std::uint32_t cachedPid_=0;
    std::unordered_map<std::wstring,bool> dxvkProof_;
};
}
