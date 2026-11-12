#!/usr/bin/env python3
# Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
"""Audit which upright architecture members have an open top (P5.5-02 wave 2, package "shader": the Stage 1 stack rule).

Read-only, standard library only. Lists every STAT/MSTT in Skyrim.esm whose model is under architecture\\ and whose OBND
is an upright member (middle span <= 128, height >= 2x the larger horizontal span), reads each model from the vanilla
mesh archives (SSE BSA v105, LZ4 frames), and measures its top: the triangles whose three vertices lie within 4 units of
the mesh's highest point. A model whose top triangles (either winding) cover less than a quarter of the top footprint
has an open top: where the piece above it in a column is removed, the hollow end shows (Dragonsreach's castle pillars).
A model with a cap (the Bannered Mare's WRIntFreeWallColum01: 1998 of 2592 square units) shows a finished top.

    open_tops.py            write modules/camera/include/fixed_spike_shader_stack_audit.h
    open_tops.py --check    compare a fresh audit with the committed header (exit 1 if different)
    open_tops.py --list     print each upright model with its measures

SKYCRPG_SKYRIM_DATA overrides the Data folder. Nothing of the game is written. Not part of the build.
"""
import json
import math
import os
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "include/fixed_spike_shader_stack_audit.h"
DATA = Path(os.environ.get("SKYCRPG_SKYRIM_DATA", os.path.expanduser("~/.local/share/Steam/steamapps/common/Skyrim Special Edition/Data")))
ARCHIVES = ("Skyrim - Meshes0.bsa", "Skyrim - Meshes1.bsa")


# ---- Skyrim.esm: upright architecture members ------------------------------------------
def subrecords(b):
    pos, extended = 0, None
    while pos + 6 <= len(b):
        tag, size = struct.unpack_from("<4sH", b, pos)
        pos += 6
        if tag == b"XXXX":
            extended = struct.unpack_from("<I", b, pos)[0]
            pos += size
            continue
        if extended is not None:
            size, extended = extended, None
        yield tag, b[pos:pos + size]
        pos += size


def normalise(path):
    path = path.lower().replace("/", "\\")
    return path[7:] if path.startswith("meshes\\") else path


def upright_models():
    data = (DATA / "Skyrim.esm").read_bytes()
    models = set()
    pos = 24 + struct.unpack_from("<I", data, 4)[0]
    while pos < len(data):
        size = struct.unpack_from("<I", data, pos + 4)[0]
        if data[pos + 8:pos + 12] in (b"STAT", b"MSTT"):
            at = pos + 24
            while at < pos + size:
                _, rsize, flags, _ = struct.unpack_from("<4sIII", data, at)
                body = data[at + 24:at + 24 + rsize]
                if flags & 0x40000:
                    body = zlib.decompress(body[4:])
                obnd = model = None
                for tag, value in subrecords(body):
                    if tag == b"OBND":
                        obnd = struct.unpack("<6h", value)
                    elif tag == b"MODL":
                        model = normalise(value.rstrip(b"\0").decode("latin1"))
                if obnd and model and model.startswith("architecture\\"):
                    sx, sy, sz = obnd[3] - obnd[0], obnd[4] - obnd[1], obnd[5] - obnd[2]
                    if max(sx, sy) <= 128 and sz >= 2 * max(sx, sy):
                        models.add(model)
                at += 24 + rsize
        pos += size
    return sorted(models)
def lz4_block(src,out_size):
    dst=bytearray(); i=0; n=len(src)
    while i<n:
        tok=src[i]; i+=1
        lit=tok>>4
        if lit==15:
            while True:
                b=src[i]; i+=1; lit+=b
                if b!=255: break
        dst+=src[i:i+lit]; i+=lit
        if i>=n: break
        off=src[i]|(src[i+1]<<8); i+=2
        ml=tok&15
        if ml==15:
            while True:
                b=src[i]; i+=1; ml+=b
                if b!=255: break
        ml+=4; start=len(dst)-off
        for k in range(ml): dst.append(dst[start+k])
    return bytes(dst)
def lz4_frame(b):
    assert struct.unpack_from('<I',b,0)[0]==0x184D2204
    flg=b[4]; pos=6
    if flg&0x08: pos+=8
    if flg&0x01: pos+=4
    pos+=1
    out=bytearray()
    while True:
        sz=struct.unpack_from('<I',b,pos)[0]; pos+=4
        if sz==0: break
        raw=sz&0x80000000; sz&=0x7fffffff
        blk=b[pos:pos+sz]; pos+=sz
        out+= blk if raw else lz4_block(blk,0)
        if flg&0x10: pos+=4
    return bytes(out)
class BSA:
    def __init__(self,path):
        self.f=open(path,'rb'); f=self.f
        h=f.read(36)
        magic,ver,off,flags,nfold,nfile,lfold,lfile,fflags=struct.unpack('<4sIIIIIIIH',h[:34])
        assert ver==105,ver
        self.flags=flags
        folders=[struct.unpack('<QIIQ',f.read(24)) for _ in range(nfold)]
        recs=[]
        for _ in folders:
            pass
        f.seek(36+24*nfold)
        entries=[]
        for h_,cnt,_,o in folders:
            ln=f.read(1)[0]; name=f.read(ln)[:-1].decode('latin1')
            for _ in range(cnt):
                fh,size,fo=struct.unpack('<QII',f.read(16)); entries.append((name,size,fo))
        names=f.read(lfile).split(b'\0')
        self.files={}
        for (folder,size,fo),nm in zip(entries,names):
            self.files[(folder+'\\'+nm.decode('latin1')).lower()]=(size,fo)
    def find(self,suffix):
        return [k for k in self.files if k.endswith(suffix)]
    def read(self,key):
        size,fo=self.files[key]; f=self.f
        comp=bool(self.flags&4)!=bool(size&0x40000000); size&=0x3fffffff
        f.seek(fo); data=f.read(size)
        if self.flags&0x100:
            ln=data[0]; data=data[1+ln:]
        if comp:
            data=lz4_frame(data[4:])
        return data
def half(h):
    s=(h>>15)&1; e=(h>>10)&31; m=h&1023
    if e==0: v=m/1024*2**-14
    elif e==31: v=float('inf')
    else: v=(1+m/1024)*2**(e-15)
    return -v if s else v
class R:
    def __init__(s,b,p=0): s.b=b; s.p=p
    def u(s,fmt):
        v=struct.unpack_from('<'+fmt,s.b,s.p); s.p+=struct.calcsize('<'+fmt); return v if len(v)>1 else v[0]
def parse(b):
    r=R(b); r.p=b.index(b'\n')+1
    ver=r.u('I'); r.u('B'); user=r.u('I'); nblocks=r.u('I'); bsver=r.u('I')
    for _ in range(3): ln=r.u('B'); r.p+=ln
    ntypes=r.u('H'); types=[]
    for _ in range(ntypes): ln=r.u('I'); types.append(b[r.p:r.p+ln].decode()); r.p+=ln
    idx=[r.u('H') for _ in range(nblocks)]
    sizes=[r.u('I') for _ in range(nblocks)]
    nstr=r.u('I'); r.u('I')
    for _ in range(nstr): ln=r.u('I'); r.p+=ln
    ngr=r.u('I'); r.p+=4*ngr
    blocks=[]; p=r.p
    for i in range(nblocks): blocks.append((types[idx[i]],p,sizes[i])); p+=sizes[i]
    return bsver,blocks
def avobject(r):
    r.u('i'); n=r.u('I'); r.p+=4*n; r.u('i')   # name, extra data, controller
    flags=r.u('I'); t=r.u('3f'); rot=r.u('9f'); sc=r.u('f'); r.u('i')
    return t,rot,sc
def shapes(b):
    bsver,blocks=parse(b); out=[]; nodes={}
    for i,(ty,p,sz) in enumerate(blocks):
        r=R(b,p)
        if ty in ('NiNode','BSFadeNode','BSLeafAnimNode','BSMultiBoundNode'):
            t,rot,sc=avobject(r); n=r.u('I'); kids=[r.u('i') for _ in range(n)]
            nodes[i]=(t,rot,sc,kids)
        if ty in ('BSTriShape','BSDynamicTriShape','BSSubIndexTriShape','BSMeshLODTriShape'):
            t,rot,sc=avobject(r); r.u('4f'); r.u('3i')
            desc=r.u('Q'); ntri=r.u('H') if bsver<130 else r.u('I'); nv=r.u('H'); dsz=r.u('I')
            if dsz==0: continue
            stride=(desc&0xF)*4; vf=desc>>44
            vs=[]
            for k in range(nv):
                q=r.p+k*stride
                if vf&0x400 or bsver==100:
                    vs.append(struct.unpack_from('<3f',b,q))
                else:
                    vs.append(tuple(half(h) for h in struct.unpack_from('<3H',b,q)))
            r.p+=nv*stride
            tris=[struct.unpack_from('<3H',b,r.p+6*k) for k in range(ntri)]
            out.append((i,ty,t,rot,sc,vs,tris))
    return out,nodes
def topcap(b):
    """Return (maxZ, top footprint area, area of up-facing cap tris (+z by winding), area of down-winding tris at top)."""
    sh,nodes=shapes(b)
    pts=[]; tris=[]
    for i,ty,t,rot,sc,vs,tr in sh:
        base=len(pts)
        for v in vs:
            x=t[0]+sc*(rot[0]*v[0]+rot[1]*v[1]+rot[2]*v[2]); y=t[1]+sc*(rot[3]*v[0]+rot[4]*v[1]+rot[5]*v[2]); z=t[2]+sc*(rot[6]*v[0]+rot[7]*v[1]+rot[8]*v[2])
            pts.append((x,y,z))
        tris+= [(a+base,c+base,d+base) for a,c,d in tr]
    if not pts: return None
    zmax=max(p[2] for p in pts); zmin=min(p[2] for p in pts)
    top=[p for p in pts if p[2]>=zmax-4]
    fx=max(p[0] for p in top)-min(p[0] for p in top); fy=max(p[1] for p in top)-min(p[1] for p in top)
    up=down=0.0
    for a,c,d in tris:
        A,B,C=pts[a],pts[c],pts[d]
        if min(A[2],B[2],C[2])<zmax-4: continue
        ux,uy=B[0]-A[0],B[1]-A[1]; vx,vy=C[0]-A[0],C[1]-A[1]
        nz=(ux*vy-uy*vx)/2
        if nz>0: up+=nz
        else: down+=-nz
    return zmin,zmax,fx*fy,up,down,len(pts),len(tris)


def measure(model, archives):
    key = "meshes\\" + model
    for archive in archives:
        if key in archive.files:
            return topcap(archive.read(key))
    return None


def is_open(m):
    # zmin, zmax, top footprint, up-wound cap area, down-wound cap area, vertices, triangles
    return m is not None and m[2] > 0 and m[3] + m[4] < 0.25 * m[2]


def fnv(text):
    value = 14695981039346656037
    for byte in text.encode("latin1"):
        value = ((value ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


def header(rows, count):
    lines = ["// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT",
             "#pragma once",
             "// GENERATED by modules/camera/tools/open_tops.py from Skyrim.esm and the vanilla mesh archives; do not edit.",
             f"// {count} upright architecture member models read; these have an open top (no cap triangles at their highest",
             "// point): FNV-1a-64 of the model path as the CK stores it, lower case, without \"meshes\\\", backslashes.",
             "#include <array>",
             "#include <cstdint>",
             "",
             "namespace skycrpg::camera::fixed::shader {",
             f"constexpr std::array<std::uint64_t,{len(rows)}> openTopModels{{"]
    for value, model in rows:
        lines.append(f"    0x{value:016x}ull, // {model}")
    lines.append("};")
    lines.append("} // namespace skycrpg::camera::fixed::shader")
    return "\n".join(lines) + "\n"


def main():
    archives = [BSA(str(DATA / name)) for name in ARCHIVES]
    models = upright_models()
    measures = {m: measure(m, archives) for m in models}
    if "--list" in sys.argv:
        for m in models:
            print(("OPEN " if is_open(measures[m]) else "     ") + m, measures[m])
        return
    rows = sorted((fnv(m), m) for m in models if is_open(measures[m]))
    text = header(rows, len(models))
    if "--check" in sys.argv:
        same = HEADER.exists() and HEADER.read_text() == text
        print(f"open tops {'MATCH' if same else 'DIFFER'}: {len(rows)} of {len(models)} upright models")
        sys.exit(0 if same else 1)
    HEADER.write_text(text)
    print(f"wrote {HEADER.name}: {len(rows)} open of {len(models)} upright models")


if __name__ == "__main__":
    main()
