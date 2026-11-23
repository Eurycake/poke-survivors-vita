"""Reader for HctrRB/pokeemerald-vita-3ds assets/voxel/relief.bin (VXL4) - same decode as voxel_relief.c."""
import struct
def load(path):
    d = open(path, 'rb').read(); U16 = lambda o: struct.unpack_from('<H', d, o)[0]
    assert d[:4] == b'VXL4'
    out = {}
    for i in range(U16(4)):
        o = 8 + i * 14
        lid, n, w, h = U16(o), U16(o + 2), U16(o + 4), U16(o + 6)
        off = struct.unpack_from('<I', d, o + 8)[0]; base = struct.unpack_from('<h', d, o + 12)[0]
        wide = bool(w & 0x4000); unit = 2 if h & 0x4000 else 1
        stride = 2 + (50 if wide else 25); cells = {}
        for k in range(n):
            p = off + k * stride; x, y = d[p], d[p + 1]
            hs = struct.unpack_from('<25h' if wide else '<25b', d, p + 2)
            cells[(x, y)] = [v * unit for v in hs]
        out[lid] = dict(base=base, cells=cells)
    return out
