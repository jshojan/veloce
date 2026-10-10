"""Independent quarter-scanline oracle, without production-module imports."""
def expected(case):
    raw,size,source,profile,phase=case
    color=bytearray([0x5a]*64);depth=bytearray([0x5a]*64)
    left,top,right,bottom=(3,1,10,9) if profile==2 else (0,0,12,12)
    sl,st,sr,sb=((4,4,12,12) if profile==1 else (5,2,11,10) if profile==2 else (0,0,16,16))
    width=3 if profile==3 else 4
    fill=raw*65536+(65535-raw)
    for y in range(4):
        if not any(q>=top and q>=st and q<(bottom|3) and q<sb for q in range(4*y,4*y+4)):continue
        if profile==3 and y%2!=raw%2:continue
        for x in range(4):
            if not any(q>=left and q>=sl and q<=right and q<=sr for q in range(4*x,4*x+4)):continue
            at=(width*y+x)*(1<<(size-1))
            for n in range(1<<(size-1)):color[at+n]=(fill>>(8*(3-((at+n)%4))))&255
            at=(width*y+x)*2;depth[at:at+2]=b'\xff\xfc'
    words=tuple(int.from_bytes(buf[i:i+8],'big') for buf in (color,depth) for i in range(0,64,8))
    return words[:8]+(0x5a5a5a5a5a5a5a5a,)*3 if phase==0 else words[8:]+(0x1050 if source else 0x700160,0x80+source,32)
