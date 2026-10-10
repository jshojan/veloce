"""Independent rational quantization, integer square roots and latch algebra."""
from math import isqrt
SELECT=((0,1,2,3,4,5,6,7),(0,1,2,3,4,5,6,7),(0,0,2,2,4,4,6,6),(1,1,3,3,5,5,7,7),(0,0,0,0,4,4,4,4),(1,1,1,1,5,5,5,5),(2,2,2,2,6,6,6,6),(3,3,3,3,7,7,7,7))+tuple((i,)*8 for i in range(8))
PARTNERS=(0,1,0x7fff,0x8000,0xffff,0xe834,0x1234,0x8001)
def signed(v):return v%32768-(v//32768)*32768
def pack(lanes):return tuple(sum(lanes[i+j]<<(48-j*16) for j in range(4)) for i in (0,4))
TABLES=(tuple(65535 if i==0 else ((17179869184//(512+i)+1)//256)%65536 for i in range(512)),tuple((isqrt(17592186044415//(256+i if i<256 else 2*i))//2)%65536 for i in range(512)))
def result(root,raw):
    if raw==0:return 2147483647
    if raw==4294934528:return 4294901760
    negative=raw>=2147483648
    magnitude=(4294967296-raw if raw>4294934528 else 4294967295-raw) if negative else raw
    exponent=magnitude.bit_length()-1
    normalized=(magnitude-2**exponent)*2**(32-exponent)
    index=normalized//(16777216 if root else 8388608)+(256 if root and exponent%2 else 0)
    positive=(1073741824+TABLES[root][index]*16384)//2**(exponent//2 if root else exponent)
    return 4294967295-positive if negative else positive

def expected(case):
    profile,u,group,e,fn=case
    v=u if profile<2 else PARTNERS[profile-2];high=0x1234 if profile<2 else u;pending=profile!=0
    left=[(u+i*4097)%65536 for i in range(8)];right=[v]*8;dst=list(left);out=0x7fffc000
    acc=[(signed(a)*signed(v)*65536)%2**48 for a in left]
    if fn not in (55,63):
        if fn==51:word=v
        elif fn in (50,54):word=out//65536;high=v;pending=True
        else:
            raw=high*65536+v if fn in (49,53) and pending else signed(v)%2**32
            out=result(fn>=52,raw);pending=False;word=out%65536
        dst[u%8]=word;low=v
    else:low=high if pending else 1
    acc=[x//65536*65536+low for x in acc]
    probe=result(False,high*65536+v if pending else signed(v)%2**32)
    words=[]
    for lanes in (dst,[x//4294967296 for x in acc],[x//65536%65536 for x in acc],[x%65536 for x in acc],left,right):words.extend(pack(lanes))
    sx=lambda x:x|(0xffff0000 if x>=32768 else 0)
    words.extend((0xffffa55a,0x5aa5,0xa5,176,3,0,sx(out//65536),sx(probe%65536),sx(probe//65536)))
    return tuple(words)
