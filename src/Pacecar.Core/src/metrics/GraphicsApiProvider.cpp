#include "pacecar/metrics/GraphicsApiProvider.h"
#include <tlhelp32.h>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace pacecar::metrics {
namespace {
bool ActiveMesaMarker(DWORD pid,unsigned kind){
    wchar_t name[96];swprintf_s(name,L"Local\\Mesa.GraphicsApi.v1.%lu.%u",pid,kind);
    HANDLE h=OpenFileMappingW(FILE_MAP_READ,FALSE,name);if(!h)return false;
    const volatile LONG* count=static_cast<const volatile LONG*>(MapViewOfFile(h,FILE_MAP_READ,0,0,sizeof(LONG)));
    bool active=count&&*count>0;
    if(count)UnmapViewOfFile(const_cast<const LONG*>(count));CloseHandle(h);return active;
}
bool IsDxvk(const wchar_t* path){
    DWORD unused=0,size=GetFileVersionInfoSizeW(path,&unused);
    if(size && size<1024*1024){
        std::vector<BYTE> data(size);
        if(GetFileVersionInfoW(path,0,size,data.data())){
            struct Translation {WORD language,codepage;};Translation* translations=nullptr;UINT bytes=0;
            if(VerQueryValueW(data.data(),L"\\VarFileInfo\\Translation",reinterpret_cast<void**>(&translations),&bytes)){
                for(unsigned i=0;i<bytes/sizeof(Translation);++i){
                    for(const wchar_t* field:{L"ProductName",L"FileDescription"}){
                        wchar_t key[96];swprintf_s(key,L"\\StringFileInfo\\%04x%04x\\%s",translations[i].language,translations[i].codepage,field);
                        wchar_t* value=nullptr;UINT chars=0;
                        if(VerQueryValueW(data.data(),key,reinterpret_cast<void**>(&value),&chars)&&value&&wcsstr(value,L"DXVK"))return true;
                    }
                }
            }
        }
    }
    // DXVK builds without VERSIONINFO retain their exact initialization/version
    // log or DXVK config key strings. Never infer from d3d11.dll's name alone.
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER len{};bool found=false;
    if(GetFileSizeEx(file,&len)&&len.QuadPart>0&&len.QuadPart<64*1024*1024){
        HANDLE map=CreateFileMappingW(file,nullptr,PAGE_READONLY,0,0,nullptr);
        if(map){const char* p=static_cast<const char*>(MapViewOfFile(map,FILE_MAP_READ,0,0,0));
            if(p){const std::string_view bytes(p,static_cast<std::size_t>(len.QuadPart));
                found=bytes.find("DXVK: ")!=bytes.npos ||
                    (bytes.find("DXVK_STATE_CACHE")!=bytes.npos && bytes.find("dxvk.conf")!=bytes.npos);
                UnmapViewOfFile(p);}
            CloseHandle(map);}
    }
    CloseHandle(file);return found;
}
}
HRESULT GraphicsApiProvider::Poll(MetricsSnapshot& snapshot){
    const DWORD pid=target_?target_():0;snapshot.graphicsApiPid=pid;snapshot.graphicsApi=0;
    if(!pid||pid==GetCurrentProcessId())return S_OK;
    if(pid!=cachedPid_){dxvkProof_.clear();cachedPid_=pid;}
    GraphicsApiEvidence e;e.zinkScreen=ActiveMesaMarker(pid,2);e.vulkanDevice=ActiveMesaMarker(pid,1);
    HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if(h!=INVALID_HANDLE_VALUE){MODULEENTRY32W m{};m.dwSize=sizeof(m);
        if(Module32FirstW(h,&m))do{
            const bool nine=_wcsicmp(m.szModule,L"d3d9.dll")==0;
            const bool ten=_wcsicmp(m.szModule,L"d3d10core.dll")==0||_wcsicmp(m.szModule,L"d3d10.dll")==0;
            const bool eleven=_wcsicmp(m.szModule,L"d3d11.dll")==0;
            if(nine||ten||eleven){
                auto [proof,inserted]=dxvkProof_.try_emplace(m.szExePath,false);
                if(inserted)proof->second=IsDxvk(m.szExePath);
                if(proof->second){e.dxvk9|=nine;e.dxvk10|=ten;e.dxvk11|=eleven;}
            }
            // A separately loaded zink_dri.dll is also an explicit driver identity.
            if(_wcsicmp(m.szModule,L"zink_dri.dll")==0)e.zinkScreen=true;
        }while(Module32NextW(h,&m));CloseHandle(h);}
    snapshot.graphicsApi=ClassifyGraphicsApi(e);return S_OK;
}
}
