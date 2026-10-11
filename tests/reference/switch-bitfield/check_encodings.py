#!/usr/bin/env python3
"""Validate every original guest field/EXTR opcode against pinned LLVM fields."""
import argparse,hashlib,importlib.util,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
REFERENCE_SHA256='285401d5bc03ad13fd29f4d3352611917d4c8ecf299336613fd88de92664eba6'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('llvm_source',type=Path);args=parser.parse_args()
    data=args.llvm_source.read_bytes();assert hashlib.sha256(data).hexdigest()==REFERENCE_SHA256,'reference identity differs'
    examples={}
    for op,width,encoding in re.findall(r'CHECK: (bfxil|sbfx|ubfx|extr) ([wx])\d+,.*?encoding: \[([^\]]+)\]',data.decode()):
        examples[op,width]=int.from_bytes(bytes(int(v.strip(),16) for v in encoding.split(',')),'little')
    spec=importlib.util.spec_from_file_location('bitfield_builder',ROOT/'tests/roms-src/switch/bitfield_registers/build.py')
    builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
    count=0;forms=set()
    for word in builder.cases():
        width='x' if word>>31 else 'w';bits=64 if width=='x' else 32
        is_extract=bool(word&(1<<23));op='extr' if is_extract else ('sbfx','bfxil','ubfx')[(word>>29)&3]
        fields=31|(31<<5)|(63<<10)|((31 if is_extract else 63)<<16)
        assert word&~fields==examples[op,width]&~fields,(op,width,hex(word))
        assert word&31==2 and word>>5&31==0
        assert word>>10&63<bits
        if is_extract:assert word>>16&31==1
        else:assert word>>16&63<bits
        forms.add((op,width));count+=1
    assert len(forms)==8 and count==15456
    print('15456 original instruction words across eight forms match pinned LLVM 20.1.3 opcode/width fields')
if __name__=='__main__':main()
