"""Host arithmetic ST/W and logical grid oracle; no command/TMEM dependencies."""
def endpoint(segment):
    denominator=64+segment
    return 14979 if segment==6 else (1048576+denominator//2)//denominator

def coordinate(raw,w):
    numerator=raw if raw<32768 else raw-65536
    magnitude=w%32768
    shift=min(14,14-(magnitude.bit_length()-1)) if magnitude else 14
    normalized=magnitude*(2**shift)%16384
    segment,fraction=divmod(normalized,256)
    reciprocal=(endpoint(segment)*(256-fraction)+endpoint(segment+1)*fraction)//256
    product=numerator*reciprocal
    limit=2**(29-shift)
    scaled=product*2 if shift==14 else product//(2**(13-shift))
    if not w or w>=32768:return 32767
    if product>=limit:return 32767
    if product< -limit:return -32768
    raw17=scaled%131072
    value=raw17 if raw17<65536 else raw17-131072
    return max(-32768,min(32767,value))

def expected(case):
    raw,(kind,index,source)=case
    if kind==0:
        ss,tt=((7,23),(31,64),(32767,32768),(65535,0))[index];w=raw
    else:ss,tt=raw,raw^65535;w=(0,1,2,255,256,16384,32767,32768)[index]
    x=coordinate(ss,w)//32%8;y=coordinate(tt,w)//32%8
    codes=(x*4,y*4,(x*3+y*5)%32)
    result=[c*8+c//4 for c in codes]
    memory=bytearray([0x5a]*80)
    points=((0,0),(2,0),(1,1),(3,1),(0,2),(2,2),(1,3),(3,3))
    for row in range(4):
        for x in range(4):
            covered=[]
            for px,py in points:
                q=row*4+py;right=q if q<8 else 16-q
                if 0<=x*4+px<right:covered.append((px,py))
            if (0,0) in covered:
                at=8+(row*4+x)*4;memory[at:at+4]=bytes(result+[(len(covered)-1)*32])
    values=[int.from_bytes(memory[8+n:16+n],'big') for n in range(0,64,8)]
    values.extend((int.from_bytes(memory[:8],'big'),int.from_bytes(memory[72:80],'big')))
    return tuple(values+[(0xff0 if source else 0x700100)+168,128+source,32])
