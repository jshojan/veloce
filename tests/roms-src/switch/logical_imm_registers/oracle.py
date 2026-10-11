"""Independent forward run/element mapping and binary-string logical oracle."""
VALUES=(0,1,2,2147483647,2147483648,4294967295,4294967296,
        9223372036854775807,9223372036854775808,18446744073709551615,
        18446744069414584320,18446744071562067968,9223372036854775809,
        81985529216486895,18364758544493064720,18446744073709551614)
def patterns(width,short):
    descriptors={}
    for element in (2,4,8,16,32,64):
        if element>width:continue
        for run in range(1,element):
            end=(64-element*2 if element<64 else 0)+(run-1)
            descriptors[(int(element==64),end)]=(element,run)
    keys=((0,0,60),(0,63,60),(0,0,0),(int(width==64),width-1,width-2)) if short else (
        (n,r,s) for n in range(2) for r in range(64) for s in range(64))
    for n,r,s in keys:
        if (n,s) not in descriptors:continue
        element,run=descriptors[n,s]
        bits='0'*(element-run)+'1'*run
        rotate=r%element;bits=bits[-rotate:]+bits[:-rotate] if rotate else bits
        yield bits*(width//element)

def result(source,mask,op,width,initial):
    value=f'{source%2**width:0{width}b}'
    pairs=zip(value,mask)
    bits=''.join(str(int((a=='1' and b=='1') if op in (0,3) else (a=='1' or b=='1') if op==1 else a!=b)) for a,b in pairs)
    answer=int(bits,2)
    flags=(8 if bits[0]=='1' else 0)+(4 if answer==0 else 0) if op==3 else initial
    return answer,flags*2**28

def observations(domain):
    short=domain==2
    for source in VALUES[:domain]:
        for initial in (9,15):
            index=0
            for width in (32,64):
                for mask in patterns(width,short):
                    for op in range(4):
                        answer,flags=result(source,mask,op,width,initial)
                        yield source,answer,flags,initial*2**28,index
                        index+=1
            assert index==(32 if short else 45312)

def references():
    assert len(tuple(patterns(32,False)))==3648
    assert len(tuple(patterns(64,False)))==7680
    assert result(2**64-1,'0'*63+'1',3,64,15)==(1,0)
    assert result(0,'1'+'0'*63,3,64,15)==(0,4*2**28)
    assert result(2**64-1,'1'+'0'*63,3,64,15)==(2**63,8*2**28)
