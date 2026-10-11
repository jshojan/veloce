"""Independent signed-coordinate, raw-transfer and copy alpha-mask grading."""
def signed16(value):return value-65536 if value>=32768 else value

def expected(case):
    raw,(size,flip,alpha,source)=case
    texture=((raw,raw^65535,raw^43690,raw^21845),(raw^4951,raw^9320,raw^3855,raw^61680))
    initial_s=raw;initial_t=(raw*257+128)%65536
    ds=(raw*73+4096)%65536;dt=((raw*128)%65536+raw//512)^1024
    pixels=bytearray([90]*32);hidden=[0]*16
    for y in range(2):
        if source and y%2!=raw%2:continue
        s=(initial_s+(signed16(ds)*y//32 if flip else 0))%65536
        t=(initial_t+(0 if flip else signed16(dt)*y//32))%65536
        first=signed16(s)//32%4;row=signed16(t)//32%2
        values=[texture[row][(first+n)%4] for n in range(4)]
        data=b''.join(value.to_bytes(2,'big') for value in values) if size else bytes(8)
        for lane in range(4):
            accepted=not alpha or (bool(values[lane]&1) if size==2 else data[4+lane]>=127 if size==1 else False)
            if not accepted:continue
            at=y*8+lane*2;pixels[at:at+2]=data[lane*2:lane*2+2];hidden[at//2]=(data[lane*2+1]&1)*3
    words=[int.from_bytes(pixels[n:n+8],'big') for n in (0,8,16,24)]
    words += [(hidden[n]*4+hidden[n+1])*4294967296+hidden[n+2]*4+hidden[n+3] for n in (0,4,8,12)]
    return tuple(words+[0x5a5a5a5a5a5a5a5a]*2+[(0xff0 if source else 0x700100)+88,128+source,32])
