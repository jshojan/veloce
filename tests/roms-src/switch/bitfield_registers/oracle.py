"""Independent contiguous-field arithmetic and double-width integer extract."""
VALUES=(0,1,2,2147483647,2147483648,4294967295,4294967296,
        9223372036854775807,9223372036854775808,18446744073709551615,
        18446744069414584320,18446744071562067968,9223372036854775809,
        81985529216486895,18364758544493064720,18446744073709551614)
def result(source,old,width,op,r,s):
    source%=2**width;old%=2**width
    if s>=r:start=0;length=s-r+1;payload=(source//2**r)%2**length
    else:start=width-r;length=s+1;payload=source%2**length
    if op==1:
        # Remove just the target field from the old destination, then insert.
        low=old%2**start;high=old//2**(start+length)*2**(start+length)
        answer=low+payload*2**start+high
    else:
        answer=payload*2**start
        if op==0 and source//2**s%2:answer+=2**width-2**(start+length)
    return answer%2**width

def observations(domain):
    short=domain==2
    for source in VALUES[:domain]:
        other=2**64-1-source
        for seed in range(2):
            old=2**64-1 if seed else 0;index=0;total=60 if short else 15456
            for width in (32,64):
                for op in range(3):
                    for r in ((0,1,width-1) if short else range(width)):
                        for s in ((0,1,width-1) if short else range(width)):
                            yield source,other,old,result(source,old,width,op,r,s),(9 if not seed else 15)*2**28,seed*total+index
                            index+=1
                for amount in ((0,1,width-1) if short else range(width)):
                    value=((source%2**width)*2**width+(other%2**width))//2**amount%2**width
                    yield source,other,old,value,(9 if not seed else 15)*2**28,seed*total+index
                    index+=1
            assert index==total

def references():
    assert result(128,0,64,0,0,7)==2**64-128
    assert result(255,0,64,2,56,7)==65280
    assert result(18,2**64-1,32,1,28,3)==4294967087
    assert result(2**64-1,0,64,2,0,63)==2**64-1
    assert result(0,2**64-1,64,1,0,63)==0
