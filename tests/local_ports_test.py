#!/usr/bin/env python3
"""Read actual four-port guest SI replies after SDL/default-plugin/App routing."""
import argparse,csv,hashlib,itertools,json,os,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser()
for key in ('test','core','input','audio','tas','rom','artifacts','xvfb'):p.add_argument('--'+key,required=True)
a=p.parse_args();root=Path(a.artifacts).resolve();root.mkdir(parents=True,exist_ok=True)
for directory,keys in [('cores',['core']),('plugins',['input','audio','tas'])]:
    dest=root/directory;dest.mkdir(exist_ok=True)
    for key in keys:
        src=Path(getattr(a,key)).resolve();shutil.copy2(src,dest/src.name)
# Device configuration from previous executions must not affect the experiment.
shutil.rmtree(root/'config',ignore_errors=True)
env=os.environ.copy()
for key in ('VELOCE_TEST_OUT','VELOCE_TEST_EXIT','INPUT','FRAMES','DEBUG','TRACE'):env.pop(key,None)
env.update(HEADLESS='0',SDL_AUDIODRIVER='dummy',SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='1')
def hashes():
    return {key:hashlib.sha256(Path(getattr(a,key)).read_bytes()).hexdigest() for key in ('test','core','input','audio','tas','rom')}
before=hashes()
r=subprocess.run([a.xvfb,'-a',str(Path(a.test).resolve()),str(Path(a.rom).resolve()),'--local-ports'],cwd=root,env=env,capture_output=True,text=True,timeout=100)
(root/'frontend.log').write_text(r.stdout+r.stderr)
assert r.returncode==0,(r.returncode,r.stdout,r.stderr)
assert before==hashes(),'loaded binaries or ROM changed during capture'
rows=list(csv.DictReader((root/'local-ports.csv').open()))
assert len(rows)==195*4,len(rows)
# Native controller wire bit positions; no implementation encoder imported.
native=(15,14,13,12,11,10,9,8,5,4,3,2,1,0)
scenarios=[(order,(0,1,3,4),(80,-80,0,0),(0,0,80,-80)) for order in itertools.permutations(range(4))]
for device in range(4):
    for control in range(14):
        for x,y in ((-80,80),(80,-80),(0,0)):
            controls=[-1]*4;xs=[0]*4;ys=[0]*4
            controls[device]=control;xs[device]=x;ys[device]=y
            scenarios.append(((0,1,2,3),controls,xs,ys))
scenarios.extend([((0,1,2,3),[-1]*4,[0]*4,[0]*4)]*2)
scenarios.append(((0,1,2,3),(0,-1,-1,-1),(0,0,0,0),(0,0,0,0)))
assert len(scenarios)==195
errors=[];negative_errors=[]
for n,row in enumerate(rows):
    row={k:int(v) for k,v in row.items()}
    assert row['case']==n//4 and row['port']==n%4,row
    order,controls,xs,ys=scenarios[n//4];device=order[n%4];control=controls[device];x=xs[device];y=ys[device]
    assert (row['device'],row['control'],row['x'],row['y'])==(device,control,x,y),'incorrect scenario identity'
    buttons=0 if control==-1 else 1<<native[control]
    expected=(buttons<<16)|((x%256)<<8)|(y%256)
    if (row['reply'],row['status'],row['cleared'])!=(expected,4096,0):(negative_errors if row['case']==194 else errors).append(dict(row,expected=expected))
report={'status':'fail' if errors else 'pass','cases':194,'guest_words':194*6,'mismatches':len(errors),'examples':errors[:8]}
(root/'local-ports.json').write_text(json.dumps(report,indent=2)+'\n')
(root/'provenance.json').write_text(json.dumps({'sha256':before,'native_cases':194,'executed_wrong_route_cases':1,'capture_sha256':hashlib.sha256((root/'local-ports.csv').read_bytes()).hexdigest()},indent=2)+'\n')
assert not errors,report
assert len(negative_errors)==2,negative_errors
(root/'executed-wrong-route.json').write_text(json.dumps({'status':'fail','mismatches':len(negative_errors),'examples':negative_errors},indent=2)+'\n')
# The two last cases actually executed neutral and mapping capture after held input.
assert all(int(row['reply'])==0 for row in rows[192*4:194*4])
print('194 SDL/default-input/ordinary frontend/native N64 cases; all 24 assignments, 14 buttons, four ports, stick endpoints/dead zone and executed neutral/capture controls and wrong-route rejection PASS')
