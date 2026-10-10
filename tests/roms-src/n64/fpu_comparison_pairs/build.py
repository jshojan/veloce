#!/usr/bin/env python3
from pathlib import Path
import argparse,importlib.util
spec=importlib.util.spec_from_file_location('comparison_assembly',Path(__file__).parent.parent/'fpu_comparison_edges/build.py')
base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base);base.SUITE='fpu_comparison_pairs'
SUITE=base.SUITE
cases=base.cases
build=base.build
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name(SUITE+'.z64'));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
