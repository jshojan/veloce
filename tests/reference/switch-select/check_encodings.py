#!/usr/bin/env python3
"""Compare guest selects with exact pinned LLVM source; reference stays external."""
import argparse,hashlib,importlib.util,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
REFERENCE_SHA256='1c82c2245a76354ab6db697412df103f3b4ee72999ad61813b5809eda3fc78a6'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('llvm_source',type=Path);args=parser.parse_args()
    source=args.llvm_source.read_bytes();assert hashlib.sha256(source).hexdigest()==REFERENCE_SHA256,'reference identity differs'
    text=source.decode().split('; Conditional select instructions',1)[1].split('; Make sure we handle upper case',1)[0]
    forms=re.findall(r'^  (csel|csinc|csinv|csneg) ([wx])1, ([wx])2, ([wx])3, eq$',text,re.M)
    encoded=re.findall(r'CHECK: encoding: \[([^\]]+)\]',text)
    spec=importlib.util.spec_from_file_location('select_builder',ROOT/'tests/roms-src/switch/select_registers/build.py')
    builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
    fields=31|(31<<5)|(31<<16)
    assert len(forms)==len(encoded)==len(builder.WORDS)==8
    for form,actual in zip((('csel','w'),('csinc','w'),('csinv','w'),('csneg','w'),('csel','x'),('csinc','x'),('csinv','x'),('csneg','x')),builder.WORDS):
        index=next(i for i,f in enumerate(forms) if f[:2]==form)
        reference=int.from_bytes(bytes(int(v.strip(),16) for v in encoded[index].split(',')),'little')
        assert actual&~fields==reference&~fields,(form,hex(actual),hex(reference))
        assert actual&31==2 and actual>>5&31==0 and actual>>16&31==1,'guest routing differs'
    print('8 original conditional-select forms match pinned LLVM 20.1.3 opcode fields')
if __name__=='__main__':main()
