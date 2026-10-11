#!/usr/bin/env python3
"""Execute rectangle faults against independent RAM/metadata observations."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
FAULTS={
 'inclusive-bottom':('rdp_triangle_shade.hpp','RdpRectangle::decode(packet,false,triangle)','RdpRectangle::decode(packet,true,triangle)'),
 'flip-derivatives':('rdp_rectangle.hpp','if(op==36){','if(op==36 || op==37){'),
 'derivative-scale':('rdp_rectangle.hpp','*2048','*4096'),
 'signed-coordinate':('rdp_rectangle.hpp','raw<32768?int32_t(raw):int32_t(raw)-65536','raw<32768?int32_t(raw):0'),
}

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='c++');p.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/component/n64_rdp_rectangle');args=p.parse_args()
 files={f.name:f.read_bytes() for f in sorted((ROOT/'cores/n64/src').glob('*.hpp'))};results=[]
 for name in ('baseline',*FAULTS):
  changed=dict(files)
  if name!='baseline':
   filename,old,new=FAULTS[name];body=changed[filename].decode();assert body.count(old)>=1,name;changed[filename]=body.replace(old,new).encode()
  directory=(args.artifacts/name).resolve();directory.mkdir(parents=True,exist_ok=True)
  for filename,content in changed.items():(directory/filename).write_bytes(content)
  binary=directory/'test';compiled=subprocess.run([args.compiler,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(directory),str(ROOT/'tests/cpp/n64_rdp_rectangle_test.cpp'),'-o',str(binary)],capture_output=True,text=True,timeout=90);(directory/'compile.log').write_text(compiled.stdout+compiled.stderr)
  if compiled.returncode:raise RuntimeError('Compilation is not a mutation rejection: '+name+' '+compiled.stderr)
  result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=90);(directory/'execution.log').write_text(result.stdout+result.stderr)
  if name=='baseline':assert result.returncode==0 and result.stdout.endswith('PASS\n'),name
  else:assert result.returncode==1 and result.stderr=='rectangle observation mismatch\n',name
  results.append(dict(name=name,returncode=result.returncode,status='pass' if name=='baseline' else 'rejected'))
 report=dict(status='pass',production_source_sha256=hashlib.sha256(b''.join(files.values())).hexdigest(),test_sha256=hashlib.sha256((ROOT/'tests/cpp/n64_rdp_rectangle_test.cpp').read_bytes()).hexdigest(),results=results);(args.artifacts/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('N64 RDP rectangles: baseline passes; four executed production faults rejected')
if __name__=='__main__':main()
