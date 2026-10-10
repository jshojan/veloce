"""Independent native button-bit assignment and real SI IRQ expectations."""
def expected(raw):
    native=(15,14,13,12,11,10,9,8,5,4,3,2,1,0)
    result=[]
    for multiplier,offset in ((1,0),(3,1),(5,7),(7,11)):
        value=(raw*multiplier+offset)%16384
        button=sum(2**native[bit] for bit in range(14) if (value//2**bit)%2)
        result.append(button*65536)
    return tuple(result)+(4096,0)
