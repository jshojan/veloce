"""Independent current-input/wire packet grading; no production imports."""
BITS=(0,1,20,6,8,9,10,11,4,5,21,22,23,24)
WIRE=(15,14,13,12,11,10,9,8,5,4,3,2,1,0)
def frontend(raw):return sum(1<<bit for n,bit in enumerate(BITS) if raw&(1<<n))
def expected(sample):
    result=[]
    for raw in (sample,sample^16383):
        buttons=sum(1<<bit for n,bit in enumerate(WIRE) if raw&(1<<n))
        result.append(int.from_bytes(bytes([255,1,4,1,buttons>>8,buttons&255,0,0]),'big'))
        result.extend([0xff01040100000000]*3+[0xaaaaaaaaaaaaaa00,4096,2])
    return tuple(result+[0,0])
