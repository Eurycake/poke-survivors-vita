#ifndef TERRAIN_H
#define TERRAIN_H
#include <math.h>
#include <stdint.h>
#include "game.h"
#include "renderer.h"
#include "maps_data.h"

enum {
    TER_GRASS = 0,
    TER_PATH = 1,
    TER_SAND = 2,
    TER_FOREST = 3,
    TER_SHALLOW = 4,
    TER_DEEP = 5,
    TER_CLIFF = 6,
    TER_LEDGE_S = 7,
    TER_LEDGE_W = 8,
    TER_LEDGE_E = 9,
    TER_LEDGE_N = 10
};

#define TER_TILE 32
#define TER_GRID_W 200
#define TER_GRID_H 200


enum {
    ET_GRASS_A = 0, ET_GRASS_B = 1, ET_FLOWER = 2, ET_TALL_GRASS = 3,
    ET_FOREST_A = 4, ET_FOREST_DARK = 110,
    ET_PATH_C = 6, ET_DESERT_SUNKEN = 7, ET_SAND_GOLD = 8, ET_SAND_SPECK = 9,
    ET_SHALLOW_C = 10, ET_DEEP_F0 = 11, ET_TRENCH = 15,
    ET_PATH_NW = 16, ET_ROCK_DESERT = 27,
    ET_DUNE_NW = 32,
    ET_POND_NW = 48,
    ET_SW_NW = 64, ET_SW_N = 65, ET_SW_NE = 66, ET_SW_W = 67, ET_SW_E = 69,
    ET_SW_SW = 70, ET_SW_S = 71, ET_SW_SE = 72, ET_SW_INE = 73, ET_SW_INW = 74,
    ET_SW_ISE = 75, ET_SW_ISW = 76,
    ET_LEDGE_G_SL = 77, ET_LEDGE_G_SR = 78, ET_LEDGE_G_W = 79, ET_LEDGE_G_E = 80,
    ET_LEDGE_S_SL = 81, ET_LEDGE_S_SC = 82, ET_LEDGE_S_SR = 83, ET_LEDGE_S_W = 84, ET_LEDGE_S_E = 85,
    ET_LEDGE_G_CS = 86,
    ET_CLIFF_TOP = 113, ET_CLIFF_MID = 121, ET_CLIFF_BOT_G = 118, ET_CLIFF_BOT_S = 126
};

static inline uint32_t ter_hash(int x, int y, int s) {
    uint32_t h = (uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663) ^ (uint32_t)(s * 83492791);
    h ^= h >> 13;
    h *= 1274126177u;
    return h ^ (h >> 16);
}

static inline int ter_in_rect(int tx, int ty, int x0, int y0, int x1, int y1) {
    return tx >= x0 && tx <= x1 && ty >= y0 && ty <= y1;
}

static inline int terrain_tile_kind(int stage, int tx, int ty) {
    if (stage >= BAKED_FIRST_STAGE) {
        const BakedMap *bm = &g_baked_maps[stage - BAKED_FIRST_STAGE];
        if (tx < 0) tx = 0;
        if (tx >= bm->w) tx = bm->w - 1;
        if (ty < 0) ty = 0;
        if (ty >= bm->h) ty = bm->h - 1;
        int k = bm->kind[ty * bm->w + tx];
        if (stage == STAGE_TOWN_HOENN && k == TER_CLIFF) {
            if (render_hoenn_tile_cleared(tx, ty)) return TER_GRASS;

            if (tx >= 6 && tx < bm->w - 6 && ty >= 6 && ty < bm->h - 6) {
                uint16_t tid = bm->tile[ty * bm->w + tx];
                if (tid == 128 || tid == 129) return TER_GRASS;
            }
        }
        return k;
    }
    if (tx < 0) tx = 0;
    if (tx >= TER_GRID_W) tx = TER_GRID_W - 1;
    if (ty < 0) ty = 0;
    if (ty >= TER_GRID_H) ty = TER_GRID_H - 1;

    if (stage == STAGE_ROUTE_29) {

        if (tx >= 92) {
            if (ter_in_rect(tx, ty, 134, 62, 146, 72)) return TER_DEEP;
            if (ter_in_rect(tx, ty, 131, 59, 149, 75)) return TER_SHALLOW;

            if (ty == 36 && tx >= 114 && tx <= 122) return TER_LEDGE_S;
            if (ty == 84 && tx >= 106 && tx <= 118) return TER_LEDGE_S;
            if (ty == 112 && tx >= 144 && tx <= 160) return TER_LEDGE_S;
            if (ty == 136 && tx >= 114 && tx <= 121) return TER_LEDGE_S;
            if (ty == 158 && tx >= 128 && tx <= 141) return TER_LEDGE_N;
            if (ty == 68 && tx >= 154 && tx <= 164) return TER_LEDGE_N;
            if (tx == 104 && ty >= 118 && ty <= 127) return TER_LEDGE_W;
            if (tx == 156 && ty >= 92 && ty <= 104) return TER_LEDGE_E;
            if (tx == 168 && ty >= 42 && ty <= 50) return TER_LEDGE_E;
            if (tx == 118 && ty >= 46 && ty <= 55) return TER_LEDGE_W;
            return TER_SAND;
        }

        if (ter_in_rect(tx, ty, 15, 140, 26, 153)) return TER_DEEP;
        if (ter_in_rect(tx, ty, 11, 136, 30, 157)) return TER_SHALLOW;

        if (ter_in_rect(tx, ty, 63, 25, 73, 33)) return TER_DEEP;
        if (ter_in_rect(tx, ty, 60, 22, 76, 36)) return TER_SHALLOW;

        if (ty == 24 && tx >= 20 && tx <= 27) return TER_LEDGE_S;
        if (ty == 46 && tx >= 18 && tx <= 31) return TER_LEDGE_S;
        if (ty == 60 && tx >= 54 && tx <= 68) return TER_LEDGE_S;
        if (ty == 112 && tx >= 20 && tx <= 28) return TER_LEDGE_S;
        if (ty == 124 && tx >= 56 && tx <= 71) return TER_LEDGE_S;
        if (ty == 148 && tx >= 58 && tx <= 65) return TER_LEDGE_N;
        if (ty == 82 && tx >= 16 && tx <= 25) return TER_LEDGE_N;
        if (tx == 28 && ty >= 70 && ty <= 79) return TER_LEDGE_W;
        if (tx == 52 && ty >= 108 && ty <= 116) return TER_LEDGE_W;
        if (tx == 76 && ty >= 110 && ty <= 122) return TER_LEDGE_E;
        if (tx == 72 && ty >= 42 && ty <= 49) return TER_LEDGE_E;

        if (ter_in_rect(tx, ty, 41, 3, 43, 196)) return TER_PATH;
        if (ter_in_rect(tx, ty, 43, 92, 91, 94)) return TER_PATH;
        return TER_GRASS;
    }
    if (stage == STAGE_NATIONAL_PARK) {

        if (ter_in_rect(tx, ty, 91, 86, 102, 97)) return TER_DEEP;
        if (ter_in_rect(tx, ty, 88, 83, 105, 100)) return TER_SHALLOW;
        if (ter_in_rect(tx, ty, 90, 33, 102, 43)) return TER_DEEP;
        if (ter_in_rect(tx, ty, 87, 30, 105, 46)) return TER_SHALLOW;

        if (ty == 28 && tx >= 24 && tx <= 31) return TER_LEDGE_S;
        if (ty == 36 && tx >= 130 && tx <= 143) return TER_LEDGE_S;
        if (ty == 58 && tx >= 46 && tx <= 63) return TER_LEDGE_S;
        if (ty == 64 && tx >= 128 && tx <= 141) return TER_LEDGE_S;
        if (ty == 104 && tx >= 134 && tx <= 142) return TER_LEDGE_N;
        if (ty == 134 && tx >= 48 && tx <= 65) return TER_LEDGE_S;
        if (ty == 138 && tx >= 124 && tx <= 135) return TER_LEDGE_N;
        if (ty == 160 && tx >= 104 && tx <= 119) return TER_LEDGE_N;
        if (tx == 38 && ty >= 66 && ty <= 79) return TER_LEDGE_W;
        if (tx == 62 && ty >= 96 && ty <= 106) return TER_LEDGE_W;
        if (tx == 30 && ty >= 146 && ty <= 156) return TER_LEDGE_W;
        if (tx == 156 && ty >= 70 && ty <= 82) return TER_LEDGE_E;
        if (tx == 148 && ty >= 114 && ty <= 122) return TER_LEDGE_E;
        if (tx == 124 && ty >= 44 && ty <= 53) return TER_LEDGE_E;
        return TER_SAND;
    }

    if (ter_in_rect(tx, ty, 52, 62, 64, 72)) return TER_DEEP;
    if (ter_in_rect(tx, ty, 49, 59, 67, 75)) return TER_SHALLOW;
    if (ter_in_rect(tx, ty, 130, 120, 142, 130)) return TER_DEEP;
    if (ter_in_rect(tx, ty, 127, 117, 145, 133)) return TER_SHALLOW;

    if (ty == 28 && tx >= 26 && tx <= 33) return TER_LEDGE_S;
    if (ty == 46 && tx >= 30 && tx <= 46) return TER_LEDGE_S;
    if (ty == 54 && tx >= 126 && tx <= 140) return TER_LEDGE_S;
    if (ty == 124 && tx >= 66 && tx <= 75) return TER_LEDGE_N;
    if (ty == 146 && tx >= 34 && tx <= 50) return TER_LEDGE_S;
    if (ty == 152 && tx >= 126 && tx <= 137) return TER_LEDGE_N;
    if (ty == 82 && tx >= 136 && tx <= 146) return TER_LEDGE_N;
    if (tx == 22 && ty >= 64 && ty <= 74) return TER_LEDGE_W;
    if (tx == 36 && ty >= 112 && ty <= 120) return TER_LEDGE_W;
    if (tx == 116 && ty >= 24 && ty <= 34) return TER_LEDGE_W;
    if (tx == 168 && ty >= 66 && ty <= 79) return TER_LEDGE_E;
    if (tx == 122 && ty >= 70 && ty <= 78) return TER_LEDGE_E;
    if (tx == 158 && ty >= 142 && ty <= 152) return TER_LEDGE_E;
    if (ter_in_rect(tx, ty, 94, 3, 96, 196)) return TER_PATH;
    if (ter_in_rect(tx, ty, 3, 94, 196, 96)) return TER_PATH;
    return TER_FOREST;
}

static inline int terrain_at(int stage, float x, float y) {
    int tx = (int)floorf(x / (float)TER_TILE);
    int ty = (int)floorf(y / (float)TER_TILE);
    return terrain_tile_kind(stage, tx, ty);
}

static inline int terrain_is_water(int t) { return t == TER_SHALLOW || t == TER_DEEP; }
static inline int terrain_is_ledge(int t) { return t >= TER_LEDGE_S && t <= TER_LEDGE_N; }
static inline int terrain_is_blocked(int t) { return t == TER_DEEP || t == TER_CLIFF || terrain_is_ledge(t); }

static inline int terrain_box_blocked(int stage, float x, float y, float rx, float rn, float rs) {
    if (terrain_is_blocked(terrain_at(stage, x, y))) return 1;
    if (terrain_is_blocked(terrain_at(stage, x, y - rn))) return 1;
    if (terrain_is_blocked(terrain_at(stage, x, y + rs))) return 1;
    if (terrain_is_blocked(terrain_at(stage, x - rx, y))) return 1;
    if (terrain_is_blocked(terrain_at(stage, x + rx, y))) return 1;
    float cx = rx * 0.707f, cn = rn * 0.707f, cs = rs * 0.707f;
    if (terrain_is_blocked(terrain_at(stage, x - cx, y - cn))) return 1;
    if (terrain_is_blocked(terrain_at(stage, x + cx, y - cn))) return 1;
    if (terrain_is_blocked(terrain_at(stage, x - cx, y + cs))) return 1;
    if (terrain_is_blocked(terrain_at(stage, x + cx, y + cs))) return 1;
    return 0;
}

static inline void terrain_apply_enemy_knockback(int stage, float *ex, float *ey, float radius, bool is_boss, float push_x, float push_y) {
    float dist = sqrtf(push_x * push_x + push_y * push_y);
    if (dist < 0.01f) return;
    float edge_m = (stage >= BAKED_FIRST_STAGE) ? 44.0f : 156.0f;
    float erx = 7.0f;
    float ern = 8.0f;
    float ers = 5.0f;
    (void)radius; (void)is_boss;
    int steps = (int)ceilf(dist / 12.0f);
    if (steps < 1) steps = 1;
    if (steps > 16) steps = 16;
    float sx = push_x / (float)steps;
    float sy = push_y / (float)steps;
    for (int s = 0; s < steps; s++) {
        float nx = *ex + sx;
        if (nx >= edge_m && nx <= WORLD_W - edge_m &&
            !terrain_box_blocked(stage, nx, *ey, erx, ern, ers) &&
            !terrain_is_water(terrain_at(stage, nx, *ey))) {
            *ex = nx;
        }
        float ny = *ey + sy;
        if (ny >= edge_m && ny <= WORLD_H - edge_m &&
            !terrain_box_blocked(stage, *ex, ny, erx, ern, ers) &&
            !terrain_is_water(terrain_at(stage, *ex, ny))) {
            *ey = ny;
        }
    }
}

static inline int terrain_under_at(int stage, int tx, int ty) {
    if (stage >= BAKED_FIRST_STAGE) {
        const BakedMap *bm = &g_baked_maps[stage - BAKED_FIRST_STAGE];
        if (tx < 0) tx = 0;
        if (tx >= bm->w) tx = bm->w - 1;
        if (ty < 0) ty = 0;
        if (ty >= bm->h) ty = bm->h - 1;
        return bm->under[ty * bm->w + tx];
    }
    if (stage == STAGE_NATIONAL_PARK || (stage == STAGE_ROUTE_29 && tx >= 92)) {
        return ET_SAND_GOLD;
    }
    return ET_GRASS_A;
}


static inline int terrain_line_clear(int stage, float x0, float y0, float x1, float y1) {
    float dx = x1 - x0, dy = y1 - y0;
    int n = (int)(sqrtf(dx * dx + dy * dy) / 20.0f) + 1;
    for (int i = 1; i <= n; i++) {
        float t = (float)i / (float)n;
        int k = terrain_at(stage, x0 + dx * t, y0 + dy * t);
        if (terrain_is_blocked(k) || terrain_is_water(k)) return 0;
    }
    return 1;
}


static inline int ter_in_sunken_desert(int stage, int tx, int ty) {
    if (stage == STAGE_ROUTE_29) {
        return ter_in_rect(tx, ty, 106, 20, 184, 52) || ter_in_rect(tx, ty, 106, 112, 184, 178);
    }
    if (stage == STAGE_NATIONAL_PARK) {
        return ter_in_rect(tx, ty, 18, 22, 78, 80) ||
               ter_in_rect(tx, ty, 114, 22, 180, 82) ||
               ter_in_rect(tx, ty, 18, 114, 80, 178) ||
               ter_in_rect(tx, ty, 112, 114, 180, 178);
    }
    return 0;
}

static inline int terrain_3x3_tile(int base_nw, int n, int s, int w, int e, int nw_same, int ne_same, int sw_same, int se_same) {
    if (!n && !w) return base_nw;
    if (!n && !e) return base_nw + 2;
    if (!s && !w) return base_nw + 6;
    if (!s && !e) return base_nw + 8;
    if (!n) return base_nw + 1;
    if (!s) return base_nw + 7;
    if (!w) return base_nw + 3;
    if (!e) return base_nw + 5;
    (void)nw_same; (void)ne_same; (void)sw_same; (void)se_same;
    return base_nw + 4;
}

static inline int terrain_is_smooth_ice_tile(int stage, int tx, int ty) {
    if (stage != STAGE_CAVE_ICE) return 0;
    const BakedMap *bm = &g_baked_maps[stage - BAKED_FIRST_STAGE];

    if (tx < 4 || ty < 4 || tx >= bm->w - 4 || ty >= bm->h - 4) return 0;
    if (bm->tile[ty * bm->w + tx] != 1552) return 0;

    for (int oy = -1; oy <= 1; oy++) {
        for (int ox = -1; ox <= 1; ox++) {
            int k = bm->kind[(ty + oy) * bm->w + (tx + ox)];
            if (terrain_is_blocked(k) || terrain_is_water(k)) return 0;
        }
    }

    int cx = tx / 14, cy = ty / 14;
    uint32_t h = ter_hash(cx, cy, 9017);
    if ((h & 3u) == 0u) return 0;
    int px = cx * 14 + 6 + (int)((h >> 2) & 1u);
    int py = cy * 14 + 6 + (int)((h >> 3) & 1u);
    int rx = 2 + (int)((h >> 4) & 1u);
    int ry = 2 + (int)((h >> 5) & 1u);
    if (rx == 3 && ry == 3) ry = 2;
    int dx = tx - px, dy = ty - py;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return (dx <= rx && dy <= ry);
}

static inline int terrain_is_smooth_ice(int stage, float wx, float wy) {
    if (stage != STAGE_CAVE_ICE) return 0;
    int tx = (int)floorf(wx * (1.0f / (float)TER_TILE));
    int ty = (int)floorf(wy * (1.0f / (float)TER_TILE));
    return terrain_is_smooth_ice_tile(stage, tx, ty);
}

static inline int terrain_tile_at(int stage, int tx, int ty, unsigned anim_tick) {
    if (stage >= BAKED_FIRST_STAGE) {
        const BakedMap *bm = &g_baked_maps[stage - BAKED_FIRST_STAGE];
        if (tx < 0) tx = 0;
        if (tx >= bm->w) tx = bm->w - 1;
        if (ty < 0) ty = 0;
        if (ty >= bm->h) ty = bm->h - 1;
        int tid = bm->tile[ty * bm->w + tx];
        if (stage == STAGE_CAVE_ICE) {

            if ((tid == 1552 && !terrain_is_smooth_ice_tile(stage, tx, ty)) || tid == 1554) {
                return ((tx + ty) & 1) ? 1561 : 1562;
            }

            if (bm->elev && (tid == 1570 || tid == 1571 || tid == 1572 ||
                             tid == 1555 || tid == 1556 || tid == 1558 || tid == 1559 || tid == 1566 || tid == 1567)) {
                int ew = bm->w + 1;
                uint8_t e0 = bm->elev[ty * ew + tx];
                if (bm->elev[ty * ew + tx + 1] == e0 &&
                    bm->elev[(ty + 1) * ew + tx] == e0 &&
                    bm->elev[(ty + 1) * ew + tx + 1] == e0) {
                    return ((tx + ty) & 1) ? 1561 : 1562;
                }
            }
        }
        return tid;
    }
    int t = terrain_tile_kind(stage, tx, ty);
    uint32_t h = ter_hash(tx, ty, stage);

    if (t == TER_CLIFF) {
        int tn = terrain_tile_kind(stage, tx, ty - 1);
        int ts = terrain_tile_kind(stage, tx, ty + 1);
        int tw = terrain_tile_kind(stage, tx - 1, ty);
        int te = terrain_tile_kind(stage, tx + 1, ty);
        int ts2 = terrain_tile_kind(stage, tx, ty + 2);
        bool is_sand = (stage == STAGE_NATIONAL_PARK ||
                        tn == TER_SAND || ts == TER_SAND || tw == TER_SAND || te == TER_SAND ||
                        tx >= 90);
        if (tn != TER_CLIFF) {
            if (is_sand) {
                if (tw != TER_CLIFF) return 28;
                if (te != TER_CLIFF) return 30;
                return 29;
            }
            if (tw != TER_CLIFF) return 112;
            if (te != TER_CLIFF) return 116;
            return ET_CLIFF_TOP;
        }
        if (ts != TER_CLIFF) {
            if (is_sand) {
                if (tw != TER_CLIFF) return 125;
                if (te != TER_CLIFF) return 127;
                return ET_CLIFF_BOT_S;
            }
            if (tw != TER_CLIFF) return 117;
            if (te != TER_CLIFF) return 119;
            return ET_CLIFF_BOT_G;
        }
        if (is_sand) {
            if (tw != TER_CLIFF) return 122;
            if (te != TER_CLIFF) return 124;
            return (ts2 != TER_CLIFF) ? ET_CLIFF_MID : ET_CLIFF_TOP;
        }
        if (tw != TER_CLIFF) return 112;
        if (te != TER_CLIFF) return 116;
        return (ts2 != TER_CLIFF) ? ET_CLIFF_MID : ET_CLIFF_TOP;
    }

    if (t == TER_LEDGE_N) {
        int sand = (stage == STAGE_NATIONAL_PARK || (stage == STAGE_ROUTE_29 && tx >= 92));
        int l = (terrain_tile_kind(stage, tx - 1, ty) == TER_LEDGE_N);
        int r = (terrain_tile_kind(stage, tx + 1, ty) == TER_LEDGE_N);
        if (sand) {
            if (!l) return ET_LEDGE_S_SL;
            if (!r) return ET_LEDGE_S_SR;
            return ET_LEDGE_S_SC;
        } else {
            if (!l) return ET_LEDGE_G_SL;
            if (!r) return ET_LEDGE_G_SR;
            return ET_LEDGE_G_CS;
        }
    }

    if (t == TER_LEDGE_S) {
        int sand = (stage == STAGE_NATIONAL_PARK || (stage == STAGE_ROUTE_29 && tx >= 92));
        int l = (terrain_tile_kind(stage, tx - 1, ty) == TER_LEDGE_S);
        int r = (terrain_tile_kind(stage, tx + 1, ty) == TER_LEDGE_S);
        if (sand) {
            if (!l) return ET_LEDGE_S_SL;
            if (!r) return ET_LEDGE_S_SR;
            return ET_LEDGE_S_SC;
        } else {
            if (!l) return ET_LEDGE_G_SL;
            if (!r) return ET_LEDGE_G_SR;
            return ET_LEDGE_G_CS;
        }
    }

    if (t == TER_LEDGE_W) {
        int sand = (stage == STAGE_NATIONAL_PARK || (stage == STAGE_ROUTE_29 && tx >= 92));
        int u = (terrain_tile_kind(stage, tx, ty - 1) == TER_LEDGE_W);
        int d = (terrain_tile_kind(stage, tx, ty + 1) == TER_LEDGE_W);
        if (sand) {
            if (!u) return ET_LEDGE_S_SL;
            if (!d) return ET_LEDGE_S_SR;
            return ET_LEDGE_S_SC;
        } else {
            if (!u) return ET_LEDGE_G_SL;
            if (!d) return ET_LEDGE_G_SR;
            return ET_LEDGE_G_CS;
        }
    }

    if (t == TER_LEDGE_E) {
        int sand = (stage == STAGE_NATIONAL_PARK || (stage == STAGE_ROUTE_29 && tx >= 92));
        int u = (terrain_tile_kind(stage, tx, ty - 1) == TER_LEDGE_E);
        int d = (terrain_tile_kind(stage, tx, ty + 1) == TER_LEDGE_E);
        if (sand) {
            if (!u) return ET_LEDGE_S_SL;
            if (!d) return ET_LEDGE_S_SR;
            return ET_LEDGE_S_SC;
        } else {
            if (!u) return ET_LEDGE_G_SL;
            if (!d) return ET_LEDGE_G_SR;
            return ET_LEDGE_G_CS;
        }
    }

    if (t == TER_DEEP) {
        int wf = (int)((anim_tick / 12u) & 3u);
        return ET_DEEP_F0 + wf;
    }

    if (t == TER_SHALLOW) {
        int tn = terrain_tile_kind(stage, tx, ty - 1);
        int ts = terrain_tile_kind(stage, tx, ty + 1);
        int tw = terrain_tile_kind(stage, tx - 1, ty);
        int te = terrain_tile_kind(stage, tx + 1, ty);
        int wn = terrain_is_water(tn), ws = terrain_is_water(ts);
        int ww = terrain_is_water(tw), we = terrain_is_water(te);
        int tnw = terrain_tile_kind(stage, tx - 1, ty - 1);
        int tne = terrain_tile_kind(stage, tx + 1, ty - 1);
        int tsw = terrain_tile_kind(stage, tx - 1, ty + 1);
        int tse = terrain_tile_kind(stage, tx + 1, ty + 1);
        int wnw = terrain_is_water(tnw), wne = terrain_is_water(tne);
        int wsw = terrain_is_water(tsw), wse = terrain_is_water(tse);


        int near_sand = (tn == TER_SAND || ts == TER_SAND || tw == TER_SAND || te == TER_SAND ||
                         tnw == TER_SAND || tne == TER_SAND || tsw == TER_SAND || tse == TER_SAND ||
                         tn == TER_PATH || ts == TER_PATH || tw == TER_PATH || te == TER_PATH ||
                         stage == STAGE_NATIONAL_PARK || (stage == STAGE_ROUTE_29 && tx >= 92));
        if (near_sand) {
            if (!wn && !ww) return ET_SW_INW;
            if (!wn && !we) return ET_SW_INE;
            if (!ws && !ww) return ET_SW_ISW;
            if (!ws && !we) return ET_SW_ISE;
            if (!wn) return ET_SW_S;
            if (!ws) return ET_SW_N;
            if (!ww) return ET_SW_E;
            if (!we) return ET_SW_W;

            if (!wse) return 64;
            if (!wsw) return 66;
            if (!wne) return 70;
            if (!wnw) return 72;
            return ET_DEEP_F0 + (int)((anim_tick / 14u) & 3u);
        }

        if (!wn && !ww) return ET_POND_NW;
        if (!wn && !we) return ET_POND_NW + 2;
        if (!ws && !ww) return ET_POND_NW + 6;
        if (!ws && !we) return ET_POND_NW + 8;
        if (!wn) return ET_POND_NW + 1;
        if (!ws) return ET_POND_NW + 7;
        if (!ww) return ET_POND_NW + 3;
        if (!we) return ET_POND_NW + 5;
        if (!wse) return 62;
        if (!wsw) return 63;
        if (!wne) return 59;
        if (!wnw) return 57;
        return ET_DEEP_F0 + (int)((anim_tick / 14u) & 3u);
    }

    if (t == TER_PATH) {
        int kn = terrain_tile_kind(stage, tx, ty - 1);
        int ks = terrain_tile_kind(stage, tx, ty + 1);
        int kw = terrain_tile_kind(stage, tx - 1, ty);
        int ke = terrain_tile_kind(stage, tx + 1, ty);
        int knw = terrain_tile_kind(stage, tx - 1, ty - 1);
        int kne = terrain_tile_kind(stage, tx + 1, ty - 1);
        int ksw = terrain_tile_kind(stage, tx - 1, ty + 1);
        int kse = terrain_tile_kind(stage, tx + 1, ty + 1);
        int n = (kn == TER_PATH || kn == TER_SAND || kn == TER_CLIFF);
        int s = (ks == TER_PATH || ks == TER_SAND || ks == TER_CLIFF);
        int w = (kw == TER_PATH || kw == TER_SAND || kw == TER_CLIFF);
        int e = (ke == TER_PATH || ke == TER_SAND || ke == TER_CLIFF);
        int nw = (knw == TER_PATH || knw == TER_SAND || knw == TER_CLIFF);
        int ne = (kne == TER_PATH || kne == TER_SAND || kne == TER_CLIFF);
        int sw = (ksw == TER_PATH || ksw == TER_SAND || ksw == TER_CLIFF);
        int se = (kse == TER_PATH || kse == TER_SAND || kse == TER_CLIFF);
        return terrain_3x3_tile(ET_PATH_NW, n, s, w, e, nw, ne, sw, se);
    }

    if (t == TER_SAND) {
        if (ter_in_sunken_desert(stage, tx, ty)) {
            int n = ter_in_sunken_desert(stage, tx, ty - 1);
            int s = ter_in_sunken_desert(stage, tx, ty + 1);
            int w = ter_in_sunken_desert(stage, tx - 1, ty);
            int e = ter_in_sunken_desert(stage, tx + 1, ty);
            int cell = terrain_3x3_tile(ET_DUNE_NW, n, s, w, e, 1, 1, 1, 1);
            return cell;
        }
        int near_water = terrain_is_water(terrain_tile_kind(stage, tx - 1, ty)) ||
                         terrain_is_water(terrain_tile_kind(stage, tx + 1, ty)) ||
                         terrain_is_water(terrain_tile_kind(stage, tx, ty - 1)) ||
                         terrain_is_water(terrain_tile_kind(stage, tx, ty + 1));
        if (stage == STAGE_NATIONAL_PARK || near_water) {
            return ET_SAND_GOLD;
        }
        return ET_PATH_C;
    }

    if (t == TER_FOREST) {
        int r = (int)(h % 100);
        if (r < 8) return ET_TALL_GRASS;
        if (r < 12) return ET_FOREST_A;
        if (r < 14) return ET_FLOWER;
        return ET_GRASS_A;
    }


    if (stage == STAGE_ROUTE_29) {
        uint32_t ph = ter_hash(tx / 4, ty / 4, stage + 17);
        if ((ph % 100) < 28 && !ter_in_rect(tx, ty, 38, 0, 46, 199) && !ter_in_rect(tx, ty, 40, 89, 93, 97)) {
            return ET_TALL_GRASS;
        }
    }
    int r = (int)(h % 100);
    if (r < 3) return ET_FLOWER;
    if (r < 12) return ET_TALL_GRASS;
    return ET_GRASS_A;
}

#endif
