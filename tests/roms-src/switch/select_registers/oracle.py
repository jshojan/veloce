"""Independent boolean condition table and unbounded-integer select oracle."""
VALUES=(0,1,2,2147483647,2147483648,4294967295,4294967296,
        9223372036854775807,9223372036854775808,18446744073709551615,
        18446744069414584320,18446744071562067968,9223372036854775809,
        81985529216486895,18364758544493064720,18446744073709551614)
def holds(flags,condition):
    n,z,c,v=(bool(flags & bit) for bit in (8,4,2,1))
    return (z,not z,c,not c,n,not n,v,not v,c and not z,
            not c or z,n==v,n!=v,not z and n==v,z or n!=v,True,True)[condition]
def result(left,right,flags,condition,form):
    width=32 if form<4 else 64
    alternate=(right,right+1,~right,-right)[form%4]
    return (left if holds(flags,condition) else alternate)%2**width

def observations(domain):
    for left in VALUES[:domain]:
        for right in VALUES[:domain]:
            for flags in range(16):
                for condition in range(16):
                    for form in range(8):
                        yield left,right,result(left,right,flags,condition,form),flags*2**28,flags*128+condition*8+form

def references():
    assert result(123,2**64-1,0,0,5)==0
    assert result(123,2**31,0,0,3)==2**31
    assert result(2**64-1,123,0,14,0)==2**32-1
    assert result(123,0,0,15,6)==123
    assert sum(holds(flags,0) for flags in range(16))==8
    assert sum(holds(flags,8) for flags in range(16))==4
    assert sum(holds(flags,12) for flags in range(16))==4
