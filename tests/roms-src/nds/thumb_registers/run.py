#!/usr/bin/env python3
"""Independently grade observed dual-CPU DS ARM result/CPSR/source registers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib
from build import build, groups, cases, PATTERNS

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
MASK=(1<<32)-1


def require(ok,message):
    if not ok: raise ValueError(message)


def signed(value): return value if value<(1<<31) else value-(1<<32)


def expected(op,left,right,flags,cpu=7,thumb_pc=0):
    q=bool(flags&1) if cpu==9 else False
    if cpu==9: flags>>=1
    carry=bool(flags&2); overflow=bool(flags&1)
    if op in ('add','sub','adc','sbc','neg','cmp','cmn','add3','sub3','add8','sub8','cmp8'):
        a=0 if op=='neg' else left; b=right
        subtract=op in ('sub','sbc','neg','cmp','sub3','sub8','cmp8')
        extra=int(not carry) if op=='sbc' else int(carry) if op=='adc' else 0
        wide=a-b-extra if subtract else a+b+extra
        math=signed(a)-signed(b)-extra if subtract else signed(a)+signed(b)+extra
        result=wide&MASK; carry=wide>=0 if subtract else wide>MASK
        overflow=not -(1<<31)<=math<(1<<31)
    elif op in ('lsl','lsr','asr','ror','lsl_i','lsr_i','asr_i'):
        count=right if op.endswith('_i') else right&255
        kind=op.removesuffix('_i')
        if not count and op in ('lsr_i','asr_i'): count=32
        if not count: result=left
        elif kind=='lsl': result=(left<<count)&MASK; carry=bool((left>>(32-count))&1) if count<=32 else False
        elif kind=='lsr': result=left>>count; carry=bool((left>>(count-1))&1) if count<=32 else False
        elif kind=='asr': result=(signed(left)>>count)&MASK; carry=bool((signed(left)>>(count-1))&1)
        else:
            residue=count%32
            result=((left>>residue)|(left<<(32-residue)))&MASK
            carry=bool(result&(1<<31))
    else:
        if op in ('and','tst'): result=left&right
        elif op=='eor': result=left^right
        elif op=='orr': result=left|right
        elif op=='mul': result=(left*right)&MASK
        elif op=='bic': result=left&(~right&MASK)
        elif op=='mov8': result=right
        else: result=~right&MASK
    psw=0xdf|(0x08000000 if q else 0)|(int(result>=(1<<31))<<31)|(int(result==0)<<30)|(int(carry)<<29)|(int(overflow)<<28)
    return (left if op in ('tst','cmp','cmn','cmp8') else result,psw,left,right,thumb_pc)


def score_mask(op,cpu): return 0xdfffffff if cpu==7 and op=='mul' else MASK


def thumb_pcs(rom,cpu,domain):
    header=0x20 if cpu==9 else 0x30
    file_offset=int.from_bytes(rom[header:header+4],'little'); address=int.from_bytes(rom[header+8:header+12],'little')
    size=int.from_bytes(rom[header+12:header+16],'little'); segment=rom[file_offset:file_offset+size]
    section=segment.find(b'VTHMB001'); require(section>=0 and section==segment.rfind(b'VTHMB001'),'missing/duplicate Thumb section')
    require(section+8+len(domain)*8<=size,'truncated Thumb section')
    for index in range(len(domain)):
        require(segment[section+8+index*8+2:section+8+index*8+8]==bytes.fromhex('7e46384700de'),'invalid PC capture/return stub')
    return [address+section+8+index*8+6 for index in range(len(domain))]


def references():
    require(len(groups())==1172 and sum(cases(g,cpu) for g in groups() for cpu in (9,7))==51426816,'declared count changed')
    for op,a,b,f,want in (
        ('add',0x7fffffff,1,0,(0x80000000,0x900000df,0x7fffffff,1,0)),
        ('adc',MASK,0,2,(0,0x600000df,MASK,0,0)),
        ('sbc',0,0,0,(MASK,0x800000df,0,0,0)),
        ('neg',0,0x80000000,0,(0x80000000,0x900000df,0,0x80000000,0)),
        ('cmp',0,1,0,(0,0x800000df,0,1,0)),
        ('lsl_i',0x80000000,0,3,(0x80000000,0xb00000df,0x80000000,0,0)),
        ('lsr_i',0x80000000,0,0,(0,0x600000df,0x80000000,0,0)),
        ('asr_i',0x80000000,0,0,(MASK,0xa00000df,0x80000000,0,0)),
        ('ror',0x80000001,32,0,(0x80000001,0xa00000df,0x80000001,32,0)),
        ('mov8',MASK,0,3,(0,0x700000df,MASK,0,0)),
        ('mul',MASK,MASK,3,(1,0x300000df,MASK,MASK,0)),
    ): require(expected(op,a,b,f)==want,'oracle reference failed: '+op)
    for op,limit in (('lsl_i',32),('lsr_i',32),('asr_i',32),('add3',8),('sub3',8),('mov8',256),('cmp8',256),('add8',256),('sub8',256)):
        require([g[4][0] for g in groups() if g[1]==op]==list(range(limit)),'immediate domain changed: '+op)
    for q in (0,1): require(expected('add',0x7fffffff,1,q,9)[1]==0x900000df|(q<<27),'ARM9 Q reference failed')


def evaluate(path,rom,full=True):
    domain=groups(full); total=sum(cases(g,cpu) for g in domain for cpu in (9,7))
    report=dict(status='error',console='nds',suite='thumb_registers',expected_cases=total,observed_cases=0,
        graded_cases=0,mismatches=0,groups=[],examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=DS rom_crc32={zlib.crc32(rom):08x} channels=dsdebug\n','wrong file identity')
            for cpu in (9,7):
                pcs=thumb_pcs(rom,cpu,domain)
                require(f.readline()==f'VELOCE 1 ds thumb_registers cpu={cpu}\n','wrong CPU/suite order')
                require(f.readline()==f'INFO full={int(full)} groups={len(domain)} flags={32 if cpu==9 else 16}\n','wrong domain')
                for group_index,(name,op,_,left,right) in enumerate(domain):
                    require(f.readline()==f'BEGIN cpu{cpu}_{name} cases={cases((name,op,0,left,right),cpu)} words=5\n','missing/reordered group')
                    group=dict(id=f'cpu{cpu}_{name}',observed_cases=0,mismatches=0); report['groups'].append(group)
                    for a in left:
                        for b in right:
                            for flags in range(32 if cpu==9 else 16):
                                line=f.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{40}\n',line),'missing/malformed observation')
                                got=tuple(int(line[n:n+8],16) for n in (6,14,22,30,38)); want=expected(op,a,b,flags,cpu,pcs[group_index])
                                report['observed_cases']+=1; report['graded_cases']+=1; group['observed_cases']+=1
                                if got[0]!=want[0] or got[2:]!=want[2:] or ((got[1]^want[1])&score_mask(op,cpu)):
                                    report['mismatches']+=1; group['mismatches']+=1
                                    if len(report['examples'])<8: report['examples'].append(dict(cpu=cpu,group=name,left=f'{a:08x}',right=f'{b:08x}',flags=flags,expected=[f'{v:08x}' for v in want],observed=[f'{v:08x}' for v in got]))
                require(f.readline()==f'LOG cpu={cpu} complete cases={sum(cases(g,cpu) for g in domain)}\n','missing CPU completion')
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
                        capture_output=True,text=True,timeout=1800)
    require(proc.returncode==0,proc.stdout+proc.stderr)


def controls(emulator,artifacts):
    baseline=build(False); image=artifacts/'control.nds'; image.write_bytes(baseline)
    result=artifacts/'control.result'; run(emulator,image,result); report=evaluate(result,baseline,False)
    require(report['status']=='pass','reduced positive failed: '+json.dumps(report))
    (artifacts/'control.json').write_text(json.dumps(report,indent=2)+'\n')
    rom=build(False,wrong_adc=True); image=artifacts/'wrong-adc-sbc.nds'; image.write_bytes(rom)
    path=artifacts/'wrong-adc-sbc.result'; run(emulator,image,path); invalid=evaluate(path,rom,False)
    require(invalid['status']=='fail' and all(g['mismatches']>0 if '_adc_' in g['id'] else g['mismatches']==0 for g in invalid['groups']),'ADC-to-SBC mutation not isolated/rejected: '+json.dumps(invalid))
    (artifacts/'wrong-adc-sbc.json').write_text(json.dumps(invalid,indent=2)+'\n')
    content=result.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for name,group,word,bit,status in (
        ('arm7-mul-c','cpu7_mul_boundary',1,0x20000000,'pass'),
        ('arm7-mul-v','cpu7_mul_boundary',1,0x10000000,'fail'),
        ('arm9-mul-c','cpu9_mul_boundary',1,0x20000000,'fail'),
        ('wrong-thumb-pc','cpu9_and_boundary',4,2,'fail')):
        offset=content.index('REG r=',content.index('BEGIN '+group))+6+word*8
        changed=content[:offset]+f'{int(content[offset:offset+8],16)^bit:08x}'+content[offset+8:]
        path=artifacts/(name+'.result'); path.write_text(changed); invalid=evaluate(path,baseline,False)
        require(invalid['status']==status and invalid['mismatches']==(0 if status=='pass' else 1),name+' scored incorrectly')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')
    for name,broken in (
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('reordered-group',content.replace('BEGIN cpu9_and','BEGIN cpu7_and',1)),
        ('false-pass',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),
        ('unterminated-trailer',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        path=artifacts/(name+'.result'); path.write_text(broken)
        invalid=evaluate(path,baseline,False); require(invalid['status']=='error',name+' accepted')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce')
    parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/nds_thumb')
    parser.add_argument('--controls-only',action='store_true')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True); references()
    rom=build(); require((HERE/'thumb_registers.nds').read_bytes()==rom,'archive differs')
    controls(args.emulator,args.artifacts)
    if args.controls_only: print('DS controls: executed mutant FAIL; 7 damaged files ERROR'); return 0
    result=args.artifacts/'thumb_registers.result'; run(args.emulator,HERE/'thumb_registers.nds',result)
    report=evaluate(result,rom); (args.artifacts/'thumb_registers.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"DS dual-CPU Thumb registers: {report['status'].upper()} {report['graded_cases']}/{report['expected_cases']} result/PSW/source/Thumb-PC snapshots; mismatches={report['mismatches']}")
    if report.get('detail'): print(report['detail'])
    for example in report['examples']: print(json.dumps(example))
    return 0 if report['status']=='pass' else 1

if __name__=='__main__': sys.exit(main())
