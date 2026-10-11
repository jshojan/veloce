"""Independent boolean conditions and unbounded signed/unsigned arithmetic."""
VALUES=(0,1,2,2147483647,2147483648,4294967295,4294967296,
        9223372036854775807,9223372036854775808,18446744073709551615,
        18446744069414584320,18446744071562067968,9223372036854775809,
        81985529216486895,18364758544493064720,18446744073709551614)
def holds(flags,condition):
    n,z,c,v=(bool(flags & bit) for bit in (8,4,2,1))
    return (z,not z,c,not c,n,not n,v,not v,c and not z,
            not c or z,n==v,n!=v,not z and n==v,z or n!=v,True,True)[condition]
def arithmetic(a,b,width,subtract):
    a%=2**width;b%=2**width
    signed=lambda value:value if value<2**(width-1) else value-2**width
    exact=a-b if subtract else a+b
    signed_exact=signed(a)-signed(b) if subtract else signed(a)+signed(b)
    result=exact%2**width
    overflow=not -2**(width-1)<=signed_exact<2**(width-1)
    carry=a>=b if subtract else exact>=2**width
    return (8 if result>=2**(width-1) else 0)+(4 if result==0 else 0)+(2 if carry else 0)+int(overflow)
def rows(left,right,initial,immediate):
    for condition in range(16):
        for width in (32,64):
            for subtract in range(2):
                calculated=arithmetic(left,right,width,subtract)
                packed=sum((calculated if holds(initial,condition) else fallback)*16**fallback for fallback in range(16))
                context=condition*4+int(width==64)*2+subtract+(64+right*64 if immediate else 0)
                yield left,right,initial*2**28,packed,context

def observations(domain):
    for left in VALUES[:domain]:
        for initial in range(16):
            for right in VALUES[:domain]:yield from rows(left,right,initial,False)
            for right in ((0,1,15,31) if domain==2 else range(32)):yield from rows(left,right,initial,True)
def references():
    assert arithmetic(0,0,64,True)==6
    assert arithmetic(2**31-1,1,32,False)==9
    assert arithmetic(2**31,1,32,True)==3
    assert next(rows(0,0,0,False))[3]==0xfedcba9876543210
    assert next(rows(0,0,4,False))[3]==0x4444444444444444
