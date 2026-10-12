#!/usr/bin/env python3
"""Independent terminal-count and rational-clock oracle for actual plugin files."""
import argparse,bisect,hashlib,json,subprocess,zlib
from pathlib import Path
from build import build
CPU=93750000
VI=48681818
PAL_VI=49656530
HERE=Path(__file__).resolve().parent
PAL_ARCHIVE=HERE.parent/'vi_field_schedule_pal'/'vi_field_schedule_pal.z64'

def require(ok,message):
    if not ok:raise RuntimeError(message)

def inputs():
    for h in range(4096):yield h,0,0,h,h,0,0,h//2,h*7919%CPU
    for v in range(1024):yield 5,v,0,5,5,0,0,3,v*104729%CPU
    for pattern in range(5):
        for leap in (0,31,21,10):yield 3,5,leap,7,11,pattern,0,1,pattern*17000000
    for pattern in range(5):
        for current in (1,2,3,4):yield 9,6,21,4,15,pattern,current,0,pattern*17000000

def expected(data,vi=VI):
    h,v,leap,a,b,pattern,current,phase,crystal=data
    events=[0];currents=[current];clocks=0;fields=0
    while fields<2:
        period=(b if leap>>pattern&1 else a)+1 if current//2==1 else h+1
        clocks+=period-phase;phase=0;current+=2
        if current>v:
            current=(current%2 if v%2 else 1-current%2)
            if current>v:current=0
            fields+=1;pattern=(pattern+1)%5
        events.append(clocks);currents.append(current)
    attempts=(clocks*CPU-crystal+vi-1)//vi
    last_store=3+4*((attempts-3)//4) if attempts>=3 else 0
    read_clocks=(crystal+(last_store-1)*vi)//CPU if last_store else 0
    guest=currents[bisect.bisect_right(events,read_clocks)-1] if last_store else 0
    return [attempts,current,0,2,pattern,(crystal+attempts*vi)%CPU,guest,2,1]

def evaluate(path,rom=None,vi=VI):
    if rom is None:rom=build()
    result=dict(status='error',cases=5160,observed=0,mismatches=0,examples=[])
    try:
        with path.open() as f:
            def record():
                line=f.readline();require(line.endswith('\n'),'unterminated record');return json.loads(line)
            require(record()==dict(suite='n64-vi-field-schedule',cases=5160,words=19,rom_crc32=zlib.crc32(rom)),'wrong identity/domain')
            for index,data in enumerate(inputs()):
                row=record()
                require(isinstance(row,list) and len(row)==19 and all(type(x)==int and x>=0 for x in row),'invalid register row')
                require(row[:10]==[index,*data],'input order/domain mismatch')
                want=expected(data,vi);result['observed']+=1
                if row[10:]!=want:
                    result['mismatches']+=1
                    if len(result['examples'])<8:result['examples'].append(dict(index=index,actual=row[10:],expected=want))
            require(record()==dict(complete=5160),'missing completion')
            require(not f.read(),'trailing data')
        result['status']='fail' if result['mismatches'] else 'pass'
    except (RuntimeError,ValueError,OSError) as e:result['detail']=str(e)
    return result

def main():
    p=argparse.ArgumentParser();p.add_argument('--capture',type=Path,required=True);p.add_argument('--core',type=Path,required=True);p.add_argument('--artifacts',type=Path,required=True);a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True)
    require(build()==(HERE/'vi_field_schedule.z64').read_bytes(),'archive differs from source')
    pal=build(region=ord('P'))
    require(pal==PAL_ARCHIVE.read_bytes(),'PAL archive differs from source')
    fingerprints={str(x.resolve()):hashlib.sha256(x.read_bytes()).hexdigest() for x in (a.capture,a.core,HERE/'vi_field_schedule.z64',PAL_ARCHIVE)}
    def execute(name,rom,quantum=False,vi=VI):
        image=a.artifacts/(name+'.z64');image.write_bytes(rom);out=a.artifacts/(name+'.registers')
        command=[str(a.capture.resolve()),str(a.core.resolve()),str(image.resolve()),str(out.resolve())]
        if quantum:command.append('quantum')
        r=subprocess.run(command,capture_output=True,text=True,timeout=240);(a.artifacts/(name+'.log')).write_text(r.stdout+r.stderr)
        require(r.returncode==0,r.stdout+r.stderr);report=evaluate(out,rom,vi=vi);report['register_sha256']=hashlib.sha256(out.read_bytes()).hexdigest();(a.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n');return report,out
    baseline,path=execute('vi-fields',build());require(baseline['status']=='pass',json.dumps(baseline))
    pal_baseline,_=execute('vi-fields-pal',pal,vi=PAL_VI);require(pal_baseline['status']=='pass',json.dumps(pal_baseline))
    wrong_region,_=execute('executed-ntsc-clock',build(region=ord('E')),vi=PAL_VI)
    require(wrong_region['status']=='fail' and wrong_region['mismatches']>0,'wrong-region clock escaped')
    for name,image,quantum in [('executed-wrong-register',build(wrong_read=True),False),('executed-old-budget',build(),True)]:
        report,_=execute(name,image,quantum);require(report['status']=='fail' and report['mismatches']>0,'executed control escaped: '+name)
    content=path.read_text();lines=content.splitlines();row=json.loads(lines[1])
    for field in range(10,19):
        changed=list(lines);bad=list(row);bad[field]^=1;changed[1]=json.dumps(bad);out=a.artifacts/f'field-{field}.registers';out.write_text('\n'.join(changed)+'\n');r=evaluate(out);require(r['status']=='fail' and r['mismatches']==1,'ungraded output field')
    for name,broken in [('missing-row','\n'.join(lines[:1]+lines[2:])+'\n'),('missing-footer','\n'.join(lines[:-1])+'\n'),('trailing',content+'{}\n'),('wrong-order',content.replace(lines[1],lines[2],1)),('malformed',content.replace(lines[1],'[]',1)),('wrong-identity',content.replace(str(zlib.crc32(build())),str(zlib.crc32(build())^1),1)),('unterminated',content.rstrip('\n'))]:
        out=a.artifacts/(name+'.registers');out.write_text(broken);require(evaluate(out)['status']=='error','invalid stream accepted: '+name)
    require(all(hashlib.sha256(Path(x).read_bytes()).hexdigest()==sha for x,sha in fingerprints.items()),'executables/archive changed during observations')
    (a.artifacts/'provenance.json').write_text(json.dumps(fingerprints,indent=2)+'\n')
    print('10320 NTSC/PAL VI boundary records PASS; wrong-region clock, wrong-register and old-budget execution, nine output fields and seven malformed streams rejected')

if __name__=='__main__':main()
