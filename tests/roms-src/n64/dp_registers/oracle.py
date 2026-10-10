"""Independent documented START latch and idle/frozen END transitions."""
def expected(value):
    address=value//8*8 % (1<<24)
    return (address,0,0x4aa,address,address,0xaa,address,address,address,0xaa)
