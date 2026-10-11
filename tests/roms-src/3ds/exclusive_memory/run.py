#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independent actual register/RAM oracle for ARM11 exclusive-memory guests."""
import argparse,hashlib,json,os,re,subprocess,zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
def require(ok,why):
 if not ok:raise ValueError(why)
def expected(kind,scenario,raw,flags):
 width=(4,8,1,2)[kind];mask=(1<<(width*8))-1;source=raw*65537;high=0xffffffff-source
 old=0x7654321089abcdef
 loaded=old&mask
 lo=0xaaaaffff if scenario==4 else loaded&0xffffffff
 hi=0x5555ffff if scenario==4 or width!=8 else loaded>>32
 final=old
 if scenario in (0,3):final=(old&~mask)|((source+(high<<32))&mask)
 if scenario==2:final=(old&~0xffffffff)|0xdeadbeef
 return (lo,hi,source,high,int(scenario!=0),0xdf|(0xf8000000 if flags else 0),final&0xffffffff,final>>32,raw,flags)
def grade(path,rom,short=False):
 count=16 if short else 65536;total=count*80;r={'status':'error','observed_snapshots':0,'expected_snapshots':total,'mismatched_snapshots':0,'mismatched_words':0,'groups':[]}
 try:
  with path.open(encoding='ascii') as f:
   require(f.readline()==f'#VELOCE 1 core=3DS rom_crc32={zlib.crc32(rom):08x} channels=3dsdebug\n','identity');require(f.readline()=='VELOCE 1 3ds exclusive_memory\n','suite');require(f.readline()==f'INFO short={int(short)}\n','domain')
   for core in (0,1):
    for kind in range(4):
     for scenario in range(5):
      name=f'arm11_{core}_k{kind}s{scenario}';require(f.readline()==f'BEGIN {name} cases={count*2} words=10\n','group');group={'id':name,'mismatched_snapshots':0,'mismatched_words':0};r['groups'].append(group)
      for raw in range(count):
       for flags in (0,1):
        line=f.readline();require(re.fullmatch(r'REG r=[0-9a-f]{80}\n',line),'row');want=expected(kind,scenario,raw,flags);r['observed_snapshots']+=1
        if line!='REG r='+''.join(f'{x:08x}' for x in want)+'\n':
         got=tuple(int(line[n:n+8],16) for n in range(6,86,8));bad=sum(x!=y for x,y in zip(want,got));r['mismatched_snapshots']+=1;r['mismatched_words']+=bad;group['mismatched_snapshots']+=1;group['mismatched_words']+=bad
         if 'example' not in r:r['example']={'id':name,'raw':raw,'flags':flags,'want':want,'got':got}
    require(f.readline()==f'BLOCK complete arm11_{core}\n','block completion')
   require(f.readline()==f'LOG complete cases={total}\n','completion');require(f.readline()=='END observations_complete\n','terminator');require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',f.readline()),'trailer');require(not f.read(),'trailing data');r['status']='fail' if r['mismatched_words'] else 'pass'
 except (ValueError,OSError,UnicodeError) as e:r['detail']=str(e)
 return r
def main():
 p=argparse.ArgumentParser();p.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce');p.add_argument('--core',type=Path,default=ROOT/'build/bin/cores/3ds.so');p.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/ctr_exclusive');p.add_argument('--controls-only',action='store_true');a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True)
 paths=[a.emulator,a.core,HERE/'build.py',HERE/'exclusive_memory.v3ds',HERE/'recipe.json',HERE.parent/'reversal_registers/build.py',Path(__file__)]+[ROOT/x for x in ('cores/nds/src/arm.cpp','cores/nds/src/arm.hpp','cores/3ds/src/machine.cpp','cores/3ds/src/machine.hpp','cores/3ds/src/exclusive.hpp','cores/3ds/src/plugin.cpp')];hashes={str(x.resolve()):hashlib.sha256(x.read_bytes()).hexdigest() for x in paths};require(build()==(HERE/'exclusive_memory.v3ds').read_bytes(),'archive differs')
 def execute(image,out):
  env=os.environ.copy()
  for key in ('INPUT','INPUT_FILE','DEBUG','TRACE','TRACE_FILE','VELOCE_TEST_RESETS'):env.pop(key,None)
  env.update(HEADLESS='1',FRAMES='150000',VELOCE_TEST_OUT=str(out.resolve()),VELOCE_TEST_EXIT='1')
  proc=subprocess.run([str(a.emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,capture_output=True,text=True,timeout=900);require(proc.returncode==0,proc.stdout+proc.stderr)
 baseline=build(True);image=a.artifacts/'short.v3ds';image.write_bytes(baseline);out=a.artifacts/'short.result';execute(image,out);report=grade(out,baseline,True);require(report['status']=='pass',json.dumps(report));(a.artifacts/'short.json').write_text(json.dumps(report,indent=2)+'\n')
 for name,option,rows,words in (('wrong-status','wrong_status',1280,1280),('missing-clear','missing_clear',256,576)):
  rom=build(True,**{option:True});image=a.artifacts/(name+'.v3ds');image.write_bytes(rom);result=a.artifacts/(name+'.result');execute(image,result);bad=grade(result,rom,True);require(bad['status']=='fail' and bad['mismatched_snapshots']==rows and bad['mismatched_words']==words,name+' '+json.dumps(bad));require(name!='missing-clear' or all(not g['mismatched_words'] for g in bad['groups'] if not g['id'].endswith('s1')),'nonisolated clear fault');(a.artifacts/(name+'.json')).write_text(json.dumps(bad,indent=2)+'\n')
 text=out.read_text();row=next(s for s in text.splitlines(True) if s.startswith('REG r='));faults=[]
 for field in range(10):
  start=6+field*8;bad=row[:start]+f'{int(row[start:start+8],16)^1:08x}'+row[start+8:];faults.append((f'field{field}',text.replace(row,bad,1),'fail'))
 faults.extend((name,bad,'error') for name,bad in (('missing',text.replace(row,'',1)),('bad-row',text.replace(row,'REG r=bad\n',1)),('group',text.replace('BEGIN arm11_0','BEGIN arm11_1',1)),('false-pass',text.replace('END observations_complete','END 0/0')),('trailer',text[:text.index('#VELOCE end ')]),('unterminated',text.rstrip('\n')),('trailing',text+'extra\n')))
 for name,content,status in faults:
  path=a.artifacts/(name+'.result');path.write_text(content);require(grade(path,baseline,True)['status']==status,'file fault escaped '+name)
 if not a.controls_only:
  rom=(HERE/'exclusive_memory.v3ds').read_bytes();out=a.artifacts/'exclusive_memory.result';execute(HERE/'exclusive_memory.v3ds',out);report=grade(out,rom);require(report['status']=='pass' and report['observed_snapshots']==5242880,json.dumps(report))
 require(all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items()),'capture input changed');report.update({'input_sha256':hashes,'executed_controls_rejected':2,'field_faults_rejected':10,'stream_faults_rejected':7,'physical_cache_mmu_timing':False});(a.artifacts/('controls.json' if a.controls_only else 'exclusive_memory.json')).write_text(json.dumps(report,indent=2)+'\n');print(f"ARM11 exclusive memory: {report['observed_snapshots']} ten-word snapshots PASS; 2 executed controls and 17 file faults rejected")
if __name__=='__main__':main()
