#!/usr/bin/env python3
"""Compare actual native checkpoint continuation using an original cartridge."""
import argparse,json,subprocess
from pathlib import Path
def require(condition,message):
    if not condition:raise RuntimeError(message)
def main():
    p=argparse.ArgumentParser();p.add_argument('--probe',type=Path,required=True);p.add_argument('--inspect',type=Path,required=True);p.add_argument('--trace',type=Path,required=True);p.add_argument('--core',type=Path,required=True);p.add_argument('--rom',type=Path,required=True);p.add_argument('--other-rom',type=Path,required=True);p.add_argument('--artifacts',type=Path,required=True);a=p.parse_args()
    a.artifacts.mkdir(parents=True,exist_ok=True);a.artifacts=a.artifacts.resolve()
    script=a.artifacts/'input.txt';script.write_text('0 c001\n200 7f80c002\n')
    def run(name,count,load=None,rom=None,expected=0):
        capture=a.artifacts/(name+'.ppm');state=a.artifacts/(name+'.state')
        command=[str(a.probe.resolve()),str(a.core.resolve()),str((rom or a.rom).resolve()),str(count),str(capture),str(script),'--save-state',str(state),'--capture-every','50']
        if load:command+=['--load-state',str(load)]
        result=subprocess.run(command,capture_output=True,text=True,timeout=30)
        require(result.returncode==expected,result.stdout+result.stderr or 'unexpected probe result')
        if expected:return
        report=json.loads(result.stdout);(a.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n')
        require(report['state_written'] and report['capture_written'],'missing checkpoint/capture')
        return report,state,capture
    continuous,full,full_capture=run('continuous',300)
    first,saved,_=run('first',100)
    resumed,replay,replay_capture=run('resumed',200,saved)
    require(resumed['starting_quantum']==100 and resumed['frames']==300 and resumed['capture_quantum']==300,'incorrect absolute continuation boundary')
    require(full.read_bytes()==replay.read_bytes(),'native final states differ after resume')
    require(full_capture.read_bytes()==replay_capture.read_bytes(),'native final framebuffer differs after resume')
    require(resumed['capture_checkpoints']==[150,200,250,300],'checkpoint labels did not retain absolute time')
    run('wrong-rom',1,saved,a.other_rom,2)
    truncated=a.artifacts/'truncated.state';truncated.write_bytes(saved.read_bytes()[:-1]);run('truncated',1,truncated,expected=2)
    run('overflow',99901,saved,expected=2)
    before=full.read_bytes()
    result=subprocess.run([str(a.inspect.resolve()),str(a.rom.resolve()),str(full),'20000','2'],capture_output=True,text=True,timeout=30)
    require(result.returncode==0,result.stdout+result.stderr);observed=json.loads(result.stdout)
    require(observed['frames']==300 and observed['words']==[
        {'address':0x20000,'ram':0xff010401,'cpu_view':0xff010401},
        {'address':0x20004,'ram':0x4000807f,'cpu_view':0x4000807f}], 'checkpoint RAM/cache words mismatch')
    require(full.read_bytes()==before,'inspection modified checkpoint file')
    for rom,start,count in ((a.other_rom,'20000','2'),(a.rom,'20001','2'),(a.rom,'7ffffc','2'),(a.rom,'20000','513')):
        result=subprocess.run([str(a.inspect.resolve()),str(rom.resolve()),str(full),start,count],capture_output=True,text=True,timeout=30)
        require(result.returncode==2,'inspector accepted wrong identity or invalid range')
    traced=subprocess.run([str(a.trace.resolve()),str(a.rom.resolve()),'500',str(script),'--load-state',str(saved)],capture_output=True,text=True,timeout=30)
    require(traced.returncode==0,traced.stdout+traced.stderr);report=json.loads(traced.stdout)
    require(report['starting_quantum']==100 and report['starting_instructions']>0 and report['executed_attempts']==500 and not report['exception_observed'],'native trace did not resume the real checkpoint')
    print('Original native 100+200 vs 300 checkpoint state/framebuffer PASS; absolute captures and wrong-ROM/truncated/bound rejection PASS')
if __name__=='__main__':main()
