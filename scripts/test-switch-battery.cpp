// SPDX-License-Identifier: MIT
#include "pacecar/metrics/SwitchBatteryTelemetry.h"
#include "pacecar/metrics/MetricsFingerprint.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::abort();} } while(0)
int main() {
    using namespace pacecar::metrics;
    SwitchSocMetrics s{};
    SWBAT_TELEMETRY_STATUS b{1,1,4000000,-1000000,-4000000,100,0,0,0x7fff,0xac};
    CHECK(ApplySwitchBatteryTelemetry(s,b,sizeof(b),true));
    CHECK(s.valid&SwitchBatteryPower);CHECK(s.batteryPowerUw==-4000000);
    b.SampleAgeMs=6001;
    CHECK(!ApplySwitchBatteryTelemetry(s,b,sizeof(b),true));
    CHECK(!(s.valid&SwitchBatteryPower) && !s.batteryPowerUw);
    b.SampleAgeMs=0;b.CurrentUa=b.PowerUw=0;
    CHECK(ApplySwitchBatteryTelemetry(s,b,sizeof(b),true) && !s.batteryPowerUw);
    CHECK(!ApplySwitchBatteryTelemetry(s,b,sizeof(b)-4,true));
    CHECK(!ApplySwitchBatteryTelemetry(s,b,sizeof(b),false));
    b.Version=2;CHECK(!ApplySwitchBatteryTelemetry(s,b,sizeof(b),true));
    b.Version=1;b.Flags=0;CHECK(!ApplySwitchBatteryTelemetry(s,b,sizeof(b),true));
    b.Flags=1;b.LastStatus=0xc00000bb;CHECK(!ApplySwitchBatteryTelemetry(s,b,sizeof(b),true));
    MetricsSnapshot frame;frame.soc.valid=SwitchBatteryPower;
    const auto zero=FingerprintSnapshot(frame);
    frame.soc.batteryPowerUw=-1;CHECK(FingerprintSnapshot(frame)!=zero);
    const auto discharge=FingerprintSnapshot(frame);
    frame.soc.batteryPowerUw=-1500000;CHECK(FingerprintSnapshot(frame)!=discharge);
    std::puts("PASS: battery telemetry ABI, negative/zero power, stale/error/old-driver clearing");
}
