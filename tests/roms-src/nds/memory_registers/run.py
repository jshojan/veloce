#!/usr/bin/env python3
"""Independently grade DS ARM/Thumb memory transfers and surrounding bytes."""
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
from build import build,groups,cases,values,width,thunk_size,DATA_WORD,GUARDS
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
MASK=(1<<32)-1
def require(ok,message):
    if not ok: raise ValueError(message)
def locations(rom,cpu,domain):
    header=0x20 if cpu==9 else 0x30
    file_offset=int.from_bytes(rom[header:header+4],'little'); address=int.from_bytes(rom[header+8:header+12],'little')
    size=int.from_bytes(rom[header+12:header+16],'little'); segment=rom[file_offset:file_offset+size]
    section=segment.find(b'VMEM0001'); require(section>=0 and section==segment.rfind(b'VMEM0001'),'missing/duplicate memory section')
    result=[]; cursor=section+8
    for g in domain:
        require(cursor+thunk_size(g)<=size,'truncated memory section')
        result.append(address+cursor); cursor+=thunk_size(g)
    return result

def expected(g,value,flags,cpu,stub):
    # Model bytes directly, independently of either CPU's instruction decoder.
    word_address=((stub+4)//4*4)+g.offset*4 if g.form=='pc' else DATA_WORD
    address=word_address+g.lane
    offset=g.offset*(width(g) if g.state=='thumb' and g.form=='imm' else 4 if g.form in ('sp','pc') else 1)
    base=address if g.state=='arm' and g.index=='post' else (address-offset if g.up else address+offset)&MASK
    after_base=((base+offset if g.up else base-offset)&MASK) if g.state=='arm' and g.index!='offset' else base
    store=g.op.startswith('str'); seed=((value&255)^255)*0x01010101 if store else value
    memory=bytearray(seed.to_bytes(4,'little'))
    if store:
        memory[g.lane:g.lane+width(g)]=value.to_bytes(4,'little')[:width(g)]; result=value
    else:
        payload=memory[g.lane:g.lane+width(g)]
        result=int.from_bytes(payload,'little',signed=g.op in ('ldrsb','ldrsh'))&MASK
    psw=0xdf|(flags<<(27 if cpu==9 else 28))
    return (result,psw,after_base,g.offset,value,int.from_bytes(memory,'little'),GUARDS[0],GUARDS[1],base)

def references():
    domain=groups(); require(len(domain)==15326 and sum(cases(g,c) for g in domain for c in (9,7))==49694208,'declared domain changed')
    from build import Group
    for g,v,want in (
        (Group('arm','ldrh'),0x12348000,(0x8000,0xdf,DATA_WORD,0,0x12348000,0x12348000,*GUARDS,DATA_WORD)),
        (Group('arm','ldrsh'),0x12348000,(0xffff8000,0xdf,DATA_WORD,0,0x12348000,0x12348000,*GUARDS,DATA_WORD)),
        (Group('thumb','ldrsb',lane=3),0x80001234,(0xffffff80,0xdf,DATA_WORD+3,0,0x80001234,0x80001234,*GUARDS,DATA_WORD+3)),
        (Group('thumb','strh',lane=2),0x1234abcd,(0x1234abcd,0xdf,DATA_WORD+2,0,0x1234abcd,0xabcd3232,*GUARDS,DATA_WORD+2)),
    ): require(expected(g,v,0,7,0)==want,'oracle byte/sign reference failed')
    require(expected(Group('arm','ldrh','imm','post',False,255),0,0,7,0)[2]==DATA_WORD-255,'post-index reference failed')
    require(expected(Group('thumb','ldr','pc',offset=255),0x89abcdef,31,9,0x02202000)==(0x89abcdef,0xf80000df,0x02202004,255,0x89abcdef,0x89abcdef,*GUARDS,0x02202004),'literal/SP/Q reference failed')
    for op in ('strh','ldrh','ldrsb','ldrsh'):
        for form in ('imm','reg'):
            for index in ('offset','pre','post'):
                for up in (False,True): require([g.offset for g in domain if (g.state,g.op,g.form,g.index,g.up,g.domain)==('arm',op,form,index,up,'pattern')]==list(range(256)),'ARM offset domain changed')


def evaluate(path,rom,full=True):
    domain=groups(full); total=sum(cases(g,c,full) for g in domain for c in (9,7))
    report=dict(status='error',console='nds',suite='memory_registers',expected_cases=total,observed_cases=0,graded_cases=0,mismatches=0,groups=[],examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=DS rom_crc32={zlib.crc32(rom):08x} channels=dsdebug\n','wrong file identity')
            for cpu in (9,7):
                stubs=locations(rom,cpu,domain)
                require(f.readline()==f'VELOCE 1 ds memory_registers cpu={cpu}\n','wrong CPU/suite order')
                require(f.readline()==f'INFO full={int(full)} groups={len(domain)} flags={32 if cpu==9 else 16}\n','wrong domain')
                for index,g in enumerate(domain):
                    require(f.readline()==f'BEGIN cpu{cpu}_g{index:05d} cases={cases(g,cpu,full)} words=9\n','missing/reordered group')
                    group=dict(id=f'cpu{cpu}_g{index:05d}',instruction=asdict(g),observed_cases=0,mismatches=0); report['groups'].append(group)
                    for value in values(g.domain,full):
                        for flags in range(32 if cpu==9 else 16):
                            line=f.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{72}\n',line),'missing/malformed observation')
                            got=tuple(int(line[n:n+8],16) for n in range(6,78,8)); want=expected(g,value,flags,cpu,stubs[index])
                            report['observed_cases']+=1; report['graded_cases']+=1; group['observed_cases']+=1
                            if got!=want:
                                report['mismatches']+=1; group['mismatches']+=1
                                if len(report['examples'])<8: report['examples'].append(dict(cpu=cpu,group=index,instruction=asdict(g),value=f'{value:08x}',flags=flags,expected=[f'{v:08x}' for v in want],observed=[f'{v:08x}' for v in got]))
                require(f.readline()==f'LOG cpu={cpu} complete cases={sum(cases(g,cpu,full) for g in domain)}\n','missing CPU completion')
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
    result=artifacts/'control.result'; run(emulator,image,result); report=evaluate(result,baseline,False)
    require(report['status']=='pass','reduced positive failed: '+json.dumps(report))
    (artifacts/'control.json').write_text(json.dumps(report,indent=2)+'\n')
    rom=build(False,wrong_signed=True); image=artifacts/'wrong-signed.nds'; image.write_bytes(rom)
    path=artifacts/'wrong-signed.result'; run(emulator,image,path); invalid=evaluate(path,rom,False)
    require(invalid['status']=='fail' and all(g['mismatches']>0 if g['instruction']['op']=='ldrsh' else g['mismatches']==0 for g in invalid['groups']),'signed-to-unsigned mutation not isolated/rejected: '+json.dumps(invalid))
    (artifacts/'wrong-signed.json').write_text(json.dumps(invalid,indent=2)+'\n')
    rom=build(False,wrong_width=True); image=artifacts/'wrong-width.nds'; image.write_bytes(rom)
    path=artifacts/'wrong-width.result'; run(emulator,image,path); invalid=evaluate(path,rom,False)
    require(invalid['status']=='fail' and invalid['mismatches']==4224 and all(g['mismatches']==0 for g in invalid['groups'] if (g['instruction']['state'],g['instruction']['form'],g['instruction']['op'])!=('thumb','reg','strh')),'halfword-to-byte store mutation not isolated/rejected')
    (artifacts/'wrong-width.json').write_text(json.dumps(invalid,indent=2)+'\n')
    rom=build(False,wrong_store=True); image=artifacts/'missing-store.nds'; image.write_bytes(rom)
    path=artifacts/'missing-store.result'; run(emulator,image,path); invalid=evaluate(path,rom,False)
    require(invalid['status']=='fail' and invalid['mismatches']==4608 and all(g['mismatches']==g['observed_cases'] if (g['instruction']['state'],g['instruction']['form'],g['instruction']['op'])==('thumb','reg','strh') else g['mismatches']==0 for g in invalid['groups']),'missing store escaped a lane/value/flag case')
    (artifacts/'missing-store.json').write_text(json.dumps(invalid,indent=2)+'\n')
    content=result.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for name,broken in (
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('reordered-group',content.replace('BEGIN cpu9_g00000','BEGIN cpu7_g00000',1)),
        ('false-pass',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),
        ('unterminated-trailer',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        path=artifacts/(name+'.result'); path.write_text(broken)
        invalid=evaluate(path,baseline,False); require(invalid['status']=='error',name+' accepted')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce')
    parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/nds_memory')
    parser.add_argument('--controls-only',action='store_true')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True); references()
    rom=build(); require((HERE/'memory_registers.nds').read_bytes()==rom,'archive differs')
    controls(args.emulator,args.artifacts)
    if args.controls_only: print('DS controls: 3 executed mutants FAIL; 7 damaged files ERROR'); return 0
    result=args.artifacts/'memory_registers.result'; run(args.emulator,HERE/'memory_registers.nds',result)
    report=evaluate(result,rom); (args.artifacts/'memory_registers.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"DS dual-CPU memory registers: {report['status'].upper()} {report['graded_cases']}/{report['expected_cases']} result/PSW/address/source/memory/SP snapshots; mismatches={report['mismatches']}")
    if report.get('detail'): print(report['detail'])
    for example in report['examples']: print(json.dumps(example))
    return 0 if report['status']=='pass' else 1

if __name__=='__main__': sys.exit(main())
