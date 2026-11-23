#!/usr/bin/env python3
"""
Builds the 4 big Emerald-based Pokemon Survivors stages from a Pokemon Emerald (USA) ROM.

  usage: python build_maps.py <emerald.gba> <workspace_dir>

Outputs (inside the workspace):
  src/maps_data.c                    baked tile ids + terrain kinds for each stage
  assets/world/emerald_tiles.png     original 128 tiles + every new metatile (16 columns)
  assets/world/stage_preview_3..6.png

Tile placement rules:
  TOWNS  - real Emerald map connections (offsets read from the ROM map headers) are stitched, so every
           road / ledge / coast / building edge is exactly the metatile Emerald placed there.
  CAVES  - real cave rooms are laid out in a grid; the rooms are joined by corridors that are solved
           with wave-function-collapse using ONLY neighbour pairs (left/right, up/down) that occur in the
           real Emerald cave maps, so wall faces and floor edges always follow Emerald's own rules.
"""
import os, sys, random, hashlib, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emr import *            # noqa  (ROM parsing, metatile rendering)
from PIL import Image

(TER_GRASS, TER_PATH, TER_SAND, TER_FOREST, TER_SHALLOW, TER_DEEP,
 TER_CLIFF, TER_LEDGE_S, TER_LEDGE_W, TER_LEDGE_E) = range(10)
BLOCKED = {TER_DEEP, TER_CLIFF, TER_LEDGE_S, TER_LEDGE_W, TER_LEDGE_E}
DEEP_B = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x19}
SHALLOW_B = {0x16, 0x17}

def kind_of(beh, col, cave):
    if beh in DEEP_B: return TER_DEEP
    if beh in SHALLOW_B: return TER_SHALLOW
    if beh == 0x38: return TER_LEDGE_E
    if beh == 0x39: return TER_LEDGE_W
    if beh == 0x3B: return TER_LEDGE_S
    if beh == 0x3A: return TER_CLIFF
    if col: return TER_CLIFF
    if beh in (0x02, 0x03): return TER_FOREST
    if beh in (0x06, 0x21): return TER_SAND
    return TER_PATH if cave else TER_GRASS

# ------------------------------------------------------------------ towns
def info(k):
    h = hdr[maps[k]]; l = lay[h[0]]
    return l[0], l[1], h[9]

def place(start, allowed):
    pos = {start: (0, 0)}; q = [start]
    while q:
        k = q.pop(0); w, h, _ = info(k); x, y = pos[k]
        for d, o, g, m in conns(maps[k]):
            n = (g, m)
            if n in pos or n not in maps or d > 4: continue
            nw, nh, nt = info(n)
            if nt not in allowed: continue
            if d == 4: p = (x + w, y + o)
            elif d == 3: p = (x - nw, y + o)
            elif d == 1: p = (x + o, y + h)
            else: p = (x + o, y - nh)
            pos[n] = p; q.append(n)
    return pos


def ter_h(x, y):
    h = (x * 73856093) ^ (y * 19349663); h ^= h >> 13; h = (h * 1274126177) & 0xFFFFFFFF
    return (h ^ (h >> 16)) & 0x7FFFFFFF

def value_noise(W, H, cell, seed):
    rng = random.Random(seed)
    gw, gh = W // cell + 2, H // cell + 2
    g = [[rng.random() for _ in range(gw)] for _ in range(gh)]
    out = [[0.0] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            fx, fy = x / cell, y / cell; ix, iy = int(fx), int(fy); tx, ty = fx - ix, fy - iy
            tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty)
            a = g[iy][ix] * (1 - tx) + g[iy][ix + 1] * tx
            b = g[iy + 1][ix] * (1 - tx) + g[iy + 1][ix + 1] * tx
            out[y][x] = a * (1 - ty) + b * ty
    return out

def field_ids():
    """Grass / tall grass / flower / tree metatiles taken from Route 102's real map data."""
    m = get_map(maps[(0, 17)]); mt = metatiles(m['pa'], m['sa'])
    c0 = collections.Counter(); c1 = collections.Counter(); tall = collections.Counter()
    for v in m['cells']:
        i = v & 0x3FF; col = (v >> 10) & 3; b = mt[i][1]
        if col == 0 and b == 0: c0[i] += 1
        elif col == 0 and b == 2: tall[i] += 1
        elif col: c1[i] += 1
    grass = c0.most_common(1)[0][0]
    words_list = load_ts(m['pa'])[3]
    flowers = [i for i, ws in enumerate(words_list) if any(508 <= (w & 0x3FF) <= 511 for w in ws) and mt[i][1] == 0]
    return dict(pair=(m['pa'], m['sa']), grass=grass, tree=c1.most_common(1)[0][0], tall=tall.most_common(1)[0][0],
                flowers=flowers or [grass])

def normalize_trees(cv):
    """Ensure every tree cell is a complete asset (full 2x2 large tree or full 1x1 small tree, never a 1/4th piece)."""
    H, W = len(cv), len(cv[0])
    P0 = {0x1E, 0x1D4, 0x1D6, 0x1FC}
    P1 = {0x1F, 0x1D5, 0x1D7, 0x1FD}
    P2 = {0x26, 0x36, 0x1DC, 0x1DE, 0x1E4, 0x1E6, 0x1EC, 0x1F3}
    P3 = {0x27, 0x37, 0x1DD, 0x1DF, 0x1E5, 0x1E7, 0x1ED, 0x1F2}
    PINE = {0x1E, 0x1F, 0x26, 0x27, 0x36, 0x37}
    def p_of(m):
        if m in P0: return 0
        if m in P1: return 1
        if m in P2: return 2
        if m in P3: return 3
        return -1
    all_t = {(x, y) for y in range(H) for x in range(W) if cv[y][x][3] != 0 and p_of(cv[y][x][2]) >= 0}
    locked = set()
    # 1. Preserve intact natural 2-wide x >=2-tall vertical tree columns from original maps
    for x in range(W - 1):
        y = 0
        while y < H:
            if (x, y) in all_t and (x + 1, y) in all_t and (x, y) not in locked and (x + 1, y) not in locked:
                pL, pR = p_of(cv[y][x][2]), p_of(cv[y][x + 1][2])
                if (pL & 1) == 0 and (pR & 1) == 1 and (pL >> 1) == (pR >> 1):
                    y0 = y; y1 = y
                    while y1 + 1 < H and (x, y1 + 1) in all_t and (x + 1, y1 + 1) in all_t and (x, y1 + 1) not in locked and (x + 1, y1 + 1) not in locked:
                        qL, qR = p_of(cv[y1 + 1][x][2]), p_of(cv[y1 + 1][x + 1][2])
                        if (qL & 1) == 0 and (qR & 1) == 1 and (qL >> 1) == (qR >> 1):
                            y1 += 1
                        else:
                            break
                    if y1 >= y0 + 1:
                        # Cap top row as row=0 and bottom row as row=1 so sliced ends become whole
                        pa0, sa0, m0, c0, f0 = cv[y0][x]; pine0 = m0 in PINE
                        if (p_of(m0) >> 1) != 0:
                            cv[y0][x] = (pa0, sa0, 0x1E if pine0 else 0x1D4, c0, f0)
                            cv[y0][x + 1] = (pa0, sa0, 0x1F if pine0 else 0x1D5, c0, f0)
                        pa1, sa1, m1, c1, f1 = cv[y1][x]; pine1 = m1 in PINE
                        if (p_of(m1) >> 1) != 1:
                            cv[y1][x] = (pa1, sa1, 0x26 if pine1 else 0x1DC, c1, f1)
                            cv[y1][x + 1] = (pa1, sa1, 0x27 if pine1 else 0x1DD, c1, f1)
                        for yy in range(y0, y1 + 1):
                            locked.add((x, yy)); locked.add((x + 1, yy))
                        y = y1 + 1
                        continue
            y += 1
    # 2. Pack remaining tree cells into complete 2x2 large trees
    rem = all_t - locked
    for even_only in (True, False):
        for y in range(0, H - 1, 2 if even_only else 1):
            for x in range(0, W - 1, 2 if even_only else 1):
                blk = ((x, y), (x + 1, y), (x, y + 1), (x + 1, y + 1))
                if all(c in rem for c in blk):
                    pa, sa, _, col, fl = cv[y][x]
                    cv[y][x]         = (pa, sa, 0x1D4, 1, 0)
                    cv[y][x + 1]     = (pa, sa, 0x1D5, 1, 0)
                    cv[y + 1][x]     = (pa, sa, 0x1DC, 1, 0)
                    cv[y + 1][x + 1] = (pa, sa, 0x1DD, 1, 0)
                    for c in blk: rem.remove(c); locked.add(c)
    # 3. Replace any leftover 1x1 cells that can't form a 2x2 block with a complete 1x1 small tree (0x1F4)
    for (x, y) in rem:
        pa, sa, _, _, _ = cv[y][x]
        cv[y][x] = (pa, sa, 0x1F4, 1, 0)
    # 4. Clean up orphaned canopy tip / shadow border metatiles whose adjacent 2x2 tree was removed
    for y in range(H):
        for x in range(W):
            pa, sa, m, col, fl = cv[y][x]
            if col == 0 and m in (0x1CE, 0x1CF):
                if y + 1 >= H or (x, y + 1) not in locked:
                    cv[y][x] = (pa, sa, 1, 0, 0)
            elif col == 0 and m in (2, 276):
                if y - 1 < 0 or cv[y - 1][x][3] == 0:
                    cv[y][x] = (pa, sa, 1, 0, 0)
    return cv

TOWN_RECTS = {}
TOWN_ORDER = []
STAGE_CELLS = []

def town_canvas(window):
    pos = place((0, 0), {1, 2, 3})
    x0 = min(p[0] for p in pos.values()); y0 = min(p[1] for p in pos.values())
    wx0, wy0, wx1, wy1 = window
    W, H = wx1 - wx0, wy1 - wy0
    cv = [[None] * W for _ in range(H)]
    rects = []
    for k, (px, py) in pos.items():
        m = get_map(maps[k]); gx = px - x0 - wx0; gy = py - y0 - wy0
        if m['lid'] == 12: continue
        mtl = metatiles(m['pa'], m['sa'])
        if sum(1 for v in m['cells'] if mtl[v & 0x3FF][1] in DEEP_B) > 0.6 * len(m['cells']): continue   # open-sea routes
        if gx >= W or gy >= H or gx + m['w'] <= 0 or gy + m['h'] <= 0: continue
        rects.append((gx, gy, m))
        TOWN_RECTS.setdefault(id(window), []).append({'lid': m['lid'], 'gx': gx, 'gy': gy, 'w': m['w'], 'h': m['h']})
        for y in range(m['h']):
            for x in range(m['w']):
                cx, cy = gx + x, gy + y
                if 0 <= cx < W and 0 <= cy < H:
                    if (m['lid'] == 2 and 8 <= x < 13 and 8 <= y < 13) or (m['lid'] == 15 and 1 <= x < 6 and 3 <= y < 8):
                        continue
                    v = m['cells'][y * m['w'] + x]
                    cv[cy][cx] = (m['pa'], m['sa'], v & 0x3FF, (v >> 10) & 3, 0)
    def border_cell(x, y):          # Emerald draws the nearest map's 2x2 border block outside map edges
        best = None
        for gx, gy, m in rects:
            dx = max(gx - x, 0, x - (gx + m['w'] - 1)); dy = max(gy - y, 0, y - (gy + m['h'] - 1))
            d = dx * dx + dy * dy
            if best is None or d < best[0]: best = (d, m)
        m = best[1]
        return (m['pa'], m['sa'], m['border'][(x & 1) + (y & 1) * 2] & 0x3FF, 1, 0)
    fid = field_ids()
    nz1 = value_noise(W, H, 7, 101); nz2 = value_noise(W, H, 5, 202)
    for y in range(H):
        for x in range(W):
            edge = x < 2 or y < 2 or x >= W - 2 or y >= H - 2
            if cv[y][x] is not None and not edge: continue
            b = border_cell(x, y)
            pa, sa = fid['pair']
            if edge or nz1[y][x] > 0.66:
                cv[y][x] = (pa, sa, fid['tree'], 1, 0)       # impassable tree border / tree clumps
            elif nz2[y][x] > 0.70:
                cv[y][x] = (pa, sa, fid['tall'], 0, 0)
            else:
                h = ter_h(x, y)
                cv[y][x] = (pa, sa, fid['flowers'][h % len(fid['flowers'])] if h % 100 < 5 else fid['grass'], 0, 0)
    return normalize_trees(cv)

# ------------------------------------------------------------------ caves
class Cave:
    def __init__(self, keys, rows, seed, gap=6):
        self.rng = random.Random(seed); self.gap = gap
        self.rooms = {}
        pairs = set()
        for k in keys:
            m = get_map(maps[k]); pairs.add((m['pa'], m['sa']))
        assert len(pairs) == 1, 'cave family must share one tileset pair'
        self.pa, self.sa = pairs.pop()
        self.mt = metatiles(self.pa, self.sa)
        m0 = get_map(maps[keys[0]])
        self.void = [(b & 0x3FF, 0, 0) for b in m0['border']]   # border block == plain floor art, so walkable
        self.void_corpus = [(b & 0x3FF, 1, 0) for b in m0['border']]
        self.rooms_src = {k: get_map(maps[k]) for k in keys}
        self.layout = rows
        self.build_corpus(keys)
        self.compose()

    def cells_of(self, k, flip):
        m = self.rooms_src[k]; w, h = m['w'], m['h']
        c = [[(m['cells'][y * w + x] & 0x3FF, (m['cells'][y * w + x] >> 10) & 3, 0) for x in range(w)] for y in range(h)]
        if flip: c = [[(i, cc, 1) for (i, cc, _) in row[::-1]] for row in c]
        return c

    def vcell(self, x, y):
        return self.void[(x & 1) + (y & 1) * 2]

    def build_corpus(self, keys):
        self.types = {}; self.tl = []; cnt = collections.Counter()
        def tid(c):
            if c not in self.types: self.types[c] = len(self.types); self.tl.append(c)
            return self.types[c]
        R = collections.defaultdict(int); D = collections.defaultdict(int)
        L = collections.defaultdict(int); U = collections.defaultdict(int)
        P = 3
        for k in keys:
            for flip in (0, 1):
                c = self.cells_of(k, flip); h = len(c); w = len(c[0])
                pad = [[self.void_corpus[(x & 1) + (y & 1) * 2] if (x < P or y < P or x >= w + P or y >= h + P) else c[y - P][x - P]
                        for x in range(w + 2 * P)] for y in range(h + 2 * P)]
                ids = [[tid(v) for v in row] for row in pad]
                for y in range(len(ids)):
                    for x in range(len(ids[0])):
                        a = ids[y][x]; cnt[a] += 1
                        if x + 1 < len(ids[0]):
                            b = ids[y][x + 1]; R[a] |= 1 << b; L[b] |= 1 << a
                        if y + 1 < len(ids):
                            b = ids[y + 1][x]; D[a] |= 1 << b; U[b] |= 1 << a
        self.R, self.L, self.D, self.U = R, L, D, U
        self.cnt = cnt; self.T = len(self.tl); self.ALL = (1 << self.T) - 1
        self.keep = 0
        for i in range(self.T):
            if cnt[i] >= 40: self.keep |= 1 << i
        self.memo = {}
        self.floor_mask = 0
        for i, (idv, col, fl) in enumerate(self.tl):
            if col == 0 and self.mt[idv][1] in (0x00, 0x08, 0x0B): self.floor_mask |= 1 << i
        self.floor_mask &= self.keep

    def compose(self):
        G = self.gap; rooms = []; y = G; W = 0
        for row in self.layout:
            x = G; rh = 0
            for (k, flip) in row:
                m = self.rooms_src[k]
                rooms.append((k, flip, x, y, m['w'], m['h'])); x += m['w'] + G; rh = max(rh, m['h'])
            W = max(W, x); y += rh + G
        H = y
        self.W, self.H = W, H
        self.cv = [[self.vcell(x, yy) for x in range(W)] for yy in range(H)]
        self.placed = rooms
        for (k, flip, rx, ry, w, h) in rooms:
            c = self.cells_of(k, flip)
            for yy in range(h):
                for xx in range(w): self.cv[ry + yy][rx + xx] = c[yy][xx]
        # neighbours: right and below inside each grid row / next row
        rows = []; i = 0
        for row in self.layout:
            rows.append(rooms[i:i + len(row)]); i += len(row)
        self.links = 0
        for ri, row in enumerate(rows):
            for ci, a in enumerate(row):
                if ci + 1 < len(row): self.try_link(a, row[ci + 1], horizontal=True)
                if ri + 1 < len(rows):
                    # connect to the room below with the largest horizontal overlap
                    below = max(rows[ri + 1], key=lambda b: max(0, min(a[2] + a[4], b[2] + b[4]) - max(a[2], b[2])))
                    if max(0, min(a[2] + a[4], below[2] + below[4]) - max(a[2], below[2])) > 12:
                        self.try_link(a, below, horizontal=False)

    def is_floor(self, x, y):
        if not (0 <= x < self.W and 0 <= y < self.H): return False
        t = self.types.get(self.cv[y][x])
        return t is not None and bool(self.floor_mask >> t & 1)

    def try_link(self, a, b, horizontal, D=5):
        cands = []
        if horizontal:
            for yy in range(max(a[3], b[3]) + 2, min(a[3] + a[5], b[3] + b[5]) - 3):
                xa, xb = a[2] + a[4] - 1 - D, b[2] + D
                if all(self.is_floor(xa, yy + d) and self.is_floor(xb, yy + d) for d in (0, 1)):
                    cands.append((yy, xa, xb))
        else:
            for xx in range(max(a[2], b[2]) + 2, min(a[2] + a[4], b[2] + b[4]) - 3):
                ya, yb = a[3] + a[5] - 1 - D, b[3] + D
                if all(self.is_floor(xx + d, ya) and self.is_floor(xx + d, yb) for d in (0, 1)):
                    cands.append((xx, ya, yb))
        self.rng.shuffle(cands)
        for c in cands[:6]:
            if horizontal:
                yy, xa, xb = c
                path = {(x, yy + d) for x in range(xa, xb + 1) for d in (0, 1)}
                region = {(x, y) for x in range(xa, xb + 1) for y in range(yy - 2, yy + 4)}
            else:
                xx, ya, yb = c
                path = {(xx + d, y) for y in range(ya, yb + 1) for d in (0, 1)}
                region = {(x, y) for y in range(ya, yb + 1) for x in range(xx - 2, xx + 4)}
            res = self.wfc(region, path)
            if res:
                for (x, y), t in res.items(): self.cv[y][x] = self.tl[t]
                self.links += 1
                return True
        print('  (no corridor solved between', a[0], 'and', b[0], ')')
        return False

    @staticmethod
    def bits(m):
        while m:
            low = m & -m; yield low.bit_length() - 1; m ^= low

    def wfc(self, region, path, tries=60):
        region = {c for c in region if 0 <= c[0] < self.W and 0 <= c[1] < self.H}
        init = {}
        for (x, y) in region:
            dom = self.keep
            if (x, y) in path: dom &= self.floor_mask
            for (dx, dy, tab) in ((-1, 0, self.R), (1, 0, self.L), (0, -1, self.D), (0, 1, self.U)):
                n = (x + dx, y + dy)
                if n in region: continue
                if 0 <= n[0] < self.W and 0 <= n[1] < self.H:
                    t = self.types.get(self.cv[n[1]][n[0]])
                    if t is not None: dom &= tab[t]
            if dom == 0: return None
            init[(x, y)] = dom
        tabs = ((-1, 0, self.L), (1, 0, self.R), (0, -1, self.U), (0, 1, self.D))
        for _ in range(tries):
            dom = dict(init); ok = True
            def propagate(start):
                q = [start]
                while q:
                    c = q.pop()
                    for dx, dy, tab in tabs:
                        n = (c[0] + dx, c[1] + dy)
                        if n not in dom: continue
                        mk = (id(tab), dom[c])
                        allow = self.memo.get(mk)
                        if allow is None:
                            allow = 0
                            for t in self.bits(dom[c]): allow |= tab[t]
                            self.memo[mk] = allow
                        new = dom[n] & allow
                        if new != dom[n]:
                            if new == 0: return False
                            dom[n] = new; q.append(n)
                return True
            for c in list(dom):
                if not propagate(c): ok = False; break
            while ok:
                open_ = [(bin(d).count('1'), c) for c, d in dom.items() if d & (d - 1)]
                if not open_: break
                m = min(n for n, _ in open_)
                c = self.rng.choice([c for n, c in open_ if n == m])
                opts = list(self.bits(dom[c]))
                t = self.rng.choices(opts, [self.cnt[o] for o in opts])[0]
                dom[c] = 1 << t
                if not propagate(c): ok = False
            if ok: return {c: d.bit_length() - 1 for c, d in dom.items()}
        return None

    def canvas(self):
        return [[(self.pa, self.sa, i, c, f) for (i, c, f) in row] for row in self.cv]

# ------------------------------------------------------------------ baking
def connected(kinds, W, H):
    walk = [[kinds[y][x] not in BLOCKED for x in range(W)] for y in range(H)]
    seen = [[-1] * W for _ in range(H)]; sizes = []
    for y in range(H):
        for x in range(W):
            if walk[y][x] and seen[y][x] < 0:
                cid = len(sizes); st = [(x, y)]; seen[y][x] = cid; n = 0
                while st:
                    cx, cy = st.pop(); n += 1
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        nx, ny = cx + dx, cy + dy
                        if 0 <= nx < W and 0 <= ny < H and walk[ny][nx] and seen[ny][nx] < 0:
                            seen[ny][nx] = cid; st.append((nx, ny))
                sizes.append(n)
    return walk, seen, sizes

def pick_start(kinds, W, H, prefer=None):
    walk, seen, sizes = connected(kinds, W, H)
    big = max(range(len(sizes)), key=lambda i: sizes[i])
    # distance to nearest blocked tile (multi-source BFS)
    INF = 10 ** 9; dist = [[INF] * W for _ in range(H)]; q = collections.deque()
    for y in range(H):
        for x in range(W):
            if not walk[y][x]: dist[y][x] = 0; q.append((x, y))
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < W and 0 <= ny < H and dist[ny][nx] == INF:
                dist[ny][nx] = dist[y][x] + 1; q.append((nx, ny))
    best = None
    for y in range(H):
        for x in range(W):
            if seen[y][x] != big: continue
            d = min(dist[y][x], 7)
            px_, py_ = prefer if prefer else (W / 2, H / 2)
            sc = (min(d, 3) if prefer else d, -((x - px_) ** 2 + (y - py_) ** 2))
            if best is None or sc > best[0]: best = (sc, x, y)
    return best[1], best[2], sizes[big], sum(sizes)

class Atlas:
    def __init__(self): self.index = {}; self.imgs = []
    def tile(self, pa, sa, idv, flip):
        mt = metatiles(pa, sa)[idv][0].convert('RGB')
        part = flip // 10; flip = flip % 10
        if part:
            # Seamless wide staircase pieces: 1 = left, 2 = middle, 3 = right (strip inner 2px divider borders)
            mt = mt.copy(); px = mt.load()
            for y in range(16):
                if part in (2, 3):
                    px[0, y] = px[2, y]; px[1, y] = px[3, y]
                if part in (1, 2):
                    px[14, y] = px[12, y]; px[15, y] = px[13, y]
        if flip == 1: mt = mt.transpose(Image.FLIP_LEFT_RIGHT)
        elif flip == 2: mt = mt.transpose(Image.ROTATE_90)
        elif flip == 3: mt = mt.transpose(Image.ROTATE_270)
        key = hashlib.md5(mt.tobytes()).hexdigest()
        if key not in self.index: self.index[key] = len(self.imgs); self.imgs.append(mt)
        return self.index[key]

def bake(canvas, atlas, cave):
    H = len(canvas); W = len(canvas[0])
    tiles = [[0] * W for _ in range(H)]; kinds = [[0] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            pa, sa, idv, col, flip = canvas[y][x]
            beh = metatiles(pa, sa)[idv][1]
            tiles[y][x] = atlas.tile(pa, sa, idv, flip)
            kinds[y][x] = kind_of(beh, col, cave)
    return tiles, kinds

def make_under(tiles, kinds, W, H):
    """Ground tile shown beneath upright structures (nearest walkable neighbour's tile)."""
    ok = (TER_GRASS, TER_PATH, TER_SAND, TER_FOREST)
    dflt = next(tiles[y][x] for y in range(H) for x in range(W) if kinds[y][x] in ok)
    under = [row[:] for row in tiles]
    for y in range(H):
        for x in range(W):
            if kinds[y][x] != TER_CLIFF: continue
            found = None
            for r in range(1, 6):
                for dy in range(-r, r + 1):
                    for dx in range(-r, r + 1):
                        if max(abs(dx), abs(dy)) != r: continue
                        nx, ny = x + dx, y + dy
                        if 0 <= nx < W and 0 <= ny < H and kinds[ny][nx] in ok: found = tiles[ny][nx]; break
                    if found is not None: break
                if found is not None: break
            under[y][x] = dflt if found is None else found
    return under

def write_preview(tiles, kinds, atlas, base, path, W, H, offset=128):
    big = Image.new('RGB', (W * 16, H * 16))
    for y in range(H):
        for x in range(W): big.paste(atlas.imgs[tiles[y][x]], (x * 16, y * 16))
    s = min(160 / big.width, 100 / big.height)
    tw, th = max(1, int(big.width * s)), max(1, int(big.height * s))
    pv = Image.new('RGBA', (160, 100), (12, 18, 34, 255))
    pv.paste(big.resize((tw, th), Image.LANCZOS).convert('RGBA'), ((160 - tw) // 2, (100 - th) // 2))
    pv.save(path)
    return big

# ------------------------------------------------------------------ HOENN TOWNS (all 16 towns/cities merged into one big open map)
HOENN_EDITED = set(); HOENN_LIDS = {}
HOENN_NAMES = 'Petalburg Slateport Mauville Rustboro Fortree Lilycove Mossdeep Sootopolis EverGrande Littleroot Oldale Dewford Lavaridge Fallarbor Verdanturf Pacifidlog'.split()
H_AVE, H_STREET, H_GAP, H_DW, H_PAD = 3, 3, 5, 84, 2     # 3-tile wide authentic Emerald streets, tight 5-tile block gap

def hoenn_pack(its):
    """Shelf-pack towns into one district; returns (rows, height). rows = [(row_h, [(idx, m)])]"""
    iw = H_DW - 2 * H_PAD; rows = []; cur = []; cw = 0
    for it in sorted(its, key=lambda t: (-t[1]['h'], -t[1]['w'])):
        w = it[1]['w']
        if cur and cw + H_GAP + w > iw: rows.append(cur); cur = []; cw = 0
        cur.append(it); cw += (H_GAP if cw else 0) + w
    if cur: rows.append(cur)
    rh = [(max(m['h'] for _, m in r), r) for r in rows]
    return rh, 2 * H_PAD + sum(h for h, _ in rh) + H_GAP * max(0, len(rh) - 1)

def hoenn_canvas():
    """Town-like neighborhood grid layout (similar to hoenn_layout.png):
    - 2-tile complete 2x2 tree perimeter around the entire stage
    - 3-to-4-tile wide Slateport cobblestone street grid with stepped block corners
    - Green grass neighborhood blocks with cobblestone courtyards, diverse clean 3D Hoenn buildings,
      tree hedgerows, tall grass patches, and flowers (zero cliffs, zero Sootopolis crater, zero Battle Tent)."""
    rng = random.Random(2026)
    fid = field_ids(); fpa, fsa = fid['pair']
    GRASS = (fpa, fsa, fid['grass'], 0, 0)
    TREE  = (fpa, fsa, fid['tree'], 1, 0)
    TALL  = (fpa, fsa, fid['tall'], 0, 0)
    FLWR  = (fpa, fsa, fid['flowers'][0], 0, 0)
    slp = get_map(maps[(0, 1)]); rpa, rsa = slp['pa'], slp['sa']
    ROAD = (rpa, rsa, 529, 0, 0)

    # Curated clean 3D buildings: (pi, lid, map_idx, bx, by, bw, bh)
    SMALL_BLDS = [
        (0,1,0,19,13,4,4), (1,1,0,24,9,4,4), (2,1,0,9,16,4,4), (4,1,0,5,2,5,4),
        (9,2,1,18,16,4,4), (10,2,1,12,23,4,4), (11,2,1,4,16,4,4), (13,2,1,24,41,6,4),
        (199,3,2,21,2,4,4), (200,3,2,22,11,4,4), (201,3,2,18,11,4,4), (202,3,2,34,2,5,4), (203,3,2,31,11,5,4), (204,3,2,36,11,4,5),
        (240,4,3,15,35,4,4), (241,4,3,15,42,4,4), (242,4,3,28,24,5,5), (248,4,3,7,34,5,5),
        (375,6,5,23,11,4,4), (376,6,5,10,19,4,4), (381,6,5,37,11,5,4),
        (429,7,6,27,13,4,4), (430,7,6,36,15,4,4), (433,7,6,17,13,4,4), (438,7,6,35,20,5,5),
        (573,10,9,2,4,5,5), (574,10,9,13,4,5,5),
        (576,11,10,5,13,4,4), (577,11,10,13,3,4,4), (578,11,10,4,4,4,4),
        (580,12,11,1,7,4,4), (581,12,11,7,5,4,4), (583,12,11,1,0,5,4),
        (588,13,12,8,3,4,4), (589,13,12,14,2,4,4), (590,13,12,11,12,4,4),
        (598,14,13,13,4,4,4), (599,14,13,14,12,4,4), (601,14,13,0,3,4,4), (602,14,13,5,13,4,5),
        (603,15,14,15,0,4,4), (604,15,14,11,0,4,4), (605,15,14,0,11,4,4), (607,15,14,8,11,5,4),
    ]
    MED_BLDS = [
        (5,1,0,12,4,6,5), (16,2,1,2,22,5,6), (17,2,1,25,7,7,6), (18,2,1,28,22,6,6),
        (205,3,2,5,10,7,5), (210,3,2,5,1,6,5), (247,4,3,24,30,7,5), (249,4,3,24,42,6,5), (250,4,3,24,15,6,5),
        (385,6,5,9,27,7,6), (386,6,5,36,20,6,5), (440,7,6,35,5,6,5), (575,10,9,3,12,7,5), (584,12,11,5,13,6,5), (594,13,12,2,11,6,5),
    ]
    BIG_BLDS = [
        (246,4,3,9,25,9,6), (245,4,3,3,43,7,9), (307,4,3,7,7,10,9),
    ]
    tmaps = {i: get_map(maps[(0, i)]) for i in range(16) if i not in (4, 7, 8, 15)}

    # 8 columns x 5 rows of neighborhood blocks
    COLS, ROWS = 8, 5
    BW, BH = 26, 26
    col_x = []; x_cur = 6
    for c in range(COLS):
        col_x.append((x_cur, x_cur + BW))
        x_cur += BW + (4 if c == 3 else 3)
    W = x_cur + 3
    if W & 1: W += 1

    row_y = []; y_cur = 6
    for r in range(ROWS):
        row_y.append((y_cur, y_cur + BH))
        y_cur += BH + (4 if r == 2 else 3)
    H = y_cur + 3
    if H & 1: H += 1

    # Start with cobblestone street everywhere inside the 2-tile tree perimeter
    cv = [[ROAD] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            if x < 2 or y < 2 or x >= W - 2 or y >= H - 2:
                cv[y][x] = TREE

    rl = []; bld_mask = set()
    def place_bld(b, gx, gy):
        pi, lid, midx, bx, by, bw, bh = b
        m = tmaps[midx]; mw = m['w']
        rl.append({'lid': lid, 'gx': gx, 'gy': gy, 'w': bw, 'h': bh, 'sx': bx, 'sy': by, 'pi': pi})
        for dy in range(bh):
            for dx in range(bw):
                v = m['cells'][(by + dy) * mw + (bx + dx)]
                cv[gy + dy][gx + dx] = (m['pa'], m['sa'], v & 0x3FF, 1, 0)
                bld_mask.add((gx + dx, gy + dy))

    def fill_rect(x0, y0, x1, y1, val):
        for yy in range(max(2, y0), min(H - 2, y1)):
            for xx in range(max(2, x0), min(W - 2, x1)):
                if (xx, yy) not in bld_mask:
                    cv[yy][xx] = val

    def tree_2x2(tx, ty):
        if 2 <= tx and tx + 2 <= W - 2 and 2 <= ty and ty + 2 <= H - 2:
            if any((tx + dx, ty + dy) in bld_mask for dy in range(2) for dx in range(2)): return
            for dy in range(2):
                for dx in range(2):
                    cv[ty + dy][tx + dx] = TREE

    def decorate_lawn(bx0, by0, bx1, by1, seed_val):
        lrng = random.Random(seed_val)
        # Add 2-3 organic tall grass patches (2x3 or 3x2) and flower clusters on plain grass cells
        for _ in range(4):
            px = lrng.randint(bx0 + 1, bx1 - 4)
            py = lrng.randint(by0 + 1, by1 - 3)
            pw, ph = lrng.choice([(3, 2), (2, 3), (3, 3)])
            for yy in range(py, min(by1 - 1, py + ph)):
                for xx in range(px, min(bx1 - 1, px + pw)):
                    if cv[yy][xx] == GRASS:
                        cv[yy][xx] = TALL
        for yy in range(by0 + 1, by1 - 1):
            for xx in range(bx0 + 1, bx1 - 1):
                if cv[yy][xx] == GRASS and lrng.random() < 0.08:
                    cv[yy][xx] = FLWR

    # Crossroads park blocks (+ shaped plazas, like in the 5th image)
    PARK_BLOCKS = {(2, 0), (5, 1), (0, 3), (4, 2), (6, 3), (2, 4), (5, 4)}
    LANDMARK_BLOCKS = {(1, 1): 0, (1, 2): 1, (6, 1): 2}
    DOUBLE_BLOCKS = {(4, 0), (7, 1), (2, 2), (5, 2), (7, 3), (1, 4)}

    si_small = 0; si_med = 0
    for r_idx, (by0, by1) in enumerate(row_y):
        for c_idx, (bx0, bx1) in enumerate(col_x):
            # 1. Fill block interior with green grass lawn, leaving 3x3 stepped cobblestone notches at the 4 corners
            for yy in range(by0, by1):
                for xx in range(bx0, bx1):
                    in_tl = (xx < bx0 + 3 and yy < by0 + 3) and ((xx - bx0) + (yy - by0) < 3)
                    in_tr = (xx >= bx1 - 3 and yy < by0 + 3) and ((bx1 - 1 - xx) + (yy - by0) < 3)
                    in_bl = (xx < bx0 + 3 and yy >= by1 - 3) and ((xx - bx0) + (by1 - 1 - yy) < 3)
                    in_br = (xx >= bx1 - 3 and yy >= by1 - 3) and ((bx1 - 1 - xx) + (by1 - 1 - yy) < 3)
                    if not (in_tl or in_tr or in_bl or in_br):
                        cv[yy][xx] = GRASS

            mx = (bx0 + bx1) // 2
            my = (by0 + by1) // 2

            if (c_idx, r_idx) in PARK_BLOCKS:
                # Crossroads Park Plaza (+ cobblestone walkway + central square + corner trees & tall grass)
                fill_rect(mx - 1, by0, mx + 2, by1, ROAD)
                fill_rect(bx0, my - 1, bx1, my + 2, ROAD)
                fill_rect(mx - 4, my - 4, mx + 4, my + 4, ROAD)
                tree_2x2(bx0 + 3, by0 + 3)
                tree_2x2(bx1 - 5, by0 + 3)
                tree_2x2(bx0 + 3, by1 - 5)
                tree_2x2(bx1 - 5, by1 - 5)
                decorate_lawn(bx0, by0, bx1, by1, r_idx * 100 + c_idx)
                continue

            if (c_idx, r_idx) in LANDMARK_BLOCKS:
                # Grand Landmark Plaza (Devon Corp / Trainer School / Apartments + side shop + trees)
                b_big = BIG_BLDS[LANDMARK_BLOCKS[(c_idx, r_idx)] % len(BIG_BLDS)]
                b_sm = SMALL_BLDS[si_small % len(SMALL_BLDS)]; si_small += 1
                fill_rect(bx0 + 4, by0 + 6, bx1 - 4, by1 - 4, ROAD)
                fill_rect(mx - 2, by1 - 4, mx + 2, by1, ROAD)
                if c_idx < 4: fill_rect(bx1 - 4, my - 1, bx1, my + 2, ROAD)
                else: fill_rect(bx0, my - 1, bx0 + 4, my + 2, ROAD)
                # Top tree hedgerow
                for tx in range(bx0 + 4, bx1 - 5, 2): tree_2x2(tx, by0 + 2)
                for ty in range(by0 + 4, by1 - 5, 2): tree_2x2(bx0 + 1, ty)
                gx_big = bx0 + 5
                gy_big = by0 + 7
                place_bld(b_big, gx_big, gy_big)
                gx_sm = gx_big + b_big[5] + 2
                if gx_sm + b_sm[5] <= bx1 - 4:
                    place_bld(b_sm, gx_sm, gy_big + 2)
                    tree_2x2(gx_sm + 1, gy_big + b_sm[6] + 3)
                decorate_lawn(bx0, by0, bx1, by1, r_idx * 100 + c_idx)
                continue

            if (c_idx, r_idx) in DOUBLE_BLOCKS:
                # Two-tier neighborhood block (upper courtyard + lower courtyard)
                fill_rect(bx0 + 4, by0 + 4, bx1 - 4, by0 + 11, ROAD)
                fill_rect(bx0 + 4, by0 + 15, bx1 - 4, by1 - 3, ROAD)
                fill_rect(mx - 1, by0 + 11, mx + 2, by1, ROAD)
                if (c_idx + r_idx) & 1: fill_rect(bx1 - 4, by0 + 8, bx1, by0 + 11, ROAD)
                else: fill_rect(bx0, by0 + 8, bx0 + 4, by0 + 11, ROAD)
                for tx in range(bx0 + 4, bx1 - 5, 2): tree_2x2(tx, by0 + 1)
                for ty in range(by0 + 3, by1 - 4, 2):
                    tree_2x2(bx0 + 1 if (c_idx & 1) == 0 else bx1 - 3, ty)
                # Upper 2 buildings
                b1 = SMALL_BLDS[si_small % len(SMALL_BLDS)]; si_small += 1
                b2 = SMALL_BLDS[si_small % len(SMALL_BLDS)]; si_small += 1
                place_bld(b1, bx0 + 6, by0 + 4)
                place_bld(b2, bx0 + 6 + b1[5] + 3, by0 + 4)
                tree_2x2(bx0 + 6 + b1[5], by0 + 5)
                # Lower 2 buildings
                b3 = SMALL_BLDS[si_small % len(SMALL_BLDS)]; si_small += 1
                b4 = SMALL_BLDS[si_small % len(SMALL_BLDS)]; si_small += 1
                place_bld(b3, bx0 + 5, by0 + 15)
                place_bld(b4, bx1 - 5 - b4[5], by0 + 15)
                decorate_lawn(bx0, by0, bx1, by1, r_idx * 100 + c_idx)
                continue

            # Standard neighborhood courtyard block (2 or 3 diverse buildings + tree hedgerows + lawn grass)
            fill_rect(bx0 + 4, by0 + 7, bx1 - 4, by0 + 18, ROAD)
            fill_rect(mx - 1, by0 + 18, mx + 2, by1, ROAD)
            if (c_idx + r_idx) % 3 == 0:
                fill_rect(bx1 - 4, by0 + 13, bx1, by0 + 16, ROAD)
            elif (c_idx + r_idx) % 3 == 1:
                fill_rect(bx0, by0 + 13, bx0 + 4, by0 + 16, ROAD)

            # Top tree hedgerow behind the buildings + side tree row (like in the 5th image)
            for tx in range(bx0 + 4, bx1 - 5, 2):
                tree_2x2(tx, by0 + 2)
            side_x = bx0 + 1 if (c_idx + r_idx) % 2 == 0 else bx1 - 3
            for ty in range(by0 + 4, by1 - 5, 2):
                tree_2x2(side_x, ty)

            if (c_idx + r_idx) % 2 == 0:
                # 3 buildings (or 2 small + 1 tree courtyard accent)
                b1 = SMALL_BLDS[si_small % len(SMALL_BLDS)]; si_small += 1
                b2 = SMALL_BLDS[si_small % len(SMALL_BLDS)]; si_small += 1
                b3 = SMALL_BLDS[si_small % len(SMALL_BLDS)]; si_small += 1
                tot_w = b1[5] + b2[5] + b3[5] + 2
                if tot_w <= (bx1 - bx0 - 8):
                    gx0 = bx0 + (BW - tot_w) // 2
                    place_bld(b1, gx0, by0 + 7)
                    place_bld(b2, gx0 + b1[5] + 1, by0 + 9)
                    place_bld(b3, gx0 + b1[5] + 1 + b2[5] + 1, by0 + 7)
                else:
                    place_bld(b1, bx0 + 5, by0 + 7)
                    place_bld(b2, bx1 - 5 - b2[5], by0 + 7)
                    tree_2x2(mx - 1, by0 + 8)
            else:
                # 1 Medium building (Gym / Hall / Museum / Lab) + 1 Small building (Mart / Center / House)
                bm_ = MED_BLDS[si_med % len(MED_BLDS)]; si_med += 1
                bs_ = SMALL_BLDS[si_small % len(SMALL_BLDS)]; si_small += 1
                tot_w = bm_[5] + bs_[5] + 3
                gx0 = bx0 + (BW - tot_w) // 2
                place_bld(bm_, gx0, by0 + 7)
                place_bld(bs_, gx0 + bm_[5] + 3, by0 + 8)
                tree_2x2(gx0 + bm_[5], by0 + 9)

            # Accent trees in the front lawn corners
            if cv[by1 - 5][bx0 + 5] == GRASS: tree_2x2(bx0 + 5, by1 - 6)
            if cv[by1 - 5][bx1 - 7] == GRASS: tree_2x2(bx1 - 7, by1 - 6)
            decorate_lawn(bx0, by0, bx1, by1, r_idx * 100 + c_idx)

    # Autotile all Slateport cobblestone road cells (9-slice outer edges + 4 inner corners)
    rd_cells = {(x, y) for y in range(H) for x in range(W) if cv[y][x][:3] == (rpa, rsa, 529)}
    def is_rd(nx, ny):
        if not (0 <= nx < W and 0 <= ny < H): return False
        return (nx, ny) in rd_cells or (nx, ny) in bld_mask
    for (x, y) in rd_cells:
        n, s, w, e = is_rd(x, y - 1), is_rd(x, y + 1), is_rd(x - 1, y), is_rd(x + 1, y)
        nw, ne, sw, se = is_rd(x - 1, y - 1), is_rd(x + 1, y - 1), is_rd(x - 1, y + 1), is_rd(x + 1, y + 1)
        if not n and not w: tid = 520
        elif not n and not e: tid = 522
        elif not s and not w: tid = 536
        elif not s and not e: tid = 538
        elif not n: tid = 521
        elif not s: tid = 537
        elif not w: tid = 528
        elif not e: tid = 530
        elif not nw: tid = 562
        elif not ne: tid = 563
        elif not sw: tid = 570
        elif not se: tid = 571
        else: tid = 529
        cv[y][x] = (rpa, rsa, tid, 0, 0)

    start = (W // 2, H // 2)
    return normalize_trees(cv), rl, start

def hoenn_fix(cv, W, H, inrect, TREE, ROAD):
    """Guarantee one fully connected playable space: connect big isolated areas with 3-wide roads, fill small pockets and thin dead-end spurs."""
    import heapq
    N8 = [(a, b) for a in (-1, 0, 1) for b in (-1, 0, 1) if a or b]; N4 = ((1, 0), (-1, 0), (0, 1), (0, -1))
    kk = lambda x, y: kind_of(metatiles(cv[y][x][0], cv[y][x][1])[cv[y][x][2]][1], cv[y][x][3], False)
    def ok(x, y): return 0 <= x < W and 0 <= y < H
    for it in range(60):
        kind = [[kk(x, y) for x in range(W)] for y in range(H)]
        walk = [[kind[y][x] not in BLOCKED for x in range(W)] for y in range(H)]
        core = [[walk[y][x] and all(ok(x + a, y + b) and walk[y + b][x + a] for a, b in N8) for x in range(W)] for y in range(H)]
        # biggest open component = the main play area
        seen = {}; comps = []
        for y in range(H):
            for x in range(W):
                if core[y][x] and (x, y) not in seen:
                    st = [(x, y)]; seen[(x, y)] = len(comps); cs = []
                    while st:
                        cx, cy = st.pop(); cs.append((cx, cy))
                        for a, b in N8:
                            n = (cx + a, cy + b)
                            if ok(*n) and core[n[1]][n[0]] and n not in seen: seen[n] = len(comps); st.append(n)
                    comps.append(cs)
        main = max(comps, key=len)
        mset = set(main); dil = set()
        for x, y in main:
            for a, b in N8 + [(0, 0)]: dil.add((x + a, y + b))
        left = {}; lcomps = []
        for y in range(H):
            for x in range(W):
                if walk[y][x] and (x, y) not in dil and (x, y) not in left:
                    st = [(x, y)]; left[(x, y)] = len(lcomps); cs = []
                    while st:
                        cx, cy = st.pop(); cs.append((cx, cy))
                        for a, b in N4:
                            n = (cx + a, cy + b)
                            if ok(*n) and walk[n[1]][n[0]] and n not in dil and n not in left: left[n] = len(lcomps); st.append(n)
                    lcomps.append(cs)
        changed = 0
        for cs in lcomps:
            cset = set(cs); contacts = [c for c in cs if any((c[0] + a, c[1] + b) in dil for a, b in N4)]
            cl = []; sc = set()
            for c in contacts:
                if c in sc: continue
                st = [c]; sc.add(c); g = [c]
                while st:
                    cx, cy = st.pop()
                    for a, b in N8:
                        n = (cx + a, cy + b)
                        if n in contacts and n not in sc and n in cset: sc.add(n); st.append(n); g.append(n)
                cl.append(g)
            if len(cl) >= 2: continue                      # a real passage between two open areas
            if len(cs) < 60:                               # dead-end pocket / spur -> becomes trees
                for x, y in cs: cv[y][x] = TREE; HOENN_EDITED.add((x, y))
                changed += 1; continue
            avoid = cl[0] if cl else []                    # big isolated area: carve a second connection
            dist = {}; prev = {}; pq = []
            for c in cs: dist[c] = 0; heapq.heappush(pq, (0, c))
            goal = None
            while pq:
                d, c = heapq.heappop(pq)
                if d > dist.get(c, 1e9): continue
                if c in dil and all(max(abs(c[0] - a[0]), abs(c[1] - a[1])) > 8 for a in avoid) and c not in cset: goal = c; break
                if c in dil and c not in cset: continue          # never route through the already-connected area
                for a, b in N4:
                    n = (c[0] + a, c[1] + b)
                    if not (4 <= n[0] < W - 4 and 4 <= n[1] < H - 4): continue
                    kd = kind[n[1]][n[0]]
                    cost = 0 if kd not in BLOCKED else (20 if kd == TER_DEEP else (5 if n in inrect else 1))
                    nd = d + 1 + cost
                    if nd < dist.get(n, 1e9): dist[n] = nd; prev[n] = c; heapq.heappush(pq, (nd, n))
            if goal is None: continue
            c = goal
            while c is not None and c not in cset:
                for a, b in N8 + [(0, 0)]:
                    n = (c[0] + a, c[1] + b)
                    if ok(*n) and 3 <= n[0] < W - 3 and 3 <= n[1] < H - 3 and (n == c or n not in inrect or kind[n[1]][n[0]] in BLOCKED and False):
                        cv[n[1]][n[0]] = ROAD; HOENN_EDITED.add(n)
                c = prev.get(c)
            changed += 1
        print(f'  hoenn fix pass {it}: {len(lcomps)} stray areas, {changed} handled')
        if not changed: break

def emit_cells(cv, rects_, t, W, H, edited, lids=None):
    cells_ = []
    for y in range(H):
        for x in range(W):
            pa, sa, idv, col, flip = cv[y][x]
            words = load_ts(sa if idv >= 512 else pa)[3][idv % 512]
            lid = lids.get((x, y), -1) if lids else -1
            if not lids and (x, y) not in edited:
                for r_ in rects_:
                    if r_['gx'] <= x < r_['gx'] + r_['w'] and r_['gy'] <= y < r_['gy'] + r_['h']: lid = r_['lid']
            cells_.append([idv, metatiles(pa, sa)[idv][1], int(any(508 <= (w_ & 0x3FF) <= 511 for w_ in words)),
                           128 + t[y][x], lid, sa])
    return cells_

def hoenn_from_image(path, S=6):
    """HOENN TOWNS = the supplied layout image (hoenn_layout.png, one tile per SxS px) read back tile-for-tile into Emerald metatiles."""
    import numpy as np
    im = Image.open(path).convert('RGB'); W, H = im.width // S, im.height // S
    cand = {}; src_lid = {}
    for key in [(0, i) for i in range(16)] + [(0, 17)]:
        m = get_map(maps[key]); mt = metatiles(m['pa'], m['sa'])
        for v in m['cells']:
            i = v & 0x3FF; col = (v >> 10) & 3
            h = hashlib.md5(mt[i][0].convert('RGB').tobytes()).hexdigest()
            if h not in cand: cand[h] = (mt[i][0].convert('RGB'), collections.Counter())
            cand[h][1][(m['pa'], m['sa'], i, col)] += 1
            src_lid.setdefault((h, m['pa'], m['sa'], i, col), m['lid'])
    keys = list(cand); kid = {k: n for n, k in enumerate(keys)}
    K = np.stack([np.asarray(cand[k][0].resize((S, S), Image.BOX), dtype=np.float32).ravel() for k in keys])
    a = np.asarray(im, dtype=np.float32)
    cells = a.reshape(H, S, W, S, 3).transpose(0, 2, 1, 3, 4).reshape(H * W, S * S * 3)
    idx = np.empty(len(cells), int); worst = 0.0
    for s0 in range(0, len(cells), 512):
        d = np.abs(cells[s0:s0 + 512, None, :] - K[None, :, :]).mean(2)
        idx[s0:s0 + 512] = d.argmin(1); worst = max(worst, float(d.min(1).max()))
    print(f'  layout image {W}x{H} tiles, worst match error {worst:.2f}')
    cv = [[None] * W for _ in range(H)]; HOENN_LIDS.clear()
    for n, ki in enumerate(idx):
        pa, sa, i, col = cand[keys[ki]][1].most_common(1)[0][0]
        cv[n // W][n % W] = (pa, sa, i, col, 0)
        HOENN_LIDS[(n % W, n // W)] = src_lid[(keys[ki], pa, sa, i, col)]       # which Emerald map this tile came from (tree / ground logic keys on it)
    # data for export_buildings.py: where each real town building sits in this layout
    maps_grid = {}
    for i in range(16):
        m = get_map(maps[(0, i)]); mt = metatiles(m['pa'], m['sa'])
        maps_grid[m['lid']] = dict(w=m['w'], h=m['h'], grid=[[kid[hashlib.md5(mt[v & 0x3FF][0].convert('RGB').tobytes()).hexdigest()]
                                   for v in m['cells'][y * m['w']:(y + 1) * m['w']]] for y in range(m['h'])])
    import json
    json.dump(dict(img=[[int(idx[y * W + x]) for x in range(W)] for y in range(H)], maps=maps_grid),
              open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'hoenn_match.json'), 'w'))
    # tree lines: the layout only holds one quarter of Emerald's 2x2 tree (0x1D5). Rebuild each run as TL/TR (+BL/BR) pairs so the reference's voxel trees draw whole.
    hs = {(x, y) for y in range(H) for x in range(W) if cv[y][x][2] == 0x1D5 and cv[y][x][3]}
    for (x, y) in hs:
        rx = 0
        while (x - rx - 1, y) in hs: rx += 1
        ry = 0
        while (x, y - ry - 1) in hs: ry += 1
        pa, sa = cv[y][x][0], cv[y][x][1]
        cv[y][x] = (pa, sa, ((0x1D4, 0x1D5), (0x1DC, 0x1DD))[ry & 1][rx & 1], 1, 0)
    # signposts / lamp posts / stalls have no 3D model in the reference -> flat walkable decoration instead of a box (cells under a matched building stay solid)
    cj = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'hoenn_cover.json')
    if os.path.exists(cj):
        import json
        covered = {tuple(c) for c in json.load(open(cj))}; flat_cells = set()
        FLAT = {(0x21a, 4), (0x288, 3), (0x256, 2), (0x257, 2), (0x25e, 2), (0x25f, 2), (0x25f, 8), (0x255, 8), (0x25c, 8)}
        for (x, y), lid in HOENN_LIDS.items():
            pa, sa, i, col, fl = cv[y][x]
            if col and (i, lid) in FLAT and (x, y) not in covered: cv[y][x] = (pa, sa, i, 0, fl); flat_cells.add((x, y))
        # no model for these in the reference and their flat art reads as junk on the road: paint plain ground over them instead
        for (x, y) in sorted(flat_cells):
            for r in range(1, 6):
                nb = [(x + a, y + b) for b in range(-r, r + 1) for a in range(-r, r + 1) if max(abs(a), abs(b)) == r
                      and 0 <= x + a < W and 0 <= y + b < H and (x + a, y + b) not in flat_cells and not cv[y + b][x + a][3]
                      and (x + a, y + b) not in hs]
                if nb:
                    cv[y][x] = cv[nb[0][1]][nb[0][0]]; HOENN_LIDS[(x, y)] = HOENN_LIDS[nb[0]]; break
    fid = field_ids(); fpa, fsa = fid['pair']; mv = get_map(maps[(0, 2)])
    hoenn_fix(cv, W, H, set(), (fpa, fsa, fid['tree'], 1, 0), (mv['pa'], mv['sa'], 273, 0, 0))
    r102 = get_map(maps[(0, 17)])['lid']
    for c in HOENN_EDITED: HOENN_LIDS[c] = r102
    return cv, [], (W // 2, H // 2)


PREFER = {0: (130, 36), 1: (220, 60)}     # start next to Oldale Town / Mauville City

def main():
    rom_arg, ws = sys.argv[1], sys.argv[2]
    atlas = Atlas(); stages = []

    # ---- 2 town stages: stitched real Emerald map connections, cropped to a wide window
    town_defs = [
        ('LITTLEROOT - SLATEPORT', (0, 216, 320, 370)),
        ('RUSTBORO - MAUVILLE',    (0, 90, 330, 226)),
    ]
    for name, win in town_defs:
        print('town', name)
        TOWN_ORDER.append(id(win))
        cv = town_canvas(win); W, H = len(cv[0]), len(cv)
        t, k = bake(cv, atlas, False)
        stages.append((name, W, H, t, k, make_under(t, k, W, H), None, None))
        cells_ = emit_cells(cv, TOWN_RECTS[id(win)], t, W, H, set())
        STAGE_CELLS.append(dict(w=W, h=H, cells=cells_))

    # ---- HOENN TOWNS: all 16 towns merged into one big map joined by wide avenues
    print('town HOENN TOWNS')
    hw = ('hoenn',); TOWN_ORDER.append(id(hw))
    cv, rl, hstart = hoenn_canvas(); W, H = len(cv[0]), len(cv)
    TOWN_RECTS[id(hw)] = rl; PREFER[2] = hstart
    t, k = bake(cv, atlas, False)
    stages.append(('HOENN TOWNS', W, H, t, k, make_under(t, k, W, H), None, None))
    STAGE_CELLS.append(dict(w=W, h=H, cells=emit_cells(cv, rl, t, W, H, HOENN_EDITED)))

    # ---- Unified Authentic Pokemon Emerald 8-Neighbor Cliff Autotiler ----
    def autotile_emerald_cliffs(hm, W, H, pa, sa, cfg, two_row_south=False):
        """
        Emits 100% authentic Pokemon Emerald cave cliff edges, outer corners, middle faces,
        and concave inner corners directly from a heightmap hm[y][x]:
          hm == -1 : water (handled by caller)
          hm ==  0 : lower cave floor (handled by caller)
          hm >=  1 : raised plateau (1, 2) or outer perimeter cave wall (9)
        cfg keys:
          FLOOR_PLAT, NW, N, NE, W, E, SW, S, SE, ISE, ISW, INW, INE (tuple (id, flip) or int id)
        """
        def cell(spec, col=1):
            if isinstance(spec, tuple): return (pa, sa, spec[0], col, spec[1])
            return (pa, sa, spec, col, 0)

        def h_at(x, y):
            if x < 0 or x >= W or y < 0 or y >= H: return 9
            return hm[y][x]

        # Pass 1: Exact 8-neighbor Emerald classification (edge, outer corner, middle, concave inner corner)
        # kind_tag tracks semantic role for Pass 2 (2-row south cliff extrusion)
        cv = [[None] * W for _ in range(H)]
        tag = [[None] * W for _ in range(H)]
        for y in range(H):
            for x in range(W):
                h = hm[y][x]
                if h <= 0:
                    continue
                n  = h_at(x, y - 1) < h
                s  = h_at(x, y + 1) < h
                w  = h_at(x - 1, y) < h
                e  = h_at(x + 1, y) < h
                nw = h_at(x - 1, y - 1) < h
                ne = h_at(x + 1, y - 1) < h
                sw = h_at(x - 1, y + 1) < h
                se = h_at(x + 1, y + 1) < h

                if s and w:     cv[y][x] = cell(cfg['SW'], 1);  tag[y][x] = 'SW'
                elif s and e:   cv[y][x] = cell(cfg['SE'], 1);  tag[y][x] = 'SE'
                elif n and w:   cv[y][x] = cell(cfg['NW'], 1);  tag[y][x] = 'NW'
                elif n and e:   cv[y][x] = cell(cfg['NE'], 1);  tag[y][x] = 'NE'
                elif s:         cv[y][x] = cell(cfg['S'],  1);  tag[y][x] = 'S'
                elif n:         cv[y][x] = cell(cfg['N'],  1);  tag[y][x] = 'N'
                elif w:         cv[y][x] = cell(cfg['W'],  1);  tag[y][x] = 'W'
                elif e:         cv[y][x] = cell(cfg['E'],  1);  tag[y][x] = 'E'
                elif se:        cv[y][x] = cell(cfg['ISE'], 1); tag[y][x] = 'ISE'
                elif sw:        cv[y][x] = cell(cfg['ISW'], 1); tag[y][x] = 'ISW'
                elif nw:        cv[y][x] = cell(cfg['INW'], 1); tag[y][x] = 'INW'
                elif ne:        cv[y][x] = cell(cfg['INE'], 1); tag[y][x] = 'INE'
                else:
                    # Solid border fill for outer wall (h==9), walkable smooth plateau top for h in (1,2)
                    if h >= 9:
                        cv[y][x] = cell(cfg['FILL'], 1); tag[y][x] = 'FILL'
                    else:
                        cv[y][x] = cell(cfg['PLAT'], 0); tag[y][x] = 'PLAT'

        # Pass 2: Authentic 2-row tall South cliff walls (outer north cave wall ALWAYS 2 rows;
        # interior plateau south walls 2 rows when two_row_south=True and room exists above)
        for y in range(1, H):
            for x in range(W):
                t = tag[y][x]
                if t not in ('SW', 'S', 'SE', 'ISE', 'ISW'):
                    continue
                h = hm[y][x]
                if h < 9 and not two_row_south:
                    continue
                # Only extrude upward if the cell above belongs to the same height level and has interior/side above it
                if hm[y - 1][x] != h:
                    continue
                if h < 9 and y >= 2 and hm[y - 2][x] < h:
                    continue  # never overwrite a north rim (keep plateau >= 1 walkable row)
                up_t = tag[y - 1][x]
                if t == 'S' and up_t in ('PLAT', 'FILL'):
                    cv[y - 1][x] = cell(cfg['S'], 1); tag[y - 1][x] = 'S2'
                elif t == 'SW' and up_t in ('W', 'PLAT', 'FILL'):
                    cv[y - 1][x] = cell(cfg['SW'], 1); tag[y - 1][x] = 'SW2'
                elif t == 'SE' and up_t in ('E', 'PLAT', 'FILL'):
                    cv[y - 1][x] = cell(cfg['SE'], 1); tag[y - 1][x] = 'SE2'
                elif t == 'ISE' and up_t in ('PLAT', 'FILL', 'E'):
                    cv[y - 1][x] = cell(cfg['ISE'], 1); tag[y - 1][x] = 'ISE2'
                elif t == 'ISW' and up_t in ('PLAT', 'FILL', 'W'):
                    cv[y - 1][x] = cell(cfg['ISW'], 1); tag[y - 1][x] = 'ISW2'
        return cv, tag

    def build_vertex_elev(hm, tag, W, H, extra_slope=None, stair_ramps=None, flat_override=None):
        """
        Compute watertight (W+1)*(H+1) 3D corner vertex elevations (in px) with bold 3D depth:
          - Flat lower floor / water sits at 0px
          - Flat Tier 1 plateaus sit at 44px (54 deg steep sculpted 1-tile cliffs!)
          - Flat Tier 2 plateaus sit at 88px (44px crisp rise above Tier 1!)
          - Outer cavern perimeter walls rise to 116px (towering 49-61 deg cavern walls!)
          - North-facing stairs span 3 tiles (24.6 deg < 28 deg min pitch) so ALWAYS visible at all camera angles!
        """
        extra_slope = extra_slope or set()
        stair_ramps = stair_ramps or []
        flat_override = flat_override or {}
        core = [[None] * W for _ in range(H)]
        for y in range(H):
            for x in range(W):
                if (x, y) in flat_override:
                    core[y][x] = flat_override[(x, y)]
                elif (x, y) in extra_slope:
                    core[y][x] = None
                elif hm[y][x] <= 0:
                    core[y][x] = 0
                elif tag[y][x] == 'PLAT':
                    core[y][x] = 44 if hm[y][x] == 1 else 88
                elif tag[y][x] == 'FILL':
                    core[y][x] = 116
                else:
                    core[y][x] = None  # sloped cliff wall or staircase!

        elev = [[0] * (W + 1) for _ in range(H + 1)]
        for gz in range(H + 1):
            for gx in range(W + 1):
                vals = []
                for dy in (-1, 0):
                    for dx in (-1, 0):
                        cx, cy = gx + dx, gz + dy
                        if 0 <= cx < W and 0 <= cy < H and core[cy][cx] is not None:
                            vals.append(core[cy][cx])
                if vals:
                    lo, hi = min(vals), max(vals)
                    elev[gz][gx] = hi if lo == hi else (lo + hi) // 2
                else:
                    nvals = []
                    for cy in range(max(0, gz - 2), min(H, gz + 2)):
                        for cx in range(max(0, gx - 2), min(W, gx + 2)):
                            if core[cy][cx] is not None:
                                nvals.append(core[cy][cx])
                    if nvals:
                        elev[gz][gx] = (min(nvals) + max(nvals)) // 2
                    else:
                        elev[gz][gx] = 44

        # Apply exact linear vertex ramp heights for all staircases (ensuring smooth 3-tile north ramps < 25 deg!)
        for (x0, y0, x1, y1, h_start, h_end, axis) in stair_ramps:
            if axis == 'NS':
                span = max(1, (y1 - y0 + 1))
                for gz in range(y0, y1 + 2):
                    t = (gz - y0) / float(span)
                    hv = int(round(h_start + (h_end - h_start) * t))
                    for gx in range(x0, x1 + 2):
                        if 0 <= gx <= W and 0 <= gz <= H:
                            elev[gz][gx] = hv
            elif axis == 'EW':
                span = max(1, (x1 - x0 + 1))
                for gx in range(x0, x1 + 2):
                    t = (gx - x0) / float(span)
                    hv = int(round(h_start + (h_end - h_start) * t))
                    for gz in range(y0, y1 + 2):
                        if 0 <= gx <= W and 0 <= gz <= H:
                            elev[gz][gx] = hv

        # Guarantee continuous towering 116px 3D cave perimeter walls on ALL 4 map edges (especially top North wall!)
        for gx in range(W + 1):
            elev[0][gx] = 116
            elev[H][gx] = 116
        for gz in range(H + 1):
            elev[gz][0] = 116
            elev[gz][W] = 116
        # Smooth sculpted 2-row slope along the entire North, West, and East perimeter walls down to floor/plateau
        for gx in range(1, W):
            base_n = elev[2][gx] if H >= 2 else 0
            elev[1][gx] = min(116, 62 + base_n // 2)
        for gz in range(2, H - 1):
            base_w = elev[gz][2] if W >= 2 else 0
            base_e = elev[gz][W - 2] if W >= 2 else 0
            elev[gz][1] = min(116, 62 + base_w // 2)
            elev[gz][W - 1] = min(116, 62 + base_e // 2)

        return [elev[gz][gx] for gz in range(H + 1) for gx in range(W + 1)]

    def _ensure_all_traversible(cv, pa, sa, bridge_tile, wall_tile, bridge_cells=None):
        """Verify 100% single-component connectivity and zero dead-end pockets."""
        H, W = len(cv), len(cv[0])
        mt = metatiles(pa, sa)
        bridge_cells = bridge_cells or set()
        def is_walk(x, y):
            if not (2 <= x < W - 2 and 2 <= y < H - 2): return False
            if (x, y) in bridge_cells: return True
            return cv[y][x][3] == 0 and mt[cv[y][x][2]][1] not in DEEP_B
        changed = True
        while changed:
            changed = False
            for y in range(2, H - 2):
                for x in range(2, W - 2):
                    if is_walk(x, y) and (x, y) not in bridge_cells:
                        nbs = sum(1 for dx, dy in ((1,0),(-1,0),(0,1),(0,-1)) if is_walk(x+dx, y+dy))
                        if nbs < 2:
                            cv[y][x] = (pa, sa, wall_tile, 1, 0)
                            changed = True
        while True:
            seen = [[-1] * W for _ in range(H)]
            comps = []
            for y in range(2, H - 2):
                for x in range(2, W - 2):
                    if is_walk(x, y) and seen[y][x] < 0:
                        cid = len(comps); q = collections.deque([(x, y)]); seen[y][x] = cid; pts = []
                        while q:
                            cx, cy = q.popleft(); pts.append((cx, cy))
                            for dx, dy in ((1,0),(-1,0),(0,1),(0,-1)):
                                nx, ny = cx + dx, cy + dy
                                if is_walk(nx, ny) and seen[ny][nx] < 0:
                                    seen[ny][nx] = cid; q.append((nx, ny))
                        comps.append(pts)
            if len(comps) <= 1:
                break
            comps.sort(key=len, reverse=True)
            best = None
            for (ax, ay) in comps[1][::max(1, len(comps[1]) // 40)]:
                for (bx, by) in comps[0][::max(1, len(comps[0]) // 200)]:
                    d = abs(ax - bx) + abs(ay - by)
                    if best is None or d < best[0]:
                        best = (d, ax, ay, bx, by)
            _, ax, ay, bx, by = best
            steps = max(abs(bx - ax), abs(by - ay), 1)
            for s in range(steps + 1):
                cx = int(round(ax + (bx - ax) * s / steps))
                cy = int(round(ay + (by - ay) * s / steps))
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        nx, ny = cx + dx, cy + dy
                        if 2 <= nx < W - 2 and 2 <= ny < H - 2:
                            cv[ny][nx] = (pa, sa, bridge_tile, 0, 0)
        return cv

    def build_ice_cave_exact():
        """Faithful 108x60 Shoal Ice Caverns: zero ladders, all stairs visible at every camera angle, bold 3D ice cliffs."""
        W, H = 108, 60
        pa, sa = 138278660, 138279116
        ICE_STRIPE = 909  # diagonal striped ice floor
        ICE_SNOW   = 903  # lower frosted snow-ice floor
        ICE_SNOW2  = 901  # lighter snow-ice floor
        ICE_PLAT   = 897  # authentic smooth frosty blue plateau interior
        ICE_GLOSS  = 655  # smooth glossy ice patch
        STAIR_NS   = 867  # N-S horizontal ice steps (flip=2/3 -> crisp vertical E-W ice steps, ZERO ladders!)

        ICE_CFG = dict(
            PLAT=ICE_PLAT, FILL=897,
            NW=888, N=889, NE=890,
            W=896,  E=898,
            SW=904, S=905, SE=906,
            ISE=891, ISW=892, INW=899, INE=(899, 1)
        )

        hm = [[0] * W for _ in range(H)]
        def fill_rect(x0, y0, x1, y1, val):
            for y in range(max(0, y0), min(H, y1 + 1)):
                for x in range(max(0, x0), min(W, x1 + 1)):
                    hm[y][x] = val

        for x in range(W):
            hm[0][x] = 9; hm[1][x] = 9; hm[H - 1][x] = 9
        for y in range(H):
            hm[y][0] = 9; hm[y][1] = 9; hm[y][W - 2] = 9; hm[y][W - 1] = 9

        fill_rect(31, 2, 44, 17, 1)
        fill_rect(10, 14, 21, 27, 1)
        fill_rect(6, 19, 21, 27, 1)
        fill_rect(13, 27, 23, 39, 1)
        fill_rect(18, 32, 29, 47, 1)
        fill_rect(2, 36, 8, 51, 1)
        fill_rect(47, 19, 65, 28, 1)
        fill_rect(63, 13, 97, 28, 2)
        fill_rect(70, 9, 86, 15, 2)
        # (Removed the narrow 4-tile ice spines x=38..41 / x=69..72 with stairs on both sides)
        fill_rect(38, 38, 51, 46, 1)
        fill_rect(56, 39, 98, 46, 1)
        fill_rect(71, 32, 98, 47, 1)
        fill_rect(79, 29, 92, 34, 1)
        fill_rect(88, 36, 98, 47, 1)
        fill_rect(2, 54, 23, 58, 1)
        fill_rect(48, 54, 89, 58, 1)

        cv_cliffs, tag = autotile_emerald_cliffs(hm, W, H, pa, sa, ICE_CFG, two_row_south=False)

        snow_mask = [[False] * W for _ in range(H)]
        for (x0, y0, x1, y1) in (
            (2, 13, 29, 32), (24, 30, 46, 44), (40, 34, 68, 44),
            (62, 8, 99, 16), (72, 28, 105, 39), (94, 48, 105, 58),
            (58, 47, 74, 54), (2, 27, 12, 37)
        ):
            for y in range(max(2, y0), min(H - 1, y1 + 1)):
                for x in range(max(2, x0), min(W - 2, x1 + 1)):
                    snow_mask[y][x] = True

        cv = [[(pa, sa, ICE_STRIPE, 0, 0) for _ in range(W)] for _ in range(H)]
        for y in range(H):
            for x in range(W):
                if cv_cliffs[y][x] is not None:
                    cv[y][x] = cv_cliffs[y][x]
                elif snow_mask[y][x]:
                    if y > 2 and hm[y - 1][x] == 0 and not snow_mask[y - 1][x]:
                        cv[y][x] = (pa, sa, 911, 0, 0)
                    elif x > 2 and hm[y][x - 1] == 0 and not snow_mask[y][x - 1]:
                        cv[y][x] = (pa, sa, 910, 0, 0)
                    else:
                        cv[y][x] = (pa, sa, ICE_SNOW if ((x + y) & 1) == 0 else ICE_SNOW2, 0, 0)
                else:
                    if x > 1 and tag[y][x - 1] in ('E', 'ISE', 'SE'):
                        cv[y][x] = (pa, sa, 908, 0, 0)
                    else:
                        cv[y][x] = (pa, sa, ICE_STRIPE, 0, 0)

        for (gx, gy) in ((12, 8), (48, 12), (22, 51), (82, 52), (55, 31)):
            for dy in range(3):
                for dx in range(3):
                    if hm[gy + dy][gx + dx] == 0:
                        cv[gy + dy][gx + dx] = (pa, sa, ICE_GLOSS, 0, 0)

        stair_ramps = []
        extra_slope = set()
        def h_px(val): return 0 if val <= 0 else (44 if val == 1 else 88)

        def carve_stair_ns(x0, x1, y_edge):
            """Carve long gentle 3-tile N-S ice staircase (24.6 deg slope, visible at ALL camera angles)."""
            xm = (x0 + x1) // 2
            hn_v = hm[max(0, y_edge - 1)][xm]
            hs_v = hm[min(H - 1, y_edge + 1)][xm]
            h_n, h_s = h_px(hn_v), h_px(hs_v)
            if h_n == h_s:
                return
            if h_s > h_n:
                # North-facing: extends inward South onto higher plateau (up to 3 tiles)
                y_start = y_edge
                y_end = y_edge
                for step in (1, 2):
                    ny = y_edge + step
                    if ny < H - 2 and hm[ny][xm] == hs_v and hm[min(H - 1, ny + 1)][xm] == hs_v:
                        y_end = ny
                    else:
                        break
            else:
                # South-facing: extends inward North onto higher plateau (up to 3 tiles)
                y_end = y_edge
                y_start = y_edge
                for step in (1, 2):
                    ny = y_edge - step
                    if ny >= 2 and hm[ny][xm] == hn_v and hm[max(0, ny - 1)][xm] == hn_v:
                        y_start = ny
                    else:
                        break
            xa, xb = max(2, x0), min(W - 3, x1)
            for y in range(y_start, y_end + 1):
                for x in range(xa, xb + 1):
                    cv[y][x] = (pa, sa, STAIR_NS, 0, 0)
                    extra_slope.add((x, y))
            stair_ramps.append((xa, y_start, xb, y_end, h_n, h_s, 'NS'))

        def carve_stair_ew(cx, y0, y1, climbing_east=True):
            """Carve long gentle 3-tile E-W vertical ice staircase (rotated 867, ZERO ladders, 24.6 deg slope)."""
            fl = 3 if climbing_east else 2
            ym = (y0 + y1) // 2
            hw_v = hm[ym][max(0, cx - 1)]
            he_v = hm[ym][min(W - 1, cx + 1)]
            h_w, h_e = h_px(hw_v), h_px(he_v)
            if h_w == h_e:
                return
            if h_e > h_w:
                # West-facing (climbs East): extends inward East onto higher plateau (up to 3 tiles; 2 on 4-tile spines)
                x_lo = cx
                x_hi = cx
                for step in (1, 2):
                    nx = cx + step
                    if nx < W - 2 and hm[ym][nx] == he_v and hm[ym][min(W - 1, nx + 1)] == he_v:
                        x_hi = nx
                    else:
                        break
            else:
                # East-facing (climbs West): extends inward West onto higher plateau (up to 3 tiles)
                x_hi = cx
                x_lo = cx
                for step in (1, 2):
                    nx = cx - step
                    if nx >= 2 and hm[ym][nx] == hw_v and hm[ym][max(0, nx - 1)] == hw_v:
                        x_lo = nx
                    else:
                        break
            ya, yb = max(2, y0), min(H - 2, y1)
            for y in range(ya, yb + 1):
                for x in range(x_lo, x_hi + 1):
                    cv[y][x] = (pa, sa, STAIR_NS, 0, fl)
                    extra_slope.add((x, y))
            stair_ramps.append((x_lo, ya, x_hi, yb, h_w, h_e, 'EW'))

        # Narrow, clean 3-tile-wide ice staircases centered away from plateau corners
        carve_stair_ns(53, 55, 19)
        carve_stair_ns(53, 55, 28)
        carve_stair_ew(63, 22, 24, True)
        carve_stair_ns(80, 82, 28)
        carve_stair_ns(12, 14, 27)
        for y in range(44, 47):
            for x in range(32, 35):
                cv[y][x] = (pa, sa, STAIR_NS, 0, 0)
        for y in range(43, 47):
            for x in range(53, 55):
                cv[y][x] = (pa, sa, STAIR_NS, 0, 0)
        carve_stair_ns(43, 45, 38)            # North stairs onto the (former spine) west plateau
        carve_stair_ns(43, 45, 46)            # South stairs off the west plateau
        carve_stair_ew(71, 35, 37, True)
        carve_stair_ns(36, 38, 17)
        carve_stair_ew(44, 8, 10, False)
        carve_stair_ew(31, 8, 10, True)
        carve_stair_ns(15, 17, 14)
        carve_stair_ew(29, 37, 39, False)
        carve_stair_ns(90, 92, 13)
        carve_stair_ew(97, 19, 21, False)
        carve_stair_ew(98, 39, 41, False)
        carve_stair_ns(82, 84, 47)
        carve_stair_ns(4, 6, 36)
        carve_stair_ew(8, 43, 45, False)
        carve_stair_ns(11, 13, 54)
        carve_stair_ns(59, 61, 54)
        carve_stair_ns(79, 81, 54)

        cv = _ensure_all_traversible(cv, pa, sa, STAIR_NS, 905)
        elev = build_vertex_elev(hm, tag, W, H, extra_slope=extra_slope, stair_ramps=stair_ramps)
        return cv, elev, []

    def build_granite_cave_exact():
        """120x66 Granite Waterfall Caverns: bold 3D walls, ZERO ladders, ZERO stairs into water, 6 real 3D bridges with clear golden paths."""
        W, H = 120, 66
        pa, sa = 138278660, 138279116
        LOW_FLOOR  = 803   # 100% seamless borderless cool gray-stone lower cavern floor
        LOW_SPECK  = 811   # pebbled cool stone floor variation
        PLAT_FLOOR = 529   # 100% seamless borderless warm tan-ochre upper plateau floor
        STAIR_NS   = 918   # crisp horizontal stone staircase (flip=2/3 -> crisp vertical E-W stone stairs, ZERO ladders!)
        WATER      = 368   # deep blue underground river/lake
        WF_TOP     = 179   # waterfall crest
        WF_BODY    = 185   # waterfall cascade body
        WF_FOAM    = 187   # waterfall splash foam

        GRAN_CFG = dict(
            PLAT=PLAT_FLOOR, FILL=537,
            NW=520, N=521, NE=522,
            W=528,  E=530,
            SW=536, S=537, SE=538,
            ISE=539, ISW=540, INW=547, INE=(547, 1)
        )

        # Heightmap: 9 = towering outer cave wall, -1 = water/waterfall, 0 = lower stone basin, 1 = Tier 1 (44px), 2 = Tier 2 (88px)
        hm = [[0] * W for _ in range(H)]
        def fill_rect(x0, y0, x1, y1, val):
            for y in range(max(0, y0), min(H, y1 + 1)):
                for x in range(max(0, x0), min(W, x1 + 1)):
                    hm[y][x] = val

        # Outer 2-tile towering cave perimeter wall (116px high!)
        for x in range(W):
            hm[0][x] = 9; hm[1][x] = 9; hm[H - 1][x] = 9
        for y in range(H):
            hm[y][0] = 9; hm[y][1] = 9; hm[y][W - 2] = 9; hm[y][W - 1] = 9

        # 1. Plateaus (cleanly leaving a wide 5-tile open scenic top promenade y=2..6 above the reservoir!)
        fill_rect(2, 2, 29, 25, 1)       # Upper-Left Plateau
        fill_rect(12, 10, 21, 20, 2)     # Raised central-left boulder mound
        fill_rect(22, 14, 34, 21, 1)     # Bridge shelf to upper-center
        fill_rect(2, 36, 20, 46, 1)      # Lower-Left Cliff Spur
        fill_rect(17, 36, 29, 44, 1)     # West-Mid ridge
        fill_rect(34, 8, 79, 23, 1)      # Upper-Center Mesa + West Waterfall Terrace
        fill_rect(42, 10, 57, 18, 2)     # Upper-Center Tier 2 Mesa
        fill_rect(87, 10, 117, 24, 1)    # East Waterfall Ridge (flush y=10..24 for top & mid waterfall bridges!)
        fill_rect(95, 2, 117, 11, 1)     # Far North-East Plateau (cleanly connects to East Canal Bridge & top loop!)
        fill_rect(31, 29, 55, 48, 1)     # Central-Left Square Golden Plateau
        fill_rect(54, 31, 66, 39, 1)     # Central Overlook Terrace above lake (cleanly launches Central Lake Bridge!)
        fill_rect(86, 24, 117, 48, 1)    # Mid-East Plateau
        fill_rect(103, 24, 117, 48, 2)   # Raised Far-East Highway Plateau
        fill_rect(76, 38, 102, 52, 1)    # South-East golden plateau overlooking lake
        fill_rect(62, 53, 94, 63, 1)     # South-Center plateau floor
        fill_rect(28, 59, 37, 64, 1)     # Southern outcrop

        # 2. Carve Iconic Waterfall, Reservoir Pool (y=7..10 leaving wide y=2..6 top loop!), East Canal, & Central Lake
        fill_rect(76, 7, 91, 10, -1)     # Top Reservoir Pool (below wide accessible y=2..6 promenade!)
        fill_rect(80, 10, 86, 31, -1)    # Vertical Cascading Waterfall channel
        fill_rect(87, 12, 115, 16, -1)   # East upper canal branch (spanned by East Canal 3D Bridge!)
        fill_rect(74, 30, 89, 37, -1)    # Waterfall splash basin
        fill_rect(66, 36, 82, 42, -1)    # Diagonal river bend
        fill_rect(50, 39, 72, 51, -1)    # Main Central Underground Lake
        fill_rect(58, 43, 76, 50, 1)     # South-Central Island Peninsula (44px, joined by 3 real 3D bridges!)
        fill_rect(48, 51, 74, 53, -1)    # Southern water channel hugging South-Central plateau

        # Ensure clean flat 44px bridge landing banks on BOTH sides of all 6 real 3D bridges!
        fill_rect(73, 11, 79, 15, 1)     # West landing of Top Waterfall Bridge
        fill_rect(87, 17, 94, 24, 1)     # East landing of Mid Waterfall Bridge
        fill_rect(87, 9, 94, 11, 1)      # East landing of Top Waterfall Bridge
        fill_rect(105, 8, 112, 11, 1)    # North landing of East Canal Bridge
        fill_rect(105, 17, 112, 23, 1)   # South landing of East Canal Bridge
        fill_rect(50, 44, 55, 48, 1)     # West landing of West Lake Bridge
        fill_rect(58, 43, 72, 50, 1)     # Peninsula landing (West, North, South 3D bridges!)
        fill_rect(64, 54, 72, 58, 1)     # South landing of South Channel Bridge

        # Re-carve exact clean water channels beneath each 3D bridge so water flows visibly underneath!
        fill_rect(80, 10, 86, 31, -1)    # Waterfall channel under Top & Mid Waterfall Bridges
        fill_rect(87, 12, 115, 16, -1)   # East canal under East Canal Bridge
        fill_rect(56, 39, 57, 51, -1)    # West lake channel under West Lake Bridge
        fill_rect(58, 39, 72, 42, -1)    # North lake channel under Central Overlook Bridge
        fill_rect(58, 51, 74, 53, -1)    # South channel under South Channel Bridge

        # 3. Authentic 8-neighbor Emerald cliff autotiler (1-row crisp 54 deg interior cliffs; 2-row 116px towering outer walls!)
        cv_cliffs, tag = autotile_emerald_cliffs(hm, W, H, pa, sa, GRAN_CFG, two_row_south=False)

        # 4. Fill lower stone basin + water with authentic Emerald water-bank edges
        cv = [[(pa, sa, LOW_FLOOR, 0, 0) for _ in range(W)] for _ in range(H)]
        for y in range(H):
            for x in range(W):
                if cv_cliffs[y][x] is not None:
                    cv[y][x] = cv_cliffs[y][x]
                elif hm[y][x] == -1:
                    wn = hm[y - 1][x] != -1; ws = hm[y + 1][x] != -1
                    ww = hm[y][x - 1] != -1; we = hm[y][x + 1] != -1
                    if wn and ww:   wt = 384
                    elif wn and we: wt = 385
                    elif ws and ww: wt = 408
                    elif ws and we: wt = 410
                    elif wn:        wt = 393
                    elif ww:        wt = 400
                    elif we:        wt = 402
                    else:           wt = WATER
                    cv[y][x] = (pa, sa, wt, 1, 0)
                else:
                    cv[y][x] = (pa, sa, LOW_SPECK if ((x * 7 + y * 13) % 11 == 0) else LOW_FLOOR, 0, 0)

        # 5. Paint Two-Tier Cascading Waterfall (179 crest, 185 body, 187 splash foam)
        for x in range(80, 87):
            cv[10][x] = (pa, sa, WF_TOP, 1, 0)
            for y in range(11, 18): cv[y][x] = (pa, sa, WF_BODY, 1, 0)
            cv[18][x] = (pa, sa, WF_FOAM, 1, 0)
            for y in range(19, 29): cv[y][x] = (pa, sa, WF_BODY, 1, 0)
            cv[29][x] = (pa, sa, WF_FOAM, 1, 0)

        # 6. Define 6 REAL 3D Elevated Bridges (deck_y = 44px):
        #    Each bridge spans OVER the 1-tile bank rim at BOTH ends so its deck rests 100% FLUSH on the flat 44px plateau
        #    crest with ZERO gap, while the earthen bank slopes naturally down into 0px flat water underneath!
        #    Tuple: (x0, y0, x1, y1, deck_y, dir) where dir 0 = EW (railings N/S), 1 = NS (railings W/E)
        bridges = [
            (79, 19, 87, 21, 44, 0),   # #1 Mid-Waterfall 3D Bridge (West Mesa crest x=79 <-> East Ridge crest x=87!)
            (59, 38, 62, 43, 44, 1),   # #2 Central Lake Overlook 3D Bridge (Overlook crest y=38 <-> Peninsula crest y=43!)
            (55, 45, 58, 47, 44, 0),   # #3 West Lake 3D Bridge (Central-Left crest x=55 <-> Peninsula crest x=58!)
            (66, 50, 69, 54, 44, 1),   # #4 South Channel 3D Bridge (Peninsula crest y=50 <-> South-Center crest y=54!)
            (107, 11, 110, 17, 44, 1), # #5 East Canal 3D Bridge (Far NE crest y=11 <-> East Plateau crest y=17!)
            (75, 31, 89, 33, 44, 0),   # #6 Lower Waterfall Splash-Basin 3D Bridge (Overlook crest x=75 <-> Mid-East crest x=89!)
        ]
        # Ensure flat 44px plateau banks cleanly meeting BOTH ends of Bridge #6 (x<=74 & x>=90 flat 44px!)
        fill_rect(54, 30, 75, 35, 1)
        fill_rect(89, 29, 95, 35, 1)
        # Re-run cliff autotiling cleanly with Bridge #6 landings baked into hm!
        cv_cliffs, tag = autotile_emerald_cliffs(hm, W, H, pa, sa, GRAN_CFG, two_row_south=False)
        for y in range(H):
            for x in range(W):
                if cv_cliffs[y][x] is not None:
                    cv[y][x] = cv_cliffs[y][x]
        # Re-paint waterfall cleanly (still 100% flat 0px water elevation so water NEVER rises with bridges!)
        for x in range(80, 87):
            cv[10][x] = (pa, sa, WF_TOP, 1, 0)
            for y in range(11, 18): cv[y][x] = (pa, sa, WF_BODY, 1, 0)
            cv[18][x] = (pa, sa, WF_FOAM, 1, 0)
            for y in range(19, 29): cv[y][x] = (pa, sa, WF_BODY, 1, 0)
            cv[29][x] = (pa, sa, WF_FOAM, 1, 0)

        # Keep earthen cliff banks sloping down naturally underneath each bridge's first/last abutment tile,
        # and ensure the 2 tiles IMMEDIATELY OUTSIDE each bridge end are 100% flat 44px walkable plateau!
        flat_override = {}
        bridge_cells = set()
        for (bx0, by0, bx1, by1, deck_y, bdir) in bridges:
            for y in range(by0, by1 + 1):
                for x in range(bx0, bx1 + 1):
                    bridge_cells.add((x, y))
                    if hm[y][x] == -1:
                        if not (80 <= x <= 86 and 10 <= y <= 29):
                            cv[y][x] = (pa, sa, WATER, 1, 0)
            if bdir == 0:
                # EW bridge: tile bx0 & bx1 sit over the bank-to-water slope (core=None); tiles bx0-2..bx0-1 & bx1+1..bx1+2 are flat 44px!
                for y in range(by0, by1 + 1):
                    for x in (bx0 - 2, bx0 - 1, bx1 + 1, bx1 + 2):
                        if 2 <= x < W - 2 and hm[y][x] > 0:
                            cv[y][x] = (pa, sa, PLAT_FLOOR, 0, 0)
                            flat_override[(x, y)] = deck_y
            else:
                # NS bridge: tile by0 & by1 sit over the bank-to-water slope (core=None); tiles by0-2..by0-1 & by1+1..by1+2 are flat 44px!
                for x in range(bx0, bx1 + 1):
                    for y in (by0 - 2, by0 - 1, by1 + 1, by1 + 2):
                        if 2 <= y < H - 2 and hm[y][x] > 0:
                            cv[y][x] = (pa, sa, PLAT_FLOOR, 0, 0)
                            flat_override[(x, y)] = deck_y

        # 7. Paint 100% CONTINUOUS Golden Road Network (ZERO dead-end paths hitting cliffs!)
        gold_mask = [[False] * W for _ in range(H)]
        def mark_gold(x0, y0, x1, y1, r=1):
            steps = max(abs(x1 - x0), abs(y1 - y0), 1)
            for s in range(steps + 1):
                cx = int(round(x0 + (x1 - x0) * s / steps))
                cy = int(round(y0 + (y1 - y0) * s / steps))
                for dy in range(-r, r + 1):
                    for dx in range(-r, r + 1):
                        nx, ny = cx + dx, cy + dy
                        if 2 <= nx < W - 2 and 2 <= ny < H - 2 and cv[ny][nx][3] == 0 and (nx, ny) not in bridge_cells:
                            gold_mask[ny][nx] = True

        # Every single golden road segment (3 tiles wide, r=1) aligns DEAD-CENTER with its 3-tile staircase or 3D bridge!
        mark_gold(8, 4, 28, 4, r=1)          # Top-left golden road -> Top-left E-W stairs (x=29, y=3..5)
        mark_gold(30, 4, 94, 4, r=1)         # Wide accessible top promenade loop above waterfall -> NE stairs (x=95, y=3..5)
        mark_gold(96, 4, 109, 4, r=1)        # Top-right road -> Bridge #5 North approach (x=109)
        mark_gold(8, 4, 8, 57, r=1)          # West highway dead-center through Upper-Left & Lower-Left stairs (x=7..9)
        mark_gold(8, 57, 23, 57, r=1)        # South-West connector -> West-Mid road (x=23)
        mark_gold(23, 57, 23, 27, r=1)       # West-Mid highway dead-center through West-Mid stairs (x=22..24)
        mark_gold(23, 27, 64, 27, r=1)       # Central valley cross-road linking West-Mid (23), Bridge Shelf (28), Central-Left (43), & Upper-Center (64)!
        mark_gold(64, 20, 64, 27, r=1)       # Upper-Center Mesa road straight down through South Staircase (63..65, y=23)
        mark_gold(28, 27, 28, 17, r=1)       # Bridge shelf connector dead-center through South Staircase (27..29, y=21)!
        mark_gold(28, 17, 37, 17, r=1)       # Bridge shelf -> East Staircase (x=34, y=16..18) onto Upper-Center Mesa!
        mark_gold(37, 17, 37, 20, r=1)       # Smooth corner link onto Upper-Center Mesa road
        mark_gold(37, 20, 78, 20, r=1)       # Upper-Center Mesa road straight to Bridge #1 (West end)
        mark_gold(88, 20, 109, 20, r=1)      # Bridge #1 (East end) straight to Far-East Highway (x=109)
        mark_gold(43, 27, 43, 58, r=1)       # Central-Left highway dead-center through BOTH North (42..44, y=29) & South (42..44, y=48) stairs!
        mark_gold(43, 58, 96, 58, r=1)       # South main highway dead-center through South-Center West Stair (x=62, y=57..59) & SE stairs!
        mark_gold(43, 46, 54, 46, r=1)       # Direct golden path to Bridge #3 (West end)
        mark_gold(59, 46, 68, 46, r=1)       # Peninsula plaza linking Bridge #3, Bridge #2, & Bridge #4!
        mark_gold(60, 44, 60, 46, r=1)       # Bridge #2 South end -> Peninsula plaza
        mark_gold(68, 46, 68, 58, r=1)       # Peninsula plaza -> Bridge #4 -> South Highway!
        mark_gold(60, 32, 60, 37, r=1)       # Overlook road straight into Bridge #2 (North end)
        mark_gold(43, 32, 74, 32, r=1)       # Continuous plateau avenue from Central-Left -> Overlook -> Bridge #6 (West end)!
        mark_gold(90, 32, 96, 32, r=1)       # Bridge #6 (East end) -> Mid-East Terrace connector
        mark_gold(96, 20, 96, 58, r=1)       # Continuous Mid-East Terrace avenue dead-center through SE Stair (95..97, y=52)!
        mark_gold(96, 43, 109, 43, r=1)      # Mid-East avenue -> East-Center Stair (x=103, y=42..44) -> Far-East Highway!
        mark_gold(109, 4, 109, 10, r=1)      # Far NE road -> Bridge #5 (North end)
        mark_gold(109, 18, 109, 58, r=1)     # Far-East Highway dead-center through North (108..110, y=24) & South (108..110, y=48) stairs!
        mark_gold(109, 58, 96, 58, r=1)      # Far South-East corner closing the loop!

        # 8. Carve staircases (ZERO ladders, ZERO water stairs, ZERO random flat stairs, 100% matched 1-tile cliff slope!)
        stair_ramps = []
        extra_slope = set()
        def h_px(val): return 0 if val <= 0 else (44 if val == 1 else 88)

        def carve_stair_ns(x0, x1, y_edge):
            """Carve long gentle 3-tile N-S stone staircase (24.6 deg slope, consistent on ALL N & S stairs, never on flat ground or water)."""
            xm = (x0 + x1) // 2
            hn_v = hm[max(0, y_edge - 1)][xm]
            hs_v = hm[min(H - 1, y_edge + 1)][xm]
            if hn_v == -1 or hs_v == -1:
                return
            h_n, h_s = h_px(hn_v), h_px(hs_v)
            if h_n == h_s:
                return
            xa, xb = max(2, x0), min(W - 3, x1)
            if h_s > h_n:
                y_start = y_edge
                y_end = y_edge
                for step in (1, 2):
                    ny = y_edge + step
                    if ny < H - 2 and hm[ny][xm] == hs_v and hm[min(H - 1, ny + 1)][xm] == hs_v and all((x, ny) not in bridge_cells for x in range(xa, xb + 1)):
                        y_end = ny
                    else:
                        break
            else:
                y_end = y_edge
                y_start = y_edge
                for step in (1, 2):
                    ny = y_edge - step
                    if ny >= 2 and hm[ny][xm] == hn_v and hm[max(0, ny - 1)][xm] == hn_v and all((x, ny) not in bridge_cells for x in range(xa, xb + 1)):
                        y_start = ny
                    else:
                        break
            for y in range(y_start, y_end + 1):
                for x in range(xa, xb + 1):
                    if hm[y][x] == -1 or (x, y) in bridge_cells: continue
                    cv[y][x] = (pa, sa, STAIR_NS, 0, 0)
                    extra_slope.add((x, y))
            stair_ramps.append((xa, y_start, xb, y_end, h_n, h_s, 'NS'))

        def carve_stair_ew(cx, y0, y1, climbing_east=True):
            """Carve long gentle 3-tile E-W vertical stone staircase (rotated 918, ZERO ladders, 24.6 deg slope)."""
            fl = 3 if climbing_east else 2
            ym = (y0 + y1) // 2
            hw_v = hm[ym][max(0, cx - 1)]
            he_v = hm[ym][min(W - 1, cx + 1)]
            if hw_v == -1 or he_v == -1:
                return
            h_w, h_e = h_px(hw_v), h_px(he_v)
            if h_w == h_e:
                return
            ya, yb = max(2, y0), min(H - 2, y1)
            if h_e > h_w:
                x_lo = cx
                x_hi = cx
                for step in (1, 2):
                    nx = cx + step
                    if nx < W - 2 and hm[ym][nx] == he_v and hm[ym][min(W - 1, nx + 1)] == he_v and all((nx, y) not in bridge_cells for y in range(ya, yb + 1)):
                        x_hi = nx
                    else:
                        break
            else:
                x_hi = cx
                x_lo = cx
                for step in (1, 2):
                    nx = cx - step
                    if nx >= 2 and hm[ym][nx] == hw_v and hm[ym][max(0, nx - 1)] == hw_v and all((nx, y) not in bridge_cells for y in range(ya, yb + 1)):
                        x_lo = nx
                    else:
                        break
            for y in range(ya, yb + 1):
                for x in range(x_lo, x_hi + 1):
                    if hm[y][x] == -1 or (x, y) in bridge_cells: continue
                    cv[y][x] = (pa, sa, STAIR_NS, 0, fl)
                    extra_slope.add((x, y))
            stair_ramps.append((x_lo, ya, x_hi, yb, h_w, h_e, 'EW'))

        # Clean, narrow 3-tile-wide staircases, DEAD-CENTER on every golden path & well clear of plateau corners!
        carve_stair_ew(29, 3, 5, False)       # Top-left plateau <-> wide scenic top promenade (centered on y=4)
        carve_stair_ew(95, 3, 5, True)        # Wide scenic top promenade <-> Far NE plateau (centered on y=4)
        carve_stair_ns(7, 9, 25)              # Upper-Left plateau south stairs (centered on x=8)
        carve_stair_ns(27, 29, 21)            # Bridge shelf south descent (centered on x=28, 5 tiles clear of corner x=34!)
        carve_stair_ns(7, 9, 36)              # Lower-Left north stairs (centered on x=8)
        carve_stair_ns(7, 9, 46)              # Lower-Left south stairs (centered on x=8)
        carve_stair_ns(22, 24, 36)            # West-Mid north stairs (centered on x=23, 5 tiles clear of corners!)
        carve_stair_ns(22, 24, 44)            # West-Mid south stairs (centered on x=23)
        carve_stair_ns(42, 44, 48)            # Central-Left South Staircase (centered on x=43, 11 tiles clear of corners!)
        carve_stair_ns(42, 44, 29)            # Central-Left North Staircase (centered on x=43)
        carve_stair_ew(34, 16, 18, True)      # West entry onto Upper-Center Mesa (centered on y=17)
        carve_stair_ns(63, 65, 23)            # Upper-Center Mesa south stairs (centered on x=64)
        carve_stair_ns(49, 51, 10)            # Upper-Center Tier 2 north stairs (centered on x=50)
        carve_stair_ns(49, 51, 18)            # Upper-Center Tier 2 south stairs (centered on x=50)
        carve_stair_ns(15, 17, 10)            # Upper-Left Tier 2 mound north stairs (centered on x=16)
        carve_stair_ns(15, 17, 20)            # Upper-Left Tier 2 mound south stairs (centered on x=16)
        carve_stair_ns(108, 110, 24)          # Mid-East golden road staircase onto Tier 2 East plateau (centered on x=109)
        carve_stair_ew(103, 42, 44, True)     # East-Center stairs climbing up onto Tier 2 East plateau (centered on y=43)
        carve_stair_ns(95, 97, 52)            # South-East staircase onto south highway (centered on x=96)
        carve_stair_ns(108, 110, 48)          # Far South-East staircase (centered on x=109)
        carve_stair_ew(62, 57, 59, True)      # South golden road onto South-Center plateau (centered on y=58)
        carve_stair_ns(31, 33, 59)            # Southern outcrop walkable stairs (centered on x=32)

        # 9. Golden roads NEVER touch cliffs: drop any road cell with a cliff (non-stair) cardinal neighbour,
        #    then drop leftover tiny specks, then autotile road borders (stairs + bridges count as road joins).
        def is_cliff(x, y):
            if not (0 <= x < W and 0 <= y < H): return True
            return cv_cliffs[y][x] is not None and (x, y) not in extra_slope and (x, y) not in bridge_cells
        for (x, y) in extra_slope:
            gold_mask[y][x] = False
        for y in range(2, H - 2):
            for x in range(2, W - 2):
                if gold_mask[y][x] and any(is_cliff(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    gold_mask[y][x] = False
        seen_g = set()
        for y in range(2, H - 2):
            for x in range(2, W - 2):
                if not gold_mask[y][x] or (x, y) in seen_g: continue
                comp = [(x, y)]; seen_g.add((x, y)); qi = 0
                while qi < len(comp):
                    cx_, cy_ = comp[qi]; qi += 1
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        n = (cx_ + dx, cy_ + dy)
                        if 0 <= n[0] < W and 0 <= n[1] < H and gold_mask[n[1]][n[0]] and n not in seen_g:
                            seen_g.add(n); comp.append(n)
                if len(comp) < 9:
                    for (gx_, gy_) in comp: gold_mask[gy_][gx_] = False
        # 1-tile-thin road slivers look broken: require each road cell to have road on both sides in some axis or be 2+ thick
        def is_g(x, y):
            if not (0 <= x < W and 0 <= y < H): return False
            return gold_mask[y][x] or (x, y) in bridge_cells or (x, y) in extra_slope
        for y in range(2, H - 2):
            for x in range(2, W - 2):
                if not gold_mask[y][x]: continue
                gn = is_g(x, y - 1); gs = is_g(x, y + 1)
                gw = is_g(x - 1, y); ge = is_g(x + 1, y)
                gnw = is_g(x - 1, y - 1); gne = is_g(x + 1, y - 1)
                if not gn and not gw:   gt = 664
                elif not gn and not ge: gt = 666
                elif not gs and not gw: gt = 680
                elif not gs and not ge: gt = 682
                elif not gn:            gt = 665
                elif not gs:            gt = 681
                elif not gw:            gt = 672
                elif not ge:            gt = 674
                elif not gnw:           gt = 670
                elif not gne:           gt = 671
                else:                   gt = 673
                cv[y][x] = (pa, sa, gt, 0, 0)
        touching = sum(1 for y in range(2, H - 2) for x in range(2, W - 2) if gold_mask[y][x] and
                       any(is_cliff(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))))
        print('  granite gold cells touching cliffs:', touching)
        assert touching == 0

        cv = _ensure_all_traversible(cv, pa, sa, STAIR_NS, 537, bridge_cells=bridge_cells)
        # Note: NO wf_slope on water! Every water cell stays strictly flat 0px so water NEVER rises with bridges!
        elev = build_vertex_elev(hm, tag, W, H, extra_slope=extra_slope,
                                 stair_ramps=stair_ramps, flat_override=flat_override)
        # Verify EVERY bridge end rests 100% flush on its deck_y ground crest!
        ew_s = W + 1
        for bi, (bx0, by0, bx1, by1, deck_y, bdir) in enumerate(bridges):
            if bdir == 0:
                for gz in range(by0, by1 + 2):
                    assert elev[gz * ew_s + bx0] == deck_y, f'Bridge {bi} W end mismatch at ({bx0},{gz}): {elev[gz*ew_s+bx0]}'
                    assert elev[gz * ew_s + bx1 + 1] == deck_y, f'Bridge {bi} E end mismatch at ({bx1+1},{gz}): {elev[gz*ew_s+bx1+1]}'
            else:
                for gx in range(bx0, bx1 + 2):
                    assert elev[by0 * ew_s + gx] == deck_y, f'Bridge {bi} N end mismatch at ({gx},{by0}): {elev[by0*ew_s+gx]}'
                    assert elev[(by1 + 1) * ew_s + gx] == deck_y, f'Bridge {bi} S end mismatch at ({gx},{by1+1}): {elev[(by1+1)*ew_s+gx]}'
        return cv, elev, bridges

    def build_chimney_cave():
        """120x66 volcanic magma cavern: bold 44px/116px 3D cliffs, zero ladders, all north stairs 3-tile visible ramps."""
        W, H = 120, 66
        pa, sa = 138278660, 138278804
        CHIM_CFG = dict(
            PLAT=930, FILL=625,
            NW=616, N=617, NE=618,
            W=624,  E=626,
            SW=632, S=628, SE=634,
            ISE=635, ISW=636, INW=638, INE=639
        )
        STAIR = 598
        hm = [[0] * W for _ in range(H)]
        for x in range(W):
            hm[0][x] = 9; hm[1][x] = 9; hm[H - 1][x] = 9
        for y in range(H):
            hm[y][0] = 9; hm[y][1] = 9; hm[y][W - 2] = 9; hm[y][W - 1] = 9
        plateaus = (
            (13, 12, 32, 22), (40, 12, 56, 22), (64, 12, 80, 22), (88, 12, 106, 22),
            (13, 28, 32, 38), (42, 28, 78, 38), (88, 28, 106, 38),
            (13, 44, 32, 54), (40, 44, 56, 54), (64, 44, 80, 54), (88, 44, 106, 54),
        )
        for (x0, y0, x1, y1) in plateaus:
            for y in range(y0, y1 + 1):
                for x in range(x0, x1 + 1): hm[y][x] = 1
        cv_cliffs, tag = autotile_emerald_cliffs(hm, W, H, pa, sa, CHIM_CFG, two_row_south=False)
        rng = random.Random(11)
        cv = [[(pa, sa, 784 if rng.random() < 0.04 else 776, 0, 0) for _ in range(W)] for _ in range(H)]
        for y in range(H):
            for x in range(W):
                if cv_cliffs[y][x] is not None: cv[y][x] = cv_cliffs[y][x]
        stair_ramps = []
        extra_slope = set()
        for (x0, y0, x1, y1) in plateaus:
            mx, my = (x0 + x1) // 2, (y0 + y1) // 2
            # Clean, narrow 3-tile-wide gentle North & South volcanic staircases centered well away from corners!
            for sy in range(y0, y0 + 3):
                for sx in range(mx - 1, mx + 2):
                    cv[sy][sx] = (pa, sa, STAIR, 0, 0)
                    extra_slope.add((sx, sy))
            stair_ramps.append((mx - 1, y0, mx + 1, y0 + 2, 0, 44, 'NS'))
            for sy in range(y1 - 2, y1 + 1):
                for sx in range(mx - 1, mx + 2):
                    cv[sy][sx] = (pa, sa, STAIR, 0, 0)
                    extra_slope.add((sx, sy))
            stair_ramps.append((mx - 1, y1 - 2, mx + 1, y1, 44, 0, 'NS'))
            # Consistent long gentle 3-tile West & East volcanic staircases (24.6 deg slope!)
            for sy in range(my - 1, my + 2):
                for sx in range(x0, x0 + 3):
                    cv[sy][sx] = (pa, sa, STAIR, 0, 3)
                    extra_slope.add((sx, sy))
                for sx in range(x1 - 2, x1 + 1):
                    cv[sy][sx] = (pa, sa, STAIR, 0, 2)
                    extra_slope.add((sx, sy))
            stair_ramps.append((x0, my - 1, x0 + 2, my + 1, 0, 44, 'EW'))
            stair_ramps.append((x1 - 2, my - 1, x1, my + 1, 44, 0, 'EW'))
        cv = _ensure_all_traversible(cv, pa, sa, STAIR, 628)
        elev = build_vertex_elev(hm, tag, W, H, extra_slope=extra_slope, stair_ramps=stair_ramps)
        return cv, elev, []

    LADDER_IDS = {526, 527, 534, 535, 573, 574, 575, 887, 895, 907}
    for name, builder in (
            ('GRANITE CAVERNS',     build_granite_cave_exact),
            ('MT. CHIMNEY CAVERNS', build_chimney_cave),
            ('SHOAL ICE CAVERNS',   build_ice_cave_exact)):
        print('cave', name)
        cv, elev, bridges = builder()
        W, H = len(cv[0]), len(cv)
        ladders = sum(1 for row in cv for c in row if c[2] in LADDER_IDS)
        assert ladders == 0, f'{name} still has {ladders} ladder tiles!'
        # Stitch every multi-tile staircase into ONE continuous wide flight (no inner vertical divider bars)
        STAIR_IDS = {918, 598, 867}
        src = [row[:] for row in cv]
        def is_st(x, y, idv, fl):
            if not (0 <= x < W and 0 <= y < H): return False
            c = src[y][x]
            return c[2] == idv and c[4] % 10 == fl
        for y in range(H):
            for x in range(W):
                c = src[y][x]
                if c[2] not in STAIR_IDS: continue
                fl = c[4] % 10
                if fl in (0, 1):
                    a = is_st(x - 1, y, c[2], fl); b = is_st(x + 1, y, c[2], fl)
                    part = 2 if (a and b) else (1 if b else (3 if a else 0))
                    if fl == 1 and part in (1, 3): part = 4 - part
                else:
                    a = is_st(x, y - 1, c[2], fl); b = is_st(x, y + 1, c[2], fl)
                    pos = 2 if (a and b) else (1 if b else (3 if a else 0))   # 1 = top, 3 = bottom
                    # ROTATE_270 maps source-left -> top; ROTATE_90 maps source-left -> bottom
                    part = pos if (fl == 3 or pos in (0, 2)) else 4 - pos
                cv[y][x] = (c[0], c[1], c[2], c[3], fl + 10 * part)
        t, k = bake(cv, atlas, True)
        # Mark elevated 3D bridge spans walkable (TER_PATH) while keeping real blue water tiles underneath!
        pa_g, sa_g = 138278660, 138279116
        bridges_baked = []
        for (bx0, by0, bx1, by1, deck_y, bdir) in bridges:
            for y in range(by0, by1 + 1):
                for x in range(bx0, bx1 + 1):
                    k[y][x] = TER_PATH
            # Authentic Pokemon Emerald wooden plank tile (924 vertical planks for EW bridge; rotated 90 deg for NS bridge!)
            plank_tid = 128 + atlas.tile(pa_g, sa_g, 924, 2 if bdir == 1 else 0)
            bridges_baked.append((bx0, by0, bx1, by1, deck_y, bdir, plank_tid))
        stages.append((name, W, H, t, k, make_under(t, k, W, H), elev, bridges_baked))

    # ---- atlas: extract top 128 classic tiles directly from Emerald ROM if emerald_tiles.png does not exist yet
    ap = os.path.join(ws, 'assets', 'world', 'emerald_tiles.png')
    os.makedirs(os.path.dirname(ap), exist_ok=True)
    if os.path.isfile(ap):
        orig = Image.open(ap).convert('RGBA').crop((0, 0, 256, 128))
    else:
        import emr
        from top128_map import TOP128_ROM_MAP
        orig = Image.new('RGBA', (256, 128), (0, 0, 0, 0))
        for tid, (pa, sa, mid) in enumerate(TOP128_ROM_MAP):
            mts = emr.metatiles(pa, sa)
            orig.paste(mts[mid][0].convert('RGBA'), ((tid & 15) * 16, (tid >> 4) * 16))
    n = len(atlas.imgs); rows_ = (n + 15) // 16
    out = Image.new('RGBA', (256, 128 + rows_ * 16), (0, 0, 0, 0)); out.paste(orig, (0, 0))
    for i, im in enumerate(atlas.imgs):
        out.paste(im.convert('RGBA'), ((i & 15) * 16, 128 + (i >> 4) * 16))
    out.save(ap); print('atlas tiles:', 128 + rows_ * 16 * 1, 'px high,', n, 'new metatiles')

    # ---- C data + previews + report (only active 4 baked stages s2..s5 match BAKED_STAGE_COUNT == 4)
    lines = ['/* AUTO-GENERATED by tools/emerald_maps/build_maps.py from Pokemon Emerald map data. Do not edit. */',
             '#include "maps_data.h"', '']
    meta = []
    for si, (name, W, H, tiles, kinds, under, elev, bridges) in enumerate(stages):
        if si < 2:
            continue  # s0 (Littleroot-Slateport) and s1 (Rustboro-Mauville) are unused
        sx, sy, big, tot = pick_start(kinds, W, H, PREFER.get(si))
        print(f'{name}: {W}x{H} tiles ({W*32}x{H*32}px) start=({sx},{sy}) reachable={big}/{tot} walkable bridges={len(bridges) if bridges else 0}')
        prev_p = os.path.join(ws, 'assets', 'world', f'stage_preview_{si + 3}.png')
        if not os.path.isfile(prev_p):
            write_preview(tiles, kinds, atlas, 0, prev_p, W, H)
        flat_t = [128 + tiles[y][x] for y in range(H) for x in range(W)]
        flat_k = [kinds[y][x] for y in range(H) for x in range(W)]
        flat_u = [128 + under[y][x] for y in range(H) for x in range(W)]
        lines.append(f'static const uint16_t s{si}_tiles[{W*H}] = {{')
        for i in range(0, len(flat_t), 24): lines.append(','.join(map(str, flat_t[i:i + 24])) + ',')
        lines.append('};')
        lines.append(f'static const uint16_t s{si}_under[{W*H}] = {{')
        for i in range(0, len(flat_u), 24): lines.append(','.join(map(str, flat_u[i:i + 24])) + ',')
        lines.append('};')
        lines.append(f'static const uint8_t s{si}_kinds[{W*H}] = {{')
        for i in range(0, len(flat_k), 48): lines.append(''.join(str(v) for v in flat_k[i:i + 48]).replace('', ',')[1:-1] + ',')
        lines.append('};')
        if elev is not None:
            lines.append(f'static const uint8_t s{si}_elev[{(W+1)*(H+1)}] = {{')
            for i in range(0, len(elev), 32): lines.append(','.join(map(str, elev[i:i + 32])) + ',')
            lines.append('};')
            elev_ref = f's{si}_elev'
        else:
            elev_ref = '0'
        if bridges:
            lines.append(f'static const BakedBridge s{si}_bridges[{len(bridges)}] = {{')
            for (bx0, by0, bx1, by1, deck_y, bdir, plank_tid) in bridges:
                lines.append(f'    {{ {bx0}, {by0}, {bx1}, {by1}, {deck_y}, {bdir}, {plank_tid} }},')
            lines.append('};')
            br_ref, br_cnt = f's{si}_bridges', len(bridges)
        else:
            br_ref, br_cnt = '0', 0
        meta.append(f'    {{ {W}, {H}, {sx}, {sy}, s{si}_tiles, s{si}_kinds, s{si}_under, {elev_ref}, {br_ref}, {br_cnt} }}')
    lines.append(f'const BakedMap g_baked_maps[BAKED_STAGE_COUNT] = {{')
    lines.append(',\n'.join(meta)); lines.append('};')
    pa_g, sa_g = 138278660, 138279116
    lines.append(f'const uint16_t g_baked_tid_water   = {128 + atlas.tile(pa_g, sa_g, 368, 0)};')
    lines.append(f'const uint16_t g_baked_tid_wf_top  = {128 + atlas.tile(pa_g, sa_g, 179, 0)};')
    lines.append(f'const uint16_t g_baked_tid_wf_body = {128 + atlas.tile(pa_g, sa_g, 185, 0)};')
    lines.append(f'const uint16_t g_baked_tid_wf_foam = {128 + atlas.tile(pa_g, sa_g, 187, 0)};')
    open(os.path.join(ws, 'src', 'maps_data.c'), 'w').write('\n'.join(lines) + '\n')
    import json
    json.dump([TOWN_RECTS[k] for k in TOWN_ORDER], open(os.path.join(ws, 'tools', 'emerald_maps', 'stage_layouts.json'), 'w'))
    json.dump(STAGE_CELLS, open(os.path.join(ws, 'tools', 'emerald_maps', 'stage_cells.json'), 'w'))
    return stages, atlas

if __name__ == '__main__':
    main()
