#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independent actual guest SRAM/mirror/guard register-file oracle."""
import argparse,hashlib,json,re,subprocess,zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
def require(ok,why):
 if not ok:raise ValueError(why)
def grade(path,rom,full=True):
 offsets=8192 if full else 16;values=256 if full else 16;total=4*offsets*values;report={'status':'error','observed_snapshots':0,'expected_snapshots':total,'mismatched_snapshots':0,'mismatched_words':0}
 try:
  with path.open(encoding='ascii') as f:
   require(f.readline()==f'#VELOCE 1 core=VB rom_crc32={zlib.crc32(rom):08x} channels=v810debug\n','identity');require(f.readline()=='VELOCE 1 vb cartridge ram\n','suite');require(f.readline()==f'INFO full={int(full)} capacity=8192 groups=4\n','domain')
   for group in range(4):
    require(f.readline()==f'BEGIN a{group} cases={offsets*values} words=8\n','group')
    for offset in range(offsets):
     for raw in range(values):
      observed=f.readline();require(re.fullmatch(r'REG r=[0-9a-f]{64}\n',observed),'row')
      signed=raw if raw<128 else raw+4294967040;want=(signed,signed,32783,raw,offset,90,4294967205,3735928559)
      require_line='REG r='+''.join(f'{v:08x}' for v in want)+'\n';report['observed_snapshots']+=1
      if observed!=require_line:
       got=tuple(int(observed[n:n+8],16) for n in range(6,70,8));report['mismatched_snapshots']+=1;report['mismatched_words']+=sum(x!=y for x,y in zip(got,want))
       if 'example' not in report:report['example']={'group':group,'offset':offset,'raw':raw,'want':want,'got':got}
   require(f.readline()==f'LOG complete cases={total}\n','completion');require(f.readline()=='END observations_complete\n','terminator');require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',f.readline()),'trailer');require(not f.read(),'trailing data');report['status']='fail' if report['mismatched_words'] else 'pass'
 except (OSError,ValueError,UnicodeError) as e:report['detail']=str(e)
 return report
def main():
 p=argparse.ArgumentParser();p.add_argument('capture',type=Path);p.add_argument('core',type=Path);p.add_argument('artifacts',type=Path);p.add_argument('--controls-only',action='store_true');a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True)
 paths=[a.capture,a.core,HERE/'build.py',HERE/'recipe.json',HERE/'cartridge_ram.vb',HERE.parent/'immediates/build.py',Path(__file__)]+[ROOT/x for x in ('cores/vb/src/machine.cpp','cores/vb/src/machine.hpp','cores/vb/src/v810.cpp','cores/vb/src/v810.hpp','cores/vb/src/plugin.cpp','cores/vb/src/controller.hpp','tests/cpp/vb_cartridge_capture.cpp')];hashes={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
 require(build()==(HERE/'cartridge_ram.vb').read_bytes(),'archive differs')
 def capture(image,out):
  proc=subprocess.run([str(a.capture.resolve()),str(a.core.resolve()),str(image.resolve()),str(out.resolve())],capture_output=True,text=True,timeout=300);require(proc.returncode==0,proc.stdout+proc.stderr)
 rom=build(False);image=a.artifacts/'short.vb';image.write_bytes(rom);out=a.artifacts/'short.result';capture(image,out);positive=grade(out,rom,False);require(positive['status']=='pass',json.dumps(positive));(a.artifacts/'short.json').write_text(json.dumps(positive,indent=2)+'\n')
 for name,option,rows,words in (('wrong-read','wrong_read',960,960),('missing-store','wrong_store',1023,2046)):
  faulty=build(False,**{option:True});image=a.artifacts/(name+'.vb');image.write_bytes(faulty);result=a.artifacts/(name+'.result');capture(image,result);bad=grade(result,faulty,False);require(bad['status']=='fail' and bad['mismatched_snapshots']==rows and bad['mismatched_words']==words,name+' '+json.dumps(bad));(a.artifacts/(name+'.json')).write_text(json.dumps(bad,indent=2)+'\n')
 text=out.read_text();row=next(s for s in text.splitlines(True) if s.startswith('REG r='));faults=[]
 for field in range(8):
  at=6+field*8;bad=row[:at]+f'{int(row[at:at+8],16)^1:08x}'+row[at+8:];faults.append((f'field{field}',text.replace(row,bad,1),'fail'))
 faults.extend((name,bad,'error') for name,bad in (('missing',text.replace(row,'',1)),('bad-row',text.replace(row,'REG r=bad\n',1)),('group',text.replace('BEGIN a0','BEGIN a1',1)),('false-pass',text.replace('END observations_complete','END 0/0')),('trailer',text[:text.index('#VELOCE end ')]),('unterminated',text.rstrip('\n')),('trailing',text+'extra\n')))
 for name,content,status in faults:
  path=a.artifacts/(name+'.result');path.write_text(content);require(grade(path,rom,False)['status']==status,'file fault escaped '+name)
 report=positive
 if not a.controls_only:
  rom=(HERE/'cartridge_ram.vb').read_bytes();result=a.artifacts/'cartridge_ram.result';capture(HERE/'cartridge_ram.vb',result);report=grade(result,rom,True);require(report['status']=='pass' and report['observed_snapshots']==8388608,json.dumps(report))
  battery=Path(str(result)+'.sav').read_bytes();require(battery==bytes([165])+bytes([90])*8190+bytes([255]),'actual guest battery output differs')
 require(all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items()),'capture inputs changed');report.update({'input_sha256':hashes,'actual_words':report['observed_snapshots']*8,'executed_controls_rejected':2,'field_faults_rejected':8,'stream_faults_rejected':7,'physical_bus_timing':False});(a.artifacts/('controls.json' if a.controls_only else 'cartridge_ram.json')).write_text(json.dumps(report,indent=2)+'\n');print(f"VB SRAM: {report['observed_snapshots']} eight-word snapshots PASS; 2 executed controls and 15 file faults rejected")
if __name__=='__main__':main()
