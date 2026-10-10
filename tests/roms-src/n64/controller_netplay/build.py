#!/usr/bin/env python3
"""Original MIT four-controller PIF/SI probe and rollback guest; no SDK bytes."""
import argparse,importlib.util
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('original_assembler',HERE.parent/'dp_registers'/'build.py')
asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
SUITE='controller_netplay'
def build(short=False,wrong_capture=False):
    count=16 if short else 16384
    p=asm.Program();asm.header(p,count,6)
    p.i(15,0,24,0xa480);p.i(15,0,25,0xa000);p.i(13,25,25,0x1100)
    p.i(15,0,26,0xa450);p.i(15,0,27,0xa000);p.i(13,27,27,0x2000)
    # A real repeated stereo PCM buffer, long enough to cross output ticks.
    p.i(15,0,8,0x1234);p.i(13,8,8,0xabcd);p.i(15,0,9,0xa070);p.i(13,9,9,0x1000)
    p.i(9,0,10,2048);p.mark('pcm_fill');p.i(43,9,8,0);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'pcm_fill')
    p.i(9,0,8,1);p.i(43,26,8,8);p.i(9,0,8,15);p.i(43,26,8,20)
    p.i(43,26,0,16);p.puts('header');p.mark('sample')
    p.i(15,0,8,0x70);p.i(13,8,8,0x1000);p.i(43,26,8,0);p.i(9,0,8,0x2000);p.i(43,26,8,4)
    # SI write/read DMA: the original guest requests all four native replies.
    p.i(43,24,16,0);p.i(43,24,0,16);p.i(43,24,25,0);p.i(43,24,0,4)
    for port in range(4):
        offset=port*7+3
        p.i(36,25,5,offset);p.r(0,rt=5,rd=5,sa=24)
        for byte,shift in ((1,16),(2,8),(3,0)):
            p.i(36,25,8,offset+byte)
            if shift:p.r(0,rt=8,rd=8,sa=shift)
            p.r(37,5,8,5)
        if wrong_capture and port==0:p.i(14,5,5,1)
        p.i(43,27,5,port*4)
    p.i(39,24,5,24);p.i(43,27,5,16);p.i(43,24,0,24)
    p.i(39,24,5,24);p.i(43,27,5,20)
    p.puts('row')
    for offset in range(0,24,4):p.i(39,27,5,offset);p.jump(3,'hex')
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,27,27,24)
    # Ensure each quantum produces audio so the network replay-output gate runs.
    p.i(9,0,8,3000);p.mark('delay');p.i(9,8,8,-1);p.branch(5,8,0,'delay')
    p.i(9,17,17,-1);p.branch(4,17,0,'complete_run')
    p.i(43,30,0,12);p.branch(4,0,0,'sample')
    p.mark('complete_run');p.puts('complete');p.i(43,30,0,4)
    p.mark('halt');p.branch(4,0,0,'halt');asm.output_routines(p)
    p.string('header',f'VELOCE 1 n64 {SUITE}\nBEGIN data cases={count} words=6\n')
    p.string('row','REG r=');p.string('complete',f'LOG complete cases={count}\nEND observations_complete\n')
    while p.pc&7:p.data.append(0)
    p.mark('table');packet=bytearray(64)
    for port in range(4):packet[port*7:port*7+3]=bytes((1,4,1))
    packet[28]=0xfe;packet[63]=1;p.data.extend(packet)
    while len(p.data)%4:p.data.append(0)
    p.resolve();return bytes(p.data)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',nargs='?',type=Path,default=HERE/(SUITE+'.z64'));args=p.parse_args();args.output.write_bytes(build())
