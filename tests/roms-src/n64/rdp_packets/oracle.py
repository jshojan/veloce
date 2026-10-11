"""Independent native packet length table; settled MMIO, not raster/timing accuracy."""
LENGTHS={8:4,9:6,10:12,11:14,12:12,13:14,14:20,15:22,36:2,37:2}
def expected(case):
    payload,byte,source,split,submitted_length=case
    length=LENGTHS[byte%64]
    assert submitted_length==length and 0<split<length and 0<=payload<256
    start=4088 if source else 7340288
    return start+split*8,168+source,0,start+length*8,168+source,0,6510615555426900570
