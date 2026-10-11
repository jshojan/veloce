#!/usr/bin/env python3
"""Production GUI input routing with independently chosen fixture masks."""
import argparse,json,os,shutil,subprocess
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--emulator',type=Path,required=True);ap.add_argument('--input',type=Path,required=True);ap.add_argument('--core',type=Path,required=True);ap.add_argument('--artifacts',type=Path,required=True);ap.add_argument('--xvfb',required=True);a=ap.parse_args()
root=a.artifacts.resolve();root.mkdir(parents=True,exist_ok=True)
for directory,source in [('cores',a.core),('plugins',a.input)]:
    (root/directory).mkdir(exist_ok=True);shutil.copy2(source,root/directory/source.name)
(root/'config').mkdir(exist_ok=True);(root/'config/plugins.json').write_text(json.dumps({'selected_plugins':{'input':'Capture Input'}}))
rom=root/'probe.vinput';rom.write_bytes(bytes([0]))
capture=root/'input.txt';capture.unlink(missing_ok=True);result=root/'core.result'
env=os.environ.copy()
for key in ('INPUT','FRAMES','SAVE_SCREENSHOT','DEBUG','TRACE','VELOCE_INPUT_REJECT','VELOCE_TEST_RESETS'):env.pop(key,None)
env.update(HEADLESS='0',SDL_AUDIODRIVER='dummy',VELOCE_TEST_OUT=str(result),VELOCE_TEST_EXIT='1',VELOCE_INPUT_CAPTURE=str(capture))
run=subprocess.run([a.xvfb,'-a',str(a.emulator.resolve()),str(rom)],cwd=root,env=env,capture_output=True,text=True,timeout=40)
(root/'frontend.log').write_text(f'exit={run.returncode}\n'+run.stdout+run.stderr);assert run.returncode==0,(run.returncode,run.stdout,run.stderr)
lines=capture.read_text().splitlines();assert lines[:2]==['destroy','initialize'] and lines[-2:]==['shutdown','destroy'],lines
assert [line for line in lines if line.startswith('begin ')]==['begin 0','begin 1','begin 2'],lines
assert [line for line in lines if line.startswith('sample ')]==['sample 0 1','sample 1 2048','sample 0 2','sample 1 2048','sample 0 2097152','sample 1 2048'],lines
assert 'layout Input Fixture' in lines,lines
text=result.read_text();assert [line for line in text.splitlines() if line.startswith('INPUT ')]==['INPUT frame=0 buttons=1','INPUT frame=1 buttons=1','INPUT frame=2 buttons=2','INPUT frame=3 buttons=2097152'],text
assert 'frames=4 cycles=300 status=0' in text,text
print('Production GUI selected input plugin delivers exact masks and polls each controller once per logical frame PASS')
