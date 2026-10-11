"""Independent signed-coordinate, triangular-filter, alpha and RAM/hidden grading."""
def signed16(value):return value-65536 if value>=32768 else value

def expected(case):
    raw,(size,flip,filter,source)=case
    texture=((raw,raw^65535,raw^43690,raw^21845),(raw^4951,raw^9320,raw^3855,raw^61680))
    def color(x,y):
        word=texture[y%2][x%4];codes=(word//2048,word//64%32,word//2%32)
        return tuple(c*8+c//4 for c in codes)+(255*(word%2),)
    ds=signed16((raw*73+4096)%65536);dt=signed16(((raw*128)%65536+raw//512)^1024)
    pixels=bytearray([90]*32);hidden=[0]*16
    for y in range(2):
        if source and y%2!=raw%2:continue
        for x in range(4):
            s=signed16((raw+ds*(y if flip else x)//32)%65536)
            t=signed16(((raw*257+128)%65536+dt*(x if flip else y)//32)%65536)
            tx,ty=s//32,t//32;sf,tf=s%32,t%32
            q=(color(tx,ty),color(tx+1,ty),color(tx,ty+1),color(tx+1,ty+1))
            if not filter:rgba=q[0]
            elif sf+tf<32:rgba=tuple(((32-sf-tf)*q[0][n]+sf*q[1][n]+tf*q[2][n]+16)//32 for n in range(4))
            else:rgba=tuple(((32-tf)*q[1][n]+(32-sf)*q[2][n]+(sf+tf-32)*q[3][n]+16)//32 for n in range(4))
            if source and min(255,256 if rgba[3]==255 else rgba[3])<127:continue
            if size==2:
                at=(y*4+x)*2;word=rgba[0]//8*2048+rgba[1]//8*64+rgba[2]//8*2+1;pixels[at:at+2]=word.to_bytes(2,'big');hidden[at//2]=3
            else:
                at=(y*4+x)*4;pixels[at:at+4]=bytes(rgba[:3]+(224,));hidden[at//2]=(rgba[1]&1)*3;hidden[at//2+1]=0
    words=[int.from_bytes(pixels[n:n+8],'big') for n in (0,8,16,24)]
    words += [(hidden[n]*4+hidden[n+1])*4294967296+hidden[n+2]*4+hidden[n+3] for n in (0,4,8,12)]
    return tuple(words+[0x5a5a5a5a5a5a5a5a]*2+[(0xff0 if source else 0x700100)+96,128+source,32])
