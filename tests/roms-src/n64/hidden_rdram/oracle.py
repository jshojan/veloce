"""Independent physical-byte model; imports no guest packing or production code."""
def expected(raw):
    value=raw|(raw//2<<16)|(raw//4<<32)|(raw//8<<48)
    ram=[255]*32;hidden=[1]*32
    for base,test in ((0,False),(16,True)):
        for offset,width in ((0,1),(2,2),(4,4),(8,8)):
            at=base+offset
            data=[value//(256**(width-i-1))%256 for i in range(width)]
            ram[at:at+width]=data
            if test:
                for i in range(width):
                    a=at+i
                    if width==1:hidden[a]=value%2 if a%4==3 else 0
                    elif width==2:hidden[a]=value//(2**(1-i))%2 if at%4==2 else 0
                    else:
                        word=value//(2**32) if width==8 and i<4 else value%(2**32)
                        hidden[a]=word//(2**(3-a%4))%2
            elif width==1:hidden[at]=value%2 if at%2 else 0
            else:
                for i in range(0,width,2):hidden[at+i:at+i+2]=[data[i+1]%2]*2
    def packed(a):
        out=0
        for b in ram[a:a+8]:out=out*256+b
        return out
    def gathered(a):
        out=0
        for n in range(2):
            nibble=0
            for b in hidden[a+n*4:a+n*4+4]:nibble=nibble*2+b
            out=out*4294967296+nibble
        return out
    return packed(0),packed(8),gathered(0),gathered(8),gathered(16),gathered(24),packed(16),packed(24),0
