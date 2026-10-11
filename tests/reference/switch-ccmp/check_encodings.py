#!/usr/bin/env python3
"""Compare original conditional compare words with exact-pinned LLVM fields."""
import argparse,hashlib,importlib.util,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
REFERENCE_SHA256='1c82c2245a76354ab6db697412df103f3b4ee72999ad61813b5809eda3fc78a6'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('llvm_source',type=Path);args=parser.parse_args()
    source=args.llvm_source.read_bytes();assert hashlib.sha256(source).hexdigest()==REFERENCE_SHA256,'reference identity differs'
    block=source.decode().split('; Conditionally set flags instructions',1)[1].split('; Conditional select instructions',1)[0]
    forms=re.findall(r'^  (ccmn|ccmp) ([wx])1, ([^,]+), #3, eq$',block,re.M)
    encoded=re.findall(r'CHECK: encoding: \[([^\]]+)\]',block);assert len(forms)==len(encoded)==8
    examples={(op,width,right.startswith('#')):int.from_bytes(bytes(int(v.strip(),16) for v in encoding.split(',')),'little') for (op,width,right),encoding in zip(forms,encoded)}
    spec=importlib.util.spec_from_file_location('ccmp_builder',ROOT/'tests/roms-src/switch/ccmp_registers/build.py')
    builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
    fields=15|(31<<5)|(15<<12)|(31<<16);count=0;seen=set()
    for immediate in (False,True):
        for value in (range(32) if immediate else (1,)):
            for word in builder.words(immediate,value):
                form=('ccmp' if word&(1<<30) else 'ccmn','x' if word>>31 else 'w',immediate)
                assert word&~fields==examples[form]&~fields,(form,hex(word))
                assert word>>5&31==0 and word>>16&31==value and word&31==0
                seen.add(form);count+=1
    assert len(seen)==8 and count==2112
    print('2112 original compare base words across eight forms match pinned LLVM 20.1.3 opcode fields')
if __name__=='__main__':main()
