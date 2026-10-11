#!/usr/bin/env python3
"""Grade bounded private SM64 US courtyard controls, never full playability."""
import argparse,hashlib,json,math,struct,subprocess
from pathlib import Path
ROM_SHA='17ce077343c6133f8c9f2d6d6d9a4ab62c8cd2aa57c40aea1f490b4c8bb21d91'
SOURCE='https://github.com/SM64-TAS-ABC/STROOP/blob/6771197ad0f50718d3d4a13387e01d1e6d9c0d1a/STROOP/'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def require(ok,message):
    if not ok:raise ValueError(message)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('rom',type=Path);p.add_argument('--artifacts',type=Path,required=True);p.add_argument('--inspect',type=Path,default=Path(__file__).resolve().parents[1]/'build/bin/n64_state_inspect');p.add_argument('--movie-artifacts',type=Path);p.add_argument('--output',type=Path)
    a=p.parse_args();require(sha(a.rom)==ROM_SHA,'watch fields apply only to this SM64 US image')
    inspector_sha=sha(a.inspect);root=a.artifacts
    def observe(name,state,expected_frames):
        state_sha=sha(state)
        result=subprocess.run([str(a.inspect.resolve()),str(a.rom.resolve()),str(state.resolve()),'33b170','32'],capture_output=True,text=True,timeout=30)
        require(result.returncode==0,result.stdout+result.stderr or 'state inspection rejected')
        raw=json.loads(result.stdout);require(raw['frames']==expected_frames and not raw['rsp_blocked'] and not raw['dp_blocked'],'wrong observation boundary or device stop')
        require([w['address'] for w in raw['words']]==list(range(0x33b170,0x33b1f0,4)),'incomplete watch range')
        require(sha(state)==state_sha and sha(a.inspect)==inspector_sha,'checkpoint or inspector changed during observation')
        words={w['address']:w['cpu_view'] for w in raw['words']}
        position=[struct.unpack('>f',struct.pack('>I',words[address]))[0] for address in (0x33b1ac,0x33b1b0,0x33b1b4)]
        require(all(math.isfinite(v) for v in position),'nonfinite Mario position')
        return dict(name=name,quantum=expected_frames,state_sha256=state_sha,action=words[0x33b17c],position=position,raw_words=raw['words'])
    observations={};reports={}
    for name in ('ready','neutral','walk','jump-control','jump','attack-control','attack'):
        report=json.loads((root/(name+'.json')).read_text());state=root/(name+'.state')
        require(report['rom_sha256']==ROM_SHA and report['status']=='execution-smoke','wrong image or unsuccessful probe')
        require(sha(state)==report['save_state']['sha256'],'state does not match probe report')
        require(sha(Path(report['input_script']['path']))==report['input_script']['sha256'],'input script changed since the probe')
        trace=report['boot_trace'];require(not any(trace[k] for k in ('exception_observed','rsp_blocked','dp_blocked')),'CPU/RSP/DP stop')
        require(trace['executed_attempts']==report['requested_quanta']*100000 and trace['starting_quantum']==report['starting_quantum'],'trace ran a different continuation interval')
        reports[name]=report;observations[name]=observe(name,state,report['frames'])
    start=observations['ready'];ready_sha=start['state_sha256'];origin=start['position']
    require(start['action']==0x0c400201,'ready checkpoint is not standing')
    for name,count in (('neutral',1000),('walk',1000),('jump-control',150),('jump',150),('attack-control',100),('attack',100)):
        report=reports[name];require(report['load_state']['sha256']==ready_sha and report['starting_quantum']==start['quantum'] and report['requested_quanta']==count and report['frames']==start['quantum']+count,'controls do not share their checkpoint/duration')
        require(report['core_sha256']==reports['ready']['core_sha256'],'controls used different cores')
    for name in ('neutral','jump-control','attack-control'):
        require(observations[name]['position']==origin and observations[name]['action']==0x0c400201,'neutral control moved or changed action')
    walk=observations['walk'];jump=observations['jump'];attack=observations['attack']
    distance=math.dist(walk['position'],origin)
    require(distance>1 and walk['action']==0x04000440,'analog input did not cause walking')
    require(jump['position'][1]>origin[1] and jump['action']==0x03000880,'A did not cause an ascending jump')
    require(attack['action']==0x00800380,'B did not cause a punch')
    movie=None
    if a.movie_artifacts:
        mroot=a.movie_artifacts;meta=json.loads((mroot/'report.json').read_text())
        require(meta['private_checkpoint'] and meta['rom_sha256']==ROM_SHA and meta['checkpoint_sha256']==ready_sha and meta['input_word']=='5000c000','movie used a different checkpoint/input')
        require(meta['executables']['core']==reports['ready']['core_sha256'],'movie used a different core')
        states={name:mroot/name for name in meta['states']}
        require(all(sha(path)==meta['states'][name] for name,path in states.items()),'movie state fingerprint differs')
        values=[states['checkpoint-'+name+'.state'].read_bytes() for name in ('baseline','recorded','playback')]
        require(values[0]==values[1]==values[2],'movie recording/playback states differ')
        moving=observe('movie-baseline',states['checkpoint-baseline.state'],start['quantum']+meta['frames'])
        neutral=observe('movie-neutral',states['checkpoint-wrong-input.state'],start['quantum']+meta['frames'])
        require(moving['action']==0x04000440 and math.dist(moving['position'],origin)>1 and neutral['action']==0x0c400201 and neutral['position']==origin,'movie neutral control did not change actual guest behavior')
        movie=dict(frames=meta['frames'],seek_boundaries=meta['seek_boundaries'],full_states_equal=True,moving=moving,neutral=neutral)
    report=dict(status='controlled-courtyard-input-pass',rom_sha256=ROM_SHA,inspector_sha256=inspector_sha,watch_sources=[SOURCE+'Structs/Configurations/MarioConfig.cs',SOURCE+'Config/MarioActions.xml'],
                walk_distance=distance,jump_height=jump['position'][1]-origin[1],observations=observations,movie=movie,full_playability_verified=False)
    output=a.output or root/'controls-evidence.json';output.write_text(json.dumps(report,indent=2)+'\n')
    print(f'Private courtyard controls PASS: walk {distance:.3f} units; jump +{report["jump_height"]:.3f}; punch action; neutral controls stationary; full playability unverified')
if __name__=='__main__':main()
