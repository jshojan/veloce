"""Independent mixed-gradient perspective LOD framebuffer and register oracle."""

TILE_WORDS=(0xf800,0x07c1,0x003e,0xffff,0x1110)
POINTS=((0,0),(2,0),(1,1),(3,1),(0,2),(2,2),(1,3),(3,3))


def endpoint(segment):
    denominator=64+segment
    return 14979 if segment==6 else (1048576+denominator//2)//denominator


def coordinate(raw,w):
    raw&=65535;w&=65535
    numerator=raw if raw<32768 else raw-65536
    magnitude=w&32767
    shift=min(14,14-(magnitude.bit_length()-1)) if magnitude else 14
    normalized=(magnitude<<shift)&16383
    segment,fraction=divmod(normalized,256)
    reciprocal=(endpoint(segment)*(256-fraction)+endpoint(segment+1)*fraction)//256
    product=numerator*reciprocal
    limit=1<<(29-shift)
    scaled=product*2 if shift==14 else product//(1<<(13-shift))
    overflow=not w or w>=32768 or product>=limit or product< -limit
    value=scaled&131071
    return (value if value<65536 else value-131072),overflow


def decode(word,format_code):
    if format_code==0:
        values=((word>>11)&31,(word>>6)&31,(word>>1)&31)
        return [value*8+value//4 for value in values],255 if word&1 else 0
    return [word>>8]*3,word&255


def absolute_delta(current,next):
    delta=next-current
    return delta if delta>=0 else -delta-1


def expected(case):
    vector,(signmask,edge_mode,format_code,source,w,(wx,wy))=case
    assert len(vector)==4 and 0<=signmask<16 and edge_mode in (0,1)
    sx,tx,sy,ty=(value*(-1 if signmask&(1<<lane) else 1)
                 for lane,value in enumerate(vector))
    se,te=(sy,ty) if edge_mode==0 else (511,-511)
    we=wy if edge_mode==0 else 256-wy
    maximum=3
    memory=bytearray([0x5a]*80)
    for y in range(4):
        for x in range(4):
            covered=[]
            for px,py in POINTS:
                quarter=y*4+py
                right=quarter if quarter<8 else 16-quarter
                if x*4+px<right:covered.append((px,py))
            if (0,0) not in covered:continue
            raw_s=x*sx+y*se;raw_t=x*tx+y*te;raw_w=w+x*wx+y*we
            def divided(ds,dt,dw):
                s,so=coordinate(raw_s+ds,raw_w+dw)
                t,to=coordinate(raw_t+dt,raw_w+dw)
                return s,t,so or to
            current=divided(0,0,0);right=divided(sx,tx,wx);below=divided(sy,ty,wy)
            units=16384 if current[2] or right[2] or below[2] else max(
                absolute_delta(current[0],right[0]),
                absolute_delta(current[1],right[1]),
                absolute_delta(current[0],below[0]),
                absolute_delta(current[1],below[1]))
            magnify=units<32
            level=(units>>5).bit_length()-1 if not magnify else 0
            distant=maximum==0 or (not magnify and level>=maximum)
            first=min(level,maximum)
            second=first+(0 if magnify or distant else 1)
            fraction=255 if distant else 0 if magnify else ((units<<3)>>level)&255
            color0,alpha0=decode(TILE_WORDS[first],format_code)
            color1,alpha1=decode(TILE_WORDS[second],format_code)
            rgb=[((color0[lane]-color1[lane])*fraction+color1[lane]*256+128)//256
                 for lane in range(3)]
            opacity=((alpha0-alpha1)*fraction+alpha1*256+128)//256
            if opacity==255:blended=rgb
            else:
                a,b=opacity//8,(255-opacity)//8
                blended=[((channel*a+0x5a*(b+1))//32)%256 for channel in rgb]
            count=len(covered)
            coverage=7 if count+2>=8 else count+2
            at=8+(y*4+x)*4
            memory[at:at+4]=bytes(blended+[coverage*32])
    words=[int.from_bytes(memory[8+n:16+n],'big') for n in range(0,64,8)]
    words.extend((int.from_bytes(memory[:8],'big'),int.from_bytes(memory[72:80],'big')))
    return tuple(words+[(0xff0 if source else 0x700100)+240,128+source,32])
