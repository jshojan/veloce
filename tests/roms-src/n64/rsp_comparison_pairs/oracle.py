"""Independent whole-integer comparison and flag grading; no emulator imports."""
SELECT=((0,1,2,3,4,5,6,7),(0,1,2,3,4,5,6,7),(0,0,2,2,4,4,6,6),(1,1,3,3,5,5,7,7),(0,0,0,0,4,4,4,4),(1,1,1,1,5,5,5,5),(2,2,2,2,6,6,6,6),(3,3,3,3,7,7,7,7))+tuple((i,)*8 for i in range(8))
def signed(v):return v%32768-(v//32768)*32768
def pack(lanes):return tuple(sum(lanes[i+j]<<(48-j*16) for j in range(4)) for i in (0,4))
def compute(fn,u,v,bits):
    a,b=signed(u),signed(v)
    carry,neq,less,greater,ext=(bool(bits//2**i%2) for i in range(5))
    out=u
    if fn<36:
        if fn==32:less=a<b or (a==b and carry and neq);out=u if less else v
        elif fn==33:less=a==b and not neq;out=v
        elif fn==34:less=a!=b or neq
        else:less=a>b or (a==b and (not carry or not neq));out=u if less else v
        carry=neq=greater=False
    elif fn==36:
        if carry:
            if not neq:less=(u+v<=65536) if ext else (u+v==0)
            out=(-v)%65536 if less else u
        else:
            if not neq:greater=u>=v
            out=v if greater else u
        carry=neq=ext=False
    elif fn in (37,38):
        opposite=(a<0)!=(b<0)
        if opposite:
            greater=b<0;less=a+b<=(0 if fn==37 else -1)
            out=(((-v)%65536) if fn==37 else 65535-v) if less else u
        else:
            less=b<0;greater=a>=b;out=v if greater else u
        carry=fn==37 and opposite
        neq=fn==37 and ((a+b not in (0,-1)) if opposite else a!=b)
        ext=fn==37 and opposite and a+b==-1
    else:out=u if less else v;carry=neq=False
    return out,sum(int(flag)*2**i for i,flag in enumerate((carry,neq,less,greater,ext)))
def expected(case,edges=False):
    profile,first,group,e,fn=case
    left=[group*8+i for i in range(8)] if edges else [first*257]*8
    right=[0,1,0x7ffe,0x7fff,0x8000,0xfffe,0xffff,0] if edges else [(group*8+i)*257 for i in range(8)]
    seed=[(signed(a)*signed(b)*65536)%2**48 for a,b in zip(left,right)]
    result=[];after=[];flags=[0,0,0]
    for i,index in enumerate(SELECT[e]):
        out,bits=compute(fn,left[i],right[index],profile*8+i);result.append(out);after.append(seed[i]//65536*65536+out)
        flags[0]+=((bits%2)+256*(bits//2%2))*2**i
        flags[1]+=((bits//4%2)+256*(bits//8%2))*2**i
        flags[2]+=(bits//16%2)*2**i
    words=[]
    for lanes in (result,[x//4294967296 for x in after],[x//65536%65536 for x in after],[x%65536 for x in after],left,right):words.extend(pack(lanes))
    words.extend((flags[0]|(0xffff0000 if flags[0]>=32768 else 0),flags[1]|(0xffff0000 if flags[1]>=32768 else 0),flags[2],108,3,0))
    return tuple(words)
