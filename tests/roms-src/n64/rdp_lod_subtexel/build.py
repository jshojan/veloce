#!/usr/bin/env python3
"""Original MIT subtexel affine LOD RAM/register observations."""
import argparse,importlib.util,itertools
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('dp_assembler',HERE.parent/'dp_registers'/'build.py')
asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
SUITE='rdp_lod_subtexel'
VARIANTS=list(itertools.product(range(4),(0,3),range(2),range(2),(0,1,3)))
INDEX={v:i for i,v in enumerate(VARIANTS)}
TILE_WORDS=(0xf800,0x07c1,0x003e,0xffff,0x1110)
TEXTURE_WORDS=tuple(word for value in TILE_WORDS for word in (value,)*4)
def texture_bytes():return b''.join(word.to_bytes(2,'big') for word in TEXTURE_WORDS)
def cases(short=False):
    units=(0,1,15,31,32,33,63,64,65,127,128,129,255,256,257,511) if short else range(512)
    for value in units:
        for v in VARIANTS:yield value,v

def commands(v,wrong_tile=False,wrong_fraction=False):
    axis,f,source,negative,maximum=v
    image=(0xfd<<56)|(2<<51)|(3<<32)|0x7f0000
    tiles=[(0xf5<<56)|(f<<53)|(2<<51)|(1<<41)|((0 if wrong_tile else n)<<32)|(n<<24) for n in range(5)]
    bounds=[(0xf2<<56)|(n<<24) for n in range(5)]
    load=(0xf4<<56)|(12<<12)|16
    color=(0xff<<56)|(3<<51)|(3<<32)|0x700000
    mode=(0xef<<56)|(1<<52)|(15<<36)|(3<<42)|(3<<26)|(2<<18)|(1<<20)|64|16384
    mode|=1<<48
    scissor=(0xed<<56)|(16<<12)|16
    primitive=(0xfa<<56)|64
    combine=(0xfc<<56)|(15<<52)|(15<<28)|(31<<47)|(1<<15)|(7<<44)|(7<<12)|(7<<41)|(1<<9)
    combine|=(2<<24)|(10<<32)|(2<<6)|(7<<21)|(7<<3)|(7<<18)|2
    combine&=~((31<<32)|(7<<21)|(7<<3)|(7<<18))
    combine|=((10 if wrong_fraction else 13)<<32)|(2<<3)|((7 if wrong_fraction else 0)<<18)
    head=(0xca<<56)|(1<<55)|(maximum<<51)|(16<<32)|(8<<16)
    edges=[(2<<48)|0xffff0000,0,65536]
    tri=[head]+edges+[0]*8
    front=[image,tiles[0],load,bounds[0]]
    for n in range(1,5):front.extend((tiles[n],bounds[n]))
    return front+[color,mode,scissor,primitive,combine]+tri+[0xe900000000000000]

def build(short=False,wrong_capture=False,wrong_tile=False,wrong_fraction=False,wrong_t=False,wrong_y=False):
    rows=list(cases(short));p=asm.Program();asm.header(p,len(rows),13)
    p.i(15,0,27,0xa070);p.i(15,0,28,0xa430);p.address(21,'templates');p.puts('header')
    p.address(8,'texture');p.i(15,0,9,0xa07f);p.i(9,0,10,10)
    p.mark('texture_copy');p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'texture_copy')
    p.mark('sample');p.i(39,16,12,0);p.r(2,rt=12,rd=8,sa=16);p.r(0,rt=8,rd=9,sa=9);p.r(33,21,9,22)
    p.i(39,22,23,0);p.i(9,0,8,0x800);p.i(43,28,8,0)
    p.i(15,0,8,0x5a5a);p.i(13,8,8,0x5a5a);p.r(0x3c,rt=8,rd=9);p.r(37,8,9,8)
    p.i(15,0,9,0xa06f);p.i(13,9,9,0xfff8);p.i(9,0,10,10)
    p.mark('clear');p.i(63,9,8,0);p.i(9,9,9,8);p.i(9,10,10,-1);p.branch(5,10,0,'clear')
    p.i(9,22,8,8);p.i(13,27,9,256);p.r(2,rt=23,rd=10,sa=2)
    p.mark('copy');p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'copy')
    # Guest table supplies signed S/T d/dX and d/dY, in texels/pixel.
    p.i(39,16,13,4);p.i(43,27,13,432)
    p.i(39,16,14,8);p.i(43,27,14,456);p.i(43,27,14,464)
    p.i(39,22,8,4);p.i(15,0,18,0x70);p.i(13,18,18,256);p.r(33,18,23,19);p.i(9,0,20,0x15);p.branch(4,8,0,'source_done')
    p.i(13,27,8,256);p.i(9,0,9,0xff0);p.r(2,rt=23,rd=10,sa=2)
    p.mark('xbus_copy');p.r(37,25,9,11);p.i(39,8,13,0);p.i(43,11,13,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(12,9,9,4095);p.i(9,10,10,-1);p.branch(5,10,0,'xbus_copy')
    p.i(9,0,18,0xff0);p.r(33,18,23,19);p.i(9,0,20,0x16)
    p.mark('source_done');p.i(43,24,20,12);p.i(43,24,18,0);p.i(43,24,19,4)
    p.mark('wait');p.i(39,24,8,8);p.branch(5,8,19,'wait')
    p.puts('row');p.r(37,27,0,18);p.i(9,0,20,8)
    p.mark('capture');p.i(55,18,5,0)
    if wrong_capture:p.i(14,5,5,1)
    p.jump(3,'hex');p.i(9,18,18,8);p.i(9,20,20,-1);p.branch(5,20,0,'capture')
    p.i(15,0,8,0xa06f);p.i(13,8,8,0xfff8);p.i(55,8,5,0);p.jump(3,'hex');p.i(55,27,5,64);p.jump(3,'hex')
    for addr,offset in ((24,8),(24,12),(28,8)):p.i(39,addr,5,offset);p.jump(3,'hex')
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,16,16,12);p.i(9,17,17,-1);p.branch(5,17,0,'sample')
    p.puts('complete');p.i(43,30,0,4);p.mark('halt');p.branch(4,0,0,'halt');asm.output_routines(p)
    p.string('header',f'VELOCE 1 n64 {SUITE}\nBEGIN data cases={len(rows)} words=13\n');p.string('row','REG r=');p.string('complete',f'LOG complete cases={len(rows)}\nEND observations_complete\n')
    while p.pc&7:p.data.append(0)
    p.mark('templates')
    for v in VARIANTS:
        words=commands(v,wrong_tile,wrong_fraction);record=(len(words)*8).to_bytes(4,'big')+v[2].to_bytes(4,'big')+b''.join(w.to_bytes(8,'big') for w in words)
        assert len(record)<=512;p.data.extend(record+b'\0'*(512-len(record)))
    p.mark('texture');p.data.extend(texture_bytes())
    p.mark('table')
    for value,v in rows:
        vector=[0,0,0,0];vector[v[0]]=value
        sx,tx,sy,ty=vector
        if wrong_t:tx=ty=0
        if wrong_y:sy=ty=0
        sign=-1 if v[3] else 1
        def pair(s,t):return ((sign*s&65535)<<16)|(sign*t&65535)
        p.word(INDEX[v]<<16);p.word(pair(sx,tx));p.word(pair(sy,ty))
    p.resolve();return bytes(p.data)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/(SUITE+'.z64'));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
