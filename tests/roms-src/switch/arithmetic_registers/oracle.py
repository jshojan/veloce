"""Independent unbounded-integer AArch64 arithmetic and NZCV reference."""
FORMS=('adds','subs','adc','adcs','sbc','sbcs')
def result(name,left,right,width,initial):
    modulus=2**width;mask=modulus-1;sign=modulus//2
    a,b=left&mask,right&mask
    signed=lambda x:x if x<sign else x-modulus
    if name in ('subs','sbc','sbcs'):
        borrow=0 if name=='subs' else 1-((initial>>1)&1)
        exact=a-b-borrow;signed_exact=signed(a)-signed(b)-borrow;carry=exact>=0
    else:
        carry_in=((initial>>1)&1) if name in ('adc','adcs') else 0
        exact=a+b+carry_in;signed_exact=signed(a)+signed(b)+carry_in;carry=exact>=modulus
    value=exact%modulus
    flags=initial
    if name in ('adds','subs','adcs','sbcs'):
        flags=(8 if value>=sign else 0)+(4 if value==0 else 0)+(2 if carry else 0)+(1 if not -sign<=signed_exact<sign else 0)
    return value,flags<<28

def observations(domain):
    for a in range(domain):
        left=a*0x0101010101010101
        for b in range(domain):
            right=b*0x0101010101010101
            for carry in (0,1):
                initial=9 if not carry else 15
                for width in (32,64):
                    for form,name in enumerate(FORMS):
                        value,flags=result(name,left,right,width,initial)
                        yield left,right,value,flags,carry*12+(width==64)*6+form

def references():
    assert result('adds',0x7fffffff,1,32,0)==(0x80000000,0x90000000)
    assert result('adds',0xffffffff,1,32,0)==(0,0x60000000)
    assert result('subs',0,1,64,0)==(0xffffffffffffffff,0x80000000)
    assert result('adcs',0,0xffffffffffffffff,64,2)==(0,0x60000000)
    assert result('sbcs',0x8000000000000000,1,64,0)==(0x7ffffffffffffffe,0x30000000)
    assert result('adc',0,0,32,15)==(1,0xf0000000)
