#!/usr/bin/env python3
from pathlib import Path
import argparse,importlib.util
spec=importlib.util.spec_from_file_location("multiply_assembly",Path(__file__).parent.parent/"rsp_multiply_pairs/build.py")
base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)
SUITE="rsp_accumulate_edges"
def cases(short=False):return base.cases(short,True)
def build(short=False,wrong_capture=False):return base.build(short,wrong_capture,True,SUITE)
if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name(SUITE+".z64"));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
