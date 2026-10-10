"""Independent byte mapping oracle for the declared vector memory profiles.
No production module or native floating-point operation is imported.
"""
def bank(seed):
    return [bytearray((seed+r*11+b*23+b//2*7)%256 for b in range(16)) for r in range(32)]
def memory(seed,origin):
    return {(origin-16+i)%4096:(seed*37+i*19+i//2*7)%256 for i in range(64)}
def transfer(regs,mem,store,kind,e,vt,address):
    a=address%4096
    if not store:
        for dest in range(16):
            src=None;value=None
            if kind<=4:
                length=(1,2,4,8,16-a%16)[kind]
                if e<=dest<min(e+length,16):src=a+dest-e
            elif kind==5:
                if e+16-a%16<=dest<16:src=a//16*16+dest-e-16+a%16
            elif kind in (6,7,8):
                source=a//8*8+((a%8-e+(dest//2)*(2 if kind==8 else 1))%16)
                code=mem[source%4096]*(256 if kind==6 else 128)
                value=(code//(256 if dest%2==0 else 1))%256
            elif kind==9 and e<=dest<min(e+8,16):
                lane=dest//2
                index=(0,4,8,12,8,12,0,4)[lane]
                source=a//8*8+(a%8+index+(e if lane==0 else -e))%16
                code=mem[source%4096]*128
                value=(code//(256 if dest%2==0 else 1))%256
            if src is not None:value=mem[src%4096]
            if value is not None:regs[vt][dest]=value
        if kind==11:
            for lane in range(8):
                reg=vt//8*8+(e//2+lane)%8
                for b in range(2):regs[reg][2*lane+b]=mem[(a//8*8+((a//8%2)*8+e+2*lane+b)%16)%4096]
        return
    if kind<=5:
        if kind==5:
            indices=range(16-a%16,16)
            for i in indices:mem[(a//16*16+i-16+a%16)%4096]=regs[vt][(e+i)%16]
        else:
            length=(1,2,4,8,16-a%16)[kind]
            for i in range(length):mem[(a+i)%4096]=regs[vt][(e+i)%16]
    elif kind in (6,7):
        for i in range(8):
            index=(e+i)%8;code=int.from_bytes(regs[vt][2*index:2*index+2],'big')
            upper=(e+i)%16<8
            divisor=(256 if upper else 128) if kind==6 else (128 if upper else 256)
            mem[(a+i)%4096]=(code//divisor)%256
    elif kind==8:
        for i in range(8):
            code=regs[vt][(e+2*i)%16]*256+regs[vt][(e+2*i+1)%16]
            mem[(a//8*8+(a%8+2*i)%16)%4096]=(code//128)%256
    elif kind==9:
        lanes={0:(0,1,2,3),1:(6,7,4,5),4:(1,2,3,0),5:(7,4,5,6),8:(4,5,6,7),11:(3,0,1,2),12:(5,6,7,4),15:(0,1,2,3)}
        for i in range(4):
            code=0 if e not in lanes else int.from_bytes(regs[vt][2*lanes[e][i]:2*lanes[e][i]+2],'big')//128
            mem[(a//8*8+(a%8+4*i)%16)%4096]=code%256
    elif kind==10:
        for i in range(16):mem[(a//8*8+(a%8+i)%16)%4096]=regs[vt][(e+i)%16]
    else:
        for i in range(16):
            register=vt//8*8+(i//2-(a//8*8)//2+e//2)%8
            source=(i+a//8*8)%16
            mem[(a//8*8+(a+i)%16)%4096]=regs[register][source]
def expected(case,store=False):
    seed,alignment,e,kind=case;vt=seed%32;group=vt//8*8;origin=0xff0 if seed%2 else 0x900
    regs=bank(seed);mem=memory(seed,origin);transfer(regs,mem,store,kind,e,vt,origin+alignment)
    data=b''.join(regs[group:group+8])+regs[(group-1)%32]+regs[(group+8)%32]
    if store:data+=bytes(mem[(origin-16+i)%4096] for i in range(64))
    return tuple(int.from_bytes(data[i:i+8],'big') for i in range(0,len(data),8))+(origin+alignment,0,280,3,0)
