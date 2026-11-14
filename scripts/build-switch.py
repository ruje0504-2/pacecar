#!/usr/bin/env python3
"""Cross-build the native Pacecar sources for Windows ARM64 (or x64 controls).
No elevated x86 sensor helper is shipped. The Switch provider uses its existing
read-only driver interface. Requires LLVM-MinGW and nlohmann/json headers.
"""
import argparse,concurrent.futures,hashlib,json,os,subprocess,sys
from pathlib import Path
import xml.etree.ElementTree as ET
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--deps',type=Path,required=True)
p.add_argument('--toolchain',type=Path,default=Path(os.environ.get('MINGW_BIN','/Users/rujian/.local/share/mesa-switch-woa-build/toolchain/llvm-mingw-20261104-ucrt-macos-universal/bin')))
p.add_argument('--wdk-include',type=Path,default=Path(os.environ.get('WDK_INCLUDE','/Users/rujian/.local/share/mesa-switch-woa-build/wdk/c/Include/10.0.26100.0')))
p.add_argument('--sdk-include',type=Path,default=Path(os.environ.get('SDK_INCLUDE','/Users/rujian/.local/share/mesa-switch-woa-build/sdk/c/Include/10.0.26100.0')))
p.add_argument('--arch',choices=['aarch64','x86_64'],default='aarch64');a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
compiler=a.toolchain/(a.arch+'-w64-mingw32-clang++')
common=[str(compiler),'-std=c++20','-O2','-g0','-fms-extensions','-DUNICODE','-D_UNICODE','-DNOMINMAX','-DWIN32_LEAN_AND_MEAN','-D_WIN32_WINNT=0x0A00','-D_WIN32_IE=0x0A00','-DPACECAR_SWITCH=1','-I'+str(root/'src/Pacecar.Core/include'),'-I'+str(a.deps.resolve()),'-idirafter',str(a.wdk_include/'shared'),'-idirafter',str(a.sdk_include/'shared'),'-idirafter',str(a.sdk_include/'um'),'-Wno-ignored-attributes','-Wno-unknown-pragmas']
sources=[]
for n in ['Core','Overlay']:
 project=root/'src'/('Pacecar.'+n)/('Pacecar.'+n+'.vcxproj')
 for item in ET.parse(project).findall('.//{*}ClCompile'):
  if 'Include' in item.attrib: sources.append(project.parent/item.attrib['Include'].replace('\\','/'))
def compile_one(s):
 obj=out/(str(s.relative_to(root)).replace('/','_')+'.o');cmd=common+['-c',str(s),'-o',str(obj)]
 r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 (obj.with_suffix('.log')).write_text(r.stdout)
 return obj,r.returncode,r.stdout
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: results=list(pool.map(compile_one,sources))
errors=[r for r in results if r[1]]
print('Compiled',len(results)-len(errors),'/',len(results),flush=True)
if errors:
 for f,c,msg in errors: print(f.name+'\n'+msg[:2500])
 sys.exit(1)
windres=a.toolchain/(a.arch+'-w64-mingw32-windres');res=out/'resources.o'
rc=out/'pacecar-switch.rc'
rc_text=(root/'src/Pacecar.Overlay/pacecar.rc').read_text().replace('..\\\\..\\\\assets\\\\app.ico',str(root/'assets/app.ico'))
rc_text+='\n1 24 "'+str(root/'src/Pacecar.Overlay/app.manifest')+'"\n'
rc.write_text(rc_text)
subprocess.run([str(windres),'-i',str(rc),'-o',str(res),'-I'+str(root/'src/Pacecar.Overlay')],cwd=root/'src/Pacecar.Overlay',check=True)
libs=['d2d1','dwrite','d3d11','dxgi','dcomp','windowscodecs','shcore','psapi','wtsapi32','dwmapi','shell32','comctl32','user32','gdi32','ole32','oleaut32','uuid','advapi32','pdh','powrprof','iphlpapi','ws2_32','ntdll','winmm','version']
exe=out/'Pacecar.Switch.exe'
cmd=[str(compiler),'-static','-municode','-mwindows',*[str(r[0]) for r in results],str(res),*['-l'+l for l in libs],'-o',str(exe)]
r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);(out/'link.log').write_text(r.stdout)
if r.returncode: print(r.stdout);sys.exit(r.returncode)
subprocess.run([str(a.toolchain/(a.arch+'-w64-mingw32-strip')),str(exe)],check=True)
(out/'manifest.json').write_text(json.dumps({'arch':a.arch,'sources':len(sources),'sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'file':str(exe)},indent=2)+'\n')
print(exe,flush=True)
