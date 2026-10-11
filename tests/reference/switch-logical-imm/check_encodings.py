#!/usr/bin/env python3
"""Check original logical-immediate opcode fields against exact-pinned LLVM."""
import argparse,hashlib,importlib.util,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
REFERENCE_SHA256='fc6cc18123d4892ad182a364383c4b90d2cf03eca750ceaeb00e43f65ee3114f'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('llvm_source',type=Path);args=parser.parse_args()
    data=args.llvm_source.read_bytes();assert hashlib.sha256(data).hexdigest()==REFERENCE_SHA256,'reference identity differs'
    text=data.decode().split('; 5.5.3 Logical (shifted register)',1)[0];examples={}
    for op,width,encoded in re.findall(r'CHECK: (and|orr|eor|ands)\s+([wx])\d+,.*?encoding: \[([^\]]+)\]',text):
        examples[op,width]=int.from_bytes(bytes(int(v.strip(),16) for v in encoded.split(',')),'little')
    spec=importlib.util.spec_from_file_location('logical_builder',ROOT/'tests/roms-src/switch/logical_imm_registers/build.py')
    builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
    fields=31|(31<<5)|(63<<10)|(63<<16)|(1<<22);count=0;forms=set()
    for word in builder.cases():
        form=(('and','orr','eor','ands')[(word>>29)&3],'x' if word>>31 else 'w')
        assert word&~fields==examples[form]&~fields,(form,hex(word))
        assert word&31==2 and word>>5&31==0
        forms.add(form);count+=1
    assert len(forms)==8 and count==45312
    print('45312 original logical-immediate words across eight forms match pinned LLVM 20.1.3 opcode fields')
if __name__=='__main__':main()
