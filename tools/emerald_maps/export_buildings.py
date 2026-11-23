#!/usr/bin/env python3
"""Cuts the building models of HctrRB/pokeemerald-vita-3ds (assets/voxel/buildings.bin, from Emerald 3DS)
for the stitched town stages. usage: export_buildings.py buildings.bin <workspace_dir>
Needs tools/emerald_maps/stage_layouts.json (written by build_maps.py). Writes
assets/world/bld_<stage>.bin + bld_<stage>.png (stage 0,1 = the two town stages)."""
import sys, os, json, struct
from PIL import Image
bp, ws = sys.argv[1], sys.argv[2]
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import reliefdata
RELIEF = reliefdata.load(os.path.join(os.path.dirname(bp), 'relief.bin'))
d = open(bp, 'rb').read()
U16 = lambda o: struct.unpack_from('<H', d, o)[0]
U32 = lambda o: struct.unpack_from('<I', d, o)[0]
assert d[:4] == b'VXB9'
nP, nM, nPM, nPL, nH, nMask = [U16(4 + 2 * i) for i in range(6)]
nV = U32(16); nVar = U16(20); nDeck = U16(22); nCond = U16(24)
o = 28
pages = []; models = []; pms = []; pls = []
for i in range(nP):
    w, h, off = U16(o), U16(o + 2), U32(o + 4); o += 8
    pages.append((w, h & 0x7FFF, off))
for i in range(nM):
    mw, mh = d[o], d[o + 1]; fv, vc = U32(o + 4), U32(o + 8) & 0x7FFFFFFF; o += 16
    models.append((mw, mh, fv, vc))
for i in range(nPM):
    pms.append((U16(o), U16(o + 2), struct.unpack_from('<h', d, o + 4)[0], struct.unpack_from('<h', d, o + 6)[0])); o += 8
for i in range(nPL):
    pls.append(dict(layout=U16(o), pm=U16(o + 2), x=U16(o + 4), y=U16(o + 6), ground=U16(o + 8), ec=U16(o + 10), ef=U32(o + 12))); o += 16
o += nH; o += o & 1
o += 2 * nH; o += nMask * 32; o += nH; o += o & 1
o += nVar * 6; o += nDeck * 8
cond = {}
for i in range(nCond):
    pi = U16(o); cond[pi] = (U16(o + 2), d[o + 4], d[o + 5]); o += 6
o += (4 - (o & 3)) & 3
VO = o
def vert(i): return struct.unpack_from('<6f', d, VO + i * 24)
def page_img(pi):
    w, h, off = pages[pi]
    src = struct.unpack_from('<%dH' % (w * h), d, off)
    img = Image.new('RGBA', (w, h)); px = img.load()
    for y in range(h):
        my = ((y & 1) << 1) | ((y & 2) << 2) | ((y & 4) << 3); rb = (y >> 3) * (w >> 3) * 64
        for x in range(w):
            mo = (x & 1) | ((x & 2) << 1) | ((x & 4) << 2)
            c = src[rb + (x >> 3) * 64 + my + mo]
            px[x, y] = (((c >> 11) & 31) * 255 // 31, ((c >> 6) & 31) * 255 // 31, ((c >> 1) & 31) * 255 // 31, 255 if c & 1 else 0)
    return img
layouts = json.load(open(os.path.join(ws, 'tools', 'emerald_maps', 'stage_layouts.json')))
by_layout = {}
for i, p in enumerate(pls): by_layout.setdefault(p['layout'], []).append((i, p))
BAD_MI = {20, 45, 59, 60, 61, 121, 122, 126, 128, 455, 457, 462}
for si, rects in enumerate(layouts):
    used = {}; items = []; seen_pos = set()
    for r in rects:
        if 'pi' in r:
            p = pls[r['pi']]
            mi, page, ox, oy = pms[p['pm']]
            if mi in BAD_MI: continue
            used.setdefault(page, None)
            items.append((r, p, pms[p['pm']]))
            continue
        for pi, p in by_layout.get(r['lid'], []):
            mi, page, ox, oy = pms[p['pm']]
            if mi in BAD_MI: continue
            wx = r['gx'] + p['x'] - r.get('sx', 0)
            wz = r['gy'] + p['y'] - r.get('sy', 0)
            if (wx, wz) in seen_pos: continue
            seen_pos.add((wx, wz))
            used.setdefault(page, None)
            items.append((r, p, pms[p['pm']]))
    mj = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'hoenn_match.json')
    if si == 2 and not rects and os.path.exists(mj):          # HOENN TOWNS: find each real building of the 16 town maps inside the layout image
        import numpy as np
        M = json.load(open(mj)); I = np.array(M['img']); IH, IW = I.shape
        plain = set(int(v) for v in np.nonzero(np.bincount(I.ravel()) > 250)[0])
        found = []
        for pi, p in enumerate(pls):
            g = M['maps'].get(str(p['layout']))
            if not g: continue
            mi, page, ox, oy = pms[p['pm']]; mw, mh = models[mi][0], models[mi][1]
            sub = np.array(g['grid'])[p['y']:p['y'] + mh, p['x']:p['x'] + mw]
            if sub.shape != (mh, mw) or mw * mh == 0: continue
            if sum(1 for v in sub.ravel() if int(v) not in plain) < max(1, mw * mh // 3): continue
            cnt = np.zeros((IH - mh + 1, IW - mw + 1), np.int16)
            for dy in range(mh):
                for dx in range(mw): cnt += (I[dy:dy + IH - mh + 1, dx:dx + IW - mw + 1] == sub[dy, dx])
            ys, xs = np.nonzero(cnt >= int(0.9 * mw * mh + 0.999))
            if len(ys) > 24: continue
            for Y, X in zip(ys, xs): found.append((mw * mh, int(X), int(Y), p, mw, mh))
        found.sort(key=lambda t: -t[0]); taken = set(); items = []; used = {}
        for _, X, Y, p, mw, mh in found:
            cells = {(X + a, Y + b) for a in range(mw) for b in range(mh)}
            if cells & taken: continue
            taken |= cells; mi, page, ox, oy = pms[p['pm']]; used.setdefault(page, None)
            items.append((dict(lid=p['layout'], gx=X - p['x'], gy=Y - p['y'], flat=True), p, pms[p['pm']]))
        json.dump(sorted({(x, z) for r_, p_, pm_ in items for x in range(r_['gx'] + p_['x'], r_['gx'] + p_['x'] + models[pm_[0]][0])
                          for z in range(r_['gy'] + p_['y'], r_['gy'] + p_['y'] + models[pm_[0]][1])}), open(os.path.join(os.path.dirname(mj), 'hoenn_cover.json'), 'w'))
        print('  hoenn: matched', len(items), 'buildings from', len(found), 'candidate positions')
    order = sorted(used); y0 = 0; base = {}; W = max([pages[p][0] for p in order] or [8])
    for p in order: base[p] = y0; y0 += pages[p][1]
    H = 8
    while H < y0: H <<= 1
    atlas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    for p in order: atlas.paste(page_img(p), (0, base[p]))
    atlas.save(os.path.join(ws, 'assets', 'world', 'bld_%d.png' % si))
    verts = bytearray(); recs = []; nv = 0
    wld_p = os.path.join(ws, 'assets', 'world', 'wld_%d.bin' % si)
    wld_grid = None; wld_gw = 0; wld_gh = 0
    if os.path.exists(wld_p):
        wd_ = open(wld_p, 'rb').read()
        _, _, _, wld_gw, wld_gh = struct.unpack_from('<4s4I', wd_, 0)
        wld_grid = struct.unpack_from('<%dh' % (wld_gw * wld_gh), wd_, 20)
    def rect_base(si_, lid_, R_):
        if not R_: return 0
        if si_ == 2:
            return {5: -48, 13: -32}.get(lid_, 0)
        if lid_ == 12: return 0
        return R_['base']
    for r, p, (mi, page, ox, oy) in items:
        m = models[mi]; wx = r['gx'] + p['x'] - r.get('sx', 0); wz = r['gy'] + p['y'] - r.get('sy', 0); pw = pages[page][0]
        first = nv
        LF = 0.0
        for (f, c, step, uo, vo) in ((m[2], m[3], 3, ox, oy), (p['ef'], p['ec'], 6, 0, 0)):
            for k in range(0, c - step + 1, step):
                vs = [vert(f + k + j) for j in range(step)]
                tri = vs if step == 3 else [vs[0], vs[1], vs[2], vs[0], vs[2], vs[5]]
                for (x, y, z, u, v, sh) in tri:
                    hb = max(0, min(255, int(round(y * 32.0))))
                    verts += struct.pack('<5fBB2x', x + wx, y + LF, z + wz + LF, (uo + u) / W, (base[page] + vo + v) / H, max(0, min(255, int(sh * 255))), hb)
                    nv += 1
        recs.append((wx, wz, wx + m[0], wz + m[1], first, nv - first))
    with open(os.path.join(ws, 'assets', 'world', 'bld_%d.bin' % si), 'wb') as f:
        f.write(struct.pack('<4sII', b'BLD1', len(recs), nv))
        for rc in recs: f.write(struct.pack('<4f2I', *rc))
        f.write(bytes(verts))
    print('stage', si, 'placements', len(recs), 'verts', nv, 'atlas', W, 'x', H)
