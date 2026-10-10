#!/usr/bin/env python3
"""Host oracle for guest branch decisions and captured special registers."""
import argparse, hashlib, json, os, re, subprocess, sys, zlib
from pathlib import Path
from build import build
CONSOLE='wii'
CORE='Wii'
EXT='vwi'
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
def require(ok,message):
    if not ok: raise ValueError(message)
def expected(rom,short=False):
    # Locate the PC-observation sequence in actual instructions, not builder metadata.
    anchors=[]
    for offset in range(32,len(rom)-8,4):
        if rom[offset:offset+12]==bytes.fromhex('428000057d2802a67ce803a6'): anchors.append(0x80000000+offset-32+12)
    index=0
    options=(0,4,12,16,20) if short else (0,1,2,3,4,5,8,9,10,11,12,13,16,17,18,19,20)
    for family in ('direct','link','count'):
        for option in options:
            if family=='count' and option not in (4,5,12,13,20): continue
            for bit in ((0,31) if short else range(32)):
                for cr_index,condition in enumerate((0,4294967295) if short else (0,4294967295,2779096485,1515870810)):
                    counters=range(4) if family=='count' else ((0,1) if short else (0,1,2,3,2147483647,2147483648,4294967294,4294967295))
                    for counter_index,counter in enumerate(counters):
                        for link in range(2):
                            require(index<len(anchors),'missing guest PC anchor'); pc=anchors[index]; index+=1
                            lane=counter_index%4
                            target_input=(pc+8)+lane
                            ctr=target_input if family=='count' else counter
                            old_lr=target_input if family=='link' else 305419899
                            test_count=option in (0,1,2,3,8,9,10,11,16,17,18,19)
                            if test_count: ctr=(ctr-1)%4294967296
                            zero_required=option in (2,3,10,11,18,19)
                            count_pass=not test_count or ((ctr==0) if zero_required else (ctr!=0))
                            bit_value=(condition//(2**(31-bit)))%2
                            if option in (16,17,18,19,20): condition_pass=True
                            elif option in (8,9,10,11,12,13): condition_pass=bit_value==1
                            else: condition_pass=bit_value==0
                            taken=int(count_pass and condition_pass)
                            lr=pc+4 if link else old_lr
                            xer=((counter_index+cr_index+option)%8)*536870912+127
                            yield taken,lr,ctr,condition,xer,pc-8,305419896
    require(index==len(anchors),'extra guest PC anchors')
def evaluate(path,rom,short=False):
    cases=256 if short else 74752
    report=dict(status='error',console=CONSOLE,suite='branch_registers',expected_cases=cases,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline()==f'#VELOCE 1 core={CORE} rom_crc32={zlib.crc32(rom):08x} channels=ppcdebug\n','wrong file identity')
            require(stream.readline()==f'VELOCE 1 {CONSOLE} branch_registers\n','wrong suite')
            require(stream.readline()==f'BEGIN branches cases={cases} words=7\n','wrong domain')
            for index,want in enumerate(expected(rom,short)):
                row=stream.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{56}\n',row),'missing/malformed row')
                got=tuple(int(row[n:n+8],16) for n in range(6,62,8))
                report['observed_cases']+=1; report['graded_cases']+=1
                if got!=want:
                    report['mismatches']+=1
                    if len(report['examples'])<8: report['examples'].append(dict(index=index,expected=list(want),observed=list(got)))
            require(report['observed_cases']==cases,'oracle domain incomplete')
            require(stream.readline()==f'LOG complete cases={cases}\n','missing completion')
            require(stream.readline()=='END observations_complete\n','false END')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',stream.readline()),'missing/ungraded trailer')
            require(not stream.read(),'trailing data'); report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as error: report['detail']=str(error)
    return report
def run(emulator,image,path):
    env=os.environ.copy()
    for key in ('INPUT','INPUT_FILE','DEBUG','TRACE','TRACE_FILE','VELOCE_TEST_RESETS'): env.pop(key,None)
    env.update(HEADLESS='1',FRAMES='200',VELOCE_TEST_OUT=str(path.resolve()),VELOCE_TEST_EXIT='1')
    proc=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,capture_output=True,text=True,timeout=90)
    require(proc.returncode==0,proc.stdout+proc.stderr)
def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce'); parser.add_argument('--artifacts',type=Path,default=ROOT/f'build/artifacts/owned/{CONSOLE}_branches')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,short=False,execute=True,content=None):
        image=args.artifacts/(name+'.'+EXT); result=args.artifacts/(name+'.result')
        if execute: image.write_bytes(rom); run(args.emulator,image,result)
        else: result.write_text(content)
        report=evaluate(result,rom,short); (args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n'); return report,result
    baseline=build(short=True); positive,path=grade('control',baseline,True)
    require(positive['status']=='pass',json.dumps(positive))
    mutant,_=grade('wrong-result',build(short=True,wrong_result=True),True)
    require(mutant['status']=='fail' and mutant['mismatches']==256,'executed mutation escaped')
    condition,_=grade('wrong-condition',build(short=True,wrong_condition=True),True)
    require(condition['status']=='fail' and condition['mismatches']==144,'executed condition mutation escaped')
    linking,_=grade('wrong-link',build(short=True,wrong_link=True),True)
    require(linking['status']=='fail' and linking['mismatches']==128,'executed link mutation escaped')
    content=path.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for field in range(7):
        n=6+field*8; changed=row[:n]+f'{int(row[n:n+8],16)^1:08x}'+row[n+8:]
        report,_=grade(f'field-{field}',baseline,True,False,content.replace(row,changed,1)); require(report['status']=='fail' and report['mismatches']==1,'field escaped')
    for name,broken in (('missing-row',content.replace(row+'\n','',1)),('bad-row',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=256 words=7','cases=255 words=7',1)),('false-pass',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        report,_=grade(name,baseline,True,False,broken); require(report['status']=='error',name+' accepted')
    rom=build(); require((HERE/('branch_registers.'+EXT)).read_bytes()==rom,'archive differs')
    report,_=grade('branch_registers',rom)
    print(f"{CORE} branches: {report['status'].upper()} {report['graded_cases']}/74752 seven-word observations; mismatches={report['mismatches']}")
    require(report['status']=='pass',json.dumps(report)); return 0
if __name__=='__main__': sys.exit(main())
