"""Exact rational oracle, including square roots by integer-square comparisons.
No native floating-point arithmetic, emulator imports, or implementation helpers.
"""
from fractions import Fraction
from math import isqrt
SENTINEL=0x12345678a5b6c7d8
def power(n):return Fraction(2**n) if n>=0 else Fraction(1,2**(-n))
def log2(x):
    e=x.numerator.bit_length()-x.denominator.bit_length()
    return e-int(x<power(e))
def rounded(x,rm):
    floor=x.numerator//x.denominator
    if rm==0:
        delta=x-floor
        return floor+int(delta>Fraction(1,2) or delta==Fraction(1,2) and floor%2)
    if rm==1:return floor if x>=0 else -((-x).numerator//(-x).denominator)
    return floor+int(x!=floor) if rm==2 else floor
def calculate(fmt,fn,a,b,control):
    if fmt not in (16,17) or fn not in range(8):return 0,32
    f,e,bias=(23,8,127) if fmt==16 else (52,11,1023)
    width=f+e+1;signbit=2**(width-1);inf=(2**e-1)*2**f;canonical=inf+2**(f-1)-1
    a%=2**width;b%=2**width
    if fn==6:return a,0
    def decode(bits):
        negative=bits>=signbit;exp=bits//2**f%(2**e);tail=bits%2**f
        if exp==2**e-1:return ('assist' if tail and tail<2**(f-1) else 'nan' if tail else 'inf',negative,Fraction(0))
        if exp==0 and tail:return 'assist',negative,Fraction(0)
        return 'finite',negative,((2**f+tail)*power(exp-bias-f)*(-1 if negative else 1)) if exp else Fraction(0)
    ka,na,va=decode(a);kb,nb,vb=decode(b)
    if ka=='assist' or fn<4 and kb=='assist':return 0,32
    if ka=='nan' or fn<4 and kb=='nan':return canonical,16
    if fn==5:return a%(signbit),0
    if fn==7:return a^signbit,0
    rm=control%4;square=False
    if fn<=1:
        if fn==1:nb=not nb;vb=-vb
        if ka=='inf' or kb=='inf':
            if ka==kb=='inf' and na!=nb:return canonical,16
            return inf+(signbit if (na if ka=='inf' else nb) else 0),0
        value=va+vb;negative=value<0 if value else na if na==nb else rm==3
    elif fn==2:
        negative=na!=nb
        if ka=='inf' and kb=='finite' and not vb or kb=='inf' and ka=='finite' and not va:return canonical,16
        if ka=='inf' or kb=='inf':return inf+int(negative)*signbit,0
        value=va*vb
    elif fn==3:
        negative=na!=nb
        if ka==kb=='inf' or ka==kb=='finite' and not va and not vb:return canonical,16
        if ka=='inf':return inf+int(negative)*signbit,0
        if kb=='inf' or not va:return int(negative)*signbit,0
        if not vb:return inf+int(negative)*signbit,8
        value=va/vb
    else:
        if na and (ka=='inf' or va):return canonical,16
        if ka=='inf' or not va:return a,0
        square=True;value=va;negative=False
    sign=int(negative)*signbit
    if not value:return sign,0
    absolute=abs(value);exponent=log2(absolute)//2 if square else log2(absolute);minimum=1-bias
    quantum=power(max(exponent,minimum)-f)
    if square:
        scaled=absolute/(quantum*quantum);integer=isqrt(scaled.numerator//scaled.denominator);inexact=integer*integer!=scaled
        if rm==0:
            half=Fraction(2*integer+1,2)**2;integer+=int(scaled>half or scaled==half and integer%2)
        elif rm==2:integer+=int(inexact)
    else:
        integer=abs(rounded(value/quantum,rm));inexact=integer*quantum!=absolute
    if integer<2**f:
        if not control&0x1000000 or control//128%32%4:return 0,32
        return sign+(2**f if (rm==2 and not negative) or (rm==3 and negative) else 0),3
    exponent=max(exponent,minimum)
    if integer>=2**(f+1):integer//=2;exponent+=1
    if exponent>bias:
        infinity=rm==0 or rm==2 and not negative or rm==3 and negative
        return sign+(inf if infinity else inf-1),5
    return sign+(exponent+bias)*2**f+integer%2**f,int(inexact)
def expected(fmt,fn,a,b,control):
    result,causes=calculate(fmt,fn,a,b,control);trap=bool(causes&32 or causes&(control//128%32))
    fcsr=control if fn==6 and not causes else (control&~0x3f000)|(causes<<12)
    if trap:result=SENTINEL
    elif fn!=6:fcsr|=(causes%32)*4
    mask=2**32-1 if fmt==16 and not trap else 2**64-1
    return (result,fcsr,int(trap),60 if trap else 0,a,b),mask
