#!/usr/bin/env python3
"""Compare original guest instruction fields with pinned LLVM encoding tests."""
import argparse,hashlib,importlib.util,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
PIN='llvmorg-20.1.3'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('llvm_source',type=Path);args=parser.parse_args()
    source=args.llvm_source.read_bytes()
    # Enforce exact downloaded reference content; third-party bytes stay outside the repository.
    assert hashlib.sha256(source).hexdigest()==REFERENCE_SHA256,'reference source identity differs'
    spec=importlib.util.spec_from_file_location('original_muldiv_builder',ROOT/'tests/roms-src/switch/muldiv_registers/build.py')
    builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
    examples={}
    for op,width,encoded in re.findall(r'CHECK: (madd|msub|udiv|sdiv|smaddl|smsubl|umaddl|umsubl|smulh|umulh)\s+([wx])1,.*?encoding: \[([^\]]+)\]',source.decode()):
        examples[(op,width)]=int.from_bytes(bytes(int(value.strip(),16) for value in encoded.split(',')),'little')
    forms=(('madd','w'),('msub','w'),('udiv','w'),('sdiv','w'),('madd','x'),('msub','x'),('udiv','x'),('sdiv','x'),('smaddl','x'),('smsubl','x'),('umaddl','x'),('umsubl','x'),('smulh','x'),('umulh','x'))
    for form,actual in zip(forms,builder.WORDS):
        example=examples[form]
        fields=31|(31<<5)|(31<<16)
        if form[0] in ('madd','msub','smaddl','smsubl','umaddl','umsubl'):fields|=31<<10
        assert actual&~fields==example&~fields,(form,hex(actual),hex(example))
        assert actual&31==2 and actual>>5&31==0 and actual>>16&31==1,'guest operand routing differs'
    assert len(builder.WORDS)==len(forms)==14
    print(f'14 original guest multiply/divide forms match pinned LLVM {PIN} encoding fields')
REFERENCE_SHA256='1c82c2245a76354ab6db697412df103f3b4ee72999ad61813b5809eda3fc78a6'
if __name__=='__main__':main()
