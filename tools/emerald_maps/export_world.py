#!/usr/bin/env python3
"""Ports voxel_tree.c (trees, tall grass, flowers) and voxel_relief.c / EmitReliefUV (cliffs, ledges, plateaus) of
HctrRB/pokeemerald-vita-3ds onto the stitched town stages. Their code is CC BY-NC-SA 4.0 - keep the credit.
usage: export_world.py <pokeemerald-vita-3ds checkout> <workspace_dir>
needs tools/emerald_maps/stage_cells.json + stage_layouts.json (both written by build_maps.py).
writes assets/world/{trees.png, rel_<s>.bin, tre_<s>.bin, wld_<s>.bin}"""
import sys, os, json, struct
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import reliefdata

def tree_png(path, out):
    d = open(path, 'rb').read(); n = 128; src = struct.unpack('<%dH' % (n * n), d[:n * n * 2])
    img = Image.new('RGBA', (n, n)); px = img.load()
    for y in range(n):
        my = ((y & 1) << 1) | ((y & 2) << 2) | ((y & 4) << 3); rb = (y >> 3) * (n >> 3) * 64
        for x in range(n):
            c = src[rb + (x >> 3) * 64 + my + ((x & 1) | ((x & 2) << 1) | ((x & 4) << 2))]
            px[x, y] = (((c >> 11) & 31) * 255 // 31, ((c >> 6) & 31) * 255 // 31, ((c >> 1) & 31) * 255 // 31, 255 if c & 1 else 0)
    # Paint organic flared brown wooden trunk with curved splayed roots (y=28..35) below canopy of 32x36 large tree
    # (transparent sides, curved natural root toes - zero dark-green box and zero flat horizontal saw-cut)
    pal = {
        '.': (0, 0, 0, 0), 'O': (58, 42, 28, 255), 'D': (90, 65, 42, 255),
        'M': (126, 92, 58, 255), 'L': (156, 118, 76, 255), 'G': (44, 68, 12, 255),
    }
    trunk = [
        '........GGGGGGGGGGGGGGGG........',
        '.........OOODDMLLMDDDOO.........',
        '.........OODMMLLLLMMDOO.........',
        '........OODMMLMLLMMLMDOO........',
        '.......OODMMLLMMLLMMLMDOO.......',
        '......OODMMMLMDMMDMMLMMDOO......',
        '.....OODDMMODDMMMMDDOMMDDOO.....',
        '......OOOD..OODDDDOO..DOOO......',
    ]
    for i, row in enumerate(trunk):
        for x, ch in enumerate(row):
            px[x, 28 + i] = pal[ch]
    for y in range(32):
        for x in range(32, 64):
            if px[x, y] in {(90, 82, 65, 255), (148, 115, 74, 255), (106, 90, 65, 255), (82, 74, 65, 255), (123, 98, 74, 255), (57, 82, 0, 255)}:
                px[x, y] = (57, 139, 49, 255)
    img.save(out)

# ---------------- voxel_tree.c tables (verbatim) ----------------
S = 0x200
SMALL, GRASS = 4, 5
def fortree_wood(m):
    return (0x224 <= m <= 0x226 or 0x22B <= m <= 0x22F or 0x233 <= m <= 0x237 or 0x23B <= m <= 0x23F
            or m == 0x243 or 0x245 <= m <= 0x247 or m == 0x256)
def small_trunk_ground(sec, m):
    if sec == 'Fortree' and m == 0x253: return 0
    if sec == 'Fortree' and (m == 0x22A or fortree_wood(m)): return 3
    if sec == 'Mauville' and m == 0x226: return 1
    if (sec == 'Rustboro' and m == 0x307) or (sec == 'Lavaridge' and m == 0x201): return 4
    if sec == 'Lavaridge' and m in (0x218, 0x21A, 0x3B8, 0x203): return 2
    return -1
def own_large(sec, m):
    part = kind = -1
    if sec == 'Mossdeep':
        if m in (0x322, 0x326, 0x355): part, kind = 2, 0
        elif m in (0x323, 0x327, 0x356): part, kind = 3, 0
        elif m in (0x2F8, 0x324): part, kind = 2, 1
        elif m in (0x2F9, 0x325): part, kind = 3, 1
    elif sec == 'Fallarbor':
        if m in (0x20E, 0x26C, 0x273, 0x2CA, 0x2CF): part, kind = 2, 2
        elif m in (0x20F, 0x26B, 0x274, 0x2CB, 0x2CE): part, kind = 3, 2
        elif m in (0x20C, 0x26E, 0x275): part, kind = 2, 3
        elif m in (0x20D, 0x26D, 0x276): part, kind = 3, 3
    return part, kind
def own_large_ground(sec, m):
    if sec == 'Mossdeep':
        if m in (0x2F0, 0x2F1, 0x2F8, 0x2F9, 0x324, 0x325): return 0x19E
        if m in (0x31A, 0x31B): return 0x001
        if m in (0x31C, 0x31D): return 0x07C
        if m in (0x31E, 0x31F): return 0x002
    elif sec == 'Fallarbor':
        if m in (0x204, 0x205): return 0x218
        if m in (0x206, 0x207): return 0x279
        if m in (0x214, 0x215): return 0x271
        if m in (0x2C2, 0x2C3): return 0x269
        if m in (0x2CC, 0x2CD): return 0x170
    return -1
def tree_part(sec, m):
    if m >= S:
        p, _ = own_large(sec, m)
        if p >= 0: return p
        if sec == 'Dewford' and m in (0x23A, 0x243): return SMALL
        if small_trunk_ground(sec, m) >= 0: return SMALL
        if sec == 'Fortree' and (m == 0x208 or 0x281 <= m <= 0x283): return GRASS
        return -1
    if m == 0x1E: return 0
    if m == 0x1F: return 1
    if m in (0x26, 0x36): return 2
    if m in (0x27, 0x37): return 3
    if m in (0x1D4, 0x1D6): return 0
    if m in (0x1D5, 0x1D7): return 1
    if m in (0x1DC, 0x1DE, 0x1E4, 0x1E6): return 2
    if m in (0x1DD, 0x1DF, 0x1E5, 0x1E7): return 3
    if m == 0x1EC: return 2
    if m == 0x1ED: return 3
    if m == 0x1F3: return 2
    if m == 0x1F2: return 3
    if m == 0x1FC: return 0
    if m == 0x1FD: return 1
    if m in (0x16, 0x17, 0xC6, 0xC7, 0x1F4, 0x1F5): return SMALL
    if m == 0x15: return GRASS
    return -1
def ground_meta(sec, m):
    if m >= S:
        o = own_large_ground(sec, m)
        if o >= 0: return o
        if sec == 'Dewford' and m == 0x239: return 0x124
        if (sec == 'Rustboro' and m == 0x2C0) or (sec == 'Lilycove' and m == 0x277): return 0x079
        if sec == 'Lavaridge' and m in (0x211, 0x20B): return 0x079 if m == 0x211 else 0x07D
        if sec == 'BattleFrontierOutsideWest' and m == 0x3F8: return 0x204
        if sec == 'Mauville' and m == 0x2C1: return 0x205
        if sec == 'EverGrande' and m == 0x262: return 0x222
        if sec == 'Fortree' and m in (0x316, 0x317): return 0x001
        if sec == 'Fortree' and (m == 0x244 or 0x248 <= m <= 0x24A or 0x250 <= m <= 0x252 or m in (0x241, 0x25C, 0x273)): return 0x001
        if sec == 'Fortree' and m in (0x24B, 0x24C): return 0x261
        return m
    return {0x1C6: 0x00D, 0x1C7: 0x00D, 0x1CE: 0x001, 0x1CF: 0x001, 0x00E: 0x001, 0x00F: 0x001, 0x040: 0x001, 0x01D: 0x002,
            0x025: 0x00D, 0x02D: 0x0A1, 0x035: 0x170, 0x193: 0x170, 0x0CE: 0x091}.get(m, m)

TRUNKS = {0: (48, 48), 1: (80, 16), 2: (64, 32), 3: (48, 32), 4: (112, 32)}
LARGE = (0, 0, 18, 32, 0, 0.35, False); BASE_T = (0, 64, 16, 96, 0, 0.66, False); BASE_O = (32, 64, 16, 96, 0, 0.66, False)
OWN = {0: (64, 64, 16, 64, 80, 1.15, False), 1: (64, 64, 16, 96, 80, 0.95, True), 2: (96, 64, 16, 0, 80, 1.00, False), 3: (96, 64, 16, 32, 80, 1.00, False)}
def large_art(sec, m):
    if m >= S and own_large(sec, m)[0] >= 0: return OWN[own_large(sec, m)[1]]
    if m in (0x1E, 0x1F, 0x26, 0x27): return BASE_T
    if m in (0x36, 0x37): return BASE_O
    return LARGE
TU = lambda px: px / 128.0
TV = lambda py: py / 128.0          # top-down (the ref's 1 - py/128 in GL's bottom-up)

class Out:
    def __init__(s): s.v = []
    def quad(s, a, b, c, d, lift, shift):
        for t in (a, b, c, a, c, d):
            s.v.append((t[0], t[1] + lift, t[2] + shift, t[3], t[4], 255, 0))
    def tri(s, a, b, c, lift, shift):
        for t in (a, b, c): s.v.append((t[0], t[1] + lift, t[2] + shift, t[3], t[4], 255, 0))
    def bb(s, cx, cy, cz, hw, h, u0, v0, u1, v1, lift, shift, flag=1):
        by = cy + lift; bz = cz + shift
        a = (cx - hw, by + h, bz, u0, v0, 255, flag)
        b = (cx + hw, by + h, bz, u1, v0, 255, flag)
        c = (cx + hw, by,     bz, u1, v1, 255, flag)
        d = (cx - hw, by,     bz, u0, v1, 255, flag)
        for t in (a, b, c, a, c, d):
            s.v.append(t)

def relief_quads(out, x, y, g, uv, lift=0.0, shift=0.0, raw=False):
    """EmitReliefUV: lattice point (a,c) -> (x + a/4, g/16, y + c/4 + g/16); flat cells are one quad."""
    u0, v0, u1, v1 = uv
    if min(g) == max(g):
        h = g[0] / 16.0
        out.quad((x, h, y + h, u0, v0), (x + 1, h, y + h, u1, v0), (x + 1, h, y + 1 + h, u1, v1), (x, h, y + 1 + h, u0, v1), lift, shift)
        return
    def P(a, c):
        k = c * 5 + a; h = g[k] / 16.0
        return (x + a / 4.0, h, y + c / 4.0 + h, u0 + (u1 - u0) * a / 4, v0 + (v1 - v0) * c / 4)
    for c in range(4):
        for a in range(4):
            out.quad(P(a, c), P(a + 1, c), P(a + 1, c + 1), P(a, c + 1), lift, shift)

def one_level_slope(g): return g is not None and max(g) - min(g) == 16

def main():
    ref, ws = sys.argv[1], sys.argv[2]
    wd = os.path.join(ws, 'assets', 'world'); td = os.path.join(ws, 'tools', 'emerald_maps')
    tree_png(os.path.join(ref, 'assets', 'voxel', 'trees.rgba5551'), os.path.join(wd, 'trees.png'))
    relief = reliefdata.load(os.path.join(ref, 'assets', 'voxel', 'relief.bin'))
    layouts = json.load(open(os.path.join(ref, 'data', 'layouts', 'layouts.json')))['layouts'] if os.path.exists(os.path.join(ref, 'data', 'layouts', 'layouts.json')) else []
    sc = os.path.join(td, 'stage_cells.json')
    if os.path.exists(sc): stages = json.load(open(sc))
    else:       # no ROM export yet: relief + lifts only (tile ids from maps_data.c); trees need build_maps.py's stage_cells.json
        import re
        src = open(os.path.join(ws, 'src', 'maps_data.c')).read(); stages = []
        dims = re.findall(r'\{\s*(\d+),\s*(\d+),\s*\d+,\s*\d+,\s*s\d+_tiles', src)
        for si, (w, h) in enumerate(dims[:2]):
            arr = re.search(r's%d_tiles\[\d+\] = \{(.*?)\};' % si, src, re.S).group(1)
            tl = [int(v) for v in arr.replace('\n', '').split(',') if v.strip()]
            stages.append(dict(w=int(w), h=int(h), cells=[[0, 0, 0, t, -1, 0] for t in tl]))
        print('stage_cells.json missing: exporting relief only (run build_maps.py with the ROM for trees)')
    rect_sets = json.load(open(os.path.join(td, 'stage_layouts.json')))
    atlas_h = Image.open(os.path.join(wd, 'emerald_tiles.png')).height; PW, PH = 16 * 20, (atlas_h // 16) * 20
    SA_NAMES = {
        138278684: 'Petalburg',
        138278708: 'Rustboro',
        138278732: 'Dewford',
        138278756: 'Slateport',
        138278780: 'Mauville',
        138278804: 'Lavaridge',
        138278828: 'Fallarbor',
        138278852: 'Fortree',
        138278876: 'Lilycove',
        138278900: 'Mossdeep',
        138278924: 'EverGrande',
        138278948: 'Pacifidlog',
        138278972: 'Sootopolis',
    }
    def secname(lid, sa=0):
        if sa in SA_NAMES: return SA_NAMES[sa]
        if lid < 1 or lid > len(layouts): return None
        n = layouts[lid - 1].get('secondary_tileset', '')
        return n[len('gTileset_'):] if n.startswith('gTileset_') else None
    def tile_uv(tid):
        u = (tid & 15) * 20 + 2; v = (tid >> 4) * 20 + 2
        return (u / PW, v / PH, (u + 16) / PW, (v + 16) / PH)
    def rect_base(si_, lid_, R_):
        if not R_: return 0
        if si_ == 2:
            return {5: -48, 13: -32}.get(lid_, 0)
        if lid_ == 12: return 0
        return R_['base']
    for si, st in enumerate(stages):
        W, H, cells = st['w'], st['h'], st['cells']; rects = rect_sets[si]
        at = lambda x, y: cells[y * W + x] if 0 <= x < W and 0 <= y < H else None
        # per-cell relief, lift grid (4 points a cell + 1) and cover
        raw_cg = {}; base_at = {}; lids = {(x, y): cells[y * W + x][4] for y in range(H) for x in range(W)}
        in_rect = set()
        for r in rects:
            R = relief.get(r['lid']); b_ = rect_base(si, r['lid'], R)
            for y in range(r['h']):
                for x in range(r['w']):
                    X, Y = r['gx'] + x, r['gy'] + y
                    if not (0 <= X < W and 0 <= Y < H): continue
                    in_rect.add((X, Y))
                    if cells[Y * W + X][4] == -1:
                        base_at[(X, Y)] = 0; raw_cg.pop((X, Y), None); continue
                    base_at[(X, Y)] = b_; lids[(X, Y)] = r['lid']; raw_cg.pop((X, Y), None)
                    if R and (x, y) in R['cells']:
                        g_ = [v + b_ for v in R['cells'][(x, y)]]
                        if si == 2: g_ = [max(0, v) for v in g_]
                        raw_cg[(X, Y)] = g_
                    elif b_ != 0:
                        raw_cg[(X, Y)] = [max(0, b_) if si == 2 else b_] * 25
        GW, GH = W * 4 + 1, H * 4 + 1; grid = [0] * (GW * GH)
        cell_g = {}
        lift = lambda X, Y: 0.0
        base = lambda X, Y: 0.0
        # trees / grass / flowers
        tre = Out(); rel = Out(); override = [0xFFFF] * (W * H); cover = bytearray(W * H)
        tid_of = {}
        for y in range(H):
            for x in range(W):
                m_, beh, fl, tid, lid, sa = cells[y * W + x]; tid_of.setdefault((sa, m_), tid)
        def ground_tid(sa, mid): return tid_of.get((sa, mid))
        def ground(X, Y, uvr, L):
            g = cell_g.get((X, Y))
            if g is not None: relief_quads(tre, X, Y, g, uvr, 0.0, 0.0)
            else:
                u0, v0, u1, v1 = uvr
                tre.quad((X, 0, Y, u0, v0), (X + 1, 0, Y, u1, v0), (X + 1, 0, Y + 1, u1, v1), (X, 0, Y + 1, u0, v1), L, L)
        def raw_part(x, y):
            c = at(x, y)
            if c is None: return -1
            return tree_part(secname(c[4], c[5]), c[0])
        def is_h_paired(x, y):
            p = raw_part(x, y)
            if not (0 <= p <= 3): return False
            ox = x + 1 if (p & 1) == 0 else x - 1
            op = raw_part(ox, y)
            return 0 <= op <= 3 and (op & 1) == ((p & 1) ^ 1)
        def eff_part(x, y):
            p = raw_part(x, y)
            if 0 <= p <= 3:
                if not is_h_paired(x, y): return SMALL
                row = p >> 1
                oy = y + 1 if row == 0 else y - 1
                if not is_h_paired(x, oy): return SMALL
            return p
        opaque_tre_ground = set()
        for y in range(H):
            for x in range(W):
                m_, beh, fl, tid, lid, sa = cells[y * W + x]; sec = secname(lid, sa)
                part = eff_part(x, y); L = lift(x, y)
                if part == SMALL and 0 <= raw_part(x, y) <= 3:
                    m_ = 0x1F4
                if part >= 0:
                    if part in (SMALL, GRASS):
                        grass = part == GRASS
                        gd = -1 if grass else small_trunk_ground(sec, m_)
                        dew = (not grass) and m_ >= S and gd < 0
                        cx, cy = (80 if grass else 64 if dew else 32), (0 if dew else 32)
                        tx, ty = TRUNKS[gd] if gd >= 0 else (cx + 16, cy)
                        ground(x, y, (TU(tx), TV(ty), TU(tx + 16), TV(ty + 16)), L)
                        opaque_tre_ground.add((x, y))
                        tre.bb(x + 0.5, 0.0, y + 0.65, 0.55, 1.85, TU(cx), TV(cy), TU(cx + 16), TV(cy + 32), L, L, flag=2 if grass else 1)
                    else:
                        art = large_art(sec, m_); col, row = part & 1, part >> 1
                        uvr = ((art[3] + col * 16) / 128.0, (art[4] + row * 16) / 128.0, (art[3] + col * 16 + 16) / 128.0, (art[4] + row * 16 + 16) / 128.0)
                        if art[6]:
                            h_ = 0.02; u0, v0, u1, v1 = uvr
                            tre.quad((x, h_, y, u0, v0), (x + 1, h_, y, u1, v0), (x + 1, h_, y + 1, u1, v1), (x, h_, y + 1, u0, v1), L, L)
                        else:
                            ground(x, y, uvr, L)
                            opaque_tre_ground.add((x, y))
                        # Emit ONE single complete 2x2 camera-facing tree billboard at the bottom-left cell (part == 2)
                        if part == 2:
                            cx_px, cy_px, rowpx = art[0], art[1], art[2]
                            u0, v0 = cx_px / 128.0, cy_px / 128.0
                            u1, v1 = (cx_px + 32.0) / 128.0, (cy_px + 2.0 * rowpx) / 128.0
                            tre.bb(x + 1.0, 0.0, y + 0.20, 1.05, (2.0 * rowpx) / 14.5, u0, v0, u1, v1, L, L, flag=1)
                elif beh in (2, 0x23):
                    ash = beh == 0x23
                    u0, u1, v0, v1 = (TU(16), TU(32), TV(112), TV(128)) if ash else (TU(80), TU(96), TV(32), TV(48))
                    tre.bb(x + 0.5, 0.0, y + 0.65, 0.52, 0.85, u0, v0, u1, v1, L, L, flag=2)
                elif fl:
                    hh = (x * 73856093) ^ (y * 19349663); hh &= 0xFFFFFFFF
                    hw = 0.36
                    for i in range(2):
                        cx = x + 0.30 + 0.40 * ((hh >> (i * 8)) & 0xFF) / 255.0; bz = y + 0.40 + 0.45 * ((hh >> (7 + i * 9)) & 0xFF) / 255.0
                        tre.bb(cx, 0.0, bz, hw, 0.60, TU(0), TV(112), TU(16), TV(128), L, L, flag=2)
                gm = ground_meta(sec, m_)
                if part >= 0 or gm != m_: cover[y * W + x] |= 2        # the tree pass owns this cell: no box extrusion of its art
                if gm != m_:
                    t2 = ground_tid(sa, gm)
                    if t2 is not None: override[y * W + x] = t2
        # relief ground: every lifted cell drawn on its lattice from the cell's own tile (unless opaque tree ground already drew it)
        for (X, Y), g in cell_g.items():
            cover[Y * W + X] |= 1
            if (X, Y) in opaque_tre_ground: continue
            tid_r = override[Y * W + X] if override[Y * W + X] != 0xFFFF else cells[Y * W + X][3]
            relief_quads(rel, X, Y, g, tile_uv(tid_r), 0.0, 0.0)
        def write(path, out):
            groups = {}
            for i in range(0, len(out.v), 3):
                t = out.v[i:i + 3]; groups.setdefault((int(t[0][0]) // 8, int(t[0][2]) // 8), []).append(t)
            recs = []; vb = bytearray(); n = 0
            for k in sorted(groups):
                first = n; xs = []; zs = []
                for t in groups[k]:
                    for v in t: vb += struct.pack('<5fBB2x', *v); xs.append(v[0]); zs.append(v[2]); n += 1
                recs.append((min(xs), min(zs), max(xs), max(zs), first, n - first))
            with open(path, 'wb') as f:
                f.write(struct.pack('<4sII', b'BLD1', len(recs), n))
                for r in recs: f.write(struct.pack('<4f2I', *r))
                f.write(bytes(vb))
        write(os.path.join(wd, 'tre_%d.bin' % si), tre); write(os.path.join(wd, 'rel_%d.bin' % si), rel)
        with open(os.path.join(wd, 'wld_%d.bin' % si), 'wb') as f:
            f.write(struct.pack('<4s4I', b'WLD1', W, H, GW, GH)); f.write(struct.pack('<%dh' % len(grid), *grid))
            f.write(struct.pack('<%dH' % len(override), *override)); f.write(bytes(cover))
        print('stage', si, 'tree verts', len(tre.v), 'relief verts', len(rel.v), 'relief cells', len(cell_g))

if __name__ == '__main__': main()
