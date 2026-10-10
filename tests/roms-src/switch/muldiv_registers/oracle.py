"""Independent unbounded-integer multiply/divide reference; no CPU helpers."""
FORMS=('madd32','msub32','udiv32','sdiv32','madd64','msub64','udiv64','sdiv64',
       'smaddl','smsubl','umaddl','umsubl','smulh','umulh')
EDGE_VALUES=(1,2,2147483647,2147483648,4294967295,4294967296,9223372036854775807,
             9223372036854775808,18446744069414584320,18446744071562067968,
             9223372036854775809,81985529216486895,18364758544493064720,
             4294967297,4294967294,18446744073709551614)
def signed(value,width):
    value%=2**width
    return value if value<2**(width-1) else value-2**width

def result(name,left,right,accumulator):
    width=32 if name.endswith('32') else 64
    if name.startswith(('sdiv','udiv')):
        a,b=(signed(left,width),signed(right,width)) if name.startswith('sdiv') else (left%2**width,right%2**width)
        quotient=0 if b==0 else (abs(a)//abs(b))*(-1 if (a<0)!=(b<0) else 1)
        return quotient%2**width
    if name=='smulh':return (signed(left,64)*signed(right,64)//2**64)%2**64
    if name=='umulh':return (left*right//2**64)%2**64
    if name in ('smaddl','smsubl'):a,b=signed(left,32),signed(right,32)
    elif name in ('umaddl','umsubl'):a,b=left%2**32,right%2**32
    else:a,b=left%2**width,right%2**width
    product=a*b
    return (accumulator-product if name in ('msub32','msub64','smsubl','umsubl') else accumulator+product)%2**width

def values(domain):
    table=tuple(byte*72340172838076673 for byte in range(256))+EDGE_VALUES
    assert len(set(table))==272
    return table[:domain]
def observations(domain):
    for left in values(domain):
        for right in values(domain):
            for seed in (0,1):
                accumulator=2**64-1 if seed else 0;flags=(15 if seed else 9)*2**28
                for form,name in enumerate(FORMS):
                    yield left,right,accumulator,result(name,left,right,accumulator),flags,seed*14+form

def references():
    assert result('sdiv64',2**63,2**64-1,0)==2**63
    assert result('sdiv32',2**31,2**32-1,0)==2**31
    assert result('sdiv64',2**64-7,3,0)==2**64-2
    assert result('sdiv32',7,2**32-3,0)==2**32-2
    assert result('udiv64',2**64-1,0,0)==0
    assert result('sdiv64',2**63,0,0)==0
    assert result('smulh',2**64-1,2,0)==2**64-1
    assert result('umulh',2**64-1,2**64-1,0)==2**64-2
    assert result('smaddl',2**31,2**32-1,2**64-1)==2**31-1
    assert result('umsubl',2**32-1,2**32-1,0)==2**33-1
