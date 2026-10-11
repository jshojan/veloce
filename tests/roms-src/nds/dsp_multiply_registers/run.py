#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independent unbounded-integer oracle; grades actual ARM9 registers/CPSR."""
import argparse,hashlib,json,os,re,subprocess,sys,zlib
from pathlib import Path
from build import build,groups,cases
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3];MASK=0xffffffff

def require(ok,why):
    if not ok:raise ValueError(why)
def signed(n,bits=32):return n if n<2**(bits-1) else n-2**bits
def predicate(cond,flags):
    n,z,c,v=(bool(flags&(1<<bit)) for bit in (4,3,2,1))
    return (z,not z,c,not c,n,not n,v,not v,c and not z,not c or z,n==v,n!=v,not z and n==v,z or n!=v,True)[cond]
def expected(op,a,b,seed,flags,cond,alias):
    kind,x,y=op
    regs=[a,b,seed&MASK,0xfacefeed,seed&MASK if kind=='smlal' else 0xdeadbeef,seed>>32]
    psw=0xdf|(flags<<27)
    if predicate(cond,flags):
        first=regs[0];second=regs[0] if alias=='same_sources' else regs[1]
        multiplicand=signed(first) if kind.endswith('w') else signed((first>>(16*x))&65535,16)
        multiplier=signed((second>>(16*y))&65535,16)
        product=multiplicand*multiplier
        if kind.endswith('w'):product//=65536
        destination=0 if alias=='destination_rm' else 1 if alias=='destination_rs' else 4
        if kind=='smlal':
            high=5
            if alias=='accumulator_rm':destination,high=0,1
            value=(((regs[high]<<32)|regs[destination])+product)&(2**64-1)
            regs[destination]=value&MASK;regs[high]=value>>32
        else:
            if kind in ('smla','smlaw'):
                accumulator=regs[0] if alias=='accumulator_rm' else regs[2]
                product+=signed(accumulator)
                if not -2**31<=product<2**31:psw|=1<<27
            regs[destination]=product&MASK
    return tuple(regs)+(psw,)
def references():
    # Literal arithmetic anchors independently pin overflow/wrap, sign and scaling.
    anchors=(
        (('smul',0,0),0xaaaa8000,0xbbbb8000,0,0,(0x40000000,0,0xdf)),
        (('smul',1,1),0x80000001,0xffff7fff,0,0,(0x8000,0,0xdf)),
        (('smla',0,0),0x7fff,1,0x7fffffff,0,(0x80007ffe,0,0x080000df)),
        (('smla',0,0),0x8000,1,0x80000000,0,(0x7fff8000,0,0x080000df)),
        (('smlal',0,0),1,1,0xffffffffffffffff,31,(0,0,0xf80000df)),
        (('smlal',0,0),0xffff,1,0x8000000000000000,0,(0xffffffff,0x7fffffff,0xdf)),
        (('smulw',0,0),0xffffffff,1,0,0,(0xffffffff,0,0xdf)),
        (('smulw',0,1),0x80000000,0x80000001,0,0,(0x40000000,0,0xdf)),
        (('smlaw',0,0),0xffffffff,1,0x80000000,0,(0x7fffffff,0,0x080000df)),
        (('smlaw',0,0),0x7fffffff,0x7fff,0x7fffffff,0,(0xbfff7ffe,0,0x080000df)))
    for op,a,b,seed,flags,want in anchors:
        row=expected(op,a,b,seed,flags,14,'distinct')
        require((row[4],row[5],row[6])==want,'literal reference '+str(op))
    require(expected(('smul',0,0),1,2,0,0,0,'distinct')[4]==0xdeadbeef,'failed condition')
    require(sum(cases(g) for g in groups())==15454208,'full domain changed')
def evaluate(path,rom,full=True):
    domain=groups(full);total=sum(cases(g) for g in domain)
    report=dict(status='error',console='nds',suite='dsp_multiply_registers',expected_cases=total,observed_cases=0,mismatches=0,groups=[],examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=DS rom_crc32={zlib.crc32(rom):08x} channels=dsdebug\n','file identity')
            require(f.readline()=='VELOCE 1 ds dsp_multiply_registers cpu=9\n','suite identity')
            require(f.readline()==f'INFO full={int(full)} groups={len(domain)} words=7\n','domain identity')
            row_pattern=re.compile(r'REG r=[0-9a-f]{56}\n')
            for g in domain:
                name,op,left,right,seed,cond,alias,flags=g
                require(f.readline()==f'BEGIN {name} cases={cases(g)} words=7\n','missing/reordered group '+name)
                group=dict(id=name,observed_cases=0,mismatches=0);report['groups'].append(group)
                for a in left:
                    for b in right:
                        for flag in flags:
                            line=f.readline();require(row_pattern.fullmatch(line),'missing/malformed observation')
                            got=tuple(int(line[i:i+8],16) for i in range(6,62,8));want=expected(op,a,b,seed,flag,cond,alias)
                            report['observed_cases']+=1;group['observed_cases']+=1
                            if got!=want:
                                report['mismatches']+=1;group['mismatches']+=1
                                if len(report['examples'])<8:report['examples'].append(dict(group=name,a=f'{a:08x}',b=f'{b:08x}',seed=f'{seed:016x}',flags=flag,expected=[f'{v:08x}' for v in want],observed=[f'{v:08x}' for v in got]))
            require(f.readline()==f'LOG complete cases={total}\n','false count')
            require(f.readline()=='END observations_complete\n','false completion')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',f.readline()),'missing/ungraded trailer')
            require(not f.read(),'extra data');report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as e:report['detail']=str(e)
    return report

def run(emulator,image,result,frames=100000):
    env=os.environ.copy()
    for key in ('INPUT','INPUT_FILE','DEBUG','TRACE','TRACE_FILE','VELOCE_TEST_RESETS'):env.pop(key,None)
    env.update(HEADLESS='1',FRAMES=str(frames),VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT='1')
    p=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,capture_output=True,text=True,timeout=1500)
    (result.with_suffix('.log')).write_text(p.stdout+p.stderr)
    require(p.returncode==0,p.stdout+p.stderr)
def write_report(path,report):path.write_text(json.dumps(report,indent=2)+'\n')
def controls(emulator,root):
    rom=build(False);image=root/'control.nds';image.write_bytes(rom);result=root/'control.result';run(emulator,image,result)
    report=evaluate(result,rom,False);write_report(root/'control.json',report);require(report['status']=='pass',str(report))
    mutant=build(False,True);image=root/'mutant.nds';image.write_bytes(mutant);result=root/'mutant.result';run(emulator,image,result)
    report=evaluate(result,mutant,False);write_report(root/'mutant.json',report)
    require(report['status']=='fail' and report['mismatches']>0,'executed wrong-half selector mutant accepted')
    require(all(g['mismatches']>0 for g in report['groups'] if g['id'].endswith('_sweep')),'mutant failed to exercise every instruction form')
    content=(root/'control.result').read_text();row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for name,broken in (
        ('result-bit',content.replace(row,row[:38]+f'{int(row[38:46],16)^1:08x}'+row[46:],1)),
        ('q-bit',content.replace(row,row[:54]+f'{int(row[54:62],16)^0x08000000:08x}'+row[62:],1)),
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('false-completion',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated-trailer',content.rstrip('\n')),
        ('trailing-data',content+'extra\n')):
        path=root/(name+'.result');path.write_text(broken);report=evaluate(path,rom,False);write_report(root/(name+'.json'),report)
        require(report['status']==('fail' if name in ('result-bit','q-bit') else 'error'),'accepted '+name)
        if name in ('result-bit','q-bit'):require(report['mismatches']==1,'mutation count '+name)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce');p.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/nds_dsp_multiply');p.add_argument('--baseline-only',action='store_true');p.add_argument('--controls-only',action='store_true');a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True);references()
    if a.baseline_only:
        rom=build(False);image=a.artifacts/'baseline.nds';image.write_bytes(rom);result=a.artifacts/'baseline.result';run(a.emulator,image,result,1);report=evaluate(result,rom,False);write_report(a.artifacts/'baseline.json',report);require(report['status']!='pass','missing instruction unexpectedly passed');print('Before implementation:',report['status'],report.get('detail'));return 0
    controls(a.emulator,a.artifacts)
    if a.controls_only:print('ARM9 DSP controls PASS; executed wrong-half mutant FAIL; damaged files ERROR');return 0
    rom=build();require((HERE/'dsp_multiply_registers.nds').read_bytes()==rom,'archived image differs');result=a.artifacts/'dsp_multiply_registers.result';run(a.emulator,HERE/'dsp_multiply_registers.nds',result);report=evaluate(result,rom);write_report(a.artifacts/'dsp_multiply_registers.json',report);print(f"ARM9 DSP multiplication: {report['status'].upper()} {report['observed_cases']}/{report['expected_cases']}; mismatches={report['mismatches']}")
    if report.get('detail'):print(report['detail'])
    return 0 if report['status']=='pass' else 1
if __name__=='__main__':sys.exit(main())
