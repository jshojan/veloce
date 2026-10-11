"""Independent signed-byte packet oracle; no production imports."""
WIRE=(15,14,13,12,11,10,9,8,5,4,3,2,1,0)
def frontend(sample,phase=0):
    pair=sample^(65535 if phase else 0)
    buttons=(sample&16383)^(16383 if phase else 0)
    return 0xc000|buttons|((pair&255)<<16)|((pair>>8)<<24)
def expected(sample):
    result=[]
    for phase in range(2):
        pair=sample^(65535 if phase else 0)
        raw=(sample&16383)^(16383 if phase else 0)
        buttons=sum(1<<bit for n,bit in enumerate(WIRE) if raw&(1<<n))
        result.append(int.from_bytes(bytes([255,1,4,1,buttons>>8,buttons&255,pair&255,pair>>8]),'big'))
        result.extend([0xff01040100000000]*3+[0xaaaaaaaaaaaaaa00,4096,2])
    return tuple(result+[0,0])
