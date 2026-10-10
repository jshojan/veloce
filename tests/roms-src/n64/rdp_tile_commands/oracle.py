"""Independent integer MMIO oracle. Attribute values have no hardware readback;
this ROM grades consumption, real FullSync IRQ and an untouched RAM guard only.
Tile/bounds/image retention is separately verified by the component gate.
"""
def expected(case):
    raw,source=case
    end=0x1008 if source else 0x700118
    return end,0xa8+source,0,end+8,0x80+source,32,0x5a5a5a5a5a5a5a5a
