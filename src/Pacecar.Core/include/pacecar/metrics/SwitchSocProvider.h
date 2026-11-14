#pragma once
// SwitchFan public ABI is mirrored in SwitchPowerIoctl.h from WindowsDrivers;
// no raw MMIO or vendor desktop sensor driver runs inside this application.
#include "pacecar/metrics/IMetricProvider.h"
#include <memory>
namespace pacecar::metrics {
class ISwitchSocSource {
public:
    virtual ~ISwitchSocSource()=default;
    virtual SwitchSocMetrics Read()=0;
};
std::unique_ptr<ISwitchSocSource> MakeSwitchSocSource();
class SwitchSocProvider final : public IMetricProvider {
public:
    explicit SwitchSocProvider(std::unique_ptr<ISwitchSocSource> source=MakeSwitchSocSource());
    const char* Name() const noexcept override {return "Switch T210B01 hardware";}
    std::uint32_t Domains() const noexcept override {return 0;} // enrichment, not GPU utilization
    std::chrono::milliseconds Cadence() const noexcept override {return std::chrono::milliseconds(1000);}
    HRESULT Poll(MetricsSnapshot& snapshot) override;
private:
    std::unique_ptr<ISwitchSocSource> source_;
    std::uint64_t tick_=0;
};
}
