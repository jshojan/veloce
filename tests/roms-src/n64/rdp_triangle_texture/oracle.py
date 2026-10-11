"""Independent logical source-pixel and barycentric framebuffer oracle.

Does not import command packing, production kernels or guest expected output.
"""
def clamp(value):
    value%=512
    return value if value<256 else 255 if value<384 else 0

def expected(case):
    raw,(format,size,filter,source,flip),s,t=case
    first=raw.to_bytes(2,'big')+(raw^65535).to_bytes(2,'big')
    rows=(first*2,bytes(v^255 for v in first)*2)
    def pixel(x,y):
        data=rows[y]
        if format==1:
            # Loader separates U/V from Y, irrespective of target size.
            chroma=data[::2];luma=data[1::2]
            if size<2:
                value=chroma[x]
                if size==0:value=value//16*17
                return [value-128,value-128,value,value]
            u,v=chroma[(x//2)*2:(x//2)*2+2]
            light=luma[x];alpha=light
            if size==3 and x%2==0:alpha=light%16+luma[x+1]//16*16
            return [u-128,v-128,light,alpha]
        kind=min(format,4)
        if size==0:
            value=data[x//2]//16 if x%2==0 else data[x//2]%16
            if kind==2:return [value]*4 # palette descriptor is zero; TLUT disabled
            if kind==3:
                i=value//2;return [i*32+i*4+i//2]*3+[255*(value%2)]
            return [value*17]*4
        if size==1:
            value=data[x]
            if kind==3:return [value//16*17]*3+[value%16*17]
            return [value]*4
        if kind==0 and size==3:return list(data[x*4:x*4+4])
        high,low=data[x*2:x*2+2]
        if kind==0:
            value=high*256+low
            code=(value//2048,value//64%32,value//2%32)
            return [c*8+c//4 for c in code]+[255*(value%2)]
        if kind==3 and size==2:return [high]*3+[low]
        return [high,low,high,low]
    q=[pixel(x,y) for y in range(2) for x in range(2)]
    if format==1:
        # Right chroma advances two S units. Both rows contain four chroma
        # bytes; the YUV16/32 pairs for the second corner are bytes 2/3.
        for point in (1,3):
            data=rows[point//2][::2]
            if size<2:
                value=data[2];value=value//16*17 if size==0 else value
                q[point]=[value-128,value-128,value,value]
            else:q[point][:2]=[data[2]-128,data[3]-128]
    color=[]
    uv=s//2 if format==1 else s
    high_y=s+t>=32;high_uv=uv+t>=32
    if format==1 and size<2 and high_y!=high_uv:
        original=[v[:] for v in q]
        for n in range(4):q[n][2:]=original[3-n][2:]
    for lane in range(3):
        if not filter:value=pixel(0,0)[lane]
        else:
            x=uv if lane<2 else s
            if filter==2 and x==16 and t==16:value=(sum(v[lane] for v in q)+2)//4
            elif x+t<32:value=((32-x-t)*q[0][lane]+x*q[1][lane]+t*q[2][lane]+16)//32
            else:value=((32-t)*q[1][lane]+(32-x)*q[2][lane]+(x+t-32)*q[3][lane]+16)//32
        color.append(clamp(value))
    memory=bytearray([0x5a]*80)
    samples=((0,0),(2,0),(1,1),(3,1),(0,2),(2,2),(1,3),(3,3))
    for row in range(4):
        for x in range(4):
            covered=[]
            for px,py in samples:
                quarter=row*4+py
                minor=quarter if quarter<8 else 16-quarter
                left,right=(0,minor) if flip else (8-minor,8)
                if left<=x*4+px<right:covered.append((px,py))
            if (0,0) not in covered:continue
            at=8+(row*4+x)*4;memory[at:at+4]=bytes(color+[(len(covered)-1)*32])
    words=[int.from_bytes(memory[8+n:16+n],'big') for n in range(0,64,8)]
    words.extend((int.from_bytes(memory[:8],'big'),int.from_bytes(memory[72:80],'big')))
    return tuple(words+[(0xff0 if source else 0x700100)+168,128+source,32])
