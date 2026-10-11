#!/usr/bin/env python3
"""Isolate production Application routing with the native NES original probe."""
import argparse,os,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser()
for key in ('test','core','input','audio','tas','rom','artifacts','xvfb'):p.add_argument('--'+key,required=True)
a=p.parse_args();root=Path(a.artifacts).resolve();root.mkdir(parents=True,exist_ok=True)
for directory,keys in [('cores',['core']),('plugins',['input','audio','tas'])]:
    dest=root/directory;dest.mkdir(exist_ok=True)
    for key in keys:
        src=Path(getattr(a,key)).resolve();shutil.copy2(src,dest/src.name)
env=os.environ.copy()
for key in ('VELOCE_TEST_OUT','VELOCE_TEST_EXIT','INPUT','FRAMES','DEBUG','TRACE'):env.pop(key,None)
env.update(HEADLESS='0',SDL_AUDIODRIVER='dummy')
r=subprocess.run([a.xvfb,'-a',str(Path(a.test).resolve()),str(Path(a.rom).resolve())],cwd=root,env=env,capture_output=True,text=True,timeout=80)
(root/'frontend.log').write_text(r.stdout+r.stderr)
assert r.returncode==0,(r.returncode,r.stdout,r.stderr)
rows=[line.split() for line in (root/'ram-observations.txt').read_text().splitlines()]
phases={phase:[row[1:] for row in rows if row[0]==phase] for phase in ('ordinary','recording','playback','mutant')}
assert len(phases['ordinary'])==121 and phases['ordinary']==phases['recording']==phases['playback']
assert phases['mutant']!=phases['ordinary'],'input mutant must change cartridge RAM'
print(r.stdout.splitlines()[-1])
