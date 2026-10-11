#!/usr/bin/env python3
"""Native Application movie regression; optional user-owned retail checkpoint."""
import argparse,hashlib,json,os,re,shutil,subprocess
from pathlib import Path
def require(ok,message):
    if not ok:raise RuntimeError(message)
def main():
    p=argparse.ArgumentParser()
    for name in ('test','core','input','audio','tas','rom','probe','inspect','artifacts','xvfb'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--checkpoint',type=Path);p.add_argument('--word',default='7f80c002');p.add_argument('--frames',type=int,default=180)
    a=p.parse_args();root=a.artifacts.resolve();root.mkdir(parents=True,exist_ok=True)
    for directory,keys in [('cores',['core']),('plugins',['input','audio','tas'])]:
        destination=root/directory;destination.mkdir(exist_ok=True)
        for key in keys:
            source=getattr(a,key).resolve();shutil.copy2(source,destination/source.name)
    checkpoint=a.checkpoint.resolve() if a.checkpoint else root/'original.state'
    require(checkpoint not in {root/('checkpoint-'+name+'.state') for name in ('baseline','recorded','playback','wrong-input')},'Use a separate input checkpoint')
    if not a.checkpoint:
        result=subprocess.run([str(a.probe.resolve()),str(a.core.resolve()),str(a.rom.resolve()),'100','-','-','--save-state',str(checkpoint)],capture_output=True,text=True,timeout=30)
        require(result.returncode==0,result.stdout+result.stderr)
    env=os.environ.copy()
    for key in ('VELOCE_TEST_OUT','VELOCE_TEST_EXIT','INPUT','INPUT_FILE','FRAMES','DEBUG','TRACE','SAVE_SCREENSHOT'):env.pop(key,None)
    env.update(HEADLESS='0',SDL_AUDIODRIVER='dummy')
    observed=subprocess.run([str(a.xvfb.resolve()),'-a',str(a.test.resolve()),str(a.rom.resolve()),'--checkpoint',str(checkpoint),str(a.frames),a.word],cwd=root,env=env,capture_output=True,text=True,timeout=180)
    (root/'frontend.log').write_text(observed.stdout+observed.stderr)
    require(observed.returncode==0,observed.stdout+observed.stderr)
    files=[root/('checkpoint-'+name+'.state') for name in ('baseline','recorded','playback','wrong-input')]
    payloads=[path.read_bytes() for path in files]
    require(payloads[0]==payloads[1]==payloads[2] and payloads[0]!=payloads[3],'native movie state comparison failed')
    if not a.checkpoint:
        for path,expected in ((files[0],0x4000807f),(files[3],0)):
            result=subprocess.run([str(a.inspect.resolve()),str(a.rom.resolve()),str(path),'20004','1'],capture_output=True,text=True,timeout=30)
            require(result.returncode==0,result.stdout+result.stderr)
            require(json.loads(result.stdout)['words']==[{'address':0x20004,'ram':expected,'cpu_view':expected}],'original guest SI RAM mismatch')
    report={'status':'pass','frames':a.frames,'input_word':a.word,'private_checkpoint':bool(a.checkpoint),'exact_recording_playback_states':True,'executed_neutral_control_differs':True,
            'checkpoint_sha256':hashlib.sha256(checkpoint.read_bytes()).hexdigest(),
            'rom_sha256':hashlib.sha256(a.rom.read_bytes()).hexdigest(),
            'states':{path.name:hashlib.sha256(data).hexdigest() for path,data in zip(files,payloads)}}
    report['executables']={name:hashlib.sha256((root/('cores' if name=='core' else 'plugins')/getattr(a,name).name).read_bytes()).hexdigest() for name in ('core','input','audio','tas')}
    report['test_sha256']=hashlib.sha256(a.test.read_bytes()).hexdigest()
    passed=[line for line in observed.stdout.splitlines() if line.endswith('PASS')]
    require(len(passed)==1 and passed[0].startswith(f'{a.frames} native checkpoint movie frames;'),'missing native movie verdict')
    match=re.search(r'states and (\d+) seek boundaries match;',passed[0]);require(match,'missing seek observations')
    report['seek_boundaries']=int(match[1]);(root/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(passed[0])
if __name__=='__main__':main()
