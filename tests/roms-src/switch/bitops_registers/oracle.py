"""Independent text/byte sequence and unbounded-integer bit-operation oracle."""
EDGE_VALUES=(1,2,2147483647,2147483648,4294967295,4294967296,9223372036854775807,
             9223372036854775808,18446744069414584320,18446744071562067968,
             9223372036854775809,81985529216486895,18364758544493064720,
             4294967297,4294967294,18446744073709551614)

def result(value,count,form):
    width=32 if form<4 or 8<=form<13 else 64
    value%=2**width
    if form<8:
        count%=width;op=form%4
        if op==0:answer=value*2**count
        elif op==1:answer=value//2**count
        elif op==2:answer=(value if value<2**(width-1) else value-2**width)//2**count
        else:answer=(value//2**count)+(value%2**count)*2**(width-count)
    else:
        op=form-8 if width==32 else form-13
        bits=f'{value:0{width}b}'
        if op==0:answer=int(bits[::-1],2)
        elif op<(3 if width==32 else 4):
            block=2**op;raw=value.to_bytes(width//8,'little')
            answer=int.from_bytes(b''.join(raw[i:i+block][::-1] for i in range(0,len(raw),block)),'little')
        elif op==(3 if width==32 else 4):answer=width-len(bits.lstrip('0'))
        else:answer=width-len(bits.lstrip(bits[0]))-1
    return answer%2**width

def observations(domain):
    values=tuple(byte*72340172838076673 for byte in range(256))+EDGE_VALUES
    assert len(set(values))==272
    for value in values[:domain]:
        for count in range(4 if domain==2 else 256):
            for seed in range(2):
                for form in range(8):
                    yield value,count,result(value,count,form),(9 if not seed else 15)*2**28,seed*19+form
        for seed in range(2):
            for form in range(8,19):
                yield value,0,result(value,0,form),(9 if not seed else 15)*2**28,seed*19+form

def references():
    assert result(1,0,8)==2**31
    assert result(0,0,11)==32 and result(0,0,18)==63
    assert result(2**64-1,0,18)==63
    assert result(2**63,1,6)==3*2**62
    assert result(1,32,0)==1 and result(1,64,4)==1
    assert result(0x0123456789abcdef,0,16)==0xefcdab8967452301
    assert result(0x0123456789abcdef,0,15)==0x67452301efcdab89
