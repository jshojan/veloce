"""Independent quarter-grid, ordered seeds, alpha gates and packed RAM grading."""
MAGIC=((0,6,1,7),(4,2,5,3),(3,5,2,4),(7,1,6,0))
BAYER=((0,4,1,5),(4,0,5,1),(3,7,2,6),(7,3,6,2))
def expected(case):
    raw,(profile,source)=case;seed=raw//256;opacity=raw%256
    rgb=(0,1,3)[profile//3];alpha=(0,1,3)[profile%3]
    color=bytearray([0x5a]*32);hidden=[0]*16
    points=((0,0),(2,0),(1,1),(3,1),(0,2),(2,2),(1,3),(3,3))
    for y in range(4):
        for x in range(4):
            count=0
            for px,py in points:
                q=y*4+py;right=q if q<8 else 16-q
                count+=int(0<=x*4+px<right)
            matrix=MAGIC[y%4][x%4] if rgb==0 else BAYER[y%4][x%4]
            cd=7 if rgb==3 else matrix;ad=0 if alpha==3 else matrix if alpha==0 else 7-matrix
            scaled=((256 if opacity==255 else opacity)*count+4)//8
            if source:count=scaled//32%16;actual_alpha=min(255,scaled)
            else:actual_alpha=min(255,(256 if opacity==255 else opacity)+ad)
            if actual_alpha<127 or not count:continue
            shade=min(255,(seed^127)+ad);a=shade//8;b=(255-shade)//8
            inputs=(seed,seed^255,seed*17%256);memory=(88,72,104)
            values=[]
            for lane in range(3):
                value=(inputs[lane]*a+memory[lane]*(b+1))//32%256
                if value%8>cd:value=min(255,value//8*8+8)
                values.append(value)
            n=y*4+x;word=values[0]//8*2048+values[1]//8*64+values[2]//8*2+1
            color[n*2:n*2+2]=word.to_bytes(2,'big');hidden[n]=3
    words=[int.from_bytes(color[n:n+8],'big') for n in (0,8,16,24)]
    words += [(hidden[n]*4+hidden[n+1])*4294967296+hidden[n+2]*4+hidden[n+3] for n in (0,4,8,12)]
    return tuple(words+[0x5a5a5a5a5a5a5a5a]*2+[(0xff0 if source else 0x700100)+160,128+source,32])
