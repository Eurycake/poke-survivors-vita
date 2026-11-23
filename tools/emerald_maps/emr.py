import struct,pickle,os
from PIL import Image
import sys, tempfile
ROM_PATH=os.environ.get('EMERALD_ROM') or (sys.argv[1] if len(sys.argv)>1 else 'Pokemon - Emerald Version (USA, Europe).gba')
rom=open(ROM_PATH,'rb').read()
GRAPH_CACHE=os.path.join(tempfile.gettempdir(),'emerald_graph.pkl')
BASE=0x08000000
def isptr(v): return BASE<=v<BASE+len(rom)-4
def ptr(v): return v-BASE
def lz77(off):
    assert rom[off]==0x10
    size=rom[off+1]|rom[off+2]<<8|rom[off+3]<<16
    out=bytearray(); p=off+4
    while len(out)<size:
        flags=rom[p]; p+=1
        for i in range(8):
            if len(out)>=size: break
            if flags&(0x80>>i):
                b1,b2=rom[p],rom[p+1]; p+=2
                ln=(b1>>4)+3; disp=((b1&15)<<8|b2)+1
                for _ in range(ln): out.append(out[-disp])
            else:
                out.append(rom[p]); p+=1
    return bytes(out[:size])
if os.path.exists(GRAPH_CACHE):
    lay,hdr,maps,groups=pickle.load(open(GRAPH_CACHE,'rb'))
else:
    lay={}
    for off in range(0,len(rom)-24,4):
        w,h,b,m,p,s=struct.unpack_from('<2i4I',rom,off)
        if 1<=w<=512 and 1<=h<=512 and isptr(b) and isptr(m) and isptr(p) and isptr(s):
            pp=ptr(p); ss=ptr(s)
            if rom[pp]<=1 and rom[pp+1]==0 and rom[ss]<=1 and rom[ss+1]==1:
                lay[BASE+off]=(w,h,b,m,p,s)
    hdr={}
    for off in range(0,len(rom)-28,4):
        lp,ev,sc,cn,mus,lid=struct.unpack_from('<4IHH',rom,off)
        if lp in lay and isptr(ev) and isptr(sc) and (cn==0 or isptr(cn)) and 1<=lid<=600:
            hdr[BASE+off]=(lp,ev,sc,cn,mus,lid,rom[off+0x14],rom[off+0x15],rom[off+0x16],rom[off+0x17])
    Hs=set(hdr); best=None
    for off in range(0,len(rom)-4*40,4):
        n=0;prev=0
        while off+n*4+4<=len(rom):
            q=struct.unpack_from('<I',rom,off+n*4)[0]
            if not isptr(q) or q<=prev: break
            t=struct.unpack_from('<I',rom,ptr(q))[0]
            if t not in Hs: break
            prev=q;n+=1
        if n>=25 and (best is None or n>best[1]): best=(off,n)
    off,n=best
    groups=[struct.unpack_from('<I',rom,off+i*4)[0] for i in range(n)]
    maps={}
    for gi,g in enumerate(groups):
        end=groups[gi+1] if gi+1<n else None
        p=ptr(g);mi=0
        while True:
            if end is not None and p+BASE>=end: break
            v=struct.unpack_from('<I',rom,p)[0]
            if v not in Hs: break
            maps[(gi,mi)]=v;p+=4;mi+=1
    pickle.dump((lay,hdr,maps,groups),open(GRAPH_CACHE,'wb'))
inv={v:k for k,v in maps.items()}
def conns(h):
    cn=hdr[h][3]
    if not cn: return []
    cnt,p_=struct.unpack_from('<iI',rom,ptr(cn))
    return [struct.unpack_from('<BxxxiBBxx',rom,ptr(p_)+i*12) for i in range(cnt)]
_ts={}
def load_ts(addr):
    if addr in _ts: return _ts[addr]
    o=ptr(addr)
    c,s=rom[o],rom[o+1]
    t,pal,mt,at,cb=struct.unpack_from('<5I',rom,o+4)
    tiles=lz77(ptr(t)) if c else rom[ptr(t):ptr(t)+0x4000]
    pals=[]
    for i in range(16):
        cols=[]
        for j in range(16):
            v=struct.unpack_from('<H',rom,ptr(pal)+i*32+j*2)[0]
            cols.append((((v&31)*255+15)//31,(((v>>5)&31)*255+15)//31,(((v>>10)&31)*255+15)//31))
        pals.append(cols)
    n=512 if not s else 512
    mts=[struct.unpack_from('<8H',rom,ptr(mt)+i*16) for i in range(n)]
    attrs=[struct.unpack_from('<H',rom,ptr(at)+i*2)[0] for i in range(n)]
    _ts[addr]=(s,tiles,pals,mts,attrs)
    return _ts[addr]
_mt={}
def metatiles(pa,sa):
    key=(pa,sa)
    if key in _mt: return _mt[key]
    P=load_ts(pa);S=load_ts(sa)
    back=P[2][0][0]
    def tile(ent):
        idx=ent&0x3FF;hf=(ent>>10)&1;vf=(ent>>11)&1;pn=ent>>12
        data,i=(P[1],idx) if idx<512 else (S[1],idx-512)
        pal=(P[2] if pn<6 else S[2])[pn]
        px=[[None]*8 for _ in range(8)]
        base=i*32
        for y in range(8):
            for x in range(8):
                if base+y*4+x//2>=len(data): c=0
                else:
                    b=data[base+y*4+x//2];c=(b>>4) if x&1 else b&15
                px[7-y if vf else y][7-x if hf else x]=None if c==0 else pal[c]
        return px
    out=[]
    for m in range(1024):
        src=P if m<512 else S
        mt=src[3][m%512]; attr=src[4][m%512]
        img=Image.new('RGBA',(16,16),(0,0,0,0))
        for layer in (0,1):
            for q in range(4):
                px=tile(mt[layer*4+q]);ox=(q&1)*8;oy=(q>>1)*8
                for y in range(8):
                    for x in range(8):
                        if px[y][x] is not None: img.putpixel((ox+x,oy+y),px[y][x]+(255,))
                        elif layer==0: img.putpixel((ox+x,oy+y),back+(255,))
        out.append((img,attr&0xFF,(attr>>12)&15))
    _mt[key]=out
    return out
def get_map(h):
    w,hh,b,m,p,s=lay[hdr[h][0]]
    cells=[struct.unpack_from('<H',rom,ptr(m)+i*2)[0] for i in range(w*hh)]
    border=[struct.unpack_from('<H',rom,ptr(b)+i*2)[0] for i in range(4)]
    return dict(w=w,h=hh,cells=cells,border=border,pa=p,sa=s,lid=hdr[h][5])
