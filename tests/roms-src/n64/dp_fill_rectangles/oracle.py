"""Independent integer pixel/guard oracle; no production module imports."""
def expected(case):
    raw,size,source,layout=case
    memory=bytearray([0x5a]*64)
    for y in range(4):
        for x in range(4):
            if layout and (x not in (1,2,3) or y not in (1,2)):continue
            if size==2:
                at=2*(y*4+x)
                value=raw if x%2==0 else 65535-raw
                memory[at:at+2]=value.to_bytes(2,'big')
            else:
                at=4*(y*4+x)
                memory[at:at+4]=(raw*65536+65535-raw).to_bytes(4,'big')
    pixels=tuple(int.from_bytes(memory[i:i+8],'big') for i in range(0,64,8))
    return pixels+(0x5a5a5a5a5a5a5a5a,)*2+(0x1028 if source else 0x700138,0x80+source,32)
