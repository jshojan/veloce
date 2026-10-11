#!/usr/bin/env python3
"""Optional private SM64 VI-field controls; no retail bytes enter git."""
import argparse,json,math,struct,subprocess
from pathlib import Path
from n64_retail_controls import ROM_SHA,SOURCE,sha,require
ROOT=Path(__file__).resolve().parents[1]
CASES=[('walk',12,'5000c000'),('neutral',12,'c000'),('jump',2,'c001'),('jump-control',2,'c000'),('attack',2,'c002'),('attack-control',2,'c000')]

def grade_actions(observations):
    origin=observations['ready']['position']
    for name in ('neutral','jump-control','attack-control'):require(observations[name]['action']==0x0c400201 and observations[name]['position']==origin,'neutral control moved Mario')
    walk=observations['walk'];jump=observations['jump'];attack=observations['attack']
    require(walk['action']==0x04000440 and math.dist(walk['position'],origin)>1,'analog input did not cause walking')
    require(jump['action']==0x03000880 and jump['position'][1]>origin[1],'A did not cause an ascending jump')
    require(attack['action']==0x00800380,'B did not cause a punch')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('rom',type=Path);p.add_argument('--checkpoint',type=Path,required=True);p.add_argument('--artifacts',type=Path,required=True)
    p.add_argument('--core',type=Path,default=ROOT/'build/bin/cores/n64.so');p.add_argument('--probe',type=Path,default=ROOT/'build/bin/retail_core_probe');p.add_argument('--inspect',type=Path,default=ROOT/'build/bin/n64_state_inspect')
    p.add_argument('--trace',type=Path,default=ROOT/'build/bin/n64_boot_trace');p.add_argument('--check-existing',action='store_true');p.add_argument('--movie-artifacts',type=Path)
    a=p.parse_args();require(sha(a.rom)==ROM_SHA,'watch fields apply only to this SM64 US image');root=a.artifacts;root.mkdir(parents=True,exist_ok=True)
    inputs=[a.rom,a.checkpoint,a.core,a.probe,a.inspect]
    outputs={root/(name+'.state') for name in ('ready',*(case[0] for case in CASES))}
    require(not ({x.resolve() for x in inputs}&{x.resolve() for x in outputs}),'Use a separate input checkpoint')
    fingerprints={str(x.resolve()):sha(x) for x in inputs};trace_sha=sha(a.trace);provenance=root/'provenance.json'
    if a.check_existing:require(json.loads(provenance.read_text())==fingerprints,'inputs changed since actual execution')
    else:provenance.write_text(json.dumps(fingerprints,indent=2)+'\n')
    def inspect(state):
        before=sha(state);r=subprocess.run([str(a.inspect.resolve()),str(a.rom.resolve()),str(state.resolve()),'33b170','32'],capture_output=True,text=True,timeout=30)
        require(r.returncode==0,r.stdout+r.stderr);raw=json.loads(r.stdout);require(sha(state)==before,'inspection changed checkpoint')
        require([w['address'] for w in raw['words']]==list(range(0x33b170,0x33b1f0,4)),'incomplete native RAM range')
        words=[w['cpu_view'] for w in raw['words']];position=[struct.unpack('>f',struct.pack('>I',words[i]))[0] for i in (15,16,17)]
        require(all(math.isfinite(x) for x in position),'nonfinite position');require(not raw['rsp_blocked'] and not raw['dp_blocked'],'native RSP/DP stop')
        require(raw['scheduling']=='video-field' and raw['vi_phase']==0,'not a VI-field checkpoint')
        return dict(action=words[3],position=position,raw=raw,state_sha256=before)
    def execute(name,count,word,checkpoint,start):
        state=root/(name+'.state');script=root/(name+'.txt');report=root/(name+'.json')
        if not a.check_existing:
            script.write_text(f'{start} {word}\n')
            cmd=[str(a.probe.resolve()),str(a.core.resolve()),str(a.rom.resolve()),str(count),str((root/(name+'.ppm')).resolve()),str(script.resolve()),'--load-state',str(checkpoint.resolve()),'--save-state',str(state.resolve()),'--scheduling','video-field']
            r=subprocess.run(cmd,capture_output=True,text=True,timeout=120);require(r.returncode==0,r.stdout+r.stderr);report.write_text(r.stdout)
        meta=json.loads(report.read_text());require(meta['loaded'] and meta['state_written'] and meta['capture_written'] and meta['scheduling']=='video-field','incomplete native probe')
        require(meta['starting_quantum']==start and meta['requested_quanta']==count and meta['frames']==start+count,'wrong continuation boundary')
        observation=inspect(state);require(observation['raw']['frames']==start+count,'state/report mismatch')
        require(meta['executed_attempts']==observation['raw']['instructions']-source_instructions[sha(checkpoint)],'instruction delta mismatch')
        # Each case holds one constant input word. Raw tracing therefore uses
        # the same input throughout the exact observed attempt interval, without
        # pretending that its 100k diagnostic labels are VI fields.
        trace_input=root/(name+'.trace-input.txt');trace_input.write_text(f'{start} {word}\n')
        command=[str(a.trace.resolve()),str(a.rom.resolve()),str(meta['executed_attempts']),str(trace_input.resolve()),'--load-state',str(checkpoint.resolve())]
        traced=subprocess.run(command,capture_output=True,text=True,timeout=120);require(traced.returncode==0,traced.stdout+traced.stderr)
        trace=json.loads(traced.stdout);(root/(name+'-trace.json')).write_text(traced.stdout)
        require(trace['executed_attempts']==meta['executed_attempts'] and not any(trace[k] for k in ('exception_observed','rsp_blocked','dp_blocked')),'CPU/RSP/DP stop in matched-attempt continuation')
        observation['trace']=trace
        return meta,observation
    # Read the input checkpoint without requiring its legacy schedule to be VI.
    result=subprocess.run([str(a.inspect.resolve()),str(a.rom.resolve()),str(a.checkpoint.resolve()),'33b170','32'],capture_output=True,text=True,timeout=30)
    require(result.returncode==0,result.stdout+result.stderr);seed=json.loads(result.stdout);source_instructions={sha(a.checkpoint):seed['instructions']}
    ready_meta,ready=execute('ready',1,'c000',a.checkpoint,seed['frames']);start=ready['raw']['frames'];source_instructions[ready['state_sha256']]=ready['raw']['instructions']
    require(ready['action']==0x0c400201 and ready['raw']['vi_fields']==seed['vi_fields']+1,'ready checkpoint is not standing at its first VI boundary')
    observations={'ready':ready}
    for name,count,word in CASES:
        meta,observation=execute(name,count,word,root/'ready.state',start);observations[name]=observation
        require(observation['raw']['vi_fields']==ready['raw']['vi_fields']+count,'frontend call did not finish exactly one VI field')
    grade_actions(observations)
    origin=ready['position'];walk=observations['walk'];jump=observations['jump']
    negatives={}
    for target,control in [('walk','neutral'),('jump','jump-control'),('attack','attack-control')]:
        wrong=dict(observations);wrong[target]=observations[control]
        try:grade_actions(wrong)
        except ValueError as error:negatives[target]=str(error)
        else:raise ValueError('executed neutral substitution escaped: '+target)
    movie=None
    if a.movie_artifacts:
        mroot=a.movie_artifacts;meta=json.loads((mroot/'report.json').read_text())
        require(meta['rom_sha256']==ROM_SHA and meta['checkpoint_sha256']==ready['state_sha256'] and meta['input_word']=='5000c000' and meta['executables']['core']==sha(a.core),'movie input/core/checkpoint mismatch')
        files={name:mroot/name for name in meta['states']};require(all(sha(file)==meta['states'][name] for name,file in files.items()),'movie state fingerprints differ')
        values=[files['checkpoint-'+name+'.state'].read_bytes() for name in ('baseline','recorded','playback')];require(values[0]==values[1]==values[2],'native movie states differ')
        moving=inspect(files['checkpoint-baseline.state']);neutral=inspect(files['checkpoint-wrong-input.state'])
        for obs in (moving,neutral):require(obs['raw']['vi_fields']==ready['raw']['vi_fields']+meta['frames'] and obs['raw']['frames']==start+meta['frames'],'movie advanced a different VI interval')
        require(math.dist(moving['position'],origin)>1 and neutral['position']==origin and neutral['action']==0x0c400201,'neutral movie did not change actual guest behavior')
        movie=dict(frames=meta['frames'],seek_boundaries=meta['seek_boundaries'],full_states_equal=True,moving=moving,neutral=neutral)
    require(all(sha(Path(x))==value for x,value in fingerprints.items()) and sha(a.trace)==trace_sha,'inputs changed during observations')
    evidence=dict(status='native-vi-courtyard-controls-pass',trace_sha256=trace_sha,executed_neutral_substitutions=negatives,walk_distance=math.dist(walk['position'],origin),jump_height=jump['position'][1]-origin[1],observations=observations,movie=movie,full_playability_verified=False,watch_sources=[SOURCE+'Structs/Configurations/MarioConfig.cs',SOURCE+'Config/MarioActions.xml'])
    (root/'vi-controls-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(f"Native VI controls PASS: walk {evidence['walk_distance']:.3f}, jump +{evidence['jump_height']:.3f}, punch; neutral controls stationary; full playability unverified")
if __name__=='__main__':main()
