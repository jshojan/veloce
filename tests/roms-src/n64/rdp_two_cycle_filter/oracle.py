"""Independent two-tile triangular texture-filter and framebuffer oracle."""

# Four source-image rows. The first two belong to tile zero, the next two to
# tile one; these are texture input bytes, never expected framebuffer values.
WORDS=((0x0021,0x7c1f,0xf801,0x5555),
       (0xace0,0x0fe1,0xe3c0,0x39af),
       (0xffff,0x1234,0x9abc,0x3456),
       (0x7654,0xdead,0xbeef,0x0247))


def decode(word, format_code):
    if format_code == 0:
        rgb=tuple(value*8+value//4 for value in
                  ((word>>11)&31,(word>>6)&31,(word>>1)&31))
        return rgb+(255 if word&1 else 0,)
    return (word>>8,)*3+(word&255,)


def sample(format_code, tile, filter_mode, s, t):
    corner=(decode(WORDS[tile*2+y][x],format_code)
            for y,x in ((0,0),(0,1),(1,0),(1,1)))
    q=tuple(corner)
    color=[]
    for lane in range(4):
        if filter_mode==2 and s==16 and t==16:
            value=(sum(pixel[lane] for pixel in q)+2)//4
        elif s+t<32:
            value=((32-s-t)*q[0][lane]+s*q[1][lane]+t*q[2][lane]+16)//32
        else:
            value=((32-t)*q[1][lane]+(32-s)*q[2][lane]+(s+t-32)*q[3][lane]+16)//32
        color.append(value)
    return color


def expected(case):
    s,t,(format_code,filter_mode,source,primitive_alpha)=case
    first=sample(format_code,0,filter_mode,s,t)
    second=sample(format_code,1,filter_mode,s,t)
    combined=[((first[lane]-second[lane])*primitive_alpha+
               second[lane]*256+128)//256 for lane in range(3)]
    opacity=second[3]
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
    return tuple(words+[(0xff0 if source else 0x700100)+192,128+source,32])
