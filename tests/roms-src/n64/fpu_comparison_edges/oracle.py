"""Independent exact rational ordering; raw NaN classes and predicate truth masks."""
from fractions import Fraction
SENTINEL=0x12345678a5b6c7d8
def expected(fmt,predicate,a,b,control):
    causes=0;condition=False
    if fmt not in (16,17):causes=32
    else:
        width,f,bias=(32,23,127) if fmt==16 else (64,52,1023)
        def decode(raw):
            raw%=2**width;fraction=raw%2**f;exponent=raw//2**f% (2*bias+2);negative=bool(raw//2**(width-1))
            if exponent==2*bias+1:return ('nan' if fraction else 'inf',negative,fraction>=2**(f-1),Fraction(0))
            power=(exponent or 1)-bias-f
            value=(fraction+(2**f if exponent else 0))*(Fraction(2**power) if power>=0 else Fraction(1,2**(-power)))
            return 'number',negative,False,-value if negative else value
        ka,na,sa,va=decode(a);kb,nb,sb,vb=decode(b)
        unordered=ka=='nan' or kb=='nan'
        causes=16 if unordered and (predicate>=8 or sa or sb) else 0
        if unordered:relation=1
        else:
            if ka==kb=='inf':ordering=(na<nb)-(na>nb)
            elif ka=='inf':ordering=-1 if na else 1
            elif kb=='inf':ordering=1 if nb else -1
            else:ordering=(va>vb)-(va<vb)
            relation=4 if ordering<0 else 2 if not ordering else 0
        condition=bool(predicate&relation)
    trap=bool(causes&32 or causes&(control//128%32))
    fcsr=(control&~0x3f000)|(causes<<12)
    if not trap:fcsr=(fcsr&~0x800000)|(int(condition)<<23)|(causes<<2)
    cc=bool(fcsr&0x800000)
    branches=sum((3 if cc==bool(mode%2) else 0 if mode>=2 else 1)<<(2*mode) for mode in range(4))
    return SENTINEL,fcsr,int(trap),60 if trap else 0,a,b,branches
