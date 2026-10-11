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
    result={key:hashlib.sha256(Path(getattr(a,key)).read_bytes()).hexdigest() for key in ('test','core','input','audio','tas','rom')}
    repo=Path(__file__).resolve().parent.parent
    for name in ('tests/local_ports_test.py','tests/cpp/tas_frontend_test.cpp','src/core/input_manager.cpp','src/core/input_manager.hpp','include/emu/n64_input.hpp','plugins/input_default/src/default_input_plugin.cpp','tests/roms-src/n64/controller_netplay/build.py','tests/roms-src/n64/controller_netplay/recipe.json'):
        result[name]=hashlib.sha256((repo/name).read_bytes()).hexdigest()
    return result
before=hashes()
r=subprocess.run([a.xvfb,'-a',str(Path(a.test).resolve()),str(Path(a.rom).resolve()),'--local-ports'],cwd=root,env=env,capture_output=True,text=True,timeout=180)
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

# Secondary profiles are graded independently from the producer's declared maps.
profile_rows=list(csv.DictReader((root/'player-profiles.csv').open()))
assert len(profile_rows)==3027*4,len(profile_rows)
profile_scenarios=[]
sources=(0,1,2,3,4,6,9,10,11,12,13,14)
for port in range(1,4):
    for control in range(14):
        for source in sources:
            for held in range(2):profile_scenarios.append((port,control,held,0,0))
for port in range(1,4):
    for control in range(14):
        for axis in range(6):
            for positive in range(2):
                for level in range(4):
                    pressed=level>=2 and (axis<4 or positive==1)
                    analog=(0,33,33,56)[level]*(1 if positive else -1)
                    x=analog if axis==0 else 0;y=-analog if axis==1 else 0
                    profile_scenarios.append((port,control,pressed,x,y))
assert len(profile_scenarios)==3024
profile_scenarios.extend([None,None,(1,0,True,0,0)])
profile_errors=[];profile_negative=[]
for n,row in enumerate(profile_rows):
    row={k:int(v) for k,v in row.items()};case=n//4;port=n%4
    assert (row['case'],row['port'])==(case,port)
    scenario=profile_scenarios[case]
    if scenario is None:expected=0x80000000 if port>0 else 0
    else:
        subject,control,pressed,x,y=scenario
        expected=(((1<<native[control]) if pressed else 0)<<16)|((x%256)<<8)|(y%256) if port==subject else 0
    if (row['reply'],row['status'],row['cleared'])!=(expected,4096,0):
        (profile_negative if case==3026 else profile_errors).append(dict(row,expected=expected))
profile_report={'status':'fail' if profile_errors else 'pass','cases':3026,'guest_words':3026*6,'mismatches':len(profile_errors),'examples':profile_errors[:8]}
(root/'player-profiles.json').write_text(json.dumps(profile_report,indent=2)+'\n')
assert not profile_errors,profile_report
assert len(profile_negative)==1,profile_negative
(root/'executed-wrong-profile.json').write_text(json.dumps({'status':'fail','mismatches':len(profile_negative),'examples':profile_negative},indent=2)+'\n')
print('3026 independently graded player-profile cases / 18156 guest RAM words; 12 button sources, six axes, polarity/threshold neighbors, persistence/port moves and executed wrong-profile rejection PASS')

provenance=json.loads((root/'provenance.json').read_text())
provenance['player_profile_cases']=3026
provenance['executed_wrong_profile_cases']=1
provenance['player_profiles_capture_sha256']=hashlib.sha256((root/'player-profiles.csv').read_bytes()).hexdigest()
provenance['remapped_movie_sha256']=hashlib.sha256((root/'four-local-ports.tas').read_bytes()).hexdigest()
(root/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')

# Analog policy oracle: no frontend quantizer imported.
import math
stick_rows=list(csv.DictReader((root/'stick-profiles.csv').open()))
assert len(stick_rows)==4036*4
stick_scenarios=[]
values=(-32768,-16384,-4915,0,4915,16383,16384,32767)
def quantized(source,invert,deadzone,extent,value):
    if source<0:return 0
    actual=value if source<4 else max(0,value)
    normal=max(-1,min(1,actual/32767))*(-1 if invert else 1)
    magnitude=abs(normal)
    if magnitude<=deadzone:return 0
    result=math.floor((magnitude-deadzone)*extent/(1-deadzone)+.5)
    return result*(-1 if normal<0 else 1)
for subject in range(4):
    for axis in range(-1,6):
        for invert in (False,True):
            for deadzone in (0,.15,.5):
                for extent in (40,80,127):
                    for value in values:
                        x=quantized(axis,invert,deadzone,extent,value)
                        y=quantized(-1 if axis<0 else (axis+1)%6,not invert,deadzone,extent,value)
                        stick_scenarios.append([((x%256)<<8)|(y%256) if port==subject else 0 for port in range(4)])
def persistent(order):
    return [(((40+port*20)*(1 if device<2 else -1)*(-1 if port%2 else 1))%256)<<8|(((40+port*20)*(1 if device%2 else -1))%256) for port,device in enumerate(order)]
persist=persistent((0,1,2,3));moved=persistent((0,3,1,2))
stick_scenarios.extend([persist,moved,[0]*4,moved])
assert len(stick_scenarios)==4036
stick_errors=[];stick_negative=[]
for n,row in enumerate(stick_rows):
    row={k:int(v) for k,v in row.items()};case=n//4;port=n%4
    assert (row['case'],row['port'])==(case,port)
    if (row['reply'],row['status'],row['cleared'])!=(stick_scenarios[case][port],4096,0):
        (stick_negative if case==4035 else stick_errors).append(dict(row,expected=stick_scenarios[case][port]))
report={'status':'fail' if stick_errors else 'pass','cases':4035,'guest_words':4035*6,'mismatches':len(stick_errors),'examples':stick_errors[:8]}
(root/'stick-profiles.json').write_text(json.dumps(report,indent=2)+'\n')
assert not stick_errors,report
assert len(stick_negative)==1,stick_negative
(root/'executed-wrong-stick.json').write_text(json.dumps({'status':'fail','mismatches':1,'examples':stick_negative},indent=2)+'\n')
provenance=json.loads((root/'provenance.json').read_text());provenance['stick_profile_cases']=4035
provenance['stick_profile_capture_sha256']=hashlib.sha256((root/'stick-profiles.csv').read_bytes()).hexdigest()
(root/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
print('4035 analog profile cases / 24210 guest words; persistence/port moves, capture, legacy migration, twelve malformed-config rejects, remapped native movies and executed wrong-source rejection PASS')
