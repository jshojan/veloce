#!/usr/bin/env python3
"""Build the original PAL-country VI field cartridge from the shared source."""
import sys
from pathlib import Path

SOURCE=Path(__file__).resolve().parents[1]/'vi_field_schedule'
sys.path.insert(0,str(SOURCE))
from build import build

if __name__=='__main__':
    output=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).with_name('vi_field_schedule_pal.z64')
    output.write_bytes(build(region=ord('P')))
