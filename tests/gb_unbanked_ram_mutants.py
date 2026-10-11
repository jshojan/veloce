#!/usr/bin/env python3
"""Compile and execute faults in the actual unbanked RAM implementation."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='c++');p.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/component/gb_unbanked_ram');args=p.parse_args()
 source=ROOT/'cores/gb/src/mbc/mbc.cpp';body=source.read_text();test=ROOT/'tests/cpp/gb_unbanked_ram_test.cpp'
 faults={'disabled-read':('return address < 0x2000 &&','return m_ram_enabled && address < 0x2000 &&',1),'aliased-address':('m_ram[address]','m_ram[address & 255]',2)};results=[]
 for name in ('baseline',*faults):
  directory=(args.artifacts/name).resolve();directory.mkdir(parents=True,exist_ok=True);changed=body
  if name!='baseline':
   old,new,count=faults[name];assert body.count(old)==count;changed=body.replace(old,new)
  path=directory/'mbc.cpp';path.write_text(changed);binary=directory/'test'
  command=[args.compiler,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(ROOT/'cores/gb/src'),'-I'+str(source.parent),str(test),str(ROOT/'cores/gb/src/cartridge.cpp'),str(path),*[str(source.parent/(name+'.cpp')) for name in ('mbc1','mbc3','mbc5')],'-o',str(binary)]
  r=subprocess.run(command,capture_output=True,text=True,timeout=90);(directory/'compile.log').write_text(r.stdout+r.stderr)
  if r.returncode:raise RuntimeError('Compilation is not a mutation rejection: '+name+' '+r.stderr)
  r=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30);(directory/'execution.log').write_text(r.stdout+r.stderr)
  if name=='baseline':assert r.returncode==0 and r.stdout.endswith('PASS\n')
  else:assert r.returncode==1 and 'unbanked RAM observation mismatch\n' in r.stderr
  results.append(dict(name=name,returncode=r.returncode,status='pass' if name=='baseline' else 'rejected'))
 args.artifacts.mkdir(parents=True,exist_ok=True);(args.artifacts/'report.json').write_text(json.dumps(dict(status='pass',source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),test_sha256=hashlib.sha256(test.read_bytes()).hexdigest(),results=results),indent=2)+'\n');print('GB unbanked RAM: baseline passes; two compiled/executed faults rejected')
if __name__=='__main__':main()
