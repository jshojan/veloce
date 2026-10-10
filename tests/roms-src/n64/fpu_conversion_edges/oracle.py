"""Independent exact-rational host oracle. No emulator code or native FP rounding."""
from fractions import Fraction
import math
SENTINEL=0x12345678a5b6c7d8
FUNCTIONS=(32,33,36,37,8,9,10,11,12,13,14,15)
def target(fn):return 16 if fn==32 else 17 if fn==33 else 20 if fn==36 or 12<=fn<=15 else 21
def power(n):return Fraction(2**n) if n>=0 else Fraction(1,2**(-n))
def rounded(value,rm):return round(value) if rm==0 else math.trunc(value) if rm==1 else math.ceil(value) if rm==2 else math.floor(value)
def convert(fmt,fn,bits,control):
    dest=target(fn);rm=fn%4 if 8<=fn<=15 else control%4;enables=control//128%32;flush=bool(control&0x1000000)
    assist=(0,32)
    if dest in (16,17):
        if fmt not in (16,17,20,21) or dest==fmt:return assist
    elif fmt not in (16,17):return assist
    negative=False
    if fmt in (20,21):
        width=32 if fmt==20 else 64;number=bits%(2**width);number-=2**width if number>=2**(width-1) else 0
        if fmt==21 and not -(2**55)<=number<2**55:return assist
        value=Fraction(number);negative=number<0
    else:
        mantissa,exponent_bits,bias=(23,8,127) if fmt==16 else (52,11,1023)
        negative=bool(bits&(1<<(mantissa+exponent_bits)));exponent=bits//2**mantissa%(2**exponent_bits);tail=bits%(2**mantissa)
        if exponent==2**exponent_bits-1:
            if dest in (20,21):return assist
            if tail:
                return (0x7fbfffff if dest==16 else 0x7ff7ffffffffffff,16) if tail>=2**(mantissa-1) else assist
            return ((0xff800000 if negative else 0x7f800000) if dest==16 else (0xfff0000000000000 if negative else 0x7ff0000000000000),0)
        if exponent==0 and tail:return assist
        value=Fraction(0) if exponent==0 else (2**mantissa+tail)*power(exponent-bias-mantissa)*(-1 if negative else 1)
    if dest in (20,21):
        integer=rounded(value,rm)
        if dest==20 and not -(2**31)<=integer<2**31 or dest==21 and not -(2**53)<integer<2**53:return assist
        return integer%(2**(32 if dest==20 else 64)),int(integer!=value)
    width,exponent_bits,bias=(23,8,127) if dest==16 else (52,11,1023)
    sign=int(negative)<<(width+exponent_bits);absolute=abs(value)
    if not absolute:return sign,0
    exponent=absolute.numerator.bit_length()-absolute.denominator.bit_length()
    if absolute<power(exponent):exponent-=1
    minimum=1-bias
    if exponent<minimum:
        quantum=power(minimum-width);integer=rounded(value/quantum,rm)
        if abs(integer)>=2**width:return sign+2**width,int(integer*quantum!=value)
        if not flush or enables%4:return assist
        return sign+(2**width if (rm==2 and not negative) or (rm==3 and negative) else 0),3
    quantum=power(exponent-width);integer=rounded(value/quantum,rm);inexact=integer*quantum!=value
    if abs(integer)>=2**(width+1):integer//=2;exponent+=1
    if exponent>bias:
        inf=rm==0 or rm==2 and not negative or rm==3 and negative
        return sign+(((2**exponent_bits-1)*2**width) if inf else ((2**exponent_bits-1)*2**width-1)),5
    return sign+(exponent+bias)*2**width+(abs(integer)%(2**width)),int(inexact)
def expected(fmt,fn,bits,control):
    result,causes=convert(fmt,fn,bits,control);trap=bool(causes&32 or (causes&(control//128%32)))
    fcsr=control&~0x3f000;fcsr|=causes*4096
    if trap:result=SENTINEL
    else:fcsr|=(causes%32)*4
    mask=(1<<32)-1 if target(fn) in (16,20) and not trap else (1<<64)-1
    return (result,fcsr,int(trap),60 if trap else 0,bits),mask
