#!/usr/bin/env python3
"""Execute Joybus phase faults against independent RAM/metadata observations."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
FAULTS={
 'stale-read':('if(raw&192)continue;','if(!(ram[63]&1))return; if(raw&192)continue;'),
 'port-alias':('wire(input[port])','wire(input[0])'),
 'format-disabled':('if(!(ram[63]&1))return;','if(true)return;'),
 'format-unconditional':('if(!(ram[63]&1))return;','if(false)return;'),
 'retained-old-layout':('reset();unsigned at=0,channel=0;','unsigned at=0,channel=0;'),
 'padding-end':('if(marker==255){++at;continue;}','if(marker==255){break;}'),
 'tx-flags-ignored':('if(raw&192)continue;','if(raw&0)continue;'),
 'stale-rx-error':('ram[at+1]=uint8_t(rx);','ram[at+1]|=uint8_t(rx);'),
 'start-wire':('32768,16384,8192,4096,2048','32768,16384,8192,2048,2048'),
}

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='c++');p.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/component/n64_pif_joybus');args=p.parse_args()
 files={f.name:f.read_bytes() for f in sorted((ROOT/'cores/n64/src').glob('*.hpp'))};files.update({name:(ROOT/'cores/n64/src'/name).read_bytes() for name in ('machine.cpp','vr4300.cpp')});results=[]
 for name in ('baseline',*FAULTS,'si-baseline','si-no-poll'):
  changed=dict(files)
  if name=='si-no-poll':
   body=changed['machine.cpp'].decode();old='joybus.poll(pif,{input_buttons,other_input_buttons[0],other_input_buttons[1],other_input_buttons[2]});';assert body.count(old)==1;changed['machine.cpp']=body.replace(old,'(void)0;').encode()
  elif name not in ('baseline','si-baseline'):
   filename="pif_joybus.hpp";old,new=FAULTS[name];body=changed[filename].decode();assert body.count(old)==1,name;changed[filename]=body.replace(old,new).encode()
  directory=(args.artifacts/name).resolve();directory.mkdir(parents=True,exist_ok=True)
  for filename,content in changed.items():(directory/filename).write_bytes(content)
  binary=directory/'test';si=name.startswith('si-');command=[args.compiler,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(directory),'-I'+str(ROOT/'include'),str(ROOT/('tests/cpp/n64_pif_state_test.cpp' if si else 'tests/cpp/n64_pif_joybus_test.cpp'))];command+=([str(directory/'machine.cpp'),str(directory/'vr4300.cpp')] if si else []);command+=['-o',str(binary)];compiled=subprocess.run(command,capture_output=True,text=True,timeout=90);(directory/'compile.log').write_text(compiled.stdout+compiled.stderr)
  if compiled.returncode:raise RuntimeError('Compilation is not a mutation rejection: '+name+' '+compiled.stderr)
  result=subprocess.run([str(binary)]+([str(ROOT/'tests/roms-src/n64/controller_cached_reads/controller_cached_reads.z64')] if si else []),capture_output=True,text=True,timeout=90);(directory/'execution.log').write_text(result.stdout+result.stderr)
  if name in ('baseline','si-baseline'):assert result.returncode==0 and result.stdout.endswith('PASS\n'),name
  else:assert result.returncode==1 and result.stderr==('PIF state observation mismatch\n' if si else 'Joybus observation mismatch\n'),name
  results.append(dict(name=name,returncode=result.returncode,status='pass' if name in ('baseline','si-baseline') else 'rejected'))
 report=dict(status='pass',production_source_sha256=hashlib.sha256(b''.join(files.values())).hexdigest(),test_sha256=hashlib.sha256(b''.join((ROOT/'tests/cpp'/name).read_bytes() for name in ('n64_pif_joybus_test.cpp','n64_pif_state_test.cpp'))).hexdigest(),results=results);(args.artifacts/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('N64 PIF Joybus: baseline passes; ten executed production faults rejected')
if __name__=='__main__':main()
