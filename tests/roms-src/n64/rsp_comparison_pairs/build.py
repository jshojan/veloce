#!/usr/bin/env python3
"""Original MIT RSP comparison/flag observation ROM; no SDK/retail bytes."""
from pathlib import Path
import argparse,importlib.util
spec=importlib.util.spec_from_file_location("vector_assembly",Path(__file__).parent.parent/"rsp_vector_loads/build.py")
assembly=importlib.util.module_from_spec(spec);spec.loader.exec_module(assembly)
Program=assembly.Program
SUITE="rsp_comparison_pairs"
EDGES=False
PARTNERS=(0,1,0x7ffe,0x7fff,0x8000,0xfffe,0xffff,0)
def cases(short=False,edges=None):
    mode=EDGES if edges is None else edges
    for profile in range(4):
        for first in range(1 if mode else (2 if short else 256)):
            for group in range(2 if short else (8192 if mode else 32)):
                for fn in range(32,40):yield profile,first,group,(profile+group+first+fn)%16,fn
def inputs(case,edges=False):
    profile,first,group,e,fn=case
    return ([group*8+i for i in range(8)],list(PARTNERS)) if edges else ([first*257]*8,[(group*8+i)*257 for i in range(8)])
def math(fn,e=0,vs=0,vt=1,vd=2):return 0x4a000000|(e<<21)|(vt<<16)|(vs<<11)|(vd<<6)|fn
def vector(store,reg,offset):return ((58 if store else 50)<<26)|(31<<21)|(reg<<16)|(4<<11)|(offset//16)
def build(short=False,wrong_capture=False,edges=None,suite=None):
    mode=EDGES if edges is None else edges
    suite=SUITE if suite is None else suite
    rows=list(cases(short,mode));words=18;p=Program();assembly.header(p,len(rows),words)
    code=[]
    for index,value in enumerate((0xccaa,0xf0,0)):code.extend((0x34010000|value,0x48c10000|(index<<11)))
    code.extend((0x341f0400,vector(False,0,0),vector(False,1,16),math(7,0,0,1,6)))
    operation=len(code)*4;code.append(0)
    for reg,e in zip(range(3,6),range(8,11)):code.append(math(29,e,0,0,reg))
    for reg,off in zip((2,3,4,5,0,1),range(512,608,16)):code.append(vector(True,reg,off))
    for index,off in enumerate((0x660,0x664,0x668)):code.extend((0x48410000|(index<<11),0xac010000|off))
    code.append(13)
    assert len(code)==27
    p.address(8,"microcode");p.i(13,25,9,4096);p.i(9,0,10,len(code))
    p.mark("copy_code");p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,"copy_code")
    p.puts("header");p.mark("sample");p.i(39,16,12,0);p.i(39,16,14,4);p.i(43,25,12,4096+operation)
    # Patch ORI immediates before CTC2: all 32 incoming flag patterns are
    # distributed across four profiles and eight lanes. No verdict in guest.
    p.r(2,rt=14,rd=8,sa=16);p.i(12,8,8,1);p.r(0,rt=8,rd=9,sa=8);p.r(35,9,8,9);p.r(0,rt=9,rd=9,sa=8)
    p.i(13,9,9,0xf0);p.i(15,0,10,0x3401);p.r(37,9,10,9);p.i(43,25,9,4104)
    p.r(2,rt=14,rd=8,sa=17);p.i(12,8,8,1);p.r(0,rt=8,rd=9,sa=8);p.r(35,9,8,9);p.r(37,9,10,9);p.i(43,25,9,4112)
    if mode:
        p.i(12,14,13,8191);p.r(0,rt=13,rd=13,sa=3)
    else:
        p.i(12,14,13,255);p.r(0,rt=13,rd=8,sa=8);p.r(37,13,8,13)
        p.r(2,rt=14,rd=15,sa=8);p.i(12,15,15,31);p.r(0,rt=15,rd=15,sa=3)
    p.i(9,0,18,0);p.mark("seed_lanes");p.r(0,rt=18,rd=8,sa=1);p.r(33,25,8,9);p.i(41,9,13,0x400)
    if mode:
        p.i(9,13,13,1);p.address(10,"partners");p.r(33,10,8,10);p.i(37,10,10,0);p.i(41,9,10,0x410)
    else:
        p.r(33,15,18,10);p.r(0,rt=10,rd=11,sa=8);p.r(37,10,11,10);p.i(41,9,10,0x410)
    p.i(9,18,18,1);p.i(9,0,8,8);p.branch(5,18,8,"seed_lanes")
    p.i(15,0,9,0xa408);p.i(43,9,0,0);p.i(9,0,8,0xad);p.i(43,26,8,16)
    p.mark("wait");p.i(39,26,8,16);p.i(12,8,8,1);p.branch(4,8,0,"wait")
    p.puts("row");p.i(9,0,22,0);p.i(13,25,18,0x600);p.i(9,0,20,12)
    p.mark("vectors_capture");p.i(55,18,5,0)
    if wrong_capture:p.i(14,5,5,1)
    assembly.emit_hex(p);p.i(9,18,18,8);p.i(9,20,20,-1);p.branch(5,20,0,"vectors_capture")
    for off in (0x660,0x664,0x668):p.i(39,25,5,off);assembly.emit_hex(p)
    p.i(15,0,8,0xa408);p.i(39,8,5,0);assembly.emit_hex(p);p.i(39,26,5,16);assembly.emit_hex(p);p.i(15,0,8,0xa430);p.i(39,8,5,8);assembly.emit_hex(p)
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,16,16,8);p.i(9,17,17,-1);p.branch(5,17,0,"sample")
    assembly.SUITE=suite;assembly.finish(p,len(rows),words)
    p.mark("microcode")
    for ins in code:p.word(ins)
    p.mark("table")
    for profile,first,group,e,fn in rows:
        p.word(math(fn,e));p.word((profile<<16)|(group if mode else first|(group<<8)))
    p.mark("partners")
    for value in PARTNERS:p.data.extend(value.to_bytes(2,"big"))
    p.resolve();return bytes(p.data)
if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name(SUITE+".z64"));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
