#!/usr/bin/env python3
"""Run the production Switch plugin and grade actual guest register stores."""
import argparse,hashlib,json,os,re,subprocess,zlib
from pathlib import Path
from build import build
from oracle import observations,references
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
def require(ok,message):
    if not ok:raise ValueError(message)
def evaluate(path,rom,domain):
    count=domain*domain*28
    report=dict(status='error',expected_cases=count,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest())
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=Switch rom_crc32={zlib.crc32(rom):08x} channels=vnxdebug\n','wrong identity')
            require(f.readline()=='VELOCE 1 switch muldiv_registers\n','wrong suite')
            require(f.readline()==f'BEGIN muldiv cases={count} words=6\n','wrong domain')
            for index,want in enumerate(observations(domain)):
                line=f.readline();require(re.fullmatch(r'REG r=[0-9a-f]{96}\n',line),'malformed/missing row')
                got=tuple(int(line[n:n+16],16) for n in range(6,102,16));report['graded_cases']+=1
                if got!=want:
                    report['mismatches']+=1
                    if len(report['examples'])<8:report['examples'].append(dict(index=index,expected=want,observed=got))
            require(f.readline()==f'LOG complete cases={count}\n','missing completion')
            require(f.readline()=='END observations_complete\n','false END')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',f.readline()),'invalid trailer')
            require(not f.read(),'trailing data');report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as e:report['detail']=str(e)
    return report

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce');parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/switch_muldiv');args=parser.parse_args();args.artifacts.mkdir(parents=True,exist_ok=True);references()
    env=os.environ.copy()
    for name in ('INPUT','INPUT_FILE','DEBUG','TRACE','TRACE_FILE','VELOCE_TEST_RESETS'):env.pop(name,None)
    def grade(name,rom,domain,content=None):
        image=args.artifacts/(name+'.nso');path=args.artifacts/(name+'.result')
        if content is None:
            image.write_bytes(rom);settings=dict(env,HEADLESS='1',FRAMES='1000',VELOCE_TEST_OUT=str(path.resolve()),VELOCE_TEST_EXIT='1')
            proc=subprocess.run([str(args.emulator.resolve()),str(image.resolve())],cwd=ROOT,env=settings,capture_output=True,text=True,timeout=180)
            (args.artifacts/(name+'.log')).write_text(proc.stdout+proc.stderr);require(proc.returncode==0,proc.stdout+proc.stderr)
        else:path.write_text(content)
        report=evaluate(path,rom,domain);(args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n');return report,path
    rom=build();require(rom==(HERE/'muldiv_registers.nso').read_bytes(),'archive differs')
    good,_=grade('muldiv_registers',rom,272);require(good['status']=='pass',json.dumps(good))
    short=build(short=True);good,path=grade('control',short,4);require(good['status']=='pass',json.dumps(good))
    bad,_=grade('executed-mutant',build(short=True,wrong_capture=True),4);require(bad['status']=='fail' and bad['mismatches']==448,'executed mutant escaped')
    content=path.read_text();row=next(x for x in content.splitlines() if x.startswith('REG r='))
    for field in range(6):
        at=6+16*field;changed=row[:at]+format(int(row[at:at+16],16)^1,'016x')+row[at+16:];r,_=grade(f'field-{field}',short,4,content.replace(row,changed,1));require(r['status']=='fail' and r['mismatches']==1,'field mutation escaped')
    for name,broken in [('missing-row',content.replace(row+'\n','',1)),('malformed',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=448 words=6','cases=447 words=6')),('false-END',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing',content+'extra\n')]:
        r,_=grade(name,short,4,broken);require(r['status']=='error',name+' escaped')
    print('Switch AArch64: 2071552 cases / 12429312 actual words PASS; executed mutant FAIL; six field and seven stream controls rejected')
if __name__=='__main__':main()
