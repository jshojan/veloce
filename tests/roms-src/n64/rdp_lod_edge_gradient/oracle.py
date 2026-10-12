"""Independent mixed-sign vertical-gradient LOD framebuffer oracle."""

# Guest source-image words, one different solid texel per TMEM tile.
TILE_WORDS=(0xf800,0x07c1,0x003e,0xffff,0x1110)


def decode(word,format_code):
    if format_code==0:
        values=((word>>11)&31,(word>>6)&31,(word>>1)&31)
        return [value*8+value//4 for value in values],255 if word&1 else 0
    return [word>>8]*3,word&255


def expected(case):
    vector,(signmask,edge_mode,format_code,source)=case
    assert len(vector)==4 and all(value in (0,1,31,32,63,255) for value in vector) and any(vector)
    assert 0<=signmask<16 and edge_mode in (0,1)
    units=max(value-(1 if signmask&(1<<lane) and value else 0) for lane,value in enumerate(vector))
    magnify=units<32
    level=(units>>5).bit_length()-1 if not magnify else 0
    distant=level>=3 if not magnify else False
    first=min(level,3)
    second=first+(0 if magnify or distant else 1)
    texel0,alpha0=decode(TILE_WORDS[first],format_code)
    texel1,alpha1=decode(TILE_WORDS[second],format_code)
    fraction=255 if distant else 0 if magnify else ((units<<3)>>level)&255
    opacity=((alpha0-alpha1)*fraction+alpha1*256+128)//256
    combined=[((texel0[lane]-texel1[lane])*fraction+
               texel1[lane]*256+128)//256 for lane in range(3)]
    if opacity==255:blended=combined
    else:
        a,b=opacity//8,(255-opacity)//8
        blended=[((value*a+0x5a*(b+1))//32)%256 for value in combined]
    memory=bytearray([0x5a]*80)
    points=((0,0),(2,0),(1,1),(3,1),(0,2),(2,2),(1,3),(3,3))
    for row in range(4):
        for x in range(4):
            covered=[]
            for px,py in points:
                quarter=row*4+py
                right=quarter if quarter<8 else 16-quarter
                if x*4+px<right:covered.append((px,py))
            if (0,0) not in covered:continue
            count=len(covered)
            coverage=7 if count+2>=8 else count+2
            at=8+(row*4+x)*4
            memory[at:at+4]=bytes(blended+[coverage*32])
    words=[int.from_bytes(memory[8+n:16+n],'big') for n in range(0,64,8)]
    words.extend((int.from_bytes(memory[:8],'big'),
                  int.from_bytes(memory[72:80],'big')))
    return tuple(words+[(0xff0 if source else 0x700100)+240,128+source,32])
