#!/usr/bin/env python3
"""Independent arithmetic and strict file grading for Nintendo V810 instructions."""
import argparse,hashlib,json,os,re,subprocess,zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3];MASK=0xffffffff
# Declared independently of the assembler and CPU implementation.
def groups():
    return [(f'xb{i}',8,x,0) for i,x in enumerate((0,0xffff0000,0x89ab0000,0x80000000))]+[(f'xh{i}',9,x,0) for i,x in enumerate((0,0xffff,0x89ab,0x8000))]+[(f'rev{i}',10,i,0) for i in range(3)]+[(f'mpy{i}-{j}',12,x,y) for i,x in enumerate((0,0x55540000,0xaaa80000,0xfffe0000)) for j,y in enumerate((0,1,0x80000000,0x89abcdef))]+[('cli',22,0,0),('sei',30,0,0)]
def domain(sub,full):
    if sub>=16:return ((x%1024)+(x//1024)*4096 for x in range(262144)) if full else (0,1,2,15,4096,32768,1045503)
    return range(131072 if sub==12 else 65536) if full else ((0,1,32767,32768,65535,65536,65537,131071) if sub==12 else (0,1,127,128,255,256,32767,32768,65535))
def flag_domain(sub):return (0,) if sub>=16 else (0,15) if sub==12 else range(16)
def count(g,full):return (262144 if g[1]>=16 else 131072 if g[1]==12 else 65536)*len(flag_domain(g[1])) if full else (7 if g[1]>=16 else 8 if g[1]==12 else 9)*len(flag_domain(g[1]))
def require(ok,why):
    if not ok:raise ValueError(why)
def expected(g,raw,flags):
    name,sub,param,b=g;source=0x76543210;before=0x89abcdef;seed=raw if sub>=16 else 32768+flags
    if sub==8:
        before=param+raw;result=(before//65536)*65536+(raw%256)*256+raw//256
    elif sub==9:
        before=raw*65536+param;result=param*65536+raw
    elif sub==10:
        source=raw if param==0 else raw*65536 if param==1 else raw*65537
        result=int(f'{source:032b}'[::-1],2)
    elif sub==12:
        source=param+raw;before=b;factor=raw if raw<65536 else raw-131072
        result=(factor*b)%4294967296
    else:result=before
    psw=seed if sub<16 else (seed&~4096) if sub==22 else seed|4096
    instruction=sub*1024 if sub>=16 else 0xf800+20*32+(25 if sub in (10,12) else 0)+sub*1024*65536
    return (result,psw,source,before,0xdeadbeef,0x76543210,instruction,seed)
def grade(path,rom,full=True):
    gs=groups();total=sum(count(g,full) for g in gs);report={'status':'error','expected_snapshots':total,'observed_snapshots':0,'mismatched_snapshots':0,'mismatched_words':0,'groups':[],'examples':[]}
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=VB rom_crc32={zlib.crc32(rom):08x} channels=v810debug\n','file identity')
            require(f.readline()=='VELOCE 1 vb nintendo\n','suite identity')
            require(f.readline()==f'INFO full={int(full)} groups=29\n','domain identity')
            for g in gs:
                name=g[0];require(f.readline()==f'BEGIN {name} cases={count(g,full)} words=8\n','group framing')
                detail={'id':name,'mismatched_snapshots':0,'mismatched_words':0};report['groups'].append(detail)
                for raw in domain(g[1],full):
                    for flags in flag_domain(g[1]):
                        line=f.readline();require(re.fullmatch(r'REG r=[0-9a-f]{64}\n',line),'register row')
                        want=expected(g,raw,flags);want_text='REG r='+''.join(f'{w:08x}' for w in want)+'\n';report['observed_snapshots']+=1
                        if line!=want_text:
                            got=tuple(int(line[n:n+8],16) for n in range(6,70,8));bad=sum(x!=y for x,y in zip(got,want))
                            report['mismatched_snapshots']+=1;report['mismatched_words']+=bad;detail['mismatched_snapshots']+=1;detail['mismatched_words']+=bad
                            if len(report['examples'])<4:report['examples'].append({'group':name,'raw':raw,'flags':flags,'want':want,'got':got})
            require(f.readline()==f'LOG complete cases={total}\n','completion')
            require(f.readline()=='END observations_complete\n','terminator')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',f.readline()),'trailer')
            require(not f.read(),'trailing data');report['status']='fail' if report['mismatched_words'] else 'pass'
    except (ValueError,OSError,UnicodeError) as e:report['detail']=str(e)
    return report
def execute(emulator,image,result):
    env=os.environ.copy()
    for key in ('INPUT','INPUT_FILE','DEBUG','TRACE','TRACE_FILE','VELOCE_TEST_RESETS'):env.pop(key,None)
    env.update(HEADLESS='1',FRAMES='100000',VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT='1')
    proc=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,capture_output=True,text=True,timeout=1800)
    require(proc.returncode==0,proc.stdout+proc.stderr)
def main():
    p=argparse.ArgumentParser();p.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce');p.add_argument('--state-test',type=Path,default=ROOT/'build/bin/vb_nintendo_plugin_state_test');p.add_argument('--core',type=Path,default=ROOT/'build/bin/cores/vb.so');p.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/vb_nintendo');p.add_argument('--controls-only',action='store_true');a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True)
    require(sum(count(g,True) for g in groups())==16252928 and sum(count(g,False) for g in groups())==1854,'domain counts')
    for raw,want in ((32768,32768),(65535,65535),(65536,0xffff0000),(131071,MASK)):
        require(expected(('reference',12,0,1),raw,0)[0]==want,'signed 17-bit reference')
    paths=[a.emulator,a.core,a.state_test,HERE/'build.py',HERE/'recipe.json',HERE/'nintendo_registers.vb',Path(__file__),HERE.parent/'immediates/build.py']+[ROOT/x for x in ('cores/vb/src/v810.cpp','cores/vb/src/v810.hpp','cores/vb/src/machine.cpp','cores/vb/src/machine.hpp','cores/vb/src/controller.hpp','cores/vb/src/plugin.cpp','tests/cpp/vb_nintendo_plugin_state_test.cpp')]
    hashes={str(x.resolve()):hashlib.sha256(x.read_bytes()).hexdigest() for x in paths};rom=build();require(rom==(HERE/'nintendo_registers.vb').read_bytes(),'archive differs')
    baseline=build(False);image=a.artifacts/'control.vb';image.write_bytes(baseline);result=a.artifacts/'control.result';execute(a.emulator,image,result);report=grade(result,baseline,False);require(report['status']=='pass','short positive '+json.dumps(report));(a.artifacts/'control.json').write_text(json.dumps(report,indent=2)+'\n')
    for name,option in (('wrong-result','wrong_result'),('wrong-flags','wrong_flags'),('wrong-mpyhw16','wrong_mpyhw16')):
        badrom=build(False,**{option:True});image=a.artifacts/(name+'.vb');image.write_bytes(badrom);out=a.artifacts/(name+'.result');execute(a.emulator,image,out);bad=grade(out,badrom,False)
        require(bad['status']=='fail',name+' escaped '+json.dumps(bad))
        if name!='wrong-mpyhw16':require(bad['mismatched_words']==1854 and bad['mismatched_snapshots']==1854,'fault isolation')
        else:require(all(not d['mismatched_words'] for d in bad['groups'] if not d['id'].startswith('mpy')) and bad['mismatched_words']==288 and bad['mismatched_snapshots']==224,'17-bit mutation domain')
        (a.artifacts/(name+'.json')).write_text(json.dumps(bad,indent=2)+'\n')
    text=result.read_text();row=next(line for line in text.splitlines(True) if line.startswith('REG r='))
    faults=[]
    for field in range(8):
        start=6+field*8;altered=row[:start]+f'{int(row[start:start+8],16)^1:08x}'+row[start+8:];faults.append((f'field-{field}',text.replace(row,altered,1),'fail'))
    faults.extend((name,broken,'error') for name,broken in (('missing-row',text.replace(row,'',1)),('bad-row',text.replace(row,'REG r=bad\n',1)),('reordered',text.replace('BEGIN xb0','BEGIN xh0',1)),('false-pass',text.replace('END observations_complete','END 0/0')),('missing-trailer',text[:text.index('#VELOCE end ')]),('unterminated',text.rstrip('\n')),('trailing',text+'extra\n')))
    for name,content,status in faults:
        out=a.artifacts/(name+'.result');out.write_text(content);require(grade(out,baseline,False)['status']==status,'file fault escaped '+name)
    for name in ('xb0','xh0','rev0','mpy0-1','cli','sei'):
        image=a.artifacts/('state-'+name+'.vb');image.write_bytes(build(True,only=name));proc=subprocess.run([str(a.state_test.resolve()),str(a.core.resolve()),str(image.resolve())],capture_output=True,text=True,timeout=30);require(proc.returncode==0,proc.stdout+proc.stderr)
    if not a.controls_only:
        result=a.artifacts/'nintendo_registers.result';execute(a.emulator,HERE/'nintendo_registers.vb',result);report=grade(result,rom);require(report['status']=='pass',json.dumps(report));require(report['observed_snapshots']==16252928,'incomplete observations')
    require(all(hashlib.sha256(Path(x).read_bytes()).hexdigest()==h for x,h in hashes.items()),'capture input changed')
    report.update({'input_sha256':hashes,'rom_sha256':hashlib.sha256(rom).hexdigest(),'executed_controls_rejected':3,'field_faults_rejected':8,'stream_faults_rejected':7,'native_state_continuation_families':6,'hardware_timing':False});(a.artifacts/('controls.json' if a.controls_only else 'nintendo_registers.json')).write_text(json.dumps(report,indent=2)+'\n')
    print(f"VB Nintendo instructions: {report['observed_snapshots']} actual eight-word snapshots PASS; 3 executed controls, 8 field faults, 7 stream faults rejected; 6 native checkpoint families PASS")
if __name__=='__main__':main()
