"""Independent detail/sharpen perspective LOD framebuffer and register oracle."""

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
    value,(style,minimum,maximum,format_code,source,negative,profile)=case
    assert 0<=style<4 and 0<=minimum<32 and maximum in (0,1,3)
    gradient=-value if negative else value
    sharpen=style&1;detail=style&2
    memory=bytearray([0x5a]*80)
    for y in range(4):
        for x in range(4):
            covered=[]
            for px,py in POINTS:
                quarter=y*4+py
                right=quarter if quarter<8 else 16-quarter
                if x*4+px<right:covered.append((px,py))
            if (0,0) not in covered:continue
            if profile:
                w=0x4000 if profile==1 else 0
                current,flag=coordinate(x*gradient,w)
                neighbor,nextflag=coordinate((x+1)*gradient,w)
                units=16384 if flag or nextflag else absolute_delta(current,neighbor)
            else:units=value-(1 if negative and value else 0)
            overflow=units>=16384
            magnify=not overflow and units<32
            integer=(units//32)%256
            level=max(0,integer.bit_length()-1)
            distant=overflow or maximum==0 or (not magnify and (units>=8192 or level>=maximum))
            if overflow:fraction=255
            elif magnify:
                fraction=(max(units,minimum)*8+(256 if sharpen else 0)) if style else (255 if distant else 0)
            else:fraction=255 if style==0 and distant else (units*8//(2**level))%256
            selected=maximum if distant else level
            first=selected+(1 if detail and not magnify else 0)
            second=(selected+(2 if not distant and not magnify else 1)) if detail else first+(1 if not distant and (not magnify or sharpen) else 0)
            color0,alpha0=decode(TILE_WORDS[first],format_code)
            color1,alpha1=decode(TILE_WORDS[second],format_code)
            factor=fraction-512 if fraction>=256 else fraction
            def combine(a,b):
                encoded=((a-b)*factor+b*256+128)//256%512
                return 0 if encoded>=384 else 255 if encoded>=256 else encoded
            rgb=[combine(color0[lane],color1[lane]) for lane in range(3)]
            opacity=combine(alpha0,alpha1)
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
