"""Independent host compression, comparator, quarter-grid and memory observations."""
EDGES=(0,131072,196608,229376,245760,253952,258048,260096,262144)
STEPS=(64,32,16,8,4,2,1,1)
def decompress(word):
    n=word//4%16384;return EDGES[n//2048]+n%2048*STEPS[n//2048]
def compress(z):
    band=next(n for n in range(8) if z<EDGES[n+1]);return (band*2048+(z-EDGES[band])//STEPS[band])*4
def highest(v):return 0 if not v else 2**(v.bit_length()-1)
def encoding(v):
    code=0
    for bit in range(16):
        if v//(2**bit)%2:code|=bit
    return code
def decision(kind,z,incoming_delta,word,bits,count):
    old=decompress(word);code=word%4*4+bits;delta=2**code;coplanar=False
    precision=word//8192
    if precision<3:
        if delta==32768:delta=65535;coplanar=True
        else:delta=max(delta*2,16//(2**precision))
    dz=highest(incoming_delta|delta);front=z<old;far=coplanar or z+dz*8>=old;near=coplanar or z-dz*8<=old;maximum=old==262143
    overflow=count>=8
    if kind==2:return front or maximum,count
    if kind==3:return far and near and not maximum,count
    if kind==1 and front and far and overflow:
        divisor=2**encoding(dz);return True,((old//divisor-z//divisor)%16*count//8)%16
    return maximum or (front if overflow else near),count

def expected(case):
    raw,(profile,source)=case;kind=profile%4;seed=raw//256;alpha=raw%256
    z=(0,1,16384,32767)[kind]*8 if profile>=4 else (raw%32768)*8
    delta=raw if profile>=4 else (0,3,32768,65535)[kind]
    code=encoding(delta)
    color=bytearray([0x5a]*32);hidden=[0]*16
    depth=bytearray(raw.to_bytes(2,'big')*16);dh=[raw%2*3]*16
    points=((0,0),(2,0),(1,1),(3,1),(0,2),(2,2),(1,3),(3,3))
    for y in range(4):
        for x in range(4):
            count=0
            for px,py in points:
                q=y*4+py;right=q if q<8 else 16-q
                count+=int(0<=x*4+px<right)
            accept,adjusted=decision(kind,z,delta,raw,raw%2*3,count)
            if not accept or not adjusted or alpha<127:continue
            n=y*4+x;packed=seed//8*2048+(seed^255)//8*64+(seed*17%256)//8*2+1
            color[n*2:n*2+2]=packed.to_bytes(2,'big');hidden[n]=3
            depth[n*2:n*2+2]=(compress(z)+code//4).to_bytes(2,'big');dh[n]=code%4
    def data(buf):return [int.from_bytes(buf[n:n+8],'big') for n in (0,8,16,24)]
    def metadata(bits):return [(bits[n]*4+bits[n+1])*4294967296+bits[n+2]*4+bits[n+3] for n in (0,4,8,12)]
    return tuple(data(color)+metadata(hidden)+data(depth)+metadata(dh)+[0x5a5a5a5a5a5a5a5a]*2+[(0xff0 if source else 0x700100)+192,128+source,32])
