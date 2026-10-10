"""Independent byte/sign/flag-mask oracle for actual scalar/vector transfers."""
def signed16(code):return code if code<32768 else code+0xffff0000
def expected(case):
    raw,e=case;register=bytearray(range(0x90,0xa0));register[e]=raw//256
    if e<15:register[e+1]=raw%256
    moved=signed16(register[e]*256+register[(e+1)%16])
    return (int.from_bytes(register[:8],'big'),int.from_bytes(register[8:],'big'),signed16(raw),signed16(65535-raw),raw%256,moved,0,80,3,0)
