"""Independent scalar pixel oracle; never imports guest geometry/command packing."""
def signed(raw,bits):
    raw%=2**bits
    return raw if raw<2**(bits-1) else raw-2**bits
def clamp(value):
    value%=512
    return value if value<256 else 255 if value<384 else 0
def expected(case):
    raw,(profile,flip,program,field,source,destination,read)=case
    yh,ym,yl=0,8,16;high,middle,low=0,0,131072;dh,dm,dl=0,65536,-65536;bounds=(0,0,16,16)
    if profile==1:yh,ym,yl=1,9,15
    if profile==2:yh=-4;middle=-65536
    if profile==3:bounds=(5,2,13,14)
    if profile==4:ym=7;low=163840;dl=-49152
    if profile==5:yh,ym,yl=1,5,11;high=81920;middle=low=114688;dm=dl=0
    if profile==6:ym=yl=0
    if profile==7:high=196608
    if not flip:high,middle,low=131072-high,131072-middle,131072-low;dh,dm,dl=-dh,-dm,-dl
    sl,st,sr,sb=bounds;top=max(yh,st);bottom=min(yl,sb)
    rgba=(raw>>8,raw&255,(raw>>8)^255,(raw&255)^255)
    primitive=rgba;environment=tuple(v^255 for v in rgba)
    dx=(65536,-65536,32768,16384);de=(32768,32768,-32768,0);dy=(16384,65536,0,-16384)
    values=[v*65536 for v in rgba]
    major=signed(high,28)&~1;minor=signed(middle,28)&~1
    major_delta=(signed(dh,30)//4)&~1;minor_delta=(signed(dm,30)//4)&~1
    offset=bool((dh%2**32)&2**31)==bool(flip);load=3 if offset else 0
    rows={}
    def clipped(position):
        x=signed(position,28);eighths=2*(x//16384)+int(x%16384>=2)
        return max(sl*2,min(sr*2,eighths)),eighths<sl*2,eighths>=sr*2
    for y in range((yh//4)*4,((bottom+3)//4)*4):
        if y==ym:minor=signed(low,28)&~1;minor_delta=(signed(dl,30)//4)&~1
        if y>=0:
            row=y//4;sub=y%4
            if row not in rows:rows[row]=dict(left=[0]*4,right=[0]*4,enabled=[False]*4,lo=None,hi=None,under=True,over=True)
            r=rows[row];a,au,ao=clipped(major);b,bu,bo=clipped(minor)
            r['under']&=au and bu;r['over']&=ao and bo
            major_quarter=signed(major,28)//16384;minor_quarter=signed(minor,28)//16384
            crossed=minor_quarter<major_quarter if flip else major_quarter<minor_quarter
            left,right=(a,b) if flip else (b,a);r['left'][sub]=left;r['right'][sub]=right
            if top<=y<bottom and not crossed:
                r['enabled'][sub]=True
                r['lo']=left//8 if r['lo'] is None else min(r['lo'],left//8)
                r['hi']=right//8 if r['hi'] is None else max(r['hi'],right//8)
            if sub==load:
                r['anchor']=signed(major>>16,12);fraction=(major>>8)&255
                adjusted=[]
                for lane in range(4):
                    correction=(de[lane]&~511)-((de[lane]&~511)//4)-(dy[lane]&~511)+((dy[lane]&~511)//4) if offset else 0
                    value=((values[lane]&~511)+correction-fraction*((dx[lane]//256)&~1))%2**32
                    adjusted.append(value-value%1024)
                r['attributes']=adjusted
        if y%4==3:values=[(v+step)%2**32 for v,step in zip(values,de)]
        major=(major+major_delta)%2**32;minor=(minor+minor_delta)%2**32
    memory=bytearray([0x5a]*80)
    points=((0,0),(2,0),(1,1),(3,1),(0,2),(2,2),(1,3),(3,3))
    for row,r in rows.items():
        if r['lo'] is None or r['under'] or r['over'] or (field and row%2!=int(field==2)):continue
        start=r['lo'] if flip else r['hi'];skip=((start-r['anchor']) if flip else (r['anchor']-start))%4096
        if not flip:skip=-skip
        traversal=range(r['lo'],r['hi']+1) if flip else range(r['hi'],r['lo']-1,-1)
        for x in traversal:
            covered=[(py,px) for px,py in points if r['enabled'][py] and r['left'][py]<=x*8+px*2<r['right'][py]]
            if (0,0) not in covered:continue
            offy,offx=min(covered);count=len(covered);shade=[]
            for lane in range(4):
                value=(r['attributes'][lane]+(dx[lane]&~31)*(skip+x-start))%2**32;q=signed(value,32)//16384
                correction=q//4 if count==8 else (q*4+offx*signed((dx[lane]&~31)>>14,13)+offy*signed(dy[lane]>>14,13))//16
                shade.append(clamp(correction))
            color=[]
            for lane in range(3):
                if program==0:v=shade[lane]
                elif program==1:v=primitive[lane]
                elif program==2:v=environment[lane]
                elif program==3:v=(shade[lane]*primitive[3]+128)//256
                elif program==4:v=((primitive[lane]-environment[lane])*shade[lane]+environment[lane]*256+128)//256
                elif program==5:v=256
                elif program==6:v=0
                else:v=((shade[lane]-primitive[lane])*73+environment[lane]*256+128)//256
                color.append(clamp(v))
            at=8+(row*4+x)*4;mem=memory[at+3]//32 if read else 7
            coverage=(count-1) if destination==0 else (count+mem)%8 if destination==1 else 7 if destination==2 else mem
            memory[at:at+4]=bytes(color+[coverage*32])
    words=[int.from_bytes(memory[8+n:16+n],'big') for n in range(0,64,8)]
    words.extend((int.from_bytes(memory[:8],'big'),int.from_bytes(memory[72:80],'big')))
    end=(0xff0 if source else 0x700100)+152
    return tuple(words+[end,128+source,32])
