#!/usr/bin/env python3
"""Execute copy-path faults against independent RAM/metadata observations."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
FAULTS={
 'odd-row-bank':('rdp_texture_copy.hpp','((row&1)?8u:0u)','0u'),
 'palette-lane':('rdp_texture_copy.hpp','1024+index*4+lane','1024+index*4'),
 'clamp-copy':('rdp_texture_copy.hpp','if(!tile.mask(t))return value;','if(tile.clamp(t)){return 0;} if(!tile.mask(t))return value;'),
 'inclusive-bottom':('rdp_rectangle.hpp','inclusive?3u:0u','inclusive?0u:0u'),
 'flip-derivatives':('rdp_rectangle.hpp','if(op==36){','if(op==36 || op==37){'),
 'alpha16-bit':('rdp_copy.hpp','(data>>(48-lane*16))&1','(data>>(49-lane*16))&1'),
 'alpha8-half':('rdp_copy.hpp','(data>>(24-lane*8))&255','(data>>(56-lane*8))&255'),
 'alpha8-threshold':('rdp_copy.hpp','>=(threshold&255)','>(threshold&255)'),
 'burst-step':('rdp_copy.hpp','value[lane]+=step[lane]','value[lane]+=step[lane]*2'),
 'history-entry':('hidden_rdram.hpp','history_&=uint16_t(~514u);','history_&=uint16_t(~0u);'),
}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='c++');p.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/component/n64_rdp_copy');args=p.parse_args()
 files={f.name:f.read_bytes() for f in sorted((ROOT/'cores/n64/src').glob('*.hpp'))};results=[]
 for name in ('baseline',*FAULTS):
  changed=dict(files)
  if name!='baseline':
   filename,old,new=FAULTS[name];body=changed[filename].decode();assert body.count(old)==1,name;changed[filename]=body.replace(old,new).encode()
  directory=(args.artifacts/name).resolve();directory.mkdir(parents=True,exist_ok=True)
  for filename,content in changed.items():(directory/filename).write_bytes(content)
  binary=directory/'test';compiled=subprocess.run([args.compiler,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(directory),str(ROOT/'tests/cpp/n64_rdp_copy_test.cpp'),'-o',str(binary)],capture_output=True,text=True,timeout=90);(directory/'compile.log').write_text(compiled.stdout+compiled.stderr)
  if compiled.returncode:raise RuntimeError('Compilation is not a mutation rejection: '+name+' '+compiled.stderr)
  result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=90);(directory/'execution.log').write_text(result.stdout+result.stderr)
  if name=='baseline':assert result.returncode==0 and result.stdout.endswith('PASS\n'),name
  else:assert result.returncode==1 and result.stderr=='copy observation mismatch\n',name
  results.append(dict(name=name,returncode=result.returncode,status='pass' if name=='baseline' else 'rejected'))
 report=dict(status='pass',production_source_sha256=hashlib.sha256(b''.join(files.values())).hexdigest(),test_sha256=hashlib.sha256((ROOT/'tests/cpp/n64_rdp_copy_test.cpp').read_bytes()).hexdigest(),results=results);(args.artifacts/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('N64 RDP copy: baseline passes; ten executed production faults rejected')
if __name__=='__main__':main()
