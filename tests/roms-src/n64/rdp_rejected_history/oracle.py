"""Independent host pixel decisions, quotient FSM and quarter-grid observations."""
def division(index):
    denominator,numerator=divmod(index,2048)
    remainder=(16-denominator+numerator//256)%8;carry=False;result=0
    for bit in range(7,-1,-1):
        trial=2*remainder+(numerator//(2**bit))%2+(16-denominator if carry else denominator)
        carry=trial>=16;remainder=trial%8
        if carry:result+=2**bit
    return result
DIVISIONS=tuple(division(n) for n in range(32768))
def channels(word):return tuple((word//(2**shift))%256 for shift in (24,16,8,0))
def expected(case):
    raw,(profile,source)=case;seed=raw//256%256
    color=[seed,seed^255,seed*17%256,raw%256];shade=seed^127
    fog=channels(0x17c96b83);blend=channels(0xe143297f)
    if profile<4:
        p,a,m,b=((0,0,1,0),(2,1,3,2),(1,2,0,1),(0,0,1,0))[profile]
        aa=profile in (1,3);force=profile!=3;destination=(0,1,3,0)[profile]
        product=select=compare=profile==2
    else:
        v=profile-4;p,a,m,b=v%4,v//4%4,v//16%4,v//64
        aa=False;force=True;destination=v//64;product=select=compare=False
    memory=bytearray([0x5a]*48);hidden=[0]*24;history=[0]*8
    points=((0,0),(2,0),(1,1),(3,1),(0,2),(2,2),(1,3),(3,3))
    for row in range(4):
        for x in range((1,2,3,2)[row]):
            covered=[]
            for px,py in points:
                quarter=row*4+py;right=quarter if quarter<8 else 16-quarter
                if 0<=x*4+px<right:covered.append((px,py))
            count=len(covered);cvbit=(0,0) in covered
            pix=color[:];mem=[88,72,104,0];memory_count=0
            scaled=((256 if pix[3]==255 else pix[3])*count+4)//8
            if product:count=scaled//32%16
            if select:pix[3]=min(255,scaled if product else count*32)
            enabled=force or (aa and (count+memory_count)//8%2==0)
            colors=(pix,mem,blend,fog)
            alpha=(pix[3],fog[3],shade,0)[a]
            beta=(255-alpha,mem[3],255,0)[b]
            first,second=alpha//8,beta//8
            if b==1:first=first//4*4;second=second//16|3
            rgb=[]
            for lane in range(3):
                if not enabled or (a==0 and b==0 and pix[3]==255):value=colors[p][lane]
                else:
                    numerator=colors[p][lane]*first+colors[m][lane]*(second+1)
                    value=numerator//32%256 if force else DIVISIONS[(first//4*4+second//4*4+4)*512+numerator//4%2048]
                rgb.append(value)
            if destination==0:
                value=count+memory_count if enabled else count-1
                coverage=7 if value//8%2 else value%8
            else:coverage=(count+memory_count)%8 if destination==1 else 7 if destination==2 else memory_count
            at=8+(row*4+x)*2;history[(row*4+x)%8]=coverage%4
            if (compare and pix[3]<blend[3]) or not (count if aa else cvbit):continue
            word=(rgb[0]//8)*2048+(rgb[1]//8)*64+(rgb[2]//8)*2+coverage//4;memory[at:at+2]=word.to_bytes(2,"big");hidden[at//2]=coverage%4
    for lane in range(8):memory[8+lane*2]=0;hidden[4+lane]=(hidden[4+lane]%2)+(history[lane]//2)*2
    values=[int.from_bytes(memory[8+n:16+n],'big') for n in range(0,32,8)]
    for n in range(0,16,4):
        first=hidden[4+n]*4+hidden[5+n];second=hidden[6+n]*4+hidden[7+n];values.append(first*4294967296+second)
    values.extend((int.from_bytes(memory[:8],'big'),int.from_bytes(memory[40:48],'big')))
    return tuple(values+[(0xff0 if source else 0x700100)+256,128+source,32])
