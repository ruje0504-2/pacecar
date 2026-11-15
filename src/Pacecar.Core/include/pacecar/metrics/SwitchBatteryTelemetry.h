// SPDX-License-Identifier: MIT
#pragma once
#include "SwitchBatteryIoctl.h"
#include "MetricsSnapshot.h"
namespace pacecar::metrics {
inline bool ApplySwitchBatteryTelemetry(SwitchSocMetrics& s,
    const SWBAT_TELEMETRY_STATUS& v, std::size_t bytes, bool ok)
{
    s.valid&=~SwitchBatteryPower;
    s.batteryVoltageUv=0;s.batteryCurrentUa=s.batteryPowerUw=0;
    s.batterySampleAgeMs=~0u;
    if (!ok || bytes<sizeof(v) || v.Version!=SWBAT_TELEMETRY_VERSION) return false;
    s.batterySampleAgeMs=v.SampleAgeMs;s.batteryPowerError=v.LastStatus;
    if (!(v.Flags&SWBAT_TELEMETRY_VALID) || v.LastStatus ||
        v.SampleAgeMs>SWBAT_TELEMETRY_MAX_AGE_MS || !v.VoltageUv) return false;
    s.batteryVoltageUv=v.VoltageUv;s.batteryCurrentUa=v.CurrentUa;
    s.batteryPowerUw=v.PowerUw;s.valid|=SwitchBatteryPower;
    return true;
}
}
