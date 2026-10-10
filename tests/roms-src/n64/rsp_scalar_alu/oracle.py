"""Independent mathematical oracle for actual scalar GPR/DMEM captures."""
def signed(v):return v-2**32 if v>=2**31 else v
def expected(ins,a,b):
    op=ins//2**26;fn=ins%64;sa=ins//64%32;imm=ins%65536;simm=imm-65536 if imm>=32768 else imm
    if op==0:
        if fn in (32,33):result=a+b
        elif fn in (34,35):result=a-b
        elif fn==36:result=a&b
        elif fn==37:result=a|b
        elif fn==38:result=a^b
        elif fn==39:result=~(a|b)
        elif fn==42:result=int(signed(a)<signed(b))
        elif fn==43:result=int(a<b)
        elif fn in (0,4):result=b*2**(sa if fn==0 else a%32)
        elif fn in (2,6):result=b//2**(sa if fn==2 else a%32)
        elif fn in (3,7):result=signed(b)//2**(sa if fn==3 else a%32)
        else:raise ValueError('unknown function')
    elif op in (8,9):result=a+simm
    elif op==10:result=int(signed(a)<simm)
    elif op==11:result=int(a<simm%2**32)
    elif op==12:result=a&imm
    elif op==13:result=a|imm
    elif op==14:result=a^imm
    elif op==15:result=imm*65536
    else:raise ValueError('unknown opcode')
    return (result%2**32,a,b,0,32,3,0),2**32-1
