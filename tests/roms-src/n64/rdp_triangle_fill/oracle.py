"""Independent scalar quarter-scanline oracle; never imports command packing."""
LENGTHS={8:4,9:6,10:12,11:14,12:12,13:14,14:20,15:22}
def signed(raw,bits):
    raw%=2**bits
    return raw if raw<2**(bits-1) else raw-2**bits
def expected(case):
    raw,(profile,flip,encoded,field,source,size)=case
    # Separate coordinate specification from the original guest's packer.
    yh,ym,yl=0,8,16;high,middle,low=0,0,131072;dh,dm,dl=0,65536,-65536;bounds=(0,0,16,16)
    if profile==1:yh,ym,yl=1,9,15
    if profile==2:yh=-4;middle=-65536
    if profile==3:bounds=(5,2,13,14)
    if profile==4:ym=7;low=163840;dl=-49152
    if profile==5:yh,ym,yl=1,5,11;high=81920;middle=low=114688;dm=dl=0
    if profile==6:ym=yl=0
    if profile==7:high=196608
    if not flip:high,middle,low=131072-high,131072-middle,131072-low;dh,dm,dl=-dh,-dm,-dl
    sl,st,sr,sb=bounds;top=max(yh,st);bottom=min(yl,sb);pixels={}
    major=signed(high,28)&~1;minor=signed(middle,28)&~1
    major_delta=(signed(dh,30)//4)&~1;minor_delta=(signed(dm,30)//4)&~1
    def clipped(position):
        x=signed(position,28)
        # Quarter-pixel integer plus sticky lower fraction, in eighth-pixels.
        eighths=2*(x//16384)+int(x%16384>=2)
        return max(sl*2,min(sr*2,eighths)),eighths<sl*2,eighths>=sr*2
    for y in range((yh//4)*4,((bottom+3)//4)*4):
        if y==ym:minor=signed(low,28)&~1;minor_delta=(signed(dl,30)//4)&~1
        if y>=0:
            row=y//4
            if row not in pixels:pixels[row]=[None,None,True,True]
            a,au,ao=clipped(major);b,bu,bo=clipped(minor);r=pixels[row]
            r[2]&=au and bu;r[3]&=ao and bo
            major_quarter=signed(major,28)//16384;minor_quarter=signed(minor,28)//16384
            crossed=minor_quarter<major_quarter if flip else major_quarter<minor_quarter
            if top<=y<bottom and not crossed:
                lo,hi=(a//8,b//8) if flip else (b//8,a//8)
                r[0]=lo if r[0] is None else min(r[0],lo);r[1]=hi if r[1] is None else max(r[1],hi)
        major=(major+major_delta)%2**32;minor=(minor+minor_delta)%2**32
    memory=bytearray([0x5a]*80);color=raw*65536+(65535-raw);bpp=2**(size-1)
    for row,(lo,hi,under,over) in pixels.items():
        if lo is None or under or over or (field and row%2!=int(field==2)):continue
        for x in range(lo,hi+1):
            offset=(row*4+x)*bpp
            for byte in range(bpp):memory[8+offset+byte]=color.to_bytes(4,'big')[(offset+byte)%4]
    words=[int.from_bytes(memory[8+n:16+n],'big') for n in range(0,64,8)]
    words.extend((int.from_bytes(memory[:8],'big'),int.from_bytes(memory[72:80],'big')))
    end=(0xff0 if source else 0x700100)+8*(LENGTHS[8+(encoded&7)]+5)
    return tuple(words+[end,128+source,32])
