#include "pacecar/metrics/SwitchSocProvider.h"
#include "pacecar/metrics/SwitchPowerIoctl.h"
#include <array>
#include <utility>

namespace pacecar::metrics {
namespace {
class Win32SwitchSocSource final : public ISwitchSocSource {
public:
    SwitchSocMetrics Read() override {
        SwitchSocMetrics s;
        SYSTEM_POWER_STATUS battery{};
        if (GetSystemPowerStatus(&battery) && battery.BatteryLifePercent<=100 && !(battery.BatteryFlag & 128)) {
            s.valid|=SwitchBattery;s.batteryPercent=battery.BatteryLifePercent;
            s.externalPower=battery.ACLineStatus==1;
            s.charging=battery.BatteryFlag!=255 && (battery.BatteryFlag & 8)!=0;
        }
        HANDLE h=CreateFileW(L"\\\\.\\SwitchBacklight",GENERIC_READ,
            FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        SWFAN_POWER_STATUS power{};DWORD bytes=0;
        if (h!=INVALID_HANDLE_VALUE) {
            BOOL ok=DeviceIoControl(h,IOCTL_SWFAN_GET_POWER_STATUS,nullptr,0,&power,sizeof(power),&bytes,nullptr);
            s.error=ok?ERROR_SUCCESS:GetLastError();CloseHandle(h);
            if (ok && bytes>=19*sizeof(DWORD) && power.IsLite && (power.Version==1 || power.Version==2)) {
                s.nativeBridge=true;s.cpuKhz=power.CpuActualKhz;s.gpuKhz=power.GpuReportedKhz;
                s.ramKhz=power.RamActualKhz;s.fanPwm=power.FanDuty;s.cpuMv=power.CpuMv;s.gpuUv=power.GpuReportedUv;
                if(s.cpuKhz)s.valid|=SwitchCpu;
                if(s.ramKhz)s.valid|=SwitchRam;
                if(s.fanPwm<=236)s.valid|=SwitchFan;
                if(power.Version==2 && bytes>=sizeof(power)) {
                    s.gpuSampleAgeMs=power.GpuSampleAgeMs;
                    if(s.gpuKhz && s.gpuSampleAgeMs<=2000 && (power.Flags&SWFAN_POWER_FLAG_LIVE_GPU_COUNTER))s.valid|=SwitchGpu;
                    if(power.TempValid && power.TempSampleAgeMs<=6000) {
                        s.socTempC=power.SocTempC;s.boardTempC=power.BoardTempC;
                        s.valid|=SwitchSocTemp|SwitchBoardTemp;
                    }
                }
                return s;
            }
        } else s.error=GetLastError();
        // Fan 13 updates CPU/PWM/TMP451 registry diagnostics every two seconds.
        // No boot GPU value is promoted into a live GPU clock measurement.
        HKEY key=nullptr;
        if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Services\\switchfan\\Parameters",0,KEY_READ,&key)==ERROR_SUCCESS) {
            FILETIME write{},now{};ULARGE_INTEGER a{},b{};
            bool fresh=false;
            if(RegQueryInfoKeyW(key,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&write)==ERROR_SUCCESS) {
                GetSystemTimeAsFileTime(&now);a.LowPart=now.dwLowDateTime;a.HighPart=now.dwHighDateTime;
                b.LowPart=write.dwLowDateTime;b.HighPart=write.dwHighDateTime;
                fresh=a.QuadPart>=b.QuadPart && a.QuadPart-b.QuadPart<=60000000ULL;
            }
            const auto value=[key](const wchar_t* name,std::uint32_t& out) {
                DWORD size=sizeof(out),type=0;
                return RegQueryValueExW(key,name,nullptr,&type,reinterpret_cast<BYTE*>(&out),&size)==ERROR_SUCCESS && type==REG_DWORD && size==sizeof(out);
            };
            if(fresh) {
                if(value(L"CpuActualKhz",s.cpuKhz)&&s.cpuKhz)s.valid|=SwitchCpu;
                if(value(L"Duty",s.fanPwm)&&s.fanPwm<=236)s.valid|=SwitchFan;
                if(value(L"TempSoC",s.socTempC))s.valid|=SwitchSocTemp;
                if(value(L"TempBoard",s.boardTempC))s.valid|=SwitchBoardTemp;
                value(L"CpuMv",s.cpuMv);
            }
            if(value(L"RamActualKhz",s.ramKhz)&&s.ramKhz)s.valid|=SwitchRam;
            RegCloseKey(key);
        }
        return s;
    }
};
}
std::unique_ptr<ISwitchSocSource> MakeSwitchSocSource(){return std::make_unique<Win32SwitchSocSource>();}
SwitchSocProvider::SwitchSocProvider(std::unique_ptr<ISwitchSocSource> source):source_(std::move(source)){}
HRESULT SwitchSocProvider::Poll(MetricsSnapshot& snapshot){
    snapshot.soc=source_?source_->Read():SwitchSocMetrics{};++tick_;
    const auto& s=snapshot.soc;
    // Windows' nominal 1.00 GHz is not T210B01's hardware clock.
    snapshot.cpu.totalFrequencyMhz=(s.valid&SwitchCpu)?s.cpuKhz/1000.0:0;
    for(auto& core:snapshot.cpu.cores)core.frequencyMhz=snapshot.cpu.totalFrequencyMhz;
    snapshot.gpu.clockStatus={(s.valid&SwitchGpu)!=0,(s.valid&SwitchGpu)==0,tick_};
    snapshot.gpu.coreClockMhz=(s.valid&SwitchGpu)?s.gpuKhz/1000.0:0;
    // Unified RAM has its own EMC field; do not call it dedicated VRAM clock.
    snapshot.gpu.memoryClockMhz=0;
    // TMP451 remote is SoC, not an independent GPU junction/package sensor.
    snapshot.cpu.temperatureStatus={};snapshot.gpu.temperatureStatus={};
    if(s.valid&SwitchBoardTemp){snapshot.board.mainboardTemperatureC=s.boardTempC;snapshot.board.status={true,false,tick_};}
    else snapshot.board.status={};
    return S_OK;
}
}
