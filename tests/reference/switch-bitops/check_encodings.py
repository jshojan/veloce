#!/usr/bin/env python3
"""Exact-pinned LLVM opcode comparison, independently parsed examples."""
import argparse,hashlib,importlib.util,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
REFERENCE_SHA256='1c82c2245a76354ab6db697412df103f3b4ee72999ad61813b5809eda3fc78a6'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('llvm_source',type=Path);args=parser.parse_args()
    source=args.llvm_source.read_bytes();assert hashlib.sha256(source).hexdigest()==REFERENCE_SHA256,'reference identity differs'
    text=source.decode();examples={}
    for heading in ('Variable shifts','One operand instructions'):
        block=text.split('; '+heading,1)[1].split(';==---',2)[1]
        instructions=re.findall(r'^  (\w+) ([wx])1, [wx]2(?:, [wx]3)?$',block,re.M)
        encodings=re.findall(r'CHECK: encoding: \[([^\]]+)\]',block)
        assert len(instructions)==len(encodings)
        for form,encoded in zip(instructions,encodings):
            examples[form]=int.from_bytes(bytes(int(v.strip(),16) for v in encoded.split(',')),'little')
    spec=importlib.util.spec_from_file_location('bitops_builder',ROOT/'tests/roms-src/switch/bitops_registers/build.py')
    builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
    forms=tuple((op,width) for width in ('w','x') for op in ('lslv','lsrv','asrv','rorv'))+(
        ('rbit','w'),('rev16','w'),('rev','w'),('clz','w'),('cls','w'),
        ('rbit','x'),('rev16','x'),('rev32','x'),('rev','x'),('clz','x'),('cls','x'))
    assert len(forms)==len(builder.WORDS)==19
    for index,(form,actual) in enumerate(zip(forms,builder.WORDS)):
        fields=31|(31<<5)|(31<<16 if index<8 else 0)
        assert actual&~fields==examples[form]&~fields,(form,hex(actual),hex(examples[form]))
        assert actual&31==2 and actual>>5&31==0
        if index<8:assert actual>>16&31==1
    print('19 original shift/unary forms match pinned LLVM 20.1.3 opcode fields')
if __name__=='__main__':main()
