#!/usr/bin/env python3
"""Independent input/file-format oracle against the dynamically linked TAS plugin."""
import argparse,struct,subprocess
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--probe',type=Path,required=True);ap.add_argument('--artifacts',type=Path,required=True);a=ap.parse_args();root=a.artifacts.resolve();root.mkdir(parents=True,exist_ok=True)
# NES hardware bit order => frontend mask, independent numeric stimulus oracle.
bits=(1,2,128,64,256,512,1024,2048)
def mask(n):return sum(value for i,value in enumerate(bits) if n&(1<<i))
def pad(n):return ''.join('q' if n&(1<<(7-i)) else '.' for i in range(8))
def header(binary=0,four=0,length=None,ports=(1,1)):
    text=f'version 3\nemuVersion 0\nfourscore {four}\nport0 {ports[0]}\nport1 {ports[1]}\nport2 0\nbinary {binary}\nromFilename original-probe.nes\n'
    if length is not None:text+=f'length {length}\n'
    return text.encode()
def check(name,data,expected):
    image=root/(name+'.fm2');image.write_bytes(data);out=root/(name+'.observations');p=subprocess.run([str(a.probe.resolve()),str(image),str(out)],capture_output=True,text=True,timeout=30)
    assert p.returncode==0,(name,p.stdout,p.stderr)
    lines=out.read_text().splitlines();assert int(lines[0])==len(expected) and len(lines)==len(expected)+1
    for i,(line,want) in enumerate(zip(lines[1:],expected)):assert list(map(int,line.split()))==[i,*want,*((0,0) if len(want)==5 else ())],(name,i,line,want)
def reject(name,data):
    image=root/(name+'.invalid');image.write_bytes(data);p=subprocess.run([str(a.probe.resolve()),str(image),str(root/(name+'.observations'))],timeout=5);assert p.returncode==1,(name,p.returncode)
pairs=[(left,right) for left in range(256) for right in range(256)]
expected=[(mask(left),mask(right),0,0,int((left+right)%2!=0)) for left,right in pairs]
check('ascii_pairs',header()+b''.join(f'|{(left+right)%2}|{pad(left)}|{pad(right)}||\r\n'.encode() for left,right in pairs),expected)
check('binary_pairs',header(binary=1)+b'|'+bytes(value for left,right in pairs for value in ((left+right)%2,left,right)),expected)
four=[(n,n^255,(n*17)%256,(n*73)%256) for n in range(256)]
check('four_score',header(four=1)+b''.join(('|0|'+'|'.join(map(pad,values))+'||\n').encode() for values in four),[(*map(mask,values),0) for values in four])
check('binary_four',header(binary=1,four=1)+b'|'+b''.join(bytes((0,*values)) for values in four),[(*map(mask,values),0) for values in four])
check('none',header(ports=(0,0))+b'|0||||\n',[(0,0,0,0,0)])
check('declared_length',header(length=1)+b'|0|        |q.......||\nignored bytes after declared end',[(0,2048,0,0,0)])
for name,data in [('short_pad',header()+b'|0|.......|........||\n'),('power',header()+b'|2|........|........||\n'),('disk',header()+b'|4|........|........||\n'),('zapper',header(ports=(2,1))),('fcs',header()+b'savestate 0x0012\n'),('missing_records',header(length=2)+b'|0|........|........||\n'),('binary_short',header(binary=1)+b'|\x00\x01'),('bad_version',header().replace(b'version 3',b'version 9')),('empty',b''),('unknown',b'nonsense'),('huge_length',header(length=2147483647))]:reject(name,data)
# Independent TAS2 packing: fixed strings, little-endian numeric fields, 29-byte frames.
meta=b'format probe\0'.ljust(256,b'\0')+bytes(128+1024)+b'NES\0'.ljust(32,b'\0')+bytes(256)
prefix=b'TAS2'+meta+struct.pack('<IQQBI',0x1234abcd,2,7,1,3)+b'\x01\x80\xff'
records=struct.pack('<QIIIIBi',0,0x80000000,1,2,3,1,-1)+struct.pack('<QIIIIBi',1,256,128,64,2048,2,9)
check('tas2_independent',prefix+records,[(0x80000000,1,2,3,1,0,-1),(256,128,64,2048,0,1,9)])
valid=prefix+records
for n in range(len(valid)):reject('truncated_tas2',valid[:n])
reject('trailing_tas2',valid+b'x')
for offset,value in [(1720,2),(1725+3+24,4),(4,255)]:
    damaged=bytearray(valid);damaged[offset]=value
    if offset==4:damaged[4:260]=b'x'*256 # no terminating title
    reject('bad_field_'+str(offset),damaged)
print('131072 FM2 pair records, 512 four-port records, independent TAS2 bytes and all 1786 truncation prefixes PASS')
