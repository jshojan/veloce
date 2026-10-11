"""Independent unbounded-integer accumulator grading, with a literal lane map."""
from build import seed,partner
SELECT=((0,1,2,3,4,5,6,7),(0,1,2,3,4,5,6,7),(0,0,2,2,4,4,6,6),(1,1,3,3,5,5,7,7),(0,0,0,0,4,4,4,4),(1,1,1,1,5,5,5,5),(2,2,2,2,6,6,6,6),(3,3,3,3,7,7,7,7))+tuple((i,)*8 for i in range(8))
def signed(v,bits):return v-(1<<bits) if v>=1<<(bits-1) else v
def signed_clamp(v):return max(-32768,min(32767,v))%65536
def pack(lanes):return tuple(sum(lanes[i+j]<<(48-j*16) for j in range(4)) for i in (0,4))
def compute(fn,left,right,raw,parity):
    a,b=signed(left,16),signed(right,16);old=signed(raw,48)
    if fn in (0,1):total=2*a*b+32768
    elif fn==3:total=(a*b+(31 if a*b<0 else 0))*65536
    elif fn in (2,10):total=old+(b*(65536 if parity else 1) if (old>=0)==(fn==2) else 0)
    elif fn==11:
        upper=old//4194304
        total=old+(2097152 if upper<0 else -2097152 if upper>0 else 0) if raw//2097152%2==0 else old
    else:
        product={4:left*right//65536,5:a*right,6:left*b,7:a*b*65536,8:2*a*b,9:2*a*b,12:left*right//65536,13:a*right,14:left*b,15:a*b*65536}[fn]
        total=product+(old if fn>=8 else 0)
    raw=total%2**48;upper=signed(raw,48)//65536
    if fn in (3,11):out=signed_clamp(upper//2)&65520
    elif fn in (1,9):out=0 if upper<0 else 65535 if upper>=32768 else upper
    elif fn in (4,6):out=raw%65536
    elif fn in (12,14):out=0 if upper<-32768 else 65535 if upper>=32768 else raw%65536
    else:out=signed_clamp(upper)
    return out,raw

def expected(case,accumulate=False):
    first,group,e,fn=case
    if accumulate:
        left=[group*8+i for i in range(8)];right=[partner(first,i) for i in range(8)];before=[seed(first,i) for i in range(8)];parity=(first+group)%2 if fn in (2,10) else 0
    else:
        left=[first*257]*8;right=[(group*8+i)*257 for i in range(8)];before=[0]*8;parity=0
    lhs=right if parity else left
    result=[];after=[]
    for i,selected in enumerate(SELECT[e]):
        out,raw=compute(fn,lhs[i],right[selected],before[i],parity);result.append(out);after.append(raw)
    words=[]
    for lanes in (result,[x//4294967296 for x in after],[x//65536%65536 for x in after],[x%65536 for x in after],left,right):words.extend(pack(lanes))
    words.extend((0xffffa55a,0x5aa5,0xa5,176 if accumulate else 104,3,0))
    if accumulate:
        for lanes in ([x//4294967296 for x in before],[x//65536%65536 for x in before],[x%65536 for x in before]):words.extend(pack(lanes))
        words.append(0)
    return tuple(words)
