import sys,struct,os
img,out=sys.argv[1],sys.argv[2]; os.makedirs(out,exist_ok=True)
f=open(img,'rb'); h=f.read(0x60); n=struct.unpack_from('<I',h,0x3c)[0]
for i in range(n):
    f.seek(0x400+i*0x400); e=f.read(0x400)
    main=e[8:16].decode().strip(); sub=e[16:32].decode(errors='replace').strip('\0')
    name=e[36:292].split(b'\0')[0].decode()
    st,_,ol,_,off,_=struct.unpack_from('<6I',e,292)
    f.seek(off); d=f.read(ol)
    fn=os.path.basename(name.replace('\\','/')) or f'{main}_{sub}'
    open(os.path.join(out,fn),'wb').write(d)
    print(f'{i:2} {main:8} {sub:16} {fn:28} {ol:>11} @0x{off:x}')
