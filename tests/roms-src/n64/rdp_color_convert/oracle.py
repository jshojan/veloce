"""Independent logical source, signed coefficient and quarter-grid oracle."""
def clamp(value):
    value%=512
    return value if value<256 else 255 if value<384 else 0
def signed9(raw):return raw if raw<256 else raw-512
def expected(case):
    parameters,(field,source,profile)=case
    seed=parameters%256
    format,size=((1,2),(1,3),(0,2),(3,2))[profile-2] if profile>=2 else ((0,2) if profile==1 else (1,2))
    s,t=(parameters%32,parameters//32%32) if field==5 else (7,23)
    coeff=[175,469,423,222,114,42]
    if field<4:coeff[field]=parameters//512%512
    if profile==1:coeff[4]=parameters//512%512;coeff[5]=parameters%512
    def color(x,y):
        v=seed if (x+y)%2==0 else seed^255
        if format==1:
            # U/V pairs are [seed, complement] in both horizontal corners;
            # the second row reverses them. Luma alternates by S and T.
            u=(seed if y==0 else seed^255)-128
            vv=((seed^255) if y==0 else seed)-128
            return [u,vv,v]
        if format==3:return [v]*3
        word=v*257;code=(word//2048,word//64%32,word//2%32)
        return [c*8+c//4 for c in code]
    if profile==1:
        texel=color(0,0);k4=coeff[4] if coeff[4]<384 else coeff[4]-512;k5=signed9(coeff[5])
        result=[clamp(((v-k4)*k5+v*256+128)//256) for v in texel]
    else:
        upper=s+t>=32;upper_uv=(s//2 if format==1 else s)+t>=32
        chroma=color(int(upper_uv),int(upper_uv));luma=color(int(upper),int(upper))[2]
        k=[2*signed9(v)+1 for v in coeff[:4]];u,v=chroma[:2]
        result=[clamp(luma+(k[0]*v+128)//256),clamp(luma+(k[1]*u+k[2]*v+128)//256),clamp(luma+(k[3]*u+128)//256)]
    memory=bytearray([0x5a]*80)
    points=((0,0),(2,0),(1,1),(3,1),(0,2),(2,2),(1,3),(3,3))
    for row in range(4):
        for x in range(4):
            covered=[]
            for px,py in points:
                q=row*4+py;right=q if q<8 else 16-q
                if 0<=x*4+px<right:covered.append((px,py))
            if (0,0) in covered:
                at=8+(row*4+x)*4;memory[at:at+4]=bytes(result+[(len(covered)-1)*32])
    values=[int.from_bytes(memory[8+n:16+n],'big') for n in range(0,64,8)]
    values.extend((int.from_bytes(memory[:8],'big'),int.from_bytes(memory[72:80],'big')))
    return tuple(values+[(0xff0 if source else 0x700100)+176,128+source,32])
