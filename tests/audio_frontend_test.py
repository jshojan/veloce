#!/usr/bin/env python3
"""Production GUI execution through dynamically loaded audio/core fixtures."""
import argparse,json,os,shutil,subprocess
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--emulator',type=Path,required=True);ap.add_argument('--audio',type=Path,required=True);ap.add_argument('--core',type=Path,required=True);ap.add_argument('--artifacts',type=Path,required=True);ap.add_argument('--xvfb',required=True);a=ap.parse_args()
for mode in ('stream','batch'):
    root=a.artifacts.resolve()/mode;root.mkdir(parents=True,exist_ok=True)
    for directory,source in [('cores',a.core),('plugins',a.audio)]:
        (root/directory).mkdir(exist_ok=True);shutil.copy2(source,root/directory/source.name)
    (root/'config').mkdir(exist_ok=True);(root/'config/plugins.json').write_text(json.dumps({'selected_plugins':{'audio':'Capture Audio'}}))
    rom=root/'probe.vaudio';rom.write_bytes(bytes([mode=='stream']))
    capture=root/'audio.txt';capture.unlink(missing_ok=True);result=root/'core.result'
    env=os.environ.copy()
    for key in ('INPUT','FRAMES','SAVE_SCREENSHOT','DEBUG','TRACE','VELOCE_AUDIO_REJECT','VELOCE_TEST_RESETS'):env.pop(key,None)
    env.update(HEADLESS='0',SDL_AUDIODRIVER='dummy',VELOCE_TEST_OUT=str(result),VELOCE_TEST_EXIT='1',VELOCE_AUDIO_CAPTURE=str(capture))
    try:
        run=subprocess.run([a.xvfb,'-a',str(a.emulator.resolve()),str(rom)],cwd=root,env=env,capture_output=True,text=True,timeout=40)
    except subprocess.TimeoutExpired as e:
        (root/'frontend.log').write_bytes((e.stdout or b'')+(e.stderr or b''))
        raise
    (root/'frontend.log').write_text(f'exit={run.returncode}\n'+run.stdout+run.stderr);assert run.returncode==0,(run.returncode,run.stdout,run.stderr)
    lines=capture.read_text().splitlines();assert lines.pop(0)=='destroy',lines # registry discovery instance
    assert lines[0]=='initialize' and lines[-2:]==['shutdown','destroy'],lines
    assert len(lines)==6,lines
    for n,(line,rate) in enumerate(zip(lines[1:4],(32040,48000,44100)),1):
        fields=line.split();assert fields[:5]==['process',str(rate),'2','32','4'],line
        assert list(map(float,fields[5:]))==[n/16,-n/16,0.5,-0.25],line
    assert 'frames=3 cycles=300 status=0' in result.read_text(),result.read_text()
print('Production GUI streaming and batch audio each deliver exactly three source-rate sample buffers PASS')
