#!/usr/bin/env python3
"""Independently grade original DS Thumb stack/block register snapshots."""
import argparse
from dataclasses import asdict
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib
from build import build,groups,seeds,cases,DATA
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
MASK=(1<<32)-1
def require(ok,message):
    if not ok: raise ValueError(message)
def locations(rom,cpu,full=True):
    off=0x20 if cpu==9 else 0x30
    file=int.from_bytes(rom[off:off+4],'little'); base=int.from_bytes(rom[off+8:off+12],'little'); size=int.from_bytes(rom[off+12:off+16],'little')
    segment=rom[file:file+size]
    positions=[]
    for marker in (b'VTGT0001',b'VSTK0001'):
        at=segment.find(marker); require(at>=0 and at==segment.rfind(marker),'missing/duplicate code marker'); positions.append(base+at+8)
    require(positions[1]+len(groups(full))*20<=base+size,'truncated thunks')
    return positions
def expected(g,seed,flags,cpu,index,locations):
    targets,thunks=locations
    regs=[seed^((n+1)*0x01010101) for n in range(8)]
    lr=seed^(15*0x01010101)
    words=[(~seed&MASK)^((n+17)*0x01010101) for n in range(11)]
    selected=[reg for reg in range(8) if g.mask&(1<<reg)]
    sp=DATA+g.count*4 if g.kind==0 else DATA
    target_pc=(thunks+index*20+16)|1
    if g.kind>=2: regs[g.base]=DATA
    if g.kind==1 and g.mask&256:
        slot=(0,1,3).index(g.target)
        target=targets+slot*16+(2 if g.target==3 else 0)
        words[g.count]=target|int(g.target!=0)
        target_pc=target+(8 if cpu==9 and g.target==0 else 4)
    if g.kind in (0,2):
        payload=[regs[reg] for reg in selected]+([lr] if g.kind==0 and g.mask&256 else [])
        words[1:1+len(payload)]=payload
    else:
        for ordinal,reg in enumerate(selected,1): regs[reg]=words[ordinal]
    if g.kind<2: sp=DATA if g.kind==0 else DATA+g.count*4
    elif g.kind==2 or not g.mask&(1<<g.base): regs[g.base]=DATA+g.count*4
    return (*regs,sp,lr,0xdf|(flags<<(27 if cpu==9 else 28)),target_pc,seed,g.instruction,*words)
def references():
    domain=groups(); require(len(domain)==4845 and sum(cases(cpu) for cpu in (9,7))==3720960,'full domain changed')
    require(len(groups(False))==100 and sum(cases(cpu,False) for cpu in (9,7))==19200,'control domain changed')
    for kind in range(4):
        for base in range(1 if kind<2 else 8):
            actual={g.mask for g in domain if g.kind==kind and g.base==base}
            want={mask for mask in range(1,512 if kind<2 else 256) if not(kind==2 and mask&(1<<base) and mask&((1<<base)-1))}
            require(actual==want,'register list/base coverage changed')
    from build import Group
    loc=(0x02003000,0x02004000)
    r=expected(Group(0,0,0x101),0,0,7,0,loc)
    require(r[:2]==(0x01010101,0x02020202) and r[8:14]==(DATA,0x0f0f0f0f,0xdf,0x02004011,0,0xb501) and r[15:17]==(0x01010101,0x0f0f0f0f),'push order/LR reference failed')
    r=expected(Group(3,2,0x84),0x89abcdef,31,9,0,loc)
    require(r[2]==((~0x89abcdef&MASK)^0x12121212) and r[7]==((~0x89abcdef&MASK)^0x13131313) and r[10]==0xf80000df,'loaded-base suppression/Q reference failed')
    for cpu in (9,7):
        for form in (0,1,3):
            r=expected(Group(1,0,0x100,form),0,0,cpu,0,loc)
            require(r[8]==DATA+4 and r[11]==loc[0]+(0,16,34)[(0,1,3).index(form)]+(8 if cpu==9 and form==0 else 4),'POP target reference failed')
def evaluate(path,rom,full=True):
    domain=groups(full); total=sum(cases(c,full) for c in (9,7))
    report=dict(status='error',console='nds',suite='multiple_registers',expected_cases=total,observed_cases=0,graded_cases=0,mismatches=0,groups=[],examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=DS rom_crc32={zlib.crc32(rom):08x} channels=dsdebug\n','wrong file identity')
            for cpu in (9,7):
                code=locations(rom,cpu,full)
                require(f.readline()==f'VELOCE 1 ds multiple_registers cpu={cpu}\n','wrong CPU/suite order')
                require(f.readline()==f'INFO full={int(full)} groups={len(domain)} flags={32 if cpu==9 else 16}\n','wrong domain')
                for index,g in enumerate(domain):
                    require(f.readline()==f'BEGIN cpu{cpu}_g{index:05d} cases={len(seeds(full))*(32 if cpu==9 else 16)} words=25\n','missing/reordered group')
                    group=dict(id=f'cpu{cpu}_g{index:05d}',instruction=asdict(g),observed_cases=0,mismatches=0); report['groups'].append(group)
                    for seed in seeds(full):
                        for flags in range(32 if cpu==9 else 16):
                            line=f.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{200}\n',line),'missing/malformed row')
                            got=tuple(int(line[n:n+8],16) for n in range(6,206,8)); want=expected(g,seed,flags,cpu,index,code)
                            report['observed_cases']+=1; report['graded_cases']+=1; group['observed_cases']+=1
                            if got!=want:
                                report['mismatches']+=1; group['mismatches']+=1
                                if len(report['examples'])<8: report['examples'].append(dict(cpu=cpu,group=index,seed=f'{seed:08x}',flags=flags,expected=[f'{v:08x}' for v in want],observed=[f'{v:08x}' for v in got]))
                require(f.readline()==f'LOG cpu={cpu} complete cases={cases(cpu,full)}\n','missing CPU completion')
            require(f.readline()=='END observations_complete\n','false END')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',f.readline()),'missing/ungraded trailer')
            require(not f.read(),'trailing data'); report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as e: report['detail']=str(e)
    return report
def run(emulator,image,result):
    env=os.environ.copy()
    for key in ('INPUT','INPUT_FILE','DEBUG','TRACE','TRACE_FILE','VELOCE_TEST_RESETS'):
        env.pop(key,None)
    env.update(HEADLESS='1',FRAMES='1000000',VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT='1')
    proc=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,
                        capture_output=True,text=True,timeout=2400)
    require(proc.returncode==0,proc.stdout+proc.stderr)


def controls(emulator,artifacts):
    baseline=build(False); image=artifacts/'control.nds'; image.write_bytes(baseline)
    path=artifacts/'control.result'; run(emulator,image,path); report=evaluate(path,baseline,False)
    require(report['status']=='pass','reduced positive failed: '+json.dumps(report))
    (artifacts/'control.json').write_text(json.dumps(report,indent=2)+'\n')
    for name,options,kinds in (('missing-store',dict(skip_store=True),(0,2)),('missing-load',dict(skip_load=True),(1,3))):
        rom=build(False,**options); image=artifacts/(name+'.nds'); image.write_bytes(rom)
        result=artifacts/(name+'.result'); run(emulator,image,result); invalid=evaluate(result,rom,False)
        count=sum(g.kind in kinds for g in groups(False))*len(seeds(False))*48
        require(invalid['status']=='fail' and invalid['mismatches']==count,'executed '+name+' escaped: '+json.dumps(invalid))
        require(all(g['mismatches']==(g['observed_cases'] if g['instruction']['kind'] in kinds else 0) for g in invalid['groups']),'mutation affected wrong groups')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')
    content=path.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for word in range(25):
        n=6+word*8; changed=row[:n]+f'{int(row[n:n+8],16)^1:08x}'+row[n+8:]
        result=artifacts/f'field-{word}.result'; result.write_text(content.replace(row,changed,1))
        invalid=evaluate(result,baseline,False); require(invalid['status']=='fail' and invalid['mismatches']==1,'field escaped grading')
        (artifacts/f'field-{word}.json').write_text(json.dumps(invalid,indent=2)+'\n')
    for name,broken in (
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('reordered-group',content.replace('BEGIN cpu9_g00000','BEGIN cpu7_g00000',1)),
        ('false-pass',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),
        ('unterminated-trailer',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        result=artifacts/(name+'.result'); result.write_text(broken)
        invalid=evaluate(result,baseline,False); require(invalid['status']=='error',name+' accepted')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce')
    parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/nds_multiple')
    parser.add_argument('--controls-only',action='store_true')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True); references()
    rom=build(); require((HERE/'multiple_registers.nds').read_bytes()==rom,'archive differs')
    controls(args.emulator,args.artifacts)
    if args.controls_only: print('DS multiple controls PASS: 19,200 positive; 2 executed/25 field mutants FAIL; 7 damaged files ERROR'); return 0
    path=args.artifacts/'multiple_registers.result'; run(args.emulator,HERE/'multiple_registers.nds',path)
    report=evaluate(path,rom); (args.artifacts/'multiple_registers.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"DS dual-CPU stack/block registers: {report['status'].upper()} {report['graded_cases']}/{report['expected_cases']} snapshots; mismatches={report['mismatches']}")
    if report.get('detail'): print(report['detail'])
    for example in report['examples']: print(json.dumps(example))
    return 0 if report['status']=='pass' else 1
if __name__=='__main__': sys.exit(main())
