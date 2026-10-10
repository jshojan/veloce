"""Independent unbounded-integer residues and per-bit Boolean truth tables."""
from functools import lru_cache
SELECT=((0,1,2,3,4,5,6,7),(0,1,2,3,4,5,6,7),(0,0,2,2,4,4,6,6),(1,1,3,3,5,5,7,7),(0,0,0,0,4,4,4,4),(1,1,1,1,5,5,5,5),(2,2,2,2,6,6,6,6),(3,3,3,3,7,7,7,7))+tuple((i,)*8 for i in range(8))
def signed(v):return v%32768-(v//32768)*32768
def pack(lanes):return tuple(sum(lanes[i+j]<<(48-j*16) for j in range(4)) for i in (0,4))
@lru_cache(maxsize=4096)
def boolean(fn,u,v):
    result=0
    for bit in range(16):
        left,right=bool(u//2**bit%2),bool(v//2**bit%2)
        on=(left and right) if fn<42 else (left or right) if fn<44 else left!=right
        if fn%2:on=not on
        if on:result+=2**bit
    return result

def compute(fn,u,v,bits):
    a,b=signed(u),signed(v);flags=bits;clamp=False
    if fn in (16,17):
        total=a+b+bits%2 if fn==16 else a-b-bits%2;clamp=True;flags=bits//4*4
    elif fn==19:total=(-1 if a<0 else 1 if a>0 else 0)*b;clamp=a<0
    elif fn==20:total=u+v;flags=bits//4*4+total//65536
    elif fn==21:total=u-v;flags=bits//4*4+int(u<v)+2*int(u!=v)
    else:total=boolean(fn,u,v)
    low=total%65536;word=max(-32768,min(32767,total))%65536 if clamp else low
    return word,low,flags

def expected(case,edges=False):
    bits,first,group,e,fn=case
    left=[group*8+i for i in range(8)] if edges else [first*257]*8
    right=[0,1,0x7ffe,0x7fff,0x8000,0xfffe,0xffff,0] if edges else [(group*8+i)*257 for i in range(8)]
    seed=[(signed(a)*signed(b)*65536)%2**48 for a,b in zip(left,right)]
    result=[];after=[];flags=[0,0,0]
    for i,index in enumerate(SELECT[e]):
        out,low,state=compute(fn,left[i],right[index],bits);result.append(out);after.append(seed[i]//65536*65536+low)
        flags[0]+=((state%2)+256*(state//2%2))*2**i
        flags[1]+=((state//4%2)+256*(state//8%2))*2**i
        flags[2]+=(state//16%2)*2**i
    words=[]
    for lanes in (result,[x//4294967296 for x in after],[x//65536%65536 for x in after],[x%65536 for x in after],left,right):words.extend(pack(lanes))
    words.extend((flags[0]|(0xffff0000 if flags[0]>=32768 else 0),flags[1]|(0xffff0000 if flags[1]>=32768 else 0),flags[2],108,3,0))
    return tuple(words)
