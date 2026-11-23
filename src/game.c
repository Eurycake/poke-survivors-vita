#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#ifdef __vita__
#include <psp2/io/stat.h>
#endif
#include "game.h"
#include "renderer.h"
#include "audio.h"
#include "save_system.h"
#include "terrain.h"
#include "type_chart.h"
#include "pokedex_descriptions.h"

GameState g_game;

void player_init(Player *p, int starter_idx);
void player_update(Player *p, float dt, const InputState *input);
void player_recalc_stats(Player *p);

void enemies_init(void);
void enemies_spawn_wave(float game_time, const Player *player);
void enemies_spawn_boss(int boss_cat_idx, const Player *player);
void enemies_update(float dt, Player *player);
void enemies_preload_stage_start(int stage_id, int mode, int first_boss_dex);

void moves_init(void);
void moves_update(float dt, Player *player);
void moves_preload_move(int move_id);
void moves_preload_assets(void);

int game_pick_next_boss(char *out_name, size_t max_len) {
    static const int BOSS_POOL[] = {
        377, 378, 379, 95, 130, 6, 94, 144, 145, 146, 384, 150,
        149, 151, 243, 244, 245, 248, 249, 250, 373, 376,
        380, 381, 382, 383, 386, 445, 483, 484, 487, 491
    };
    static const char *BOSS_NAMES[] = {
        "Regirock", "Regice", "Registeel", "Onix", "Gyarados", "Charizard",
        "Gengar", "Articuno", "Zapdos", "Moltres", "Rayquaza", "Mewtwo",
        "Dragonite", "Mew", "Raikou", "Entei", "Suicune", "Tyranitar",
        "Lugia", "Ho-Oh", "Salamence", "Metagross",
        "Latias", "Latios", "Kyogre", "Groudon", "Deoxys", "Garchomp",
        "Dialga", "Palkia", "Giratina", "Darkrai"
    };
    int pool_n = (int)(sizeof(BOSS_POOL) / sizeof(BOSS_POOL[0]));
    int avail[32];
    int n_avail = 0;
    for (int i = 0; i < pool_n; i++) {
        if ((g_game.used_boss_mask & (1u << i)) == 0 && i != g_game.last_boss_idx) {
            avail[n_avail++] = i;
        }
    }
    if (n_avail == 0) {
        g_game.used_boss_mask = 0;
        for (int i = 0; i < pool_n; i++) {
            if (i != g_game.last_boss_idx) avail[n_avail++] = i;
        }
    }
    int b_idx = avail[rand() % n_avail];
    g_game.used_boss_mask |= (1u << b_idx);
    g_game.last_boss_idx = b_idx;
    if (out_name && max_len > 0) snprintf(out_name, max_len, "%s", BOSS_NAMES[b_idx]);
    return BOSS_POOL[b_idx];
}

static const char *TAGLINES[] = {
    "Survive one more minute!",
    "Now with even more grass to cut!",
    "Recommended by 9 out of 10 Pikachu.",
    "A Charmander never sleeps: it only blinks.",
    "Kecleon accepts coins, not compliments.",
    "Watch out: Meowth might steal your lunch."
};
#define TOTAL_TAGLINES 6

static inline bool is_tall_prop(int type) {
    return (type <= PROP_ROCK_B) || type == PROP_ROCK_ICE || type == PROP_CHEST || type == PROP_BERRY_TREE ||
           type == PROP_CACTUS_A || type == PROP_CACTUS_B;
}

static inline void render_draw_box_text_centered(float bx, float by, float bw, float bh, float scale, uint32_t color, const char *text) {
    if (!text) return;
    int tw = render_get_text_width(scale, text);
    float tx = bx + (bw - (float)tw) * 0.5f;
    float ty = by + (bh - 18.0f * scale) * 0.5f;
    render_draw_text(tx, ty, scale, color, text);
}

static inline void render_draw_box_text_left(float bx, float by, float bw, float bh, float pad_x, float scale, uint32_t color, const char *text) {
    if (!text) return;
    (void)bw;
    float ty = by + (bh - 18.0f * scale) * 0.5f;
    render_draw_text(bx + pad_x, ty, scale, color, text);
}

#define UI_MODERN_BG          RGBA8_C(10, 16, 28, 140)
#define UI_BG_FLAT            RGBA8_C(216, 224, 236, 212)
#define UI_MODERN_BORDER      RGBA8_C(32, 38, 46, 255)
#define UI_MODERN_SHADOW      RGBA8_C(0, 0, 0, 40)
#define UI_MODERN_WHITE       RGBA8_C(255, 255, 255, 255)
#define UI_MODERN_TEXT_DARK   RGBA8_C(28, 34, 42, 255)
#define UI_MODERN_TEXT_MUTED  RGBA8_C(98, 108, 122, 255)

#define UI_BTN_GREEN          RGBA8_C(57, 216, 70, 255)
#define UI_BTN_BLUE           RGBA8_C(58, 160, 232, 255)
#define UI_BTN_GREY           RGBA8_C(142, 148, 153, 255)
#define UI_BANNER_YELLOW      RGBA8_C(255, 202, 40, 255)
#define UI_BANNER_CORAL       RGBA8_C(255, 122, 102, 255)
#define UI_TAB_INACTIVE       RGBA8_C(154, 161, 169, 255)
#define UI_CARD_SEL_ORANGE    RGBA8_C(245, 158, 11, 255)

#define UI_WELL_CYAN          RGBA8_C(191, 234, 232, 255)
#define UI_WELL_MINT          RGBA8_C(220, 252, 231, 255)
#define UI_WELL_BLUE          RGBA8_C(226, 238, 249, 255)
#define UI_WELL_YELLOW        RGBA8_C(255, 248, 214, 255)
#define UI_WELL_CREAM         RGBA8_C(253, 246, 236, 255)
#define UI_WELL_GREY          RGBA8_C(230, 223, 213, 255)

#define GBA_CHARCOAL      UI_MODERN_BORDER
#define GBA_MINT          RGBA8_C(115, 205, 164, 255)
#define GBA_EMERALD       UI_BTN_GREEN
#define GBA_EMERALD_FRAME UI_MODERN_BORDER
#define GBA_GOLD_RING     UI_CARD_SEL_ORANGE
#define GBA_GOLD_HI       RGBA8_C(250, 218, 92, 255)
#define GBA_GOLD_SHAD     RGBA8_C(158, 108, 16, 255)
#define GBA_AMBER_HEADER  UI_BANNER_YELLOW
#define GBA_AMBER_HI      RGBA8_C(235, 178, 68, 255)
#define GBA_AMBER_SHADE   RGBA8_C(168, 116, 28, 255)
#define GBA_DEEP_TEAL     UI_WELL_CYAN
#define GBA_DEEP_TEAL_DK  RGBA8_C(150, 215, 215, 255)
#define GBA_CREAM_FILL    UI_MODERN_WHITE
#define GBA_CREAM_ALT     UI_WELL_BLUE
#define GBA_CREAM_SEL     UI_BTN_GREEN
#define GBA_CURSOR_BLACK  UI_MODERN_BORDER
#define GBA_DARK_SH       UI_MODERN_BORDER
#define GBA_YELLOW        RGBA8_C(255, 230, 57, 255)
#define GBA_INK           UI_MODERN_TEXT_DARK
#define GBA_INK_SOFT      UI_MODERN_TEXT_MUTED
#define GBA_PAPER0        UI_MODERN_WHITE
#define GBA_PAPER1        UI_WELL_BLUE
#define GBA_SHAD_LIGHT    RGBA8_C(16, 28, 34, 255)
#define GBA_SHAD_DARK     RGBA8_C(16, 24, 28, 220)

enum { UI_GBA_WIN_DEFAULT = 0, UI_GBA_WIN_DARK = 1, UI_GBA_WIN_GOLD = 2 };

#define UI_SOFT_BORDER RGBA8_C(204, 213, 226, 255)
static uint32_t ui_darken(uint32_t c, float k) {
    return RGBA8_C((int)((c & 255u) * k), (int)(((c >> 8) & 255u) * k), (int)(((c >> 16) & 255u) * k), (c >> 24) & 255u);
}
static uint32_t ui_mix_white(uint32_t c, float t) {
    int r = (int)(c & 255u), g = (int)((c >> 8) & 255u), b = (int)((c >> 16) & 255u);
    return RGBA8_C((int)(r + (255 - r) * t), (int)(g + (255 - g) * t), (int)(b + (255 - b) * t), 255);
}
static uint32_t ui_edge_for(uint32_t bg, uint32_t b) {
    if (b != UI_MODERN_BORDER) return b;
    if (bg == UI_MODERN_WHITE) return UI_SOFT_BORDER;
    if (bg == UI_WELL_CYAN || bg == UI_WELL_BLUE || bg == UI_WELL_CREAM || bg == UI_WELL_MINT || bg == UI_WELL_YELLOW) return ui_darken(bg, 0.84f);
    return b;
}

static float ui_mid_off(int mode, float scale) {
    float s = scale * 0.70f;
    if (mode == 0) return 20.0f * s - 0.36f * 21.0f * s;
    float px = ((mode == 1) ? 28.0f : 32.0f) * s, n = px / 18.0f, r = floorf(n + 0.5f);
    float em = (r >= 1.0f && fabsf(n - r) < 0.18f) ? r * 18.0f : px;
    em = floorf(em * 0.5f + 0.5f) * 2.0f;
    if (em < 8.0f) em = 8.0f;
    return 21.0f * s - 0.25f * em;
}
static int ui_text_w(int mode, float scale, const char *t) {
    return mode == 0 ? render_get_text_width(scale, t) : (mode == 1 ? render_get_text_width_8bit(scale, t) : render_get_text_width_pix(scale, t));
}
static void ui_text_mid(int mode, float x, float cy, float scale, uint32_t col, const char *t) {
    if (!t || !t[0]) return;
    float y = cy - ui_mid_off(mode, scale);
    if (mode == 0) render_draw_text(x, y, scale, col, t);
    else if (mode == 1) render_draw_text_8bit(x, y, scale, col, t);
    else render_draw_text_pix(x, y, scale, col, t);
}
static void ui_text_center(int mode, float x, float y, float w, float h, float scale, uint32_t col, const char *t) {
    if (!t || !t[0]) return;
    ui_text_mid(mode, x + (w - (float)ui_text_w(mode, scale, t)) * 0.5f, y + h * 0.5f, scale, col, t);
}
static void ui_warn_icon(float cx, float cy, float sz, uint32_t col) {
    for (float yy = -sz; yy <= sz * 0.8f; yy += 1.0f) {
        float t = (yy + sz) / (sz * 1.8f);
        render_draw_rect(cx - sz * t, cy + yy, sz * t * 2.0f, 1.0f, RGBA8_C(255, 205, 40, 255));
    }
    render_draw_line(cx, cy - sz, cx - sz, cy + sz * 0.8f, col, 1.6f);
    render_draw_line(cx, cy - sz, cx + sz, cy + sz * 0.8f, col, 1.6f);
    render_draw_line(cx - sz, cy + sz * 0.8f, cx + sz, cy + sz * 0.8f, col, 1.6f);
    render_draw_rect(cx - 1.0f, cy - sz * 0.35f, 2.0f, sz * 0.75f, col);
    render_draw_rect(cx - 1.0f, cy + sz * 0.55f, 2.0f, 2.0f, col);
}

static void ui_draw_rounded_rect(float x, float y, float w, float h, float r, uint32_t fill) {
    render_draw_rounded_rect(x, y, w, h, r, fill);
}

static void ui_draw_rounded_box(float x, float y, float w, float h, float r, uint32_t fill, uint32_t border, float th) {
    if (th <= 0.0f) { ui_draw_rounded_rect(x, y, w, h, r, fill); return; }
    ui_draw_rounded_rect(x, y, w, h, r, border);
    ui_draw_rounded_rect(x + th, y + th, w - 2.0f * th, h - 2.0f * th, fmaxf(0.0f, r - th), fill);
}

static void ui_draw_card(float x, float y, float w, float h, uint32_t bg_col, uint32_t border_col, float border_w, float r) {
    border_col = ui_edge_for(bg_col, border_col);

    ui_draw_rounded_rect(x, y + 3.0f, w, h, r, RGBA8_C(40, 55, 80, 42));
    if (bg_col == UI_MODERN_WHITE && border_col == UI_SOFT_BORDER) { ui_draw_rounded_rect(x, y, w, h, r, bg_col); return; }
    ui_draw_rounded_box(x, y, w, h, r, bg_col, border_col, border_w);
}

static void ui_draw_pill(float x, float y, float w, float h, uint32_t bg_col, uint32_t border_col) {
    ui_draw_card(x, y, w, h, bg_col, border_col, 2.0f, h * 0.5f);
}

static void ui_pix_notch(float x, float y, float w, float h, uint32_t c);
static void ui_pix_corners(float x, float y, float w, float h, uint32_t c);
static void ui_label_outlined(float x, float cy, float scale, const char *t);
static void ui_draw_badge(float x, float y, float w, float h, uint32_t bg_col, uint32_t border_col, const char *text, bool has_dots) {
    (void)has_dots;
    ui_pix_notch(x, y, w, h, border_col);
    render_draw_rect(x + 3.0f, y + 3.0f, w - 6.0f, h - 6.0f, bg_col);
    ui_pix_corners(x + 3.0f, y + 3.0f, w - 6.0f, h - 6.0f, border_col);
    if (text && text[0]) {
        float tw = (float)ui_text_w(1, 1.25f, text);
        ui_label_outlined(floorf(x + (w - tw) * 0.5f + 0.5f), floorf(y + h * 0.5f + 0.5f), 1.25f, text);
    }
}

static void ui_ellipse(float cx, float cy, float rx, float ry, uint32_t col) {
    for (int i = -(int)ry; i <= (int)ry; i++) {
        float t = (float)i / ry;
        float hw = rx * sqrtf(fmaxf(0.0f, 1.0f - t * t));
        render_draw_rect(cx - hw, cy + (float)i, hw * 2.0f, 1.0f, col);
    }
}

static void ui_pix_notch(float x, float y, float w, float h, uint32_t c) {
    render_draw_rect(x + 6.0f, y, w - 12.0f, h, c);
    render_draw_rect(x + 3.0f, y + 3.0f, w - 6.0f, h - 6.0f, c);
    render_draw_rect(x, y + 6.0f, w, h - 12.0f, c);
}
static void ui_pix_corners(float x, float y, float w, float h, uint32_t c) {
    render_draw_rect(x, y, 3.0f, 3.0f, c);
    render_draw_rect(x + w - 3.0f, y, 3.0f, 3.0f, c);
    render_draw_rect(x, y + h - 3.0f, 3.0f, 3.0f, c);
    render_draw_rect(x + w - 3.0f, y + h - 3.0f, 3.0f, 3.0f, c);
}
static void ui_bevel_btn(float x, float y, float w, float h, uint32_t fill, uint32_t hi, uint32_t lip) {
    const uint32_t K = RGBA8_C(0, 0, 0, 255);
    ui_pix_notch(x, y, w, h, K);
    float ix = x + 3.0f, iy = y + 3.0f, iw = w - 6.0f, ih = h - 7.0f;
    render_draw_rect(ix, iy, iw, ih, fill);
    render_draw_rect(ix, iy, iw, 4.0f, hi);
    render_draw_rect(ix, iy, 4.0f, ih - 5.0f, hi);
    render_draw_rect(ix, iy + ih - 5.0f, iw, 5.0f, lip);
    ui_pix_corners(ix, iy, iw, ih, K);
}
#define UI_GREEN_FILL RGBA8_C(80, 205, 88, 255)
#define UI_GREEN_HI   RGBA8_C(150, 236, 140, 255)
#define UI_GREEN_LIP  RGBA8_C(38, 150, 48, 255)
#define UI_BLUE_FILL  RGBA8_C(62, 134, 214, 255)
#define UI_BLUE_HI    RGBA8_C(120, 180, 242, 255)
#define UI_BLUE_LIP   RGBA8_C(34, 92, 170, 255)
#define UI_GREY_FILL  RGBA8_C(150, 150, 156, 255)
#define UI_GREY_HI    RGBA8_C(196, 196, 200, 255)
#define UI_GREY_LIP   RGBA8_C(102, 102, 108, 255)

static void ui_pix_chevron(float x, float cy) {
    static const int sp[9][2] = {{0,1},{1,2},{2,3},{3,4},{4,5},{3,4},{2,3},{1,2},{0,1}};
    const float u = 2.0f, y0 = cy - 5.5f * u;
    const uint32_t K = RGBA8_C(0, 0, 0, 255);
    for (int r = -1; r <= 9; r++) for (int c = -1; c <= 6; c++) {
        bool edge = false;
        for (int dr = -1; dr <= 1 && !edge; dr++) for (int dc = -1; dc <= 1; dc++) {
            int rr = r + dr, cc = c + dc;
            if (rr >= 0 && rr < 9 && cc >= sp[rr][0] && cc <= sp[rr][1]) { edge = true; break; }
        }
        if (edge) render_draw_rect(x + (float)(c + 1) * u, y0 + (float)(r + 1) * u, u, u, K);
    }
    for (int r = 0; r < 9; r++) for (int c = sp[r][0]; c <= sp[r][1]; c++) {
        uint32_t col = (c == sp[r][0]) ? RGBA8_C(255, 248, 140, 255) : RGBA8_C(246, 224, 52, 255);
        render_draw_rect(x + (float)(c + 1) * u, y0 + (float)(r + 1) * u, u, u, col);
    }
}

static void ui_label_outlined(float x, float cy, float scale, const char *t) {
    if (!t || !t[0]) return;
    const uint32_t K = RGBA8_C(0, 0, 0, 255);
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        if (dx == 0 && dy == 0) continue;
        ui_text_mid(1, x + (float)dx, cy + (float)dy, scale, K, t);
    }
    ui_text_mid(1, x, cy, scale, 0xFFFFFFFF, t);
}

static void ui_label_outlined_icon(float bx, float by, float bw, float bh, float scale, const char *left, const char *icon, const char *right) {
    float cy = floorf(by + (bh - 4.0f) * 0.5f + 0.5f);
    float wl = (left && left[0]) ? (float)ui_text_w(1, scale, left) : 0.0f;
    float wi = (icon && icon[0]) ? (float)ui_text_w(0, scale * 1.05f, icon) : 0.0f;
    float wr = (right && right[0]) ? (float)ui_text_w(1, scale, right) : 0.0f;
    float g1 = (wl > 0.0f && wi > 0.0f) ? 6.0f : 0.0f;
    float g2 = (wi > 0.0f && wr > 0.0f) ? 6.0f : 0.0f;
    float x = floorf(bx + (bw - (wl + g1 + wi + g2 + wr)) * 0.5f + 0.5f);
    if (wl > 0.0f) { ui_label_outlined(x, cy, scale, left); x += wl + g1; }
    if (wi > 0.0f) { ui_text_mid(0, x, cy, scale * 1.05f, 0xFFFFFFFF, icon); x += wi + g2; }
    if (wr > 0.0f) { ui_label_outlined(x, cy, scale, right); }
}

static void ui_draw_crossed_wrenches(float cx, float cy) {
    static const char *const grid[13] = {
        "..KK.....KK..",
        ".KGGK...KGGK.",
        "KGGGK...KGGGK",
        "KGGK.....KGGK",
        ".KGGK...KGGK.",
        "..KGGK.KGGK..",
        "...KGGGGGK...",
        "..KGGK.KGGK..",
        ".KGGK...KGGK.",
        "KGGK.....KGGK",
        "KGGGK...KGGGK",
        ".KGGK...KGGK.",
        "..KK.....KK.."
    };
    const float u = 2.0f;
    float x0 = cx - 6.5f * u, y0 = cy - 6.5f * u;
    const uint32_t K = RGBA8_C(28, 32, 38, 255);
    const uint32_t G = RGBA8_C(176, 182, 190, 255);
    for (int r = 0; r < 13; r++) {
        for (int c = 0; c < 13; c++) {
            char ch = grid[r][c];
            if (ch == '.') continue;
            render_draw_rect(x0 + (float)c * u, y0 + (float)r * u, u, u, (ch == 'K') ? K : G);
        }
    }
}

static void ui_pix_box(float x, float y, float w, float h, uint32_t fill, uint32_t border, float th) {
    ui_pix_notch(x, y, w, h, border);
    ui_pix_notch(x + th, y + th, w - 2.0f * th, h - 2.0f * th, fill);
}

static void ui_draw_button(float x, float y, float w, float h, const char *text, bool is_sel, int btn_type, const char *right_icon) {
    if (is_sel)               ui_bevel_btn(x, y, w, h, UI_GREEN_FILL, UI_GREEN_HI, UI_GREEN_LIP);
    else if (btn_type == 0)   ui_bevel_btn(x, y, w, h, UI_BLUE_FILL, UI_BLUE_HI, UI_BLUE_LIP);
    else                      ui_bevel_btn(x, y, w, h, UI_GREY_FILL, UI_GREY_HI, UI_GREY_LIP);
    float cyb = floorf(y + (h - 4.0f) * 0.5f + 0.5f);
    float tx = x + 18.0f;
    if (is_sel) {
        ui_text_mid(0, x + 11.0f, cyb, 1.55f, 0xFFFFFFFF, "[Arrow]");
        tx = x + 38.0f;
    }
    float ts = 1.44f;
    float max_tw = w - (tx - x) - ((right_icon && right_icon[0]) ? 52.0f : 16.0f);
    while (ts > 1.0f && (float)ui_text_w(1, ts, text) > max_tw) ts -= 0.04f;
    if (!is_sel && btn_type == 1) {
        ui_text_mid(1, tx, cyb, ts, UI_MODERN_TEXT_DARK, text);
    } else {
        ui_label_outlined(tx, cyb, ts, text);
    }
    if (right_icon && right_icon[0]) {
        const char *ic = (strcmp(right_icon, "[Wrench]") == 0) ? "[SettingsX]" : right_icon;
        ui_text_mid(0, x + w - 42.0f, cyb, 2.35f, 0xFFFFFFFF, ic);
    }
}

static void ui_draw_top_pills(const char *left_title, int coins, int owned_mon, int total_mon, int tickets) {
    char b[3][40];
    snprintf(b[0], 40, "%d", coins);
    snprintf(b[1], 40, "%d/%d", owned_mon, total_mon);
    snprintf(b[2], 40, "%d TKT", tickets);
    static const char *const ic[3] = { "[Coin]", "[Ball]", "[Ticket]" };
    const float py = 10.0f, ph = 36.0f, cy = py + ph * 0.5f, ts = 1.3f;
    if (left_title && left_title[0]) {
        float pw = 41.0f + (float)ui_text_w(2, ts, left_title) + 14.0f;
        ui_draw_pill(16.0f, py, pw, ph, UI_MODERN_WHITE, UI_MODERN_BORDER);
        ui_text_mid(0, 25.0f, cy, 1.75f, 0xFFFFFFFF, "[Ball]");
        ui_text_mid(2, 57.0f, cy, ts, UI_MODERN_TEXT_DARK, left_title);
    }
    float x = SCREEN_W - 17.0f;
    for (int i = 2; i >= 0; i--) {
        float pw = 37.0f + (float)ui_text_w(2, ts, b[i]) + 13.0f;
        x -= pw;
        ui_draw_pill(x, py, pw, ph, UI_MODERN_WHITE, UI_MODERN_BORDER);
        ui_text_mid(0, x + 9.0f, cy, 1.75f, 0xFFFFFFFF, ic[i]);
        ui_text_mid(2, x + 37.0f, cy, ts, UI_MODERN_TEXT_DARK, b[i]);
        x -= 13.0f;
    }
}

static void ui_draw_footer_pill_at(const char *text, float py, float ph) {
    if (!text || !text[0]) return;
    float tw = (float)render_get_text_width(1.32f, text);
    float pw = tw + 54.0f;
    float px = (SCREEN_W - pw) * 0.5f;
    ui_draw_pill(px, py, pw, ph, UI_MODERN_WHITE, UI_MODERN_BORDER);
    ui_text_center(0, px, py, pw, ph, 1.32f, UI_MODERN_TEXT_DARK, text);
}
static void ui_draw_footer_pill(const char *text) { ui_draw_footer_pill_at(text, 498.0f, 33.0f); }

static void ui_draw_black_triangle_cursor(float x, float y, float w, float h) {
    (void)x; (void)y; (void)w; (void)h;
}

static void ui_gba_text(float x, float y, float scale, uint32_t ink, uint32_t shadow, const char *text) {
    (void)shadow;
    if (!text || !text[0]) return;
    render_draw_text(x, y, scale, ink, text);
}
static void ui_gba_text_mid(float x, float y, float h, float scale, uint32_t ink, uint32_t shadow, const char *text) {
    if (!text || !text[0]) return;
    float ty = y + (h - 18.0f * scale) * 0.5f;
    ui_gba_text(x, ty, scale, ink, shadow, text);
}
static void ui_gba_text_centred(float x, float y, float w, float h, float scale, uint32_t ink, uint32_t shadow, const char *text) {
    if (!text || !text[0]) return;
    int tw = render_get_text_width(scale, text);
    float tx = x + (w - (float)tw) * 0.5f;
    float ty = y + (h - 18.0f * scale) * 0.5f;
    ui_gba_text(tx, ty, scale, ink, shadow, text);
}

static void ui_gba_win(float x, float y, float w, float h, int kind) {
    uint32_t bcol = (kind == UI_GBA_WIN_GOLD) ? UI_CARD_SEL_ORANGE : UI_MODERN_BORDER;
    float bw = (kind == UI_GBA_WIN_GOLD) ? 3.0f : 2.0f;
    ui_draw_card(x, y, w, h, UI_MODERN_WHITE, bcol, bw, 14.0f);
}

static void ui_gba_msg(float x, float y, float w, float h, bool redArrow) {
    ui_draw_pill(x, y, w, h, UI_MODERN_WHITE, UI_MODERN_BORDER);
    if (redArrow) {
        ui_text_mid(0, x + w - 38.0f, y + h * 0.5f, 1.30f, 0xFFFFFFFF, "[Down]");
    }
}

static void ui_gba_bevel(float x, float y, float w, float h, uint32_t edge, uint32_t fill, uint32_t hi, uint32_t shade, float shadow) {
    (void)hi; (void)shade; (void)shadow;
    ui_draw_card(x, y, w, h, fill, edge, 2.0f, 10.0f);
}

static void ui_gba_ring_box(float x, float y, float w, float h, uint32_t outerRing, uint32_t innerRing, uint32_t fill, uint32_t innerHi) {
    (void)innerRing; (void)innerHi;
    ui_draw_card(x, y, w, h, fill, outerRing, 3.0f, 10.0f);
}

static void ui_gba_row(float x, float y, float w, float h, bool sel, int idx) {
    (void)idx;
    uint32_t fill = sel ? UI_BTN_GREEN : UI_WELL_BLUE;
    uint32_t border = sel ? UI_MODERN_BORDER : RGBA8_C(180, 195, 210, 255);
    ui_draw_card(x, y, w, h - 2.0f, fill, border, sel ? 2.5f : 1.5f, 10.0f);
    if (sel) {
        render_draw_text(x + 12.0f, y + (h - 20.0f) * 0.5f, 1.20f, 0xFFFFFFFF, "[Arrow]");
    }
}

static void ui_gba_section_strip(float x, float y, float w, float h, const char *label, bool active) {
    ui_draw_badge(x, y, w, h, active ? UI_BTN_GREEN : UI_TAB_INACTIVE, UI_MODERN_BORDER, label, true);
}

static void ui_gba_hp_bar(float x, float y, float w, int hp, int max_hp) {
    float h = 15.0f;
    if (max_hp <= 0) max_hp = 1;
    float pct = (float)hp / (float)max_hp;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    ui_draw_pill(x, y, w, h, RGBA8_C(32, 42, 48, 255), UI_MODERN_BORDER);
    render_draw_text_8bit(x + 5.0f, y + 2.0f, 0.75f, 0xFFFFFFFF, "HP");
    float tx = x + 24.0f, ty = y + 2.5f, tw = w - 28.0f, th = h - 5.0f;
    render_draw_rect(tx, ty, tw, th, RGBA8_C(20, 26, 32, 255));
    float fw = tw * pct;
    if (fw > 0.0f) {
        uint32_t hp_col = (pct > 0.5f) ? RGBA8_C(60, 208, 112, 255) : ((pct > 0.2f) ? RGBA8_C(255, 205, 55, 255) : RGBA8_C(235, 65, 65, 255));
        render_draw_rect(tx, ty, fw, th, hp_col);
    }
}

static void ui_gba_exp_bar(float x, float y, float w, int cur_exp, int max_exp) {
    if (max_exp <= 0) max_exp = 1;
    float pct = (float)cur_exp / (float)max_exp;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    float h = 15.0f;
    ui_gba_bevel(x, y, w, h, RGBA8_C(24, 32, 32, 255), RGBA8_C(62, 68, 82, 255), 0, 0, 0.0f);
    render_draw_rect(x + 2.0f, y + 2.0f, 32.0f, h - 4.0f, RGBA8_C(36, 40, 48, 255));
    ui_gba_text_mid(x + 5.0f, y, h, 0.88f, GBA_YELLOW, GBA_SHAD_DARK, "EXP");
    float tx = x + 36.0f, ty = y + 3.5f, tw = w - 40.0f, th = h - 7.0f;
    render_draw_rect(tx - 1.0f, ty - 1.0f, tw + 2.0f, th + 2.0f, RGBA8_C(16, 20, 24, 255));
    render_draw_rect(tx, ty, tw, th, RGBA8_C(24, 32, 42, 255));
    float fw = tw * pct;
    if (fw > 0.0f) {
        render_draw_rect(tx, ty, fw, th, RGBA8_C(56, 176, 224, 255));
        render_draw_rect(tx, ty, fw, 2.5f, RGBA8_C(170, 244, 255, 255));
    }
    for (float sx = tx + 14.0f; sx < tx + tw; sx += 14.0f)
        render_draw_rect(sx, ty, 2.0f, th, RGBA8_C(24, 32, 42, 210));
}

static void ui_gba_screen_chrome(const char *secTitle, const char *footerHint) {
    render_draw_rect(0.0f, 0.0f, SCREEN_W, SCREEN_H, UI_BG_FLAT);

    int owned_cnt = 0;
    for (int i = 0; i < TOTAL_CATALOG_POKEMON; i++) if (save_system_is_owned(i)) owned_cnt++;

    ui_draw_top_pills(secTitle, g_save.coins, owned_cnt, TOTAL_CATALOG_POKEMON, g_save.tickets);
    ui_draw_footer_pill(footerHint);
}

typedef struct {
    const char *name;
    const char *subtitle;
    const char *biome;
    const char *perk;
    const char *encounters;
} StageInfo;

float g_world_w = 6000.0f, g_world_h = 6000.0f;

static const StageInfo STAGE_DEFS[TOTAL_STAGES] = {
    {
        "ROUTE 111 SOUTH",
        "Emerald Meadow & Route",
        "Grassland, Lakes & Border Trails",
        "Meadow, Bug, Flying & Electric",
        "Wurmple, Zigzagoon, Ralts, Electrike, Scyther, Sceptile"
    },
    {
        "ROUTE 111 DESERT",
        "Hoenn Desert & Badlands",
        "Ancient Ruins, Cacti & Dunes",
        "Ground, Rock, Steel & Dragon",
        "Sandshrew, Trapinch, Cacnea, Hippopotas, Flygon, Garchomp"
    },
    {
        "ILEX & PETALBURG FOREST",
        "Ancient Mystic Woods",
        "Deep Shaded Moss Canopy",
        "Ghost, Dark, Bug & Grass",
        "Spinarak, Duskull, Phantump, Zorua, Gengar, Trevenant"
    },
    {
        "HOENN TOWNS",
        "All Hoenn Towns Merged",
        "Wide Avenues & Open Plazas",
        "Normal, Electric, Psychic & Fairy",
        "Pikachu, Eevee, Meowth, Skitty, Growlithe, Lucario, Snorlax"
    },
    {
        "GRANITE CAVERNS",
        "Waterfall & Golden Trail Caves",
        "Underground River & Bridges",
        "Rock, Steel, Fighting & Dark",
        "Zubat, Geodude, Aron, Sableye, Larvitar, Aggron, Steelix"
    },
    {
        "MT. CHIMNEY CAVERNS",
        "Volcanic Magma Caves",
        "Scorched Ridges & Lava Rock",
        "Fire, Ground, Rock & Fighting",
        "Numel, Slugma, Torkoal, Houndour, Magmar, Camerupt, Magmortar"
    },
    {
        "SHOAL ICE CAVERNS",
        "Frozen Hoenn Ice Grotto",
        "Glacial Floors & Frost Plateaus",
        "Ice, Water & Dark",
        "Spheal, Snorunt, Swinub, Sneasel, Lapras, Weavile, Mamoswine"
    }
};

static void prop_add(int *p, int type, float x, float y) {
    if (*p >= MAX_PROPS) return;
    WorldProp *w = &g_game.props[*p];
    w->active = true; w->x = x; w->y = y; w->type = type;
    (*p)++;
}

static bool prop_has_nearby_blocker(int count, float x, float y, float min_d) {
    float md2 = min_d * min_d;
    for (int i = 0; i < count; i++) {
        const WorldProp *w = &g_game.props[i];
        if (!w->active || !is_tall_prop(w->type)) continue;
        float dx = w->x - x, dy = w->y - y;
        if (dx * dx + dy * dy < md2) return true;
    }
    return false;
}

static bool prop_has_nearby_any(int count, float x, float y, float min_d) {
    float md2 = min_d * min_d;
    for (int i = 0; i < count; i++) {
        const WorldProp *w = &g_game.props[i];
        if (!w->active) continue;
        float dx = w->x - x, dy = w->y - y;
        if (dx * dx + dy * dy < md2) return true;
    }
    return false;
}

static bool prop_near_blocked_or_road(int stage, float x, float y, bool check_road) {
    for (int oy = -2; oy <= 2; oy++) {
        for (int ox = -2; ox <= 2; ox++) {
            int tk = terrain_at(stage, x + (float)ox * 24.0f, y + (float)oy * 24.0f);
            if (terrain_is_ledge(tk) || tk == TER_CLIFF || terrain_is_water(tk)) return true;
            if (check_road && tk == TER_PATH) return true;
        }
    }
    return false;
}

static void spawn_world_props_for_stage(int stage_id) {
    uint32_t lcg = 12345u + (uint32_t)stage_id * 100u;
    #define PROP_RAND() ((int)((lcg = lcg * 1664525u + 1013904223u) >> 16) & 0x7FFF)
    memset(g_game.props, 0, sizeof(g_game.props));
    int p = 0;
    if (stage_id >= BAKED_FIRST_STAGE) {

        const BakedMap *bm = &g_baked_maps[stage_id - BAKED_FIRST_STAGE];
        g_world_w = (float)(bm->w * TER_TILE);
        g_world_h = (float)(bm->h * TER_TILE);
        if (stage_id <= STAGE_TOWN_HOENN) {
            int att = 0;
            while (p < 90 && att++ < 30000) {
                int tx = 8 + PROP_RAND() % (bm->w - 16), ty = 8 + PROP_RAND() % (bm->h - 16);
                int ok = 1;
                for (int oy = -2; oy <= 2 && ok; oy++)
                    for (int ox = -2; ox <= 2; ox++)
                        if (terrain_tile_kind(stage_id, tx + ox, ty + oy) != TER_GRASS) { ok = 0; break; }
                float x = ((float)tx + 0.5f) * (float)TER_TILE, y = ((float)ty + 0.5f) * (float)TER_TILE;
                if (ok && !prop_has_nearby_blocker(p, x, y, 300.0f)) prop_add(&p, PROP_BERRY_TREE, x, y);
            }
        } else {

            int primary_rock = (stage_id == STAGE_CAVE_ICE) ? PROP_ROCK_ICE : PROP_ROCK_B;
            float sx_c = ((float)bm->start_tx + 0.5f) * (float)TER_TILE;
            float sy_c = ((float)bm->start_ty + 0.5f) * (float)TER_TILE;
            int ew = bm->w + 1;

            uint16_t plain_a = 0, plain_b = 0;
            if (stage_id == STAGE_CAVE_GRANITE) {
                uint16_t tids[32]; int tcnt[32], nt = 0;
                for (int cy_ = 2; cy_ < bm->h - 2; cy_++) {
                    for (int cx_ = 2; cx_ < bm->w - 2; cx_++) {
                        if (bm->kind[cy_ * bm->w + cx_] != TER_PATH) continue;
                        uint16_t tid = bm->tile[cy_ * bm->w + cx_];
                        int s = -1;
                        for (int q = 0; q < nt; q++) if (tids[q] == tid) { s = q; break; }
                        if (s >= 0) tcnt[s]++;
                        else if (nt < 32) { tids[nt] = tid; tcnt[nt] = 1; nt++; }
                    }
                }
                int b1 = -1, b2 = -1;
                for (int q = 0; q < nt; q++) {
                    if (b1 < 0 || tcnt[q] > tcnt[b1]) { b2 = b1; b1 = q; }
                    else if (b2 < 0 || tcnt[q] > tcnt[b2]) { b2 = q; }
                }
                if (b1 >= 0) plain_a = tids[b1];
                if (b2 >= 0) plain_b = tids[b2];
            }
            int att = 0;

            while (p < 110 && att++ < 48000) {
                int tx = 4 + PROP_RAND() % (bm->w - 8), ty = 4 + PROP_RAND() % (bm->h - 8);
                int ok = 1;
                int e0 = bm->elev ? bm->elev[ty * ew + tx] : 0;
                if (bm->bridges) {
                    for (int b = 0; b < bm->num_bridges; b++) {
                        const BakedBridge *br = &bm->bridges[b];
                        if (tx >= (int)br->x0 - 2 && tx <= (int)br->x1 + 2 &&
                            ty >= (int)br->y0 - 2 && ty <= (int)br->y1 + 2) { ok = 0; break; }
                    }
                }
                for (int oy = -1; oy <= 1 && ok; oy++) {
                    for (int ox = -1; ox <= 1; ox++) {
                        int cx_ = tx + ox, cy_ = ty + oy;
                        int k = terrain_tile_kind(stage_id, cx_, cy_);
                        if (terrain_is_blocked(k) || terrain_is_water(k)) { ok = 0; break; }
                        if (stage_id == STAGE_CAVE_ICE && terrain_is_smooth_ice_tile(stage_id, cx_, cy_)) { ok = 0; break; }
                        if (plain_a != 0) {
                            uint16_t ctid = bm->tile[cy_ * bm->w + cx_];
                            if (ctid != plain_a && ctid != plain_b) { ok = 0; break; }
                        }
                        if (bm->elev &&
                            (bm->elev[cy_ * ew + cx_] != e0 ||
                             bm->elev[cy_ * ew + cx_ + 1] != e0 ||
                             bm->elev[(cy_ + 1) * ew + cx_] != e0 ||
                             bm->elev[(cy_ + 1) * ew + cx_ + 1] != e0)) { ok = 0; break; }
                    }
                }
                float x = ((float)tx + 0.5f) * (float)TER_TILE, y = ((float)ty + 0.5f) * (float)TER_TILE;
                if (ok && (fabsf(x - sx_c) > 130.0f || fabsf(y - sy_c) > 130.0f) && !prop_has_nearby_any(p, x, y, 76.0f)) {
                    int rtype = (stage_id != STAGE_CAVE_ICE && (PROP_RAND() % 3 == 0)) ? PROP_ROCK_A : primary_rock;
                    prop_add(&p, rtype, x, y);
                }
            }

            att = 0;
            while (stage_id != STAGE_CAVE_ICE && p < 220 && att++ < 36000) {
                int tx = 4 + PROP_RAND() % (bm->w - 8), ty = 4 + PROP_RAND() % (bm->h - 8);
                int k = terrain_tile_kind(stage_id, tx, ty);
                if (terrain_is_blocked(k) || terrain_is_water(k)) continue;
                if (plain_a != 0) {
                    uint16_t ctid = bm->tile[ty * bm->w + tx];
                    if (ctid != plain_a && ctid != plain_b) continue;
                }
                if (bm->bridges) {
                    int near_br = 0;
                    for (int b = 0; b < bm->num_bridges; b++) {
                        const BakedBridge *br = &bm->bridges[b];
                        if (tx >= (int)br->x0 - 1 && tx <= (int)br->x1 + 1 &&
                            ty >= (int)br->y0 - 1 && ty <= (int)br->y1 + 1) { near_br = 1; break; }
                    }
                    if (near_br) continue;
                }
                if (bm->elev) {
                    int e0 = bm->elev[ty * ew + tx];
                    if (bm->elev[ty * ew + tx + 1] != e0 || bm->elev[(ty + 1) * ew + tx] != e0 || bm->elev[(ty + 1) * ew + tx + 1] != e0) continue;
                }
                float x = ((float)tx + 0.5f) * (float)TER_TILE, y = ((float)ty + 0.5f) * (float)TER_TILE;
                if (!prop_has_nearby_any(p, x, y, 42.0f)) {
                    prop_add(&p, PROP_PEBBLE, x, y);
                }
            }
        }
        return;
    }
    g_world_w = 6000.0f; g_world_h = 6000.0f;
    int tree = (stage_id == STAGE_ILEX_FOREST) ? PROP_TREE_DARK : PROP_TREE_A;
    int alt_tree = (stage_id == STAGE_ILEX_FOREST) ? PROP_TREE_DARK : PROP_TREE_B;

    static const float ring_offsets[3] = { 28.0f, 70.0f, 112.0f };
    for (int ring = 0; ring < 3; ring++) {
        float off = ring_offsets[ring];
        float stag = (ring & 1) ? 24.0f : 0.0f;
        for (float t = 28.0f + stag; t < WORLD_W - 28.0f; t += 48.0f) {
            float j1 = (float)(PROP_RAND() % 8 - 4);
            float j2 = (float)(PROP_RAND() % 8 - 4);
            int idx = (int)(t / 48.0f) + ring;
            int rprop = PROP_ROCK_B;
            int tprop = (idx % 3 == 0) ? alt_tree : tree;
            int ns = (stage_id == STAGE_NATIONAL_PARK) ? rprop : tprop;
            int we_w = (stage_id == STAGE_NATIONAL_PARK) ? rprop : tprop;
            int we_e = (stage_id == STAGE_ILEX_FOREST) ? tprop : rprop;
            float x1 = t + j1, y1 = off + j2;
            if (!prop_has_nearby_blocker(p, x1, y1, 40.0f)) prop_add(&p, ns, x1, y1);
            float x2 = t - j1, y2 = WORLD_H - off + j2;
            if (!prop_has_nearby_blocker(p, x2, y2, 40.0f)) prop_add(&p, ns, x2, y2);
            float x3 = off + j2, y3 = t + j1;
            if (!prop_has_nearby_blocker(p, x3, y3, 40.0f)) prop_add(&p, we_w, x3, y3);
            float x4 = WORLD_W - off + j2, y4 = t - j1;
            if (!prop_has_nearby_blocker(p, x4, y4, 40.0f)) prop_add(&p, we_e, x4, y4);
        }
    }

    float sx0 = (stage_id == STAGE_ROUTE_29) ? 1350.0f : ((stage_id == STAGE_NATIONAL_PARK) ? 1500.0f : 4500.0f);
    float sy0 = (stage_id == STAGE_ROUTE_29) ? 3000.0f : 1500.0f;
    int attempts = 0;
    while (p < MAX_PROPS && attempts++ < 80000) {
        float x = 220.0f + (float)(PROP_RAND() % ((int)WORLD_W - 440));
        float y = 220.0f + (float)(PROP_RAND() % ((int)WORLD_H - 440));
        if (fabsf(x - sx0) < 160.0f && fabsf(y - sy0) < 160.0f) continue;

        int t = terrain_at(stage_id, x, y);
        int r = PROP_RAND() % 100;
        if (t == TER_GRASS) {
            if (r < 16) {
                if (!prop_near_blocked_or_road(stage_id, x, y, true) && !prop_has_nearby_any(p, x, y, 112.0f)) {
                    prop_add(&p, (PROP_RAND() & 1) ? PROP_TREE_A : PROP_TREE_B, x, y);
                }
            } else if (r < 24) {
                if (!prop_has_nearby_any(p, x, y, 36.0f)) prop_add(&p, PROP_FLOWER_PINK, x, y);
            } else if (r < 32) {
                if (!prop_has_nearby_any(p, x, y, 36.0f)) prop_add(&p, PROP_FLOWER_YELLOW, x, y);
            } else if (r < 76) {

                if (!prop_has_nearby_any(p, x, y, 32.0f)) {
                    float gx0 = floorf(x / 32.0f) * 32.0f + 16.0f;
                    float gy0 = floorf(y / 32.0f) * 32.0f + 16.0f;
                    int pw = 2 + (PROP_RAND() % 2), ph = 2 + (PROP_RAND() % 2);
                    for (int dy = 0; dy < ph; dy++) {
                        for (int dx = 0; dx < pw; dx++) {
                            float px = gx0 + (float)dx * 32.0f, py = gy0 + (float)dy * 32.0f;
                            if (terrain_at(stage_id, px, py) == TER_GRASS && !prop_has_nearby_any(p, px, py, 26.0f)) {
                                prop_add(&p, PROP_TUFT_A, px, py);
                            }
                        }
                    }
                }
            } else if (r < 79) {
                if (!prop_near_blocked_or_road(stage_id, x, y, true) && !prop_has_nearby_any(p, x, y, 110.0f))
                    prop_add(&p, PROP_BERRY_TREE, x, y);
            } else if (r < 84) {
                if (!prop_has_nearby_any(p, x, y, 30.0f)) prop_add(&p, PROP_PEBBLE, x, y);
            }
        } else if (t == TER_FOREST) {
            if (r < 28) {
                if (!prop_near_blocked_or_road(stage_id, x, y, true) && !prop_has_nearby_any(p, x, y, 96.0f)) {
                    prop_add(&p, PROP_TREE_DARK, x, y);
                }
            } else if (r < 60) {
                if (!prop_has_nearby_any(p, x, y, 26.0f)) prop_add(&p, PROP_TUFT_B, x, y);
            } else if (r < 65) {
                if (!prop_near_blocked_or_road(stage_id, x, y, true) && !prop_has_nearby_any(p, x, y, 110.0f))
                    prop_add(&p, PROP_BERRY_TREE, x, y);
            } else if (r < 76) {
                if (!prop_has_nearby_any(p, x, y, 30.0f)) prop_add(&p, PROP_PEBBLE, x, y);
            } else if (r < 80) {
                if (!prop_near_blocked_or_road(stage_id, x, y, true) && !prop_has_nearby_any(p, x, y, 96.0f))
                    prop_add(&p, PROP_ROCK_B, x, y);
            }
        } else if (t == TER_SAND) {
            bool near_water = false;
            for (int wd = 0; wd < 8 && !near_water; wd++) {
                static const float WDX[8] = { 1, -1, 0, 0, 0.707f, -0.707f, 0.707f, -0.707f };
                static const float WDY[8] = { 0, 0, 1, -1, 0.707f, 0.707f, -0.707f, -0.707f };
                if (terrain_is_water(terrain_at(stage_id, x + WDX[wd] * 180.0f, y + WDY[wd] * 180.0f)) ||
                    terrain_is_water(terrain_at(stage_id, x + WDX[wd] * 360.0f, y + WDY[wd] * 360.0f))) {
                    near_water = true;
                }
            }
            if (near_water && r < 28) {
                if (!prop_near_blocked_or_road(stage_id, x, y, true) && !prop_has_nearby_any(p, x, y, 96.0f))
                    prop_add(&p, (PROP_RAND() & 1) ? PROP_CACTUS_A : PROP_CACTUS_B, x, y);
            } else if (!near_water && r < 6) {
                if (!prop_near_blocked_or_road(stage_id, x, y, true) && !prop_has_nearby_any(p, x, y, 185.0f))
                    prop_add(&p, (PROP_RAND() & 1) ? PROP_CACTUS_A : PROP_CACTUS_B, x, y);
            } else if (r < 42) {
                if (!prop_near_blocked_or_road(stage_id, x, y, true) && !prop_has_nearby_any(p, x, y, 108.0f))
                    prop_add(&p, PROP_ROCK_B, x, y);
            } else if (r < 76) {
                if (!prop_has_nearby_any(p, x, y, 30.0f)) prop_add(&p, PROP_PEBBLE, x, y);
            }
        } else if (t == TER_DEEP) {

            bool all_deep = true;
            for (float dy = -56.0f; dy <= 56.0f; dy += 28.0f) {
                for (float dx = -56.0f; dx <= 56.0f; dx += 28.0f) {
                    if (terrain_at(stage_id, x + dx, y + dy) != TER_DEEP) {
                        all_deep = false;
                        break;
                    }
                }
                if (!all_deep) break;
            }
            if (all_deep && !prop_has_nearby_any(p, x, y, 64.0f)) {
                prop_add(&p, PROP_LILY, x, y);
            }
        }
    }
    #undef PROP_RAND
}

static bool g_title_bgm_pending = false;
static int  g_menu_visible_frames = 0;

static void render_menu_hub(void);
static void render_menu_modes(void);
static void render_menu_roster(void);
static void render_menu_shop(void);
static void render_menu_gacha(void);
static void render_menu_missions(void);
static void render_menu_settings(void);

static void draw_boot_loading_scrim(const char *msg, float anim_phase) {

    render_draw_loading_overlay(msg, anim_phase, 248);
}

void game_init(void) {
    memset(&g_game, 0, sizeof(GameState));
    save_system_init();

    g_game.state = STATE_MENU_HUB;
    g_title_bgm_pending = true;
    g_menu_visible_frames = 0;
    g_game.selected_starter = g_save.selected_starter;
    g_game.selected_stage = 0;
    g_game.grace_timer = 0.3f;
    g_game.tagline_index = rand() % TOTAL_TAGLINES;

    render_set_stage(g_game.selected_stage);

    static const int wanderer_dex_pool[MAX_WANDERERS] = { 25, 133, 4, 1, 7, 143 };
    for (int w = 0; w < MAX_WANDERERS; w++) {
        MenuWanderer *mw = &g_game.wanderers[w];
        mw->dex = wanderer_dex_pool[w];
        mw->x = 100.0f + (float)(rand() % 760);
        mw->y = 100.0f + (float)(rand() % 344);
        mw->target_x = mw->x + (float)(rand() % 200 - 100);
        mw->target_y = mw->y + (float)(rand() % 200 - 100);
        mw->anim_type = ANIM_WALK;
        mw->speed = 40.0f + (float)(rand() % 30);
        mw->dir = rand() % 8;
        mw->frame = 0;
        mw->anim_timer = 0.0f;
        mw->wait_timer = 0.0f;
    }

    spawn_world_props_for_stage(g_game.selected_stage);

    for (int f = 0; f < 2; f++) {
        render_start_frame();
        render_clear(RGBA8_C(12, 18, 34, 255));
        render_draw_loading_overlay("PRELOADING MENUS & ASSETS...", (float)f * 0.45f, 255);
        render_end_frame();
    }
    render_preload_hud_fonts();
    for (int w = 0; w < MAX_WANDERERS; w++) {
        render_preload_pokemon(g_game.wanderers[w].dex);
    }
    if (g_game.selected_starter >= 0 && g_game.selected_starter < TOTAL_CATALOG_POKEMON) {
        int dex = POKEMON_CATALOG[g_game.selected_starter].dex;
        for (int chain = 0; chain < 3 && dex > 0 && dex < 1025; chain++) {
            render_preload_pokemon(dex);
            dex = POKEMON_EVO_TARGET[dex];
        }
    }

    for (int r = 0; r < 16 && r < TOTAL_CATALOG_POKEMON; r++) {
        render_preload_pokemon(POKEMON_CATALOG[r].dex);
    }

    render_start_frame();
    render_menu_hub();
    render_menu_modes();
    render_menu_roster();
    draw_boot_loading_scrim("PRELOADING MENUS & ASSETS...", 0.90f);
    render_end_frame();

    render_start_frame();
    render_menu_shop();
    render_menu_gacha();
    render_menu_missions();
    render_menu_settings();
    draw_boot_loading_scrim("PRELOADING MENUS & ASSETS...", 1.50f);
    render_end_frame();
}

static void run_save_path_for_mode(int mode, char *out, size_t sz) {
    if (mode < 0 || mode >= TOTAL_RUN_MODES) mode = 0;
#ifdef __vita__
    snprintf(out, sz, "ux0:data/poke-survivors vita/run_save_%d.dat", mode);
#else
    snprintf(out, sz, "run_save_%d.dat", mode);
#endif
}

bool game_has_saved_run(int mode) {
    char path[128];
    run_save_path_for_mode(mode, path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    uint32_t hdr[2] = {0, 0};
    size_t n = fread(hdr, sizeof(uint32_t), 2, f);
    fclose(f);
    return (n == 2 && hdr[0] == RUN_SAVE_MAGIC && hdr[1] == RUN_SAVE_VERSION);
}

bool game_get_saved_run_info(int mode, int *out_char_idx, int *out_level, float *out_time, int *out_stage) {
    char path[128];
    run_save_path_for_mode(mode, path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    RunSaveData *sd = (RunSaveData *)malloc(sizeof(RunSaveData));
    if (!sd) { fclose(f); return false; }
    size_t n = fread(sd, 1, sizeof(RunSaveData), f);
    fclose(f);
    bool ok = (n == sizeof(RunSaveData) && sd->magic == RUN_SAVE_MAGIC && sd->version == RUN_SAVE_VERSION);
    if (ok) {
        if (out_char_idx) *out_char_idx = sd->player.char_idx;
        if (out_level)    *out_level    = sd->player.level;
        if (out_time)     *out_time     = sd->run_timer;
        if (out_stage)    *out_stage    = sd->selected_stage;
    }
    free(sd);
    return ok;
}

void game_delete_saved_run(int mode) {
    char path[128];
    run_save_path_for_mode(mode, path, sizeof(path));
    remove(path);
}

bool game_save_current_run(void) {
    RunSaveData *sd = (RunSaveData *)calloc(1, sizeof(RunSaveData));
    if (!sd) return false;
    sd->magic = RUN_SAVE_MAGIC;
    sd->version = RUN_SAVE_VERSION;
    sd->run_mode = g_game.run_mode;
    sd->selected_stage = g_game.selected_stage;
    sd->run_timer = g_game.run_timer;
    sd->wave_clock = g_game.wave_clock;
    sd->endless_cycle = g_game.endless_cycle;
    sd->swarm_triggered = g_game.swarm_triggered;
    sd->encircle_triggered = g_game.encircle_triggered;
    sd->final_reaper_spawned = g_game.final_reaper_spawned;
    sd->spawn_timer = g_game.spawn_timer;
    sd->coins_collected = g_game.coins_collected;
    sd->tickets_collected = g_game.tickets_collected;
    sd->rerolls_left = g_game.rerolls_left;
    sd->run_rerolls_bought = g_game.run_rerolls_bought;
    sd->revives_left = g_game.revives_left;
    sd->bosses_defeated_in_run = g_game.bosses_defeated_in_run;
    sd->next_boss_timer = g_game.next_boss_timer;
    sd->next_heal_timer = g_game.next_heal_timer;
    sd->next_boss_dex = g_game.next_boss_dex;
    memcpy(sd->next_boss_name, g_game.next_boss_name, sizeof(sd->next_boss_name));
    memcpy(sd->current_boss_name, g_game.current_boss_name, sizeof(sd->current_boss_name));
    sd->boss_active = g_game.boss_active;
    sd->used_boss_mask = g_game.used_boss_mask;
    sd->last_boss_idx = g_game.last_boss_idx;
    sd->player = g_game.player;
    memcpy(sd->enemies, g_game.enemies, sizeof(sd->enemies));
    for (int i = 0; i < MAX_ENEMIES; i++) sd->enemies[i].name = NULL;
    memcpy(sd->gems, g_game.gems, sizeof(sd->gems));
    memcpy(sd->pickups, g_game.pickups, sizeof(sd->pickups));

#ifdef __vita__
    sceIoMkdir("ux0:data/poke-survivors vita", 0777);
#endif
    char path[128];
    run_save_path_for_mode(g_game.run_mode, path, sizeof(path));
    FILE *f = fopen(path, "wb");
    bool ok = false;
    if (f) {
        ok = (fwrite(sd, 1, sizeof(RunSaveData), f) == sizeof(RunSaveData));
        fclose(f);
    }
    free(sd);
    return ok;
}

bool game_resume_saved_run(int mode) {
    char path[128];
    run_save_path_for_mode(mode, path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    RunSaveData *sd = (RunSaveData *)malloc(sizeof(RunSaveData));
    if (!sd) { fclose(f); return false; }
    size_t n = fread(sd, 1, sizeof(RunSaveData), f);
    fclose(f);
    if (n != sizeof(RunSaveData) || sd->magic != RUN_SAVE_MAGIC || sd->version != RUN_SAVE_VERSION) {
        free(sd);
        return false;
    }

    for (int fr = 0; fr < 2; fr++) {
        render_start_frame();
        render_clear(RGBA8_C(12, 18, 34, 255));
        render_draw_text_centered(SCREEN_W * 0.5f, SCREEN_H * 0.5f - 14.0f, 1.8f, RGBA8_C(255, 225, 80, 255), "RESUMING SAVED RUN...");
        render_end_frame();
    }

    g_game.run_mode = sd->run_mode;
    g_game.selected_stage = (sd->selected_stage >= 0 && sd->selected_stage < TOTAL_STAGES) ? sd->selected_stage : 0;
    render_set_stage(g_game.selected_stage);
    spawn_world_props_for_stage(g_game.selected_stage);

    srand((unsigned)time(NULL) ^ ((unsigned)clock() << 16));

    g_game.player = sd->player;
    if (g_game.player.char_idx < 0 || g_game.player.char_idx >= TOTAL_CATALOG_POKEMON) g_game.player.char_idx = 0;
    player_recalc_stats(&g_game.player);
    if (g_game.player.hp > g_game.player.max_hp) g_game.player.hp = g_game.player.max_hp;
    if (g_game.player.hp < 1.0f) g_game.player.hp = 1.0f;

    memcpy(g_game.enemies, sd->enemies, sizeof(g_game.enemies));
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (g_game.enemies[i].active) {
            int d = g_game.enemies[i].dex;
            int ci = (d > 0 && d < 1025) ? POKEMON_DEX_TO_CATALOG[d] : 0;
            g_game.enemies[i].name = (ci >= 0 && ci < TOTAL_CATALOG_POKEMON) ? POKEMON_CATALOG[ci].name : "Wild Pokemon";
            render_preload_pokemon(d);
        } else {
            g_game.enemies[i].name = NULL;
        }
    }
    memcpy(g_game.gems, sd->gems, sizeof(g_game.gems));
    memcpy(g_game.pickups, sd->pickups, sizeof(g_game.pickups));

    moves_init();
    memset(g_game.projectiles, 0, sizeof(g_game.projectiles));
    memset(g_game.whips, 0, sizeof(g_game.whips));
    memset(g_game.popups, 0, sizeof(g_game.popups));
    memset(g_game.particles, 0, sizeof(g_game.particles));
    memset(g_game.dash_ghosts, 0, sizeof(g_game.dash_ghosts));

    g_game.run_timer = sd->run_timer;
    g_game.wave_clock = sd->wave_clock;
    g_game.endless_cycle = sd->endless_cycle;
    g_game.swarm_triggered = sd->swarm_triggered;
    g_game.encircle_triggered = sd->encircle_triggered;
    g_game.final_reaper_spawned = sd->final_reaper_spawned;
    g_game.reaper_banner_timer = 0.0f;
    g_game.spawn_timer = sd->spawn_timer;
    g_game.coins_collected = sd->coins_collected;
    g_game.tickets_collected = sd->tickets_collected;
    g_game.rerolls_left = sd->rerolls_left;
    g_game.run_rerolls_bought = sd->run_rerolls_bought;
    g_game.revives_left = sd->revives_left;
    g_game.revive_anim_timer = 0.0f;
    g_game.bosses_defeated_in_run = sd->bosses_defeated_in_run;
    g_game.next_boss_timer = sd->next_boss_timer;
    g_game.next_heal_timer = sd->next_heal_timer;
    g_game.next_boss_dex = sd->next_boss_dex;
    memcpy(g_game.next_boss_name, sd->next_boss_name, sizeof(g_game.next_boss_name));
    memcpy(g_game.current_boss_name, sd->current_boss_name, sizeof(g_game.current_boss_name));
    g_game.boss_active = sd->boss_active;
    g_game.used_boss_mask = sd->used_boss_mask;
    g_game.last_boss_idx = sd->last_boss_idx;
    g_game.grace_timer = 0.4f;
    g_game.cam_x = g_game.player.x - SCREEN_W * 0.5f;
    g_game.cam_y = g_game.player.y - SCREEN_H * 0.5f - 48.0f;

    free(sd);

    render_preload_hud_fonts();
    int dex = POKEMON_CATALOG[g_game.player.char_idx].dex;
    for (int chain = 0; chain < 3 && dex > 0 && dex < 1025; chain++) {
        render_preload_pokemon(dex);
        dex = POKEMON_EVO_TARGET[dex];
    }
    enemies_preload_stage_start(g_game.selected_stage, g_game.run_mode, g_game.next_boss_dex);
    moves_preload_assets();

    game_delete_saved_run(g_game.run_mode);

    g_game.state = STATE_PLAYING;
    render_warmup_match(g_game.player.x, g_game.player.y);
    audio_play_sfx(SFX_SELECT);
    game_play_random_battle_bgm();
    return true;
}

void game_endless_warp_random_map(void) {
    int next_stg = rand() % TOTAL_STAGES;
    if (next_stg == g_game.selected_stage) next_stg = (next_stg + 1) % TOTAL_STAGES;
    g_game.selected_stage = next_stg;
    render_set_stage(next_stg);
    spawn_world_props_for_stage(next_stg);

    memset(g_game.enemies, 0, sizeof(g_game.enemies));
    memset(g_game.projectiles, 0, sizeof(g_game.projectiles));
    memset(g_game.zones, 0, sizeof(g_game.zones));
    memset(g_game.gems, 0, sizeof(g_game.gems));
    memset(g_game.pickups, 0, sizeof(g_game.pickups));
    memset(g_game.whips, 0, sizeof(g_game.whips));

    if (next_stg == STAGE_ROUTE_29) {
        g_game.player.x = 1350.0f; g_game.player.y = 3000.0f;
    } else if (next_stg == STAGE_NATIONAL_PARK) {
        g_game.player.x = 1500.0f; g_game.player.y = 1500.0f;
    } else if (next_stg == STAGE_ILEX_FOREST) {
        g_game.player.x = 4500.0f; g_game.player.y = 1500.0f;
    } else if (next_stg >= BAKED_FIRST_STAGE) {
        const BakedMap *bm = &g_baked_maps[next_stg - BAKED_FIRST_STAGE];
        g_game.player.x = ((float)bm->start_tx + 0.5f) * (float)TER_TILE;
        g_game.player.y = ((float)bm->start_ty + 0.5f) * (float)TER_TILE;
    }
    g_game.cam_x = g_game.player.x - SCREEN_W * 0.5f;
    g_game.cam_y = g_game.player.y - SCREEN_H * 0.5f - 48.0f;
    g_game.player.iframe_timer = fmaxf(g_game.player.iframe_timer, 1.5f);
    enemies_preload_stage_start(next_stg, g_game.run_mode, g_game.next_boss_dex);
    render_warmup_match(g_game.player.x, g_game.player.y);
    game_spawn_particles(g_game.player.x, g_game.player.y, RGBA8_C(255, 225, 80, 255), 36, 180.0f);
    game_play_random_battle_bgm();
}

void game_reset_run(int starter_idx, int mode) {
    game_delete_saved_run(mode);
    g_game.run_mode = mode;
    g_game.selected_starter = starter_idx;
    g_save.selected_starter = starter_idx;
    save_system_save();

    for (int f = 0; f < 2; f++) {
        render_start_frame();
        render_clear(RGBA8_C(12, 18, 34, 255));
        render_draw_text_centered(SCREEN_W * 0.5f, SCREEN_H * 0.5f - 14.0f, 1.8f, RGBA8_C(255, 225, 80, 255), "PREPARING BATTLEFIELD...");
        render_end_frame();
    }

    render_set_stage(g_game.selected_stage);
    spawn_world_props_for_stage(g_game.selected_stage);

    srand((unsigned)time(NULL) ^ ((unsigned)clock() << 16));

    player_init(&g_game.player, starter_idx);
    if (g_game.run_mode == MODE_BOSSRUSH) {

        int cand_elem[TOTAL_MOVES], n_elem = 0;
        int cand_any[TOTAL_MOVES],  n_any = 0;
        for (int m = 0; m < TOTAL_MOVES; m++) {
            if (!is_move_compatible(starter_idx, m)) continue;
            if (MOVE_DB[m].kind == MK_DASH) continue;
            bool dup = false;
            for (int k = 0; k < g_game.player.move_count; k++) {
                if (g_game.player.moves[k].id == m) { dup = true; break; }
            }
            if (dup) continue;
            if (MOVE_DB[m].kind != MK_BUFF && MOVE_DB[m].type != 0) cand_elem[n_elem++] = m;
            else if (MOVE_DB[m].kind != MK_BUFF) cand_any[n_any++] = m;
        }
        for (int i = n_elem - 1; i > 0; i--) { int j = rand() % (i + 1); int t = cand_elem[i]; cand_elem[i] = cand_elem[j]; cand_elem[j] = t; }
        for (int i = n_any - 1; i > 0; i--)  { int j = rand() % (i + 1); int t = cand_any[i];  cand_any[i]  = cand_any[j];  cand_any[j]  = t; }
        int ei = 0, ai = 0;
        while (g_game.player.move_count < 4) {
            int pick = -1;
            if (ei < n_elem) pick = cand_elem[ei++];
            else if (ai < n_any) pick = cand_any[ai++];
            else break;
            moves_init_state(&g_game.player.moves[g_game.player.move_count], pick);
            g_game.player.move_count++;
        }
    }
    if (g_game.selected_stage == STAGE_ROUTE_29) {
        g_game.player.x = 1350.0f;
        g_game.player.y = 3000.0f;
    } else if (g_game.selected_stage == STAGE_NATIONAL_PARK) {
        g_game.player.x = 1500.0f;
        g_game.player.y = 1500.0f;
    } else if (g_game.selected_stage == STAGE_ILEX_FOREST) {
        g_game.player.x = 4500.0f;
        g_game.player.y = 1500.0f;
    } else if (g_game.selected_stage >= BAKED_FIRST_STAGE) {
        const BakedMap *bm = &g_baked_maps[g_game.selected_stage - BAKED_FIRST_STAGE];
        g_game.player.x = ((float)bm->start_tx + 0.5f) * (float)TER_TILE;
        g_game.player.y = ((float)bm->start_ty + 0.5f) * (float)TER_TILE;
    }
    enemies_init();
    moves_init();

    memset(g_game.projectiles, 0, sizeof(g_game.projectiles));
    memset(g_game.whips, 0, sizeof(g_game.whips));
    memset(g_game.gems, 0, sizeof(g_game.gems));
    memset(g_game.pickups, 0, sizeof(g_game.pickups));
    memset(g_game.popups, 0, sizeof(g_game.popups));
    memset(g_game.particles, 0, sizeof(g_game.particles));
    memset(g_game.dash_ghosts, 0, sizeof(g_game.dash_ghosts));

    g_game.run_timer = 0.0f;
    g_game.wave_clock = 0.0f;
    g_game.endless_cycle = 0;
    g_game.swarm_triggered = false;
    g_game.encircle_triggered = false;
    g_game.final_reaper_spawned = false;
    g_game.reaper_banner_timer = 0.0f;
    g_game.spawn_timer = 0.0f;
    g_game.coins_collected = 0;
    g_game.tickets_collected = 0;
    g_game.bosses_defeated_in_run = 0;
    g_game.rerolls_left = g_save.upgrades[10];
    g_game.run_rerolls_bought = 0;
    g_game.revives_left = g_save.upgrades[11];
    g_game.revive_anim_timer = 0.0f;
    g_game.grace_timer = 0.5f;

    g_game.used_boss_mask = 0;
    g_game.last_boss_idx = -1;
    g_game.next_boss_dex = game_pick_next_boss(g_game.next_boss_name, sizeof(g_game.next_boss_name));
    g_game.current_boss_name[0] = '\0';
    g_game.next_boss_timer = 120.0f;
    g_game.next_heal_timer = 120.0f;
    g_game.boss_active = false;

    render_preload_hud_fonts();
    if (starter_idx >= 0 && starter_idx < TOTAL_CATALOG_POKEMON) {
        int dex = POKEMON_CATALOG[starter_idx].dex;
        for (int chain = 0; chain < 3 && dex > 0 && dex < 1025; chain++) {
            render_preload_pokemon(dex);
            dex = POKEMON_EVO_TARGET[dex];
        }
    }
    enemies_preload_stage_start(g_game.selected_stage, g_game.run_mode, g_game.next_boss_dex);
    moves_preload_assets();

    if (g_game.run_mode == MODE_BOSSRUSH) {
        snprintf(g_game.current_boss_name, sizeof(g_game.current_boss_name), "%s", g_game.next_boss_name);
        enemies_spawn_boss_dex(g_game.next_boss_dex, &g_game.player);
        g_game.boss_active = true;
        g_game.next_boss_dex = game_pick_next_boss(g_game.next_boss_name, sizeof(g_game.next_boss_name));
        render_preload_pokemon(g_game.next_boss_dex);
    }

    g_game.state = STATE_PLAYING;
    render_warmup_match(g_game.player.x, g_game.player.y);

    audio_play_sfx(SFX_SELECT);
    if (g_game.run_mode != MODE_BOSSRUSH) {
        game_play_random_battle_bgm();
    }
}

void game_spawn_damage_popup(float x, float y, int amount, uint32_t color) {
    for (int i = 0; i < MAX_POPUPS; i++) {
        DamagePopup *pop = &g_game.popups[i];
        if (!pop->active) {
            pop->active = true;
            pop->x = x + (float)(rand() % 16 - 8);
            pop->y = y + (float)(rand() % 8 - 4);
            pop->amount = amount;
            pop->timer = 0.6f;
            pop->max_timer = 0.6f;
            pop->color = color;
            break;
        }
    }
}

void game_spawn_particles(float x, float y, uint32_t color, int count, float speed) {
    for (int i = 0; i < count; i++) {
        for (int p = 0; p < MAX_PARTICLES; p++) {
            Particle *pt = &g_game.particles[p];
            if (!pt->active) {
                pt->active = true;
                pt->x = x;
                pt->y = y;
                float ang = ((float)(rand() % 360)) * (3.14159265f / 180.0f);
                float spd = speed * (0.3f + (float)(rand() % 70) / 100.0f);
                pt->vx = cosf(ang) * spd;
                pt->vy = sinf(ang) * spd;
                pt->life = 0.4f + (float)(rand() % 25) / 100.0f;
                pt->max_life = pt->life;
                pt->size = 2.0f + (float)(rand() % 3);
                pt->color = color;
                break;
            }
        }
    }
}

static int gem_tier_for_exp(int exp_pts) {
    if (exp_pts >= 75) return 4;
    if (exp_pts >= 25) return 3;
    if (exp_pts >= 10) return 2;
    return 1;
}

void game_play_random_battle_bgm(void) {
    static const MusicTrack EXCITING_TRACKS[4] = {
        BGM_BATTLE_WILD,
        BGM_ROUTE_MEADOW,
        BGM_ROUTE_DESERT,
        BGM_ROUTE_LAKESIDE
    };
    int pick = rand() % 4;
    if ((int)EXCITING_TRACKS[pick] == g_game.last_battle_bgm) {
        pick = (pick + 1 + (rand() % 3)) % 4;
    }
    g_game.last_battle_bgm = (int)EXCITING_TRACKS[pick];
    g_game.bgm_rotate_timer = 75.0f;
    audio_play_bgm(EXCITING_TRACKS[pick]);
}

void game_spawn_gem(float x, float y, int value) {
    int exp_pts = (value >= 3) ? 25 : ((value == 2) ? 10 : 3);

    int free_idx = -1;
    int far_idx = -1;
    float far_d2 = -1.0f;
    int near_idx = -1;
    float near_d2 = 1e12f;

    for (int i = 0; i < MAX_GEMS; i++) {
        ExpGem *gem = &g_game.gems[i];
        if (!gem->active) {
            if (free_idx < 0) free_idx = i;
            continue;
        }
        float pdx = gem->x - g_game.player.x;
        float pdy = gem->y - g_game.player.y;
        float pd2 = pdx * pdx + pdy * pdy;
        if (!gem->magnetized && pd2 > far_d2) {
            far_d2 = pd2;
            far_idx = i;
        }
        float dx = gem->x - x;
        float dy = gem->y - y;
        float d2 = dx * dx + dy * dy;
        if (d2 < near_d2) {
            near_d2 = d2;
            near_idx = i;
        }
    }

    if (free_idx >= 0) {
        ExpGem *gem = &g_game.gems[free_idx];
        gem->active = true;
        gem->x = x + (float)(rand() % 16 - 8);
        gem->y = y + (float)(rand() % 16 - 8);
        gem->exp = exp_pts;
        gem->value = gem_tier_for_exp(exp_pts);
        gem->magnetized = false;
    } else {

        int merge_idx = (far_idx >= 0) ? far_idx : near_idx;
        if (merge_idx >= 0) {
            g_game.gems[merge_idx].exp += exp_pts;
            g_game.gems[merge_idx].value = gem_tier_for_exp(g_game.gems[merge_idx].exp);
        }
    }
}

void game_spawn_pickup(float x, float y, int type) {
    bool is_priority = (type == 1 || type == 4 || type == 5 || type == 6 || type == 7);
    int slot = -1;
    for (int i = 0; i < MAX_PICKUPS; i++) {
        if (!g_game.pickups[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0 && is_priority) {
        float best_score = -1.0f;
        for (int i = 0; i < MAX_PICKUPS; i++) {
            PickupItem *pk = &g_game.pickups[i];
            int pt = pk->type;
            if (pt == 1 || pt == 4 || pt == 5 || pt == 6 || pt == 7) continue;
            float dx = pk->x - g_game.player.x;
            float dy = pk->y - g_game.player.y;
            float dist = sqrtf(dx * dx + dy * dy);

            float score = (pt == 3 ? 20000.0f : 0.0f) + dist + (25.0f - pk->timer) * 40.0f;
            if (score > best_score) {
                best_score = score;
                slot = i;
            }
        }

        if (slot < 0) {
            float far_d2 = -1.0f;
            for (int i = 0; i < MAX_PICKUPS; i++) {
                PickupItem *pk = &g_game.pickups[i];
                if (pk->type == 1 && type != 1) continue;
                float dx = pk->x - g_game.player.x, dy = pk->y - g_game.player.y;
                float d2 = dx * dx + dy * dy;
                if (d2 > far_d2) { far_d2 = d2; slot = i; }
            }
        }
    }
    if (slot >= 0) {
        PickupItem *pk = &g_game.pickups[slot];
        pk->active = true;
        pk->x = x;
        pk->y = y;
        pk->type = type;
        pk->timer = (type == 1 || type == 4 || type == 5 || type == 6 || type == 7) ? 999999.0f : 25.0f;
        pk->magnetized = false;
    }
}

void game_trigger_revive(Player *player) {
    if (!player || g_game.revives_left <= 0) return;
    g_game.revives_left--;
    if (g_save.upgrades[11] > 0) {
        g_save.upgrades[11]--;
        save_system_save();
    }
    g_game.revive_anim_timer = 2.1f;
    player->hp = player->max_hp;
    player->iframe_timer = 3.0f;
    g_game.screen_shake = 0.35f;

    for (int p = 0; p < MAX_PROJECTILES; p++) {
        if (g_game.projectiles[p].active && g_game.projectiles[p].is_enemy) {
            g_game.projectiles[p].active = false;
        }
    }
    int stage = g_game.selected_stage;
    for (int e = 0; e < MAX_ENEMIES; e++) {
        Enemy *en = &g_game.enemies[e];
        if (!en->active || en->is_boss) continue;
        float dx = en->x - player->x, dy = en->y - player->y;
        float d = sqrtf(dx * dx + dy * dy);
        if (d < 190.0f && d > 0.1f) {
            float nx = en->x + (dx / d) * 90.0f;
            float ny = en->y + (dy / d) * 90.0f;
            if (!terrain_is_blocked(terrain_at(stage, nx, ny))) { en->x = nx; en->y = ny; }
        }
    }
    game_spawn_particles(player->x, player->y, RGBA8_C(255, 228, 70, 255), 26, 185.0f);
    game_spawn_particles(player->x, player->y, RGBA8_C(110, 255, 165, 255), 18, 135.0f);
    audio_play_sfx(SFX_LEVELUP);
}

static void game_spawn_heal_station(void) {

    int active_heals = 0;
    for (int i = 0; i < MAX_PICKUPS; i++) {
        if (g_game.pickups[i].active && g_game.pickups[i].type == 6) active_heals++;
    }
    if (active_heals >= 3) return;
    int stage = g_game.selected_stage;
    float best_x = g_game.player.x, best_y = g_game.player.y;
    bool found = false;
    for (int att = 0; att < 140; att++) {
        float ang = ((float)(rand() % 3600)) * (6.2831853f / 3600.0f);
        float dist = 420.0f + (float)(rand() % 520);
        float tx = g_game.player.x + cosf(ang) * dist;
        float ty = g_game.player.y + sinf(ang) * dist;
        if (tx < 128.0f || tx > WORLD_W - 128.0f || ty < 128.0f || ty > WORLD_H - 128.0f) continue;
        int k = terrain_at(stage, tx, ty);
        if (terrain_is_blocked(k) || terrain_is_water(k)) continue;

        bool ok = true;
        for (int oy = -1; oy <= 1 && ok; oy++) {
            for (int ox = -1; ox <= 1; ox++) {
                int nk = terrain_at(stage, tx + (float)ox * 28.0f, ty + (float)oy * 28.0f);
                if (terrain_is_blocked(nk) || terrain_is_water(nk)) { ok = false; break; }
            }
        }
        if (!ok) continue;
        for (int p = 0; p < MAX_PROPS && ok; p++) {
            const WorldProp *pr = &g_game.props[p];
            if (!pr->active || !is_tall_prop(pr->type)) continue;
            float pdx = tx - pr->x, pdy = ty - pr->y;
            if (pdx * pdx + pdy * pdy < 46.0f * 46.0f) ok = false;
        }
        if (ok) { best_x = tx; best_y = ty; found = true; break; }
    }
    if (!found) {
        best_x = g_game.player.x + 120.0f;
        best_y = g_game.player.y;
        if (terrain_is_blocked(terrain_at(stage, best_x, best_y))) { best_x = g_game.player.x; best_y = g_game.player.y; }
    }
    game_spawn_pickup(best_x, best_y, 6);
}

static int count_equipped_passives(const Player *p) {
    int cnt = 0;
    for (int i = 0; i < TOTAL_PASSIVES; i++) {
        if (p->passives[i] > 0) cnt++;
    }
    return cnt;
}

static float get_mode_coin_mult(void) {
    float mult = 1.0f + (float)g_save.upgrades[8] * 0.08f;
    if (g_game.run_mode == MODE_TURBO)   mult *= 1.50f;
    return mult;
}

void game_trigger_chest_reward(void) {
    g_game.state = STATE_CHEST_REWARD;
    g_game.chest_timer = 0.0f;
    g_save.stats_chests++;

    int base_chest_coins = 150 + (rand() % 151);
    g_game.chest_coins = (int)((float)base_chest_coins * get_mode_coin_mult());
    g_game.coins_collected += g_game.chest_coins;

    int roll = rand() % 100;
    int num_items = (roll < 15) ? 5 : ((roll < 45) ? 3 : 1);
    g_game.chest_reward_count = num_items;

    Player *p = &g_game.player;
    bool can_evolve = (g_game.bosses_defeated_in_run >= 3 || g_game.wave_clock >= 360.0f || g_game.run_mode == MODE_BOSSRUSH);

    for (int r = 0; r < num_items; r++) {
        UpgradeCard *card = &g_game.chest_rewards[r];
        memset(card, 0, sizeof(UpgradeCard));

        bool accepted = false;

        if (can_evolve) {
            for (int m = 0; m < p->move_count; m++) {
                if (p->moves[m].level >= MAX_SKILL_LEVEL && !p->moves[m].evolved) {
                    int req_pass = get_move_paired_passive(p->moves[m].id);
                    if (p->passives[req_pass] >= 1) {
                        card->type = 2;
                        card->id = p->moves[m].id;
                        snprintf(card->title, sizeof(card->title), "%s EVO!", MOVE_CATALOG[card->id].name);
                        snprintf(card->desc, sizeof(card->desc), "Tier-2 Evolved Form! (Paired w/ %s)", PASSIVE_CATALOG[req_pass].name);
                        card->cur_level = MAX_SKILL_LEVEL;
                        p->moves[m].evolved = true;
                        moves_apply_evo(&p->moves[m]);
                        accepted = true;
                        break;
                    }
                }
            }
        }

        for (int attempts = 0; attempts < 45 && !accepted; attempts++) {
            int pick_type = (p->move_count > 0 && (rand() % 100 < 55)) ? 0 : 1;
            if (pick_type == 0) {
                int m_idx = rand() % p->move_count;
                if (p->moves[m_idx].level < MAX_SKILL_LEVEL || g_game.run_mode == MODE_ENDLESS) {
                    card->type = 0;
                    card->id = p->moves[m_idx].id;
                    snprintf(card->title, sizeof(card->title), "%s", MOVE_CATALOG[card->id].name);
                    snprintf(card->desc, sizeof(card->desc), "Level %d - Level %d Upgrade!", p->moves[m_idx].level, p->moves[m_idx].level + 1);
                    card->cur_level = p->moves[m_idx].level;
                } else {
                    pick_type = 1;
                }
            }

            if (pick_type == 1) {
                int eq_pass = count_equipped_passives(p);
                int pass_id = rand() % TOTAL_PASSIVES;
                if (p->passives[pass_id] >= PASSIVE_CATALOG[pass_id].max_level) continue;
                if (p->passives[pass_id] == 0 && eq_pass >= MAX_EQUIPPED_PASSIVES) continue;
                card->type = 1;
                card->id = pass_id;
                snprintf(card->title, sizeof(card->title), "%s", PASSIVE_CATALOG[pass_id].name);
                snprintf(card->desc, sizeof(card->desc), "%s", PASSIVE_CATALOG[pass_id].desc);
                card->cur_level = p->passives[pass_id];
            }

            bool dup = false;
            for (int prev = 0; prev < r; prev++) {
                if (g_game.chest_rewards[prev].type == card->type &&
                    g_game.chest_rewards[prev].id == card->id) {
                    dup = true;
                    break;
                }
            }

            if (!dup) {
                accepted = true;
                if (card->type == 0) {
                    for (int m = 0; m < p->move_count; m++) {
                        if (p->moves[m].id == card->id && p->moves[m].level < MAX_SKILL_LEVEL) {
                            p->moves[m].level++;
                            moves_apply_level(&p->moves[m]);
                            break;
                        }
                    }
                } else if (card->type == 1) {
                    if (p->passives[card->id] < PASSIVE_CATALOG[card->id].max_level) {
                        p->passives[card->id]++;
                        player_recalc_stats(p);
                    }
                }
                break;
            }
        }

        if (!accepted) {
            if (r & 1) {
                card->type = 4;
                card->id = 0;
                snprintf(card->title, sizeof(card->title), "Oran Floor Heal");
                snprintf(card->desc, sizeof(card->desc), "Restores +30 HP immediately!");
                card->cur_level = 0;
                p->hp = fminf(p->max_hp, p->hp + 30.0f);
            } else {
                card->type = 5;
                card->id = 0;
                int bonus_c = (int)(25.0f * get_mode_coin_mult());
                snprintf(card->title, sizeof(card->title), "Gold Coin Bag");
                snprintf(card->desc, sizeof(card->desc), "Grants +%d Bonus Pokecoins!", bonus_c);
                card->cur_level = 0;
                g_game.coins_collected += bonus_c;
            }
        }
    }

    audio_play_sfx(SFX_LEVELUP);
}

static void shuffle_cards(UpgradeCard *arr, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        UpgradeCard tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

static bool card_already_picked(int count, const UpgradeCard *cand) {
    for (int i = 0; i < count; i++) {
        if (g_game.cards[i].type == cand->type && g_game.cards[i].id == cand->id) return true;
    }
    return false;
}

static bool pick_first_from_pool(int card_idx, const UpgradeCard *pool, int pool_n) {
    for (int i = 0; i < pool_n; i++) {
        if (!card_already_picked(card_idx, &pool[i])) {
            g_game.cards[card_idx] = pool[i];
            return true;
        }
    }
    return false;
}

void game_generate_level_up_cards(void) {
    Player *p = &g_game.player;
    g_game.selected_card = 0;

    UpgradeCard new_moves[TOTAL_MOVES], upg_moves[8], extra_whips[4], passives[TOTAL_PASSIVES];
    int n_new = 0, n_upg = 0, n_extra = 0, n_pass = 0;

    int auto_count = 0, dash_count = 0;
    for (int k = 0; k < p->move_count; k++) {
        if (MOVE_DB[p->moves[k].id].kind == MK_DASH) dash_count++;
        else auto_count++;
    }

    if (p->move_count < 6) {

        for (int pass_fb = 0; pass_fb < 2; pass_fb++) {
            if (pass_fb == 1 && n_new > 0) break;
            for (int m = 0; m < TOTAL_MOVES && n_new < TOTAL_MOVES; m++) {
                if (pass_fb == 0) {
                    if (!is_move_strict_element(p->char_idx, m)) continue;
                } else {
                    if (!is_move_compatible(p->char_idx, m)) continue;
                }
                bool is_dash = (MOVE_DB[m].kind == MK_DASH);
                if (is_dash && dash_count >= 1) continue;
                if (!is_dash && auto_count >= 5) continue;
                bool owned = false;
                for (int k = 0; k < p->move_count; k++) {
                    if (p->moves[k].id == m) { owned = true; break; }
                }
                if (!owned) {
                    UpgradeCard *c = &new_moves[n_new++];
                    memset(c, 0, sizeof(*c));
                    c->type = 0;
                    c->id = m;
                    snprintf(c->title, sizeof(c->title), "%s", MOVE_CATALOG[m].name);
                    snprintf(c->desc, sizeof(c->desc), "%s", MOVE_CATALOG[m].desc);
                    c->cur_level = 0;
                }
            }
        }
    }

    for (int m = 0; m < p->move_count && n_upg < 8; m++) {
        if (p->moves[m].level < MAX_SKILL_LEVEL || g_game.run_mode == MODE_ENDLESS) {
            UpgradeCard *c = &upg_moves[n_upg++];
            memset(c, 0, sizeof(*c));
            c->type = 0;
            c->id = p->moves[m].id;
            int next_rk = p->moves[m].level + 1;
            int pair_p = get_move_paired_passive(c->id);
            snprintf(c->title, sizeof(c->title), "%s", MOVE_CATALOG[c->id].name);
            if (next_rk > MAX_SKILL_LEVEL) {
                snprintf(c->desc, sizeof(c->desc), "Level %d - %d: +15%% Power (Endless Scaling)", p->moves[m].level, next_rk);
            } else if (MOVE_DB[c->id].kind == MK_DASH) {
                if (next_rk <= 3)      snprintf(c->desc, sizeof(c->desc), "Dash Lv %d: -0.3s Cooldown & +15%% Dash Distance/Speed", next_rk);
                else if (next_rk <= 5) snprintf(c->desc, sizeof(c->desc), "Dash Lv %d: Unlocks Contact Damage, Knockback & Wider Hitbox", next_rk);
                else if (next_rk <= 7) snprintf(c->desc, sizeof(c->desc), "Dash Lv %d: +2nd Dash Charge & 0.10s Exit i-Frame Buffer", next_rk);
                else                   snprintf(c->desc, sizeof(c->desc), "Dash Lv 8 (MAX): Elemental Trail & Phase Reload! (Evo: %s)", PASSIVE_CATALOG[pair_p].name);
            } else if (next_rk == MAX_SKILL_LEVEL) {
                snprintf(c->desc, sizeof(c->desc), "Max Level 8! +40%% Power. Pair w/ %s for Boss Chest Evo!", PASSIVE_CATALOG[pair_p].name);
            } else {
                snprintf(c->desc, sizeof(c->desc), "Level %d - %d: +25%% Power, Speed/Area (Evo Item: %s)", p->moves[m].level, next_rk, PASSIVE_CATALOG[pair_p].name);
            }
            c->cur_level = p->moves[m].level;
        }
    }

    for (int m = 0; m < p->move_count && n_extra < 4; m++) {
        MoveState *ms = &p->moves[m];
        if (!strcmp(MOVE_DB[ms->id].id, "vine-whip") && ms->mod.count < MAX_WHIP_EFFECTS - 1) {
            UpgradeCard *c = &extra_whips[n_extra++];
            memset(c, 0, sizeof(*c)); c->type = 3; c->id = ms->id;
            snprintf(c->title, sizeof(c->title), "Additional Vine Whip");
            snprintf(c->desc, sizeof(c->desc), "Add another trailing Vine Whip lash");
            c->cur_level = ms->mod.count;
        }
    }

    int eq_pass_cnt = count_equipped_passives(p);
    for (int pa = 0; pa < TOTAL_PASSIVES && n_pass < TOTAL_PASSIVES; pa++) {
        if (p->passives[pa] < PASSIVE_CATALOG[pa].max_level) {
            if (p->passives[pa] == 0 && eq_pass_cnt >= MAX_EQUIPPED_PASSIVES) continue;
            UpgradeCard *c = &passives[n_pass++];
            memset(c, 0, sizeof(*c));
            c->type = 1;
            c->id = pa;
            snprintf(c->title, sizeof(c->title), "%s", PASSIVE_CATALOG[pa].name);
            snprintf(c->desc, sizeof(c->desc), "%s", PASSIVE_CATALOG[pa].desc);
            c->cur_level = p->passives[pa];
        }
    }

    shuffle_cards(new_moves, n_new);
    shuffle_cards(upg_moves, n_upg);
    shuffle_cards(extra_whips, n_extra);
    shuffle_cards(passives, n_pass);

    for (int c = 0; c < 3; c++) {
        memset(&g_game.cards[c], 0, sizeof(UpgradeCard));
        bool ok = false;
        if (c == 0) {
            ok = pick_first_from_pool(c, new_moves, n_new)
              || pick_first_from_pool(c, upg_moves, n_upg)
              || pick_first_from_pool(c, extra_whips, n_extra)
              || pick_first_from_pool(c, passives, n_pass);
        } else if (c == 1) {
            ok = pick_first_from_pool(c, upg_moves, n_upg)
              || pick_first_from_pool(c, extra_whips, n_extra)
              || pick_first_from_pool(c, new_moves, n_new)
              || pick_first_from_pool(c, passives, n_pass);
        } else {
            if ((rand() % 3 != 0) && (n_new + n_upg > 0)) {
                ok = pick_first_from_pool(c, new_moves, n_new)
                  || pick_first_from_pool(c, upg_moves, n_upg)
                  || pick_first_from_pool(c, extra_whips, n_extra)
                  || pick_first_from_pool(c, passives, n_pass);
            } else {
                ok = pick_first_from_pool(c, passives, n_pass)
                  || pick_first_from_pool(c, new_moves, n_new)
                  || pick_first_from_pool(c, upg_moves, n_upg)
                  || pick_first_from_pool(c, extra_whips, n_extra);
            }
        }
        if (!ok) {

            if (c % 2 == 0) {
                g_game.cards[c].type = 4;
                g_game.cards[c].id = 2;
                snprintf(g_game.cards[c].title, sizeof(g_game.cards[c].title), "Floor Heal (+30 HP)");
                snprintf(g_game.cards[c].desc, sizeof(g_game.cards[c].desc), "All slots maxed! Instantly restores +30 Health.");
                g_game.cards[c].cur_level = 0;
            } else {
                int bag_c = (int)(25.0f * get_mode_coin_mult());
                g_game.cards[c].type = 5;
                g_game.cards[c].id = 3;
                snprintf(g_game.cards[c].title, sizeof(g_game.cards[c].title), "Gold Bag (+%d Coins)", bag_c);
                snprintf(g_game.cards[c].desc, sizeof(g_game.cards[c].desc), "All slots maxed! Claim +%d Bonus Pokecoins.", bag_c);
                g_game.cards[c].cur_level = 0;
            }
        }

        if (g_game.cards[c].type == 0 && g_game.cards[c].id >= 0 && g_game.cards[c].id < TOTAL_MOVES) {
            moves_preload_move(g_game.cards[c].id);
        }
    }
}

static void apply_upgrade_card(UpgradeCard *card) {
    Player *p = &g_game.player;
    if (card->type == 2) {

        for (int m = 0; m < p->move_count; m++) {
            if (p->moves[m].id == card->id) {
                p->moves[m].evolved = true;
                moves_apply_evo(&p->moves[m]);
                break;
            }
        }
    } else if (card->type == 0) {

        bool found = false;
        for (int m = 0; m < p->move_count; m++) {
            if (p->moves[m].id == card->id) {
                if (p->moves[m].level < MAX_SKILL_LEVEL) {
                    p->moves[m].level++;
                    moves_apply_level(&p->moves[m]);
                }
                found = true;
                break;
            }
        }
        if (!found && p->move_count < 6) {
            int slot = p->move_count++;
            moves_init_state(&p->moves[slot], card->id);
            moves_preload_move(card->id);
        }
    } else if (card->type == 3) {

        for (int m = 0; m < p->move_count; m++) {
            if (p->moves[m].id == card->id && !strcmp(MOVE_DB[card->id].id, "vine-whip")) {
                if (p->moves[m].mod.count < MAX_WHIP_EFFECTS - 1) p->moves[m].mod.count++;
                break;
            }
        }
    } else if (card->type == 4) {

        p->hp = fminf(p->max_hp, p->hp + 30.0f);
        game_spawn_damage_popup(p->x, p->y - 26.0f, 30, RGBA8_C(75, 255, 130, 255));
    } else if (card->type == 5) {

        int bag_c = (int)(25.0f * get_mode_coin_mult());
        g_game.coins_collected += bag_c;
    } else {

        if (p->passives[card->id] < PASSIVE_CATALOG[card->id].max_level) {
            p->passives[card->id]++;
            player_recalc_stats(p);
        }
    }
    audio_play_sfx(SFX_SELECT);
}

static void menu_world_update(float dt) {
    g_game.menu_cam_x += 16.0f * dt;
    g_game.menu_cam_y += 6.0f * dt;

    for (int w = 0; w < MAX_WANDERERS; w++) {
        MenuWanderer *mw = &g_game.wanderers[w];
        if (mw->wait_timer > 0.0f) {
            mw->wait_timer -= dt;
            mw->anim_type = ANIM_IDLE;
            mw->anim_timer += dt * 5.0f;
            if (mw->anim_timer >= 1.0f) {
                mw->anim_timer -= 1.0f;
                mw->frame++;
            }
            if (mw->wait_timer <= 0.0f) {
                mw->target_x = 80.0f + (float)(rand() % 800);
                mw->target_y = 80.0f + (float)(rand() % 380);
            }
        } else {
            float dx = mw->target_x - mw->x;
            float dy = mw->target_y - mw->y;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < 5.0f) {
                mw->wait_timer = 1.5f + (float)(rand() % 20) / 10.0f;
            } else {
                mw->anim_type = ANIM_WALK;
                mw->x += (dx / d) * mw->speed * dt;
                mw->y += (dy / d) * mw->speed * dt;
                float ang = atan2f(dy, dx);
                int nd = (int)floorf((1.57079632679f - ang) / 0.78539816339f + 0.5f);
                mw->dir = ((nd % 8) + 8) % 8;

                mw->anim_timer += dt * 8.0f;
                if (mw->anim_timer >= 1.0f) {
                    mw->anim_timer -= 1.0f;
                    mw->frame++;
                }
            }
        }
    }
}

static void render_top_bar(const char *screen_title) {

    float py = 8.0f;
    float ph = 30.0f;

    float c_w = 135.0f;
    render_draw_rect(20.0f, py, c_w, ph, RGBA8_C(24, 52, 44, 235));
    render_draw_rect_outline(20.0f, py, c_w, ph, RGBA8_C(65, 115, 95, 255), 1.5f);
    render_draw_ui_texture(UI_TEX_POUCH, 24.0f, py + 4.0f, 0.38f);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d", g_save.coins);
    render_draw_box_text_left(52.0f, py, c_w - 56.0f, ph, 0.0f, 1.40f, RGBA8_C(255, 225, 75, 255), buf);

    int owned_cnt = 0;
    for (int i = 0; i < TOTAL_CATALOG_POKEMON; i++) {
        if (save_system_is_owned(i)) owned_cnt++;
    }
    float b_x = 165.0f, b_w = 140.0f;
    render_draw_rect(b_x, py, b_w, ph, RGBA8_C(24, 52, 44, 235));
    render_draw_rect_outline(b_x, py, b_w, ph, RGBA8_C(65, 115, 95, 255), 1.5f);
    render_draw_ui_texture(UI_TEX_BALL, b_x + 4.0f, py + 4.0f, 0.38f);
    snprintf(buf, sizeof(buf), "%d/%d", owned_cnt, TOTAL_CATALOG_POKEMON);
    render_draw_box_text_left(b_x + 32.0f, py, b_w - 36.0f, ph, 0.0f, 1.40f, RGBA8_C(240, 248, 240, 255), buf);

    float t_x = 315.0f, t_w = 120.0f;
    render_draw_rect(t_x, py, t_w, ph, RGBA8_C(24, 52, 44, 235));
    render_draw_rect_outline(t_x, py, t_w, ph, RGBA8_C(65, 115, 95, 255), 1.5f);
    render_draw_rect(t_x + 6.0f, py + 7.0f, 20.0f, 16.0f, RGBA8_C(255, 205, 45, 255));
    render_draw_rect_outline(t_x + 6.0f, py + 7.0f, 20.0f, 16.0f, RGBA8_C(185, 140, 25, 255), 1.0f);
    snprintf(buf, sizeof(buf), "%d", g_save.tickets);
    render_draw_box_text_left(t_x + 32.0f, py, t_w - 36.0f, ph, 0.0f, 1.40f, RGBA8_C(255, 225, 75, 255), buf);

    if (screen_title) {
        render_draw_box_text_centered(450.0f, py, SCREEN_W - 460.0f, ph, 1.6f, RGBA8_C(255, 225, 60, 255), screen_title);
    }
}

void game_update(float dt, const InputState *input) {
    InputState rin = *input;
    if (g_game.state == STATE_PLAYING) {
        float yw = render_camera_yaw() * 0.01745329f;
        if (yw != 0.0f) {
            float mx = rin.move_x, my = rin.move_y;
            rin.move_x = cosf(yw) * mx + sinf(yw) * my;
            rin.move_y = -sinf(yw) * mx + cosf(yw) * my;
        }
    }
    if (g_game.state == STATE_PLAYING || g_game.state == STATE_PAUSED) {
        if (input->cam_preset_pressed) render_camera_next_preset();
        render_camera_adjust(dt, input->cam_yaw, input->cam_tilt, input->cam_zoom);
    }
    input = &rin;
    if (g_game.screen_shake > 0.0f && g_game.state == STATE_PLAYING) {
        g_game.screen_shake -= dt * 2.0f;
        if (g_game.screen_shake < 0.0f) g_game.screen_shake = 0.0f;
    }

    if (g_game.grace_timer > 0.0f) {
        g_game.grace_timer -= dt;
        if (g_game.grace_timer < 0.0f) g_game.grace_timer = 0.0f;
    }

    if (g_game.evolution_banner_timer > 0.0f) {
        g_game.evolution_banner_timer -= dt;
        if (g_game.evolution_banner_timer < 0.0f) g_game.evolution_banner_timer = 0.0f;
    }

    if (g_game.state <= STATE_MENU_SETTINGS) {
        menu_world_update(dt);
    }

    switch (g_game.state) {

        case STATE_MENU_HUB: {
            if (input->btn_left_pressed) {
                g_game.selected_stage = (g_game.selected_stage - 1 + TOTAL_STAGES) % TOTAL_STAGES;
                render_set_stage(g_game.selected_stage);
                spawn_world_props_for_stage(g_game.selected_stage);
                audio_play_sfx(SFX_SELECT);
            }
            if (input->btn_right_pressed) {
                g_game.selected_stage = (g_game.selected_stage + 1) % TOTAL_STAGES;
                render_set_stage(g_game.selected_stage);
                spawn_world_props_for_stage(g_game.selected_stage);
                audio_play_sfx(SFX_SELECT);
            }
            if (input->btn_up_pressed) {
                g_game.hub_selected_idx = (g_game.hub_selected_idx - 1 + 6) % 6;
                audio_play_sfx(SFX_SELECT);
            }
            if (input->btn_down_pressed) {
                g_game.hub_selected_idx = (g_game.hub_selected_idx + 1) % 6;
                audio_play_sfx(SFX_SELECT);
            }
            if (input->btn_confirm_pressed && g_game.grace_timer <= 0.0f) {
                audio_play_sfx(SFX_SELECT);
                g_game.grace_timer = 0.25f;
                switch (g_game.hub_selected_idx) {
                    case 0:
                        g_game.roster_from_modes = false;
                        if (g_game.modes_selected_idx >= TOTAL_RUN_MODES) g_game.modes_selected_idx = 0;
                        g_game.state = STATE_MENU_MODES;
                        break;
                    case 1:
                        g_game.roster_from_modes = false;
                        g_game.roster_tab = 0;
                        if (!save_system_is_owned(g_game.roster_selected_idx)) {
                            g_game.roster_selected_idx = save_system_is_owned(g_game.selected_starter) ? g_game.selected_starter : 0;
                        }
                        g_game.state = STATE_MENU_ROSTER;
                        break;
                    case 2:
                        g_game.state = STATE_MENU_SHOP;
                        break;
                    case 3:
                        g_game.state = STATE_MENU_GACHA;
                        break;
                    case 4:
                        g_game.state = STATE_MENU_MISSIONS;
                        break;
                    case 5:
                        g_game.state = STATE_MENU_SETTINGS;
                        break;
                }
            }
            break;
        }

        case STATE_MENU_MODES: {
            if (input->btn_cancel_pressed) {
                g_game.state = STATE_MENU_HUB;
                audio_play_bgm(BGM_TITLE);
                g_game.grace_timer = 0.25f;
                audio_play_sfx(SFX_SELECT);
                break;
            }
            bool is_coming_soon = (g_game.modes_selected_idx == MODE_COMING_SOON);
            bool has_save = !is_coming_soon && game_has_saved_run(g_game.modes_selected_idx);
            bool touch_poke_card = (input->touch_pressed &&
                                    input->touch_x >= 38.0f && input->touch_x <= 386.0f &&
                                    input->touch_y >= 380.0f && input->touch_y <= 486.0f);
            if ((input->btn_triangle_pressed || touch_poke_card) && g_game.grace_timer <= 0.0f) {
                g_game.roster_from_modes = true;
                g_game.roster_tab = 0;
                g_game.roster_selected_idx = save_system_is_owned(g_game.selected_starter) ? g_game.selected_starter : 0;
                g_game.state = STATE_MENU_ROSTER;
                g_game.grace_timer = 0.20f;
                audio_play_sfx(SFX_SELECT);
                break;
            }
            if (has_save && input->btn_square_pressed && g_game.grace_timer <= 0.0f) {

                game_reset_run(g_game.selected_starter, g_game.modes_selected_idx);
                break;
            }
            if (input->btn_left_pressed) {
                g_game.selected_stage = (g_game.selected_stage - 1 + TOTAL_STAGES) % TOTAL_STAGES;
                render_set_stage(g_game.selected_stage);
                spawn_world_props_for_stage(g_game.selected_stage);
                audio_play_sfx(SFX_SELECT);
            }
            if (input->btn_right_pressed) {
                g_game.selected_stage = (g_game.selected_stage + 1) % TOTAL_STAGES;
                render_set_stage(g_game.selected_stage);
                spawn_world_props_for_stage(g_game.selected_stage);
                audio_play_sfx(SFX_SELECT);
            }
            if (input->btn_up_pressed) {
                g_game.modes_selected_idx = (g_game.modes_selected_idx - 1 + TOTAL_RUN_MODES) % TOTAL_RUN_MODES;
                audio_play_sfx(SFX_SELECT);
            }
            if (input->btn_down_pressed) {
                g_game.modes_selected_idx = (g_game.modes_selected_idx + 1) % TOTAL_RUN_MODES;
                audio_play_sfx(SFX_SELECT);
            }
            if (input->btn_confirm_pressed && g_game.grace_timer <= 0.0f) {
                if (is_coming_soon) {
                    audio_play_sfx(SFX_SELECT);
                } else if (has_save) {

                    if (!game_resume_saved_run(g_game.modes_selected_idx)) {
                        game_reset_run(g_game.selected_starter, g_game.modes_selected_idx);
                    }
                } else {
                    game_reset_run(g_game.selected_starter, g_game.modes_selected_idx);
                }
            }
            break;
        }

        case STATE_MENU_ROSTER: {
            if (g_game.roster_from_modes) {
                g_game.roster_tab = 0;
            }
            if (input->btn_cancel_pressed) {
                if (g_game.roster_from_modes) {
                    g_game.roster_from_modes = false;
                    g_game.state = STATE_MENU_MODES;
                } else {
                    g_game.state = STATE_MENU_HUB;
                    audio_play_bgm(BGM_TITLE);
                }
                g_game.grace_timer = 0.25f;
                audio_play_sfx(SFX_SELECT);
                break;
            }

            int tab_req = -1;
            if (!g_game.roster_from_modes) {
                if (input->btn_l_pressed) tab_req = 0;
                if (input->btn_r_pressed) tab_req = 1;
                if (input->btn_triangle_pressed) tab_req = (g_game.roster_tab == 0) ? 1 : 0;
                if (input->touch_pressed && input->touch_y >= 58.0f && input->touch_y <= 92.0f) {
                    if (input->touch_x >= 440.0f && input->touch_x <= 675.0f) tab_req = 0;
                    else if (input->touch_x >= 685.0f && input->touch_x <= 920.0f) tab_req = 1;
                }
            }
            if (tab_req >= 0 && tab_req != g_game.roster_tab) {
                g_game.roster_tab = tab_req;
                audio_play_sfx(SFX_SELECT);
                if (g_game.roster_tab == 0 && !save_system_is_owned(g_game.roster_selected_idx)) {
                    g_game.roster_selected_idx = save_system_is_owned(g_game.selected_starter) ? g_game.selected_starter : 0;
                }
            }

            int vis_list[TOTAL_CATALOG_POKEMON];
            int vis_n = 0;
            for (int i = 0; i < TOTAL_CATALOG_POKEMON; i++) {
                if (g_game.roster_tab == 0 && !save_system_is_owned(i)) continue;
                vis_list[vis_n++] = i;
            }
            if (vis_n == 0) vis_list[vis_n++] = 0;

            int cur_pos = 0;
            for (int p = 0; p < vis_n; p++) {
                if (vis_list[p] == g_game.roster_selected_idx) { cur_pos = p; break; }
            }
            g_game.roster_selected_idx = vis_list[cur_pos];

            if (input->btn_left_pressed) {
                cur_pos = (cur_pos - 1 + vis_n) % vis_n;
                g_game.roster_selected_idx = vis_list[cur_pos];
            }
            if (input->btn_right_pressed) {
                cur_pos = (cur_pos + 1) % vis_n;
                g_game.roster_selected_idx = vis_list[cur_pos];
            }
            if (input->btn_up_pressed) {
                cur_pos = (cur_pos - 5 + vis_n) % vis_n;
                g_game.roster_selected_idx = vis_list[cur_pos];
            }
            if (input->btn_down_pressed) {
                cur_pos = (cur_pos + 5) % vis_n;
                g_game.roster_selected_idx = vis_list[cur_pos];
            }
            if (input->btn_square_pressed) {
                int step = (vis_n > 20) ? 20 : vis_n;
                cur_pos = (cur_pos + step) % vis_n;
                g_game.roster_selected_idx = vis_list[cur_pos];
            }
            if (input->btn_confirm_pressed && g_game.grace_timer <= 0.0f) {
                if (save_system_is_owned(g_game.roster_selected_idx)) {
                    g_game.selected_starter = g_game.roster_selected_idx;
                    g_save.selected_starter = g_game.roster_selected_idx;
                    save_system_save();
                    audio_play_sfx(SFX_SELECT);
                    if (g_game.roster_from_modes) {
                        g_game.roster_from_modes = false;
                        g_game.state = STATE_MENU_MODES;
                        g_game.grace_timer = 0.25f;
                    }
                } else if (!g_game.roster_from_modes && save_system_unlock_with_tickets(g_game.roster_selected_idx)) {
                    g_game.selected_starter = g_game.roster_selected_idx;
                    g_save.selected_starter = g_game.roster_selected_idx;
                    save_system_save();
                    audio_play_sfx(SFX_LEVELUP);
                } else {
                    audio_play_sfx(SFX_HIT);
                }
            }
            break;
        }

        case STATE_MENU_SHOP: {
            if (input->btn_cancel_pressed) {
                g_game.state = STATE_MENU_HUB;
                audio_play_bgm(BGM_TITLE);
                g_game.grace_timer = 0.25f;
                audio_play_sfx(SFX_SELECT);
                break;
            }
            if (input->btn_up_pressed) {
                g_game.shop_selected_idx = (g_game.shop_selected_idx - 1 + TOTAL_UPGRADES) % TOTAL_UPGRADES;
                audio_play_sfx(SFX_SELECT);
            }
            if (input->btn_down_pressed) {
                g_game.shop_selected_idx = (g_game.shop_selected_idx + 1) % TOTAL_UPGRADES;
                audio_play_sfx(SFX_SELECT);
            }
            if (input->btn_confirm_pressed && g_game.grace_timer <= 0.0f) {
                if (save_system_buy_upgrade(g_game.shop_selected_idx)) {
                    audio_play_sfx(SFX_LEVELUP);
                } else {
                    audio_play_sfx(SFX_HIT);
                }
            }
            break;
        }

        case STATE_MENU_GACHA: {
            if (input->btn_cancel_pressed && !g_game.gacha_pulling) {
                g_game.state = STATE_MENU_HUB;
                audio_play_bgm(BGM_TITLE);
                g_game.grace_timer = 0.25f;
                audio_play_sfx(SFX_SELECT);
                break;
            }
            if (g_game.gacha_pulling) {
                g_game.gacha_timer += dt;
                if (g_game.gacha_timer >= 1.6f) {
                    g_game.gacha_pulling = false;
                    audio_play_sfx(g_game.gacha_is_new ? SFX_LEVELUP : SFX_SELECT);
                }
            } else if (input->btn_confirm_pressed && g_game.grace_timer <= 0.0f) {

                if (g_save.tickets >= 1) {
                    g_save.tickets -= 1;
                    g_game.gacha_pulling = true;
                    g_game.gacha_timer = 0.0f;
                    g_game.gacha_result_idx = save_system_gacha_pull_ex(0, &g_game.gacha_is_new, &g_game.gacha_refund_coins);
                    audio_play_sfx(SFX_ATTACK);
                } else {
                    audio_play_sfx(SFX_HIT);
                }
            } else if (input->btn_square_pressed && g_game.grace_timer <= 0.0f) {

                if (g_save.tickets >= 10) {
                    g_save.tickets -= 10;
                    g_game.gacha_pulling = true;
                    g_game.gacha_timer = 0.0f;
                    g_game.gacha_result_idx = save_system_gacha_pull_ex(1, &g_game.gacha_is_new, &g_game.gacha_refund_coins);
                    audio_play_sfx(SFX_ATTACK);
                } else {
                    audio_play_sfx(SFX_HIT);
                }
            }
            break;
        }

        case STATE_MENU_MISSIONS: {
            if (input->btn_cancel_pressed) {
                g_game.state = STATE_MENU_HUB;
                audio_play_bgm(BGM_TITLE);
                g_game.grace_timer = 0.25f;
                audio_play_sfx(SFX_SELECT);
                break;
            }
            if (input->btn_up_pressed) {
                g_game.missions_selected_idx = (g_game.missions_selected_idx - 1 + TOTAL_GOALS) % TOTAL_GOALS;
                audio_play_sfx(SFX_SELECT);
            }
            if (input->btn_down_pressed) {
                g_game.missions_selected_idx = (g_game.missions_selected_idx + 1) % TOTAL_GOALS;
                audio_play_sfx(SFX_SELECT);
            }
            if (input->btn_confirm_pressed && g_game.grace_timer <= 0.0f) {
                if (save_system_claim_goal(g_game.missions_selected_idx)) {
                    audio_play_sfx(SFX_LEVELUP);
                } else {
                    audio_play_sfx(SFX_HIT);
                }
            }
            break;
        }

        case STATE_MENU_SETTINGS: {
            if (input->btn_cancel_pressed || (input->btn_confirm_pressed && g_game.settings_selected_idx == 2)) {
                g_game.state = STATE_MENU_HUB;
                audio_play_bgm(BGM_TITLE);
                g_game.grace_timer = 0.25f;
                audio_play_sfx(SFX_SELECT);
                break;
            }
            if (input->btn_up_pressed) {
                g_game.settings_selected_idx = (g_game.settings_selected_idx - 1 + 3) % 3;
                audio_play_sfx(SFX_SELECT);
            }
            if (input->btn_down_pressed) {
                g_game.settings_selected_idx = (g_game.settings_selected_idx + 1) % 3;
                audio_play_sfx(SFX_SELECT);
            }
            static float s_saved_bgm_vol = 1.0f;
            static float s_saved_sfx_vol = 1.0f;
            int delta = (input->btn_right_pressed ? 1 : 0) - (input->btn_left_pressed ? 1 : 0);
            if (g_game.settings_selected_idx == 0 && (delta != 0 || input->btn_confirm_pressed)) {
                float v = audio_get_bgm_volume();
                if (delta != 0) {
                    v = floorf((v + (float)delta * 0.10f) * 10.0f + 0.5f) * 0.10f;
                    if (v < 0.0f) v = 0.0f;
                    if (v > 1.0f) v = 1.0f;
                    if (v > 0.05f) s_saved_bgm_vol = v;
                } else {
                    if (v > 0.05f) { s_saved_bgm_vol = v; v = 0.0f; }
                    else { v = (s_saved_bgm_vol > 0.05f) ? s_saved_bgm_vol : 1.0f; }
                }
                audio_set_bgm_volume(v);
                audio_play_sfx(SFX_SELECT);
            } else if (g_game.settings_selected_idx == 1 && (delta != 0 || input->btn_confirm_pressed)) {
                float v = audio_get_sfx_volume();
                if (delta != 0) {
                    v = floorf((v + (float)delta * 0.10f) * 10.0f + 0.5f) * 0.10f;
                    if (v < 0.0f) v = 0.0f;
                    if (v > 1.0f) v = 1.0f;
                    if (v > 0.05f) s_saved_sfx_vol = v;
                } else {
                    if (v > 0.05f) { s_saved_sfx_vol = v; v = 0.0f; }
                    else { v = (s_saved_sfx_vol > 0.05f) ? s_saved_sfx_vol : 1.0f; }
                }
                audio_set_sfx_volume(v);
                audio_play_sfx(SFX_SELECT);
            }
            break;
        }

        case STATE_PLAYING: {
            for (int sb = 0; sb < 4; sb++) g_game.skill_pressed[sb] = input->btn_skill_pressed[sb];
            int dash_n = 0;
            for (int m = 0; m < g_game.player.move_count; m++) {
                if (MOVE_DB[g_game.player.moves[m].id].kind == MK_DASH && dash_n < 4) dash_n++;
            }
            if (input->touch_pressed && dash_n > 0) {
                for (int s = 0; s < dash_n; s++) {
                    float bw = 68.0f, bh = 40.0f;
                    float bx = SCREEN_W - 19.0f - (float)(s + 1) * bw - (float)s * 8.0f;
                    float by = SCREEN_H - 51.0f;
                    if (input->touch_x >= bx && input->touch_x <= bx + bw &&
                        input->touch_y >= by && input->touch_y <= by + bh) {
                        g_game.skill_pressed[s] = true;
                    }
                }
            }
            if (input->btn_pause_pressed && g_game.grace_timer <= 0.0f) {
                g_game.state = STATE_PAUSED;
                g_game.pause_selected_idx = 0;
                g_game.pause_sub_menu = 0;
                g_game.grace_timer = 0.3f;
                audio_play_sfx(SFX_SELECT);
                break;
            }

            const float play_dt = dt * 0.78f;
            const float clock_dt = (g_game.run_mode == MODE_TURBO) ? (dt * 2.0f) : dt;

            g_game.run_timer += clock_dt;
            g_game.spawn_timer += play_dt;
            if (g_game.revive_anim_timer > 0.0f) {
                g_game.revive_anim_timer -= dt;
                if (g_game.revive_anim_timer < 0.0f) g_game.revive_anim_timer = 0.0f;
            }
            if (g_game.reaper_banner_timer > 0.0f) {
                g_game.reaper_banner_timer -= dt;
                if (g_game.reaper_banner_timer < 0.0f) g_game.reaper_banner_timer = 0.0f;
            }

            g_game.cam_x = g_game.player.x - SCREEN_W * 0.5f;
            g_game.cam_y = g_game.player.y - SCREEN_H * 0.5f - 48.0f;

            player_update(&g_game.player, play_dt, input);

            bool has_boss = false;
            for (int e = 0; e < MAX_ENEMIES; e++) {
                if (g_game.enemies[e].active && g_game.enemies[e].is_boss) {
                    has_boss = true;
                    break;
                }
            }
            g_game.boss_active = has_boss;

            if (!has_boss && g_game.run_mode == MODE_BOSSRUSH && !g_game.final_reaper_spawned) {
                g_game.next_boss_timer -= play_dt;
                if (g_game.next_boss_timer <= 0.0f) {
                    snprintf(g_game.current_boss_name, sizeof(g_game.current_boss_name), "%s", g_game.next_boss_name);
                    enemies_spawn_boss_dex(g_game.next_boss_dex, &g_game.player);
                    g_game.boss_active = true;
                    g_game.next_boss_dex = game_pick_next_boss(g_game.next_boss_name, sizeof(g_game.next_boss_name));
                    render_preload_pokemon(g_game.next_boss_dex);
                    g_game.next_boss_timer = 3.0f;
                }
            }

            if (!has_boss && g_game.run_mode != MODE_BOSSRUSH && !g_game.final_reaper_spawned) {
                g_game.wave_clock += clock_dt;
                if (g_game.wave_clock > 899.0f) g_game.wave_clock = 899.0f;

                if (g_game.wave_clock >= 300.0f && !g_game.swarm_triggered) {
                    g_game.swarm_triggered = true;
                    enemies_spawn_swarm_event(&g_game.player);
                }

                g_game.next_boss_timer -= clock_dt;
                if (g_game.next_boss_timer <= 0.0f) {
                    int boss_mult = (g_game.run_mode == MODE_ENDLESS) ? (1 + g_game.endless_cycle) : 1;
                    if (boss_mult > 5) boss_mult = 5;
                    if (boss_mult > 1) {
                        if (g_game.next_boss_dex == 493)
                            snprintf(g_game.current_boss_name, sizeof(g_game.current_boss_name), "ARCEUS x%d", boss_mult);
                        else
                            snprintf(g_game.current_boss_name, sizeof(g_game.current_boss_name), "%s +%d BOSSES", g_game.next_boss_name, boss_mult - 1);
                    } else {
                        snprintf(g_game.current_boss_name, sizeof(g_game.current_boss_name), "%s", g_game.next_boss_name);
                    }
                    enemies_spawn_boss_dex(g_game.next_boss_dex, &g_game.player);
                    for (int bm_i = 1; bm_i < boss_mult; bm_i++) {
                        int extra_dex = (g_game.next_boss_dex == 493) ? 493 : game_pick_next_boss(NULL, 0);
                        enemies_spawn_boss_dex(extra_dex, &g_game.player);
                    }
                    g_game.boss_active = true;
                    g_game.next_boss_dex = game_pick_next_boss(g_game.next_boss_name, sizeof(g_game.next_boss_name));
                    render_preload_pokemon(g_game.next_boss_dex);
                    g_game.next_boss_timer = 120.0f;
                }
            }

            float cur_spawn_delay = enemies_get_current_spawn_delay(g_game.wave_clock);
            if (g_game.spawn_timer >= cur_spawn_delay) {
                g_game.spawn_timer = 0.0f;
                enemies_spawn_wave(g_game.wave_clock, &g_game.player);
            }

            g_game.next_heal_timer -= clock_dt;
            if (g_game.next_heal_timer <= 0.0f) {
                game_spawn_heal_station();
                g_game.next_heal_timer = 120.0f;
            }

            enemies_update(play_dt, &g_game.player);
            moves_update(play_dt, &g_game.player);

            if (g_game.run_mode != MODE_BOSSRUSH && !g_game.boss_active) {
                g_game.bgm_rotate_timer -= play_dt;
                if (g_game.bgm_rotate_timer <= 0.0f) {
                    game_play_random_battle_bgm();
                }
            }

            for (int i = 0; i < MAX_GEMS; i++) {
                ExpGem *gem = &g_game.gems[i];
                if (!gem->active) continue;

                float dx = g_game.player.x - gem->x;
                float dy = g_game.player.y - gem->y;
                float d = sqrtf(dx * dx + dy * dy);

                if ((gem->magnetized || d < g_game.player.magnet_radius) && d > 1.0f) {
                    float spd = gem->magnetized ? 760.0f : (360.0f + (g_game.player.magnet_radius - d) * 2.0f);
                    gem->x += (dx / d) * spd * play_dt;
                    gem->y += (dy / d) * spd * play_dt;
                }

                if (d < 24.0f) {
                    gem->active = false;
                    int exp_gain = (gem->exp > 0) ? gem->exp : ((gem->value >= 3) ? 25 : ((gem->value == 2) ? 10 : 3));

                    exp_gain = (int)ceilf((float)exp_gain * (g_game.player.growth_mult > 0.5f ? g_game.player.growth_mult : 1.0f));
                    g_game.player.exp += exp_gain;
                    audio_play_sfx(SFX_SELECT);

                    if (g_game.player.exp >= g_game.player.exp_to_next) {
                        g_game.player.level++;
                        g_game.player.exp -= g_game.player.exp_to_next;

                        if (g_game.player.level == 10 || g_game.player.level == 20 ||
                            g_game.player.level == 30 || g_game.player.level == 40) {
                            g_game.player.exp += 150;
                        }
                        g_game.player.exp_to_next = player_calc_xp_needed(g_game.player.level);
                        player_check_evolution(&g_game.player);
                        game_generate_level_up_cards();
                        g_game.state = STATE_LEVEL_UP;
                        g_game.grace_timer = 0.4f;
                        audio_play_sfx(SFX_LEVELUP);
                        break;
                    }
                }
            }

            for (int i = 0; i < MAX_PICKUPS; i++) {
                PickupItem *pk = &g_game.pickups[i];
                if (!pk->active) continue;

                if (!pk->magnetized) {
                    pk->timer -= play_dt;
                    if (pk->timer <= 0.0f) { pk->active = false; continue; }
                }

                float dx = g_game.player.x - pk->x;
                float dy = g_game.player.y - pk->y;
                float d = sqrtf(dx * dx + dy * dy);

                if ((pk->magnetized || d < g_game.player.magnet_radius) && d > 1.0f) {
                    float spd = pk->magnetized ? 720.0f : (380.0f + (g_game.player.magnet_radius - d) * 1.5f);
                    if (spd < 320.0f) spd = 320.0f;
                    pk->x += (dx / d) * spd * play_dt;
                    pk->y += (dy / d) * spd * play_dt;
                }

                float pick_r = (pk->type == 6) ? 34.0f : 28.0f;
                if (d < pick_r) {
                    pk->active = false;
                    if (pk->type == 0) {
                        g_game.player.hp += g_game.player.max_hp * 0.45f;
                        if (g_game.player.hp > g_game.player.max_hp) g_game.player.hp = g_game.player.max_hp;
                        audio_play_sfx(SFX_SELECT);
                    } else if (pk->type == 1) {
                        game_trigger_chest_reward();
                    } else if (pk->type == 2) {
                        const float mag_area = 750.0f;
                        const float mag_area2 = mag_area * mag_area;
                        for (int g = 0; g < MAX_GEMS; g++) {
                            if (!g_game.gems[g].active) continue;
                            float gdx = g_game.gems[g].x - g_game.player.x;
                            float gdy = g_game.gems[g].y - g_game.player.y;
                            if (gdx * gdx + gdy * gdy <= mag_area2) g_game.gems[g].magnetized = true;
                        }
                        for (int j = 0; j < MAX_PICKUPS; j++) {
                            if (!g_game.pickups[j].active || j == i) continue;
                            float pdx = g_game.pickups[j].x - g_game.player.x;
                            float pdy = g_game.pickups[j].y - g_game.player.y;
                            if (pdx * pdx + pdy * pdy <= mag_area2) g_game.pickups[j].magnetized = true;
                        }
                        audio_play_sfx(SFX_LEVELUP);
                    } else if (pk->type == 3) {
                        int base_c = 5;
                        int coin_gain = (int)ceilf((float)base_c * get_mode_coin_mult());
                        g_game.coins_collected += coin_gain;
                        audio_play_sfx(SFX_SELECT);
                    } else if (pk->type == 4) {
                        g_game.player.level++;
                        g_game.player.exp = 0;
                        g_game.player.exp_to_next = player_calc_xp_needed(g_game.player.level);
                        player_recalc_stats(&g_game.player);
                        g_game.player.hp = g_game.player.max_hp;
                        player_check_evolution(&g_game.player);
                        game_spawn_damage_popup(g_game.player.x, g_game.player.y - 32.0f, 1, RGBA8_C(255, 220, 50, 255));
                        game_spawn_particles(g_game.player.x, g_game.player.y, RGBA8_C(255, 225, 60, 255), 35, 190.0f);
                        audio_play_sfx(SFX_LEVELUP);
                        game_generate_level_up_cards();
                        g_game.state = STATE_LEVEL_UP;
                        g_game.grace_timer = 0.4f;
                    } else if (pk->type == 5) {
                        g_game.tickets_collected++;
                        game_spawn_damage_popup(g_game.player.x, g_game.player.y - 28.0f, 1, RGBA8_C(255, 230, 55, 255));
                        game_spawn_particles(g_game.player.x, g_game.player.y, RGBA8_C(255, 215, 40, 255), 24, 160.0f);
                        audio_play_sfx(SFX_LEVELUP);
                    } else if (pk->type == 6) {
                        float heal_amt = fmaxf(g_game.player.max_hp * 0.60f, 45.0f);
                        g_game.player.hp += heal_amt;
                        if (g_game.player.hp > g_game.player.max_hp) g_game.player.hp = g_game.player.max_hp;
                        game_spawn_damage_popup(g_game.player.x, g_game.player.y - 30.0f, (int)ceilf(heal_amt), RGBA8_C(75, 255, 130, 255));
                        game_spawn_particles(g_game.player.x, g_game.player.y, RGBA8_C(70, 245, 125, 255), 28, 155.0f);
                        audio_play_sfx(SFX_LEVELUP);
                    } else if (pk->type == 7) {
                        g_game.revives_left++;
                        game_spawn_damage_popup(g_game.player.x, g_game.player.y - 30.0f, 1, RGBA8_C(255, 232, 75, 255));
                        game_spawn_particles(g_game.player.x, g_game.player.y, RGBA8_C(255, 224, 65, 255), 26, 165.0f);
                        game_spawn_particles(g_game.player.x, g_game.player.y, RGBA8_C(110, 255, 165, 255), 14, 125.0f);
                        audio_play_sfx(SFX_LEVELUP);
                    }
                }
            }

            for (int i = 0; i < MAX_POPUPS; i++) {
                DamagePopup *pop = &g_game.popups[i];
                if (!pop->active) continue;
                pop->timer -= play_dt;
                pop->y -= 25.0f * play_dt;
                if (pop->timer <= 0.0f) pop->active = false;
            }

            for (int i = 0; i < MAX_PARTICLES; i++) {
                Particle *pt = &g_game.particles[i];
                if (!pt->active) continue;
                pt->life -= play_dt;
                pt->x += pt->vx * play_dt;
                pt->y += pt->vy * play_dt;
                if (pt->life <= 0.0f) pt->active = false;
            }

            for (int i = 0; i < MAX_DASH_GHOSTS; i++) {
                DashGhost *dg = &g_game.dash_ghosts[i];
                if (!dg->active) continue;
                dg->life -= play_dt;
                if (dg->life <= 0.0f) dg->active = false;
            }
            break;
        }

        case STATE_CHEST_REWARD: {
            g_game.chest_timer += dt;
            if (input->btn_confirm_pressed || input->btn_cancel_pressed) {
                g_game.state = STATE_PLAYING;
                audio_play_sfx(SFX_SELECT);
            }
            break;
        }
        case STATE_LEVEL_UP: {
            if (input->btn_left_pressed) {
                g_game.selected_card = (g_game.selected_card - 1 + 3) % 3;
                audio_play_sfx(SFX_SELECT);
            }
            if (input->btn_right_pressed) {
                g_game.selected_card = (g_game.selected_card + 1) % 3;
                audio_play_sfx(SFX_SELECT);
            }

            if (input->btn_confirm_pressed && g_game.grace_timer <= 0.0f) {
                apply_upgrade_card(&g_game.cards[g_game.selected_card]);
                g_game.state = STATE_PLAYING;
                g_game.grace_timer = 0.3f;
            }

            if (input->btn_square_pressed) {
                int reroll_cost = 60 + g_game.run_rerolls_bought * 40;
                if (g_game.rerolls_left > 0) {
                    g_game.rerolls_left--;
                    game_generate_level_up_cards();
                    audio_play_sfx(SFX_SELECT);
                } else if (g_game.coins_collected >= reroll_cost) {
                    g_game.coins_collected -= reroll_cost;
                    g_game.run_rerolls_bought++;
                    game_generate_level_up_cards();
                    audio_play_sfx(SFX_SELECT);
                } else {
                    audio_play_sfx(SFX_HIT);
                }
            }

            if (input->btn_triangle_pressed) {
                g_game.state = STATE_PLAYING;
                g_game.grace_timer = 0.3f;
                audio_play_sfx(SFX_SELECT);
            }
            break;
        }

        case STATE_PAUSED: {
            if (g_game.pause_sub_menu == 0) {

                if (input->btn_up_pressed) {
                    g_game.pause_selected_idx = (g_game.pause_selected_idx - 1 + 4) % 4;
                    audio_play_sfx(SFX_SELECT);
                }
                if (input->btn_down_pressed) {
                    g_game.pause_selected_idx = (g_game.pause_selected_idx + 1) % 4;
                    audio_play_sfx(SFX_SELECT);
                }
                if (input->btn_confirm_pressed && g_game.grace_timer <= 0.0f) {
                    audio_play_sfx(SFX_SELECT);
                    if (g_game.pause_selected_idx == 0) {

                        g_game.state = STATE_PLAYING;
                        g_game.grace_timer = 0.25f;
                    } else if (g_game.pause_selected_idx == 1) {

                        g_game.pause_sub_menu = 1;
                        g_game.settings_selected_idx = 0;
                        g_game.grace_timer = 0.2f;
                    } else if (g_game.pause_selected_idx == 2) {

                        game_save_current_run();
                        save_system_save();
                        g_game.state = STATE_MENU_HUB;
                        audio_play_bgm(BGM_TITLE);
                        g_game.grace_timer = 0.35f;
                    } else if (g_game.pause_selected_idx == 3) {

                        game_delete_saved_run(g_game.run_mode);
                        g_save.coins += g_game.coins_collected;
                        g_save.tickets += g_game.tickets_collected;
                        save_system_save();
                        g_game.state = STATE_MENU_HUB;
                        audio_play_bgm(BGM_TITLE);
                        g_game.grace_timer = 0.35f;
                    }
                } else if ((input->btn_pause_pressed || input->btn_cancel_pressed) && g_game.grace_timer <= 0.0f) {

                    g_game.state = STATE_PLAYING;
                    g_game.grace_timer = 0.25f;
                    audio_play_sfx(SFX_SELECT);
                }
            } else {

                if (input->btn_up_pressed) {
                    g_game.settings_selected_idx = (g_game.settings_selected_idx - 1 + 3) % 3;
                    audio_play_sfx(SFX_SELECT);
                }
                if (input->btn_down_pressed) {
                    g_game.settings_selected_idx = (g_game.settings_selected_idx + 1) % 3;
                    audio_play_sfx(SFX_SELECT);
                }
                static float s_pause_bgm_vol = 1.0f;
                static float s_pause_sfx_vol = 1.0f;
                int delta = (input->btn_right_pressed ? 1 : 0) - (input->btn_left_pressed ? 1 : 0);
                if (g_game.settings_selected_idx == 0 && (delta != 0 || (input->btn_confirm_pressed && g_game.grace_timer <= 0.0f))) {
                    float v = audio_get_bgm_volume();
                    if (delta != 0) {
                        v = floorf((v + (float)delta * 0.10f) * 10.0f + 0.5f) * 0.10f;
                        if (v < 0.0f) v = 0.0f;
                        if (v > 1.0f) v = 1.0f;
                        if (v > 0.05f) s_pause_bgm_vol = v;
                    } else {
                        if (v > 0.05f) { s_pause_bgm_vol = v; v = 0.0f; }
                        else { v = (s_pause_bgm_vol > 0.05f) ? s_pause_bgm_vol : 1.0f; }
                    }
                    audio_set_bgm_volume(v);
                    audio_play_sfx(SFX_SELECT);
                } else if (g_game.settings_selected_idx == 1 && (delta != 0 || (input->btn_confirm_pressed && g_game.grace_timer <= 0.0f))) {
                    float v = audio_get_sfx_volume();
                    if (delta != 0) {
                        v = floorf((v + (float)delta * 0.10f) * 10.0f + 0.5f) * 0.10f;
                        if (v < 0.0f) v = 0.0f;
                        if (v > 1.0f) v = 1.0f;
                        if (v > 0.05f) s_pause_sfx_vol = v;
                    } else {
                        if (v > 0.05f) { s_pause_sfx_vol = v; v = 0.0f; }
                        else { v = (s_pause_sfx_vol > 0.05f) ? s_pause_sfx_vol : 1.0f; }
                    }
                    audio_set_sfx_volume(v);
                    audio_play_sfx(SFX_SELECT);
                } else if ((input->btn_cancel_pressed || input->btn_pause_pressed ||
                           (input->btn_confirm_pressed && g_game.settings_selected_idx == 2)) && g_game.grace_timer <= 0.0f) {
                    g_game.pause_sub_menu = 0;
                    g_game.grace_timer = 0.2f;
                    audio_play_sfx(SFX_SELECT);
                }
            }
            break;
        }

        case STATE_GAME_OVER:
        case STATE_VICTORY: {
            if ((input->btn_confirm_pressed || input->btn_pause_pressed) && g_game.grace_timer <= 0.0f) {
                g_game.state = STATE_MENU_HUB;
    audio_play_bgm(BGM_TITLE);
                g_game.grace_timer = 0.4f;
                audio_play_sfx(SFX_SELECT);
            }
            break;
        }
    }
}

static void render_menu_world_3d(void) {
    render_world_begin(g_game.menu_cam_x, g_game.menu_cam_y);
    render_draw_ground(g_game.menu_cam_x, g_game.menu_cam_y);
    for (int p = 0; p < MAX_PROPS; p++) {
        WorldProp *prop = &g_game.props[p];
        if (!prop->active) continue;
        float px = prop->x - g_game.menu_cam_x, py = prop->y - g_game.menu_cam_y;
        if (px < -240.0f || px > SCREEN_W + 240.0f || py < -240.0f || py > SCREEN_H + 320.0f) continue;
        if (prop->type != PROP_LILY) render_draw_world_prop(prop->type, px, py, is_tall_prop(prop->type) ? 1.1f : 1.40f);
    }
    for (int w = 0; w < MAX_WANDERERS; w++) {
        MenuWanderer *mw = &g_game.wanderers[w];
        render_draw_shadow(mw->x, mw->y, 14.0f);
        render_draw_pokemon_sprite(mw->dex, mw->anim_type, mw->x, mw->y, mw->frame, mw->dir, 1.0f, 0, 0);
    }
    render_world_end();
}

static void render_draw_wrapped_box_text(float bx, float by, float bw, float scale, float line_h, int max_lines, uint32_t col, const char *text);

static void ui_draw_pokemon_trading_card(float card_x, float card_y, float card_w, float card_h,
                                         const PokemonData *pm, int anim_frame, uint32_t sprite_tint, bool compact_for_btn) {
    char buf[128];
    bool is_locked = (sprite_tint != 0);
    int ptype_id = is_locked ? 0 : type_name_to_id(pm->type1);
    if (ptype_id < 0 || ptype_id > 17) ptype_id = 0;

    uint32_t base = (ptype_id == 0) ? RGBA8_C(168, 172, 182, 255) : MOVE_TYPE_COLOR[ptype_id];
    uint32_t card_border = ui_mix_white(base, 0.14f);
    uint32_t card_bg     = ui_mix_white(base, 0.70f);
    uint32_t well_bg     = RGBA8_C(255, 255, 255, 78);

    float win_x = card_x + 16.0f, win_y = card_y + 41.0f, win_w = card_w - 32.0f;
    float win_h = compact_for_btn ? 110.0f : 134.0f;
    float ix = win_x + 4.0f, iy = win_y + 4.0f, iw = win_w - 8.0f, ih = win_h - 8.0f;

    render_draw_rect(card_x + 8.0f, card_y + card_h, card_w - 12.0f, 4.0f, RGBA8_C(30, 42, 62, 55));
    render_draw_rect(card_x + card_w, card_y + 8.0f, 3.0f, card_h - 12.0f, RGBA8_C(30, 42, 62, 55));

    float cx0 = card_x + 4.0f, cy0 = card_y + 4.0f, cw0 = card_w - 8.0f, ch0 = card_h - 8.0f;

    render_draw_rect(card_x + 6.0f, card_y, card_w - 12.0f, 4.0f, card_border);
    render_draw_rect(card_x + 3.0f, card_y + 3.0f, 3.0f, 3.0f, card_border);
    render_draw_rect(card_x + card_w - 6.0f, card_y + 3.0f, 3.0f, 3.0f, card_border);
    render_draw_rect(card_x, card_y + 6.0f, 4.0f, card_h - 12.0f, card_border);
    render_draw_rect(card_x + card_w - 4.0f, card_y + 6.0f, 4.0f, card_h - 12.0f, card_border);
    render_draw_rect(card_x + 3.0f, card_y + card_h - 6.0f, 3.0f, 3.0f, card_border);
    render_draw_rect(card_x + card_w - 6.0f, card_y + card_h - 6.0f, 3.0f, 3.0f, card_border);
    render_draw_rect(card_x + 6.0f, card_y + card_h - 4.0f, card_w - 12.0f, 4.0f, card_border);

    render_draw_rect(cx0 + 2.0f, cy0, cw0 - 4.0f, 2.0f, card_bg);
    render_draw_rect(cx0, cy0 + 2.0f, cw0, iy - (cy0 + 2.0f), card_bg);
    render_draw_rect(cx0, iy, ix - cx0, ih, card_bg);
    render_draw_rect(ix + iw, iy, (cx0 + cw0) - (ix + iw), ih, card_bg);
    render_draw_rect(cx0, iy + ih, cw0, (cy0 + ch0 - 2.0f) - (iy + ih), card_bg);
    render_draw_rect(cx0 + 2.0f, cy0 + ch0 - 2.0f, cw0 - 4.0f, 2.0f, card_bg);

    float hdr_y = card_y + 11.0f;
    float pill_x = card_x + 14.0f, pill_w = 64.0f, pill_h = 24.0f;
    ui_pix_notch(pill_x, hdr_y + 2.0f, pill_w, pill_h, ui_darken(card_bg, 0.84f));
    ui_pix_notch(pill_x, hdr_y, pill_w, pill_h, UI_MODERN_WHITE);
    snprintf(buf, sizeof(buf), "#%03d", pm->dex);
    ui_text_center(0, pill_x, hdr_y, pill_w, pill_h, 1.12f, UI_MODERN_TEXT_DARK, buf);

    ui_text_mid(0, pill_x + pill_w + 10.0f, hdr_y + pill_h * 0.5f, 1.55f, UI_MODERN_TEXT_DARK, pm->name);

    if (is_locked) {
        ui_text_mid(0, card_x + card_w - 46.0f, hdr_y + pill_h * 0.5f, 1.25f, RGBA8_C(120, 128, 138, 255), "????");
    } else if (pm->type2 && pm->type2[0]) {
        render_draw_type_badge(pm->type1, card_x + card_w - 68.0f, hdr_y - 2.0f, 0.25f);
        render_draw_type_badge(pm->type2, card_x + card_w - 38.0f, hdr_y - 2.0f, 0.25f);
    } else {
        render_draw_type_badge(pm->type1 ? pm->type1 : "Normal", card_x + card_w - 38.0f, hdr_y - 2.0f, 0.25f);
    }

    render_draw_rect(ix, iy, iw, ih, well_bg);

    render_draw_rect(win_x + 4.0f, win_y, win_w - 8.0f, 4.0f, UI_MODERN_WHITE);
    render_draw_rect(win_x + 4.0f, iy + ih, win_w - 8.0f, 4.0f, UI_MODERN_WHITE);
    render_draw_rect(win_x, win_y + 4.0f, 4.0f, win_h - 8.0f, UI_MODERN_WHITE);
    render_draw_rect(ix + iw, win_y + 4.0f, 4.0f, win_h - 8.0f, UI_MODERN_WHITE);
    ui_pix_corners(ix, iy, iw, ih, UI_MODERN_WHITE);
    render_draw_rect(win_x + 4.0f, win_y + win_h, win_w - 8.0f, 3.0f, ui_darken(card_bg, 0.80f));

    float ped_cx = ix + iw * 0.5f;
    float ped_y  = iy + ih - (compact_for_btn ? 22.0f : 26.0f);
    render_draw_ui_texture_centered(UI_TEX_PEDESTAL, ped_cx, ped_y, compact_for_btn ? 0.56f : 0.64f);
    float pmax = (float)(pm->walk.w > pm->walk.h ? pm->walk.w : pm->walk.h);
    if (pmax < 16.0f) pmax = 16.0f;
    float psc = (pmax > 46.0f ? (46.0f / pmax) : 1.0f) * (compact_for_btn ? 1.72f : 1.96f);
    render_draw_pokemon_sprite(pm->dex, ANIM_IDLE, ped_cx, ped_y + 2.0f, anim_frame, 0, psc, 0, sprite_tint);

    float box_x = card_x + 16.0f, box_y = win_y + win_h + 8.0f, box_w = card_w - 32.0f;
    float box_h = card_h - (box_y - card_y) - (compact_for_btn ? 54.0f : 14.0f);
    ui_pix_notch(box_x, box_y + 2.0f, box_w, box_h, ui_darken(card_bg, 0.84f));
    ui_pix_notch(box_x, box_y, box_w, box_h, UI_MODERN_WHITE);

    if (is_locked) {
        snprintf(buf, sizeof(buf), "HP: ????    SPD: ????    ATK: ????");
    } else {
        float shop_hp_pct = (float)g_save.upgrades[0] * 0.05f;
        if (shop_hp_pct > 0.50f) shop_hp_pct = 0.50f;
        float shop_atk = (float)g_save.upgrades[1] * 0.04f;
        float shop_spd = (float)g_save.upgrades[2] * 0.03f;
        int eff_hp = (int)floorf((float)pm->base_hp * (1.0f + shop_hp_pct) + 0.5f);
        float eff_spd = pm->base_speed * (1.0f + shop_spd);
        float eff_atk = pm->base_atk * (1.0f + shop_atk);
        snprintf(buf, sizeof(buf), "HP: %d    SPD: %.0f    ATK: x%.2f", eff_hp, eff_spd, eff_atk);
    }
    ui_text_center(0, box_x, box_y + 6.0f, box_w, 24.0f, 1.32f, UI_MODERN_TEXT_DARK, buf);

    if (is_locked) {
        ui_text_center(0, box_x, box_y + 28.0f, box_w, 22.0f, 1.14f, RGBA8_C(120, 128, 138, 255), "Passive: ????");
    } else {
        const char *pd = pm->passive_desc ? pm->passive_desc : "+2.5 HP/sec Health Regen";
        const char *cut = strstr(pd, "/sec");
        cut = cut ? cut + 4 : strchr(pd, ' ');
        char pa[64];
        memset(pa, 0, sizeof(pa));
        if (cut) {
            int n = (int)(cut - pd);
            if (n > 63) n = 63;
            memcpy(pa, pd, (size_t)n);
        } else {
            snprintf(pa, sizeof(pa), "%s", pd);
        }
        float p_sc = 1.14f;
        while (p_sc > 0.78f && (float)ui_text_w(0, p_sc, pd) > box_w - 20.0f) p_sc -= 0.04f;
        float tot_w = (float)ui_text_w(0, p_sc, pd);
        float px0 = box_x + (box_w - tot_w) * 0.5f;
        ui_text_mid(0, px0, box_y + 39.0f, p_sc, RGBA8_C(42, 138, 68, 255), pa);
        if (cut) {
            ui_text_mid(0, px0 + (float)ui_text_w(0, p_sc, pa), box_y + 39.0f, p_sc, UI_MODERN_TEXT_DARK, cut);
        }
    }

    float div_y = box_y + 53.0f;
    for (float dx = box_x + 12.0f; dx < box_x + box_w - 18.0f; dx += 12.0f) {
        render_draw_rect(dx, div_y, 7.0f, 2.0f, RGBA8_C(196, 202, 210, 255));
    }

    {
        const float stat_x = box_x + 14.0f;
        const float ix0 = box_x + 114.0f;
        ui_text_mid(0, stat_x, box_y + 70.0f, 1.12f, RGBA8_C(180, 62, 62, 255), "Weak To:");
        ui_text_mid(0, stat_x, box_y + 95.0f, 1.12f, RGBA8_C(64, 120, 172, 255), "Resists:");
        ui_text_mid(0, stat_x, box_y + 120.0f, 1.12f, RGBA8_C(64, 145, 80, 255), "Strong vs:");

        if (is_locked) {
            ui_text_mid(0, ix0, box_y + 70.0f, 1.10f, RGBA8_C(120, 128, 138, 255), "????");
            ui_text_mid(0, ix0, box_y + 95.0f, 1.10f, RGBA8_C(120, 128, 138, 255), "????");
            ui_text_mid(0, ix0, box_y + 120.0f, 1.10f, RGBA8_C(120, 128, 138, 255), "????");
        } else {
            int dt1 = type_name_to_id(pm->type1);
            int dt2 = type_name_to_id(pm->type2);
            int weak_t[18], nw = 0;
            int res_t[18],  nr = 0;
            int eff_t[18],  ne = 0;
            for (int t = 0; t < 18; t++) {
                float def_m = type_effectiveness_dual(t, dt1, dt2);
                if (def_m >= 1.45f) weak_t[nw++] = t;
                else if (def_m <= 0.75f) res_t[nr++] = t;

                float atk_m1 = (dt1 >= 0) ? TYPE_CHART[dt1][t] : 1.0f;
                float atk_m2 = (dt2 >= 0) ? TYPE_CHART[dt2][t] : 1.0f;
                if (atk_m1 >= 1.45f || atk_m2 >= 1.45f) eff_t[ne++] = t;
            }
            const float isc = 0.20f;
            const float istep = 24.5f;
            const int max_icons = (int)((box_w - 124.0f) / istep);

            if (nw == 0) ui_text_mid(0, ix0, box_y + 70.0f, 1.02f, RGBA8_C(130, 136, 145, 255), "None");
            for (int k = 0; k < nw && k < max_icons; k++)
                render_draw_type_badge(MOVE_TYPE_NAMES[weak_t[k]], ix0 + (float)k * istep, box_y + 59.0f, isc);

            if (nr == 0) ui_text_mid(0, ix0, box_y + 95.0f, 1.02f, RGBA8_C(130, 136, 145, 255), "None");
            for (int k = 0; k < nr && k < max_icons; k++)
                render_draw_type_badge(MOVE_TYPE_NAMES[res_t[k]], ix0 + (float)k * istep, box_y + 84.0f, isc);

            if (ne == 0) ui_text_mid(0, ix0, box_y + 120.0f, 1.02f, RGBA8_C(130, 136, 145, 255), "None");
            for (int k = 0; k < ne && k < max_icons; k++)
                render_draw_type_badge(MOVE_TYPE_NAMES[eff_t[k]], ix0 + (float)k * istep, box_y + 109.0f, isc);
        }
    }

    float ex = box_x + 10.0f, ey = box_y + 137.0f, ew = box_w - 20.0f, eh = box_h - 145.0f;
    if (eh > 36.0f) {
        ui_pix_notch(ex, ey + 2.0f, ew, eh, RGBA8_C(224, 210, 188, 255));
        ui_pix_notch(ex, ey, ew, eh, RGBA8_C(250, 234, 208, 255));
        render_draw_text(ex + 10.0f, ey + 5.0f, 1.02f, RGBA8_C(202, 126, 42, 255), "POKEDEX ENTRY:");
        if (is_locked) {
            ui_text_center(0, ex + 10.0f, ey + 22.0f, ew - 20.0f, eh - 26.0f, 1.25f, RGBA8_C(120, 128, 138, 255), "????");
        } else {
            int max_l = (eh >= 88.0f) ? 3 : 2;
            float l_sc = (box_w >= 390.0f) ? 0.96f : 0.88f;
            render_draw_wrapped_box_text(ex + 10.0f, ey + 23.0f, ew - 20.0f, l_sc, 18.5f, max_l, UI_MODERN_TEXT_DARK, get_pokedex_description(pm->dex));
        }
    }
}

static void render_hub_overview_overlay(int sel_idx, bool is_pause) {
    char buf[128];
    int lead = is_pause ? g_game.player.char_idx : g_game.selected_starter;
    if (lead < 0 || lead >= TOTAL_CATALOG_POKEMON) lead = 0;
    const PokemonData *pm = &POKEMON_CATALOG[lead];

    render_draw_rect(0.0f, 0.0f, SCREEN_W, SCREEN_H, is_pause ? UI_MODERN_BG : UI_BG_FLAT);

    int owned_cnt = 0;
    for (int i = 0; i < TOTAL_CATALOG_POKEMON; i++) if (save_system_is_owned(i)) owned_cnt++;

    int total_coins = g_save.coins + (is_pause ? g_game.coins_collected : 0);
    char pbuf[3][48];
    snprintf(pbuf[0], 48, "%d Coin", total_coins);
    snprintf(pbuf[1], 48, "Pokemon: %d/%d", owned_cnt, TOTAL_CATALOG_POKEMON);
    snprintf(pbuf[2], 48, "%d Tickets", g_save.tickets);
    static const char *const picons[3] = { "[Coin]", "[Ball]", "[Ticket]" };
    float p_y = is_pause ? 40.0f : 12.0f, p_h = 36.0f, p_x = is_pause ? 48.0f : 108.0f, p_ts = 1.35f;
    { float tot = 3.0f * 55.0f + 26.0f;
      for (int pi = 0; pi < 3; pi++) tot += (float)ui_text_w(0, p_ts, pbuf[pi]);
      if (tot > 480.0f) p_ts *= (480.0f - 3.0f * 55.0f - 26.0f) / (tot - 3.0f * 55.0f - 26.0f); }
    for (int pi = 0; pi < 3; pi++) {
        float pw = 43.0f + (float)ui_text_w(0, p_ts, pbuf[pi]) + 12.0f;
        ui_draw_pill(p_x, p_y, pw, p_h, UI_MODERN_WHITE, UI_MODERN_BORDER);
        ui_text_mid(0, p_x + 8.0f, p_y + p_h * 0.5f, 2.4f, 0xFFFFFFFF, picons[pi]);
        ui_text_mid(0, p_x + 43.0f, p_y + p_h * 0.5f, p_ts, UI_MODERN_TEXT_DARK, pbuf[pi]);
        p_x += pw + 12.0f;
    }

    if (!is_pause) {
        static float hub_anim_t = 0.0f;
        hub_anim_t += 0.10f;
        int a_fr = (int)hub_anim_t % 6;

        ui_draw_pokemon_trading_card(107.0f, 54.0f, 368.0f, 430.0f, pm, a_fr, 0, false);

        float menu_x = 503.0f, menu_y = 54.0f, menu_w = 350.0f, menu_h = 430.0f;
        ui_draw_card(menu_x, menu_y, menu_w, menu_h, UI_MODERN_WHITE, UI_MODERN_BORDER, 2.0f, 13.0f);

        static const char *const main_labels[6] = {
            "PLAY", "POKEDEX", "UPGRADES SHOP", "GACHAPON SUMMON", "MISSIONS & GOALS", "SETTINGS"
        };
        static const char *const main_icons[6] = { "", "[Ball]", "[Coin]", "[Trophy]", "[Star]", "[Wrench]" };

        float btn_x = menu_x + 16.0f, btn_w = menu_w - 35.0f, btn_h = 54.0f, btn_start_y = menu_y + 18.0f, btn_gap = 13.5f;
        for (int i = 0; i < 6; i++) {
            float by = btn_start_y + (float)i * (btn_h + btn_gap);
            ui_draw_button(btn_x, by, btn_w, btn_h, main_labels[i], (i == sel_idx), (i == 5) ? 1 : 0, main_icons[i]);
        }
        ui_draw_footer_pill("[X] Confirm  |  [Left/Right] Stage  |  [Up/Down] Navigate");
    } else {

        float menu_w = 400.0f, menu_h = 326.0f;
        float menu_x = (SCREEN_W - menu_w) * 0.5f;
        float menu_y = (SCREEN_H - menu_h) * 0.5f - 8.0f;
        ui_draw_card(menu_x, menu_y, menu_w, menu_h, UI_MODERN_WHITE, UI_MODERN_BORDER, 2.0f, 14.0f);
        render_draw_text_pix_centered(menu_x + menu_w * 0.5f, menu_y + 14.0f, 1.55f, UI_MODERN_TEXT_DARK, "PAUSED");

        static const char *const pause_labels[4] = {
            "RESUME BATTLE",
            "AUDIO & SETTINGS",
            "SAVE & EXIT TO MENU",
            "ABANDON RUN & EXIT"
        };
        float btn_x = menu_x + 20.0f;
        float btn_w = menu_w - 40.0f;
        float btn_h = 46.0f;
        float btn_start_y = menu_y + 66.0f;
        float btn_gap = 12.0f;

        for (int i = 0; i < 4; i++) {
            float by = btn_start_y + (float)i * (btn_h + btn_gap);
            bool is_sel = (i == sel_idx);
            ui_draw_button(btn_x, by, btn_w, btn_h, pause_labels[i], is_sel, (i == 3) ? 1 : 0, "");
        }

        ui_draw_footer_pill("[X] Confirm  |  [O] Resume  |  [Up/Down] Navigate");
    }
}

static void render_menu_hub(void) {
    render_menu_world_3d();
    render_hub_overview_overlay(g_game.hub_selected_idx, false);
}

static void render_menu_modes(void) {
    render_menu_world_3d();
    int sav_cidx = 0, sav_lv = 1, sav_stg = 0;
    float sav_time = 0.0f;
    bool is_coming_soon = (g_game.modes_selected_idx == MODE_COMING_SOON);
    bool has_save = !is_coming_soon && game_get_saved_run_info(g_game.modes_selected_idx, &sav_cidx, &sav_lv, &sav_time, &sav_stg);
    const char *footer = is_coming_soon
        ? "[Tri] Change Pokemon  |  [Left/Right] Stage  |  [Up/Down] Mode  |  [O] Back"
        : (has_save
            ? "[X] Resume Run  |  [Sq] Start New  |  [Tri] Pokemon  |  [Left/Right] Stage  |  [Up/Down] Mode  |  [O] Back"
            : "[X] Start Run  |  [Tri] Change Pokemon  |  [Left/Right] Stage  |  [Up/Down] Mode  |  [O] Back");
    ui_gba_screen_chrome("SELECT GAME MODE & STAGE", footer);

    float lx = 24.0f, ly = 52.0f, lw = 376.0f, lh = 432.0f;
    ui_gba_win(lx, ly, lw, lh, UI_GBA_WIN_DEFAULT);
    ui_gba_section_strip(lx + 14.0f, ly + 14.0f, lw - 28.0f, 30.0f, "BATTLE MODE", true);

    static const char *mode_names[TOTAL_RUN_MODES] = {
        "Standard Mode",
        "Turbo Mode",
        "Boss Rush Gauntlet",
        "Endless Mode",
        "(More Modes Coming Soon)"
    };

    for (int i = 0; i < TOTAL_RUN_MODES; i++) {
        float my = ly + 50.0f + (float)i * 53.0f;
        bool is_sel = (i == g_game.modes_selected_idx);
        ui_gba_row(lx + 16.0f, my, lw - 32.0f, 47.0f, is_sel, i);
        ui_gba_text_mid(lx + 48.0f, my, 47.0f, 1.38f, GBA_INK, GBA_SHAD_LIGHT, mode_names[i]);
    }

    int s_idx = (g_game.selected_starter >= 0 && g_game.selected_starter < TOTAL_CATALOG_POKEMON) ? g_game.selected_starter : 0;
    const PokemonData *pstar = &POKEMON_CATALOG[s_idx];
    int ptype_id = type_name_to_id(pstar->type1);
    if (ptype_id < 0 || ptype_id > 17) ptype_id = 0;
    uint32_t pc_edge = (ptype_id == 0) ? RGBA8_C(150, 155, 165, 255) : MOVE_TYPE_COLOR[ptype_id];
    uint32_t pc_bg   = (ptype_id == 0) ? RGBA8_C(242, 238, 232, 255) : ui_mix_white(pc_edge, 0.76f);
    uint32_t pc_well = (ptype_id == 0) ? RGBA8_C(214, 208, 198, 255) : ui_mix_white(pc_edge, 0.50f);

    float pc_x = lx + 16.0f, pc_y = ly + 318.0f, pc_w = lw - 32.0f, pc_h = 102.0f;
    ui_draw_card(pc_x, pc_y, pc_w, pc_h, pc_bg, pc_edge, 2.2f, 10.0f);
    ui_draw_rounded_rect(pc_x + 10.0f, pc_y + 10.0f, 78.0f, 82.0f, 8.0f, pc_well);
    ui_ellipse(pc_x + 49.0f, pc_y + 74.0f, 22.0f, 7.0f, RGBA8_C(60, 75, 95, 120));

    static float mode_poke_anim = 0.0f;
    mode_poke_anim += 0.08f;
    int pf = ((int)mode_poke_anim) & 3;
    float pmax = (float)(pstar->walk.w > pstar->walk.h ? pstar->walk.w : pstar->walk.h);
    float psc = ((pmax > 40.0f) ? (40.0f / pmax) * 1.75f : 1.75f);
    render_draw_pokemon_sprite(pstar->dex, ANIM_IDLE, pc_x + 49.0f, pc_y + 72.0f, pf, 0, psc, 0, 0);

    char p_title[48];
    snprintf(p_title, sizeof(p_title), "#%03d %s", pstar->dex, pstar->name);
    render_draw_text(pc_x + 98.0f, pc_y + 12.0f, 1.28f, UI_MODERN_TEXT_DARK, p_title);
    render_draw_type_badge(pstar->type1 ? pstar->type1 : "Normal", pc_x + pc_w - 68.0f, pc_y + 10.0f, 0.23f);
    if (pstar->type2 && pstar->type2[0]) {
        render_draw_type_badge(pstar->type2, pc_x + pc_w - 38.0f, pc_y + 10.0f, 0.23f);
    }
    char p_stat[48];
    {
        float shop_hp_pct = (float)g_save.upgrades[0] * 0.05f;
        if (shop_hp_pct > 0.50f) shop_hp_pct = 0.50f;
        float shop_atk = (float)g_save.upgrades[1] * 0.04f;
        float shop_spd = (float)g_save.upgrades[2] * 0.03f;
        int eff_hp = (int)floorf((float)pstar->base_hp * (1.0f + shop_hp_pct) + 0.5f);
        snprintf(p_stat, sizeof(p_stat), "HP:%d  SPD:%.0f  ATK:x%.2f",
                 eff_hp, pstar->base_speed * (1.0f + shop_spd), pstar->base_atk * (1.0f + shop_atk));
    }
    render_draw_text(pc_x + 98.0f, pc_y + 42.0f, 1.02f, RGBA8_C(70, 82, 98, 255), p_stat);
    render_draw_text(pc_x + 98.0f, pc_y + 70.0f, 1.05f, RGBA8_C(217, 119, 6, 255), "[Tri] CHANGE POKEMON");

    float rx = 416.0f, ry = 52.0f, rw = 520.0f, rh = 432.0f;
    ui_draw_card(rx, ry, rw, rh, UI_MODERN_WHITE, UI_MODERN_BORDER, 2.0f, 14.0f);

    float sc_x = rx + 16.0f, sc_y = ry + 14.0f, sc_w = rw - 32.0f, sc_h = 172.0f;
    ui_draw_card(sc_x, sc_y, sc_w, sc_h, UI_WELL_CYAN, UI_MODERN_BORDER, 1.8f, 10.0f);

    char stg_header[80];
    snprintf(stg_header, sizeof(stg_header), "[Left]  STAGE %d/%d: %s  [Right]", g_game.selected_stage + 1, TOTAL_STAGES, STAGE_DEFS[g_game.selected_stage].name);
    ui_draw_pill(sc_x + 12.0f, sc_y + 10.0f, sc_w - 24.0f, 28.0f, UI_BANNER_YELLOW, UI_MODERN_BORDER);
    ui_text_center(1, sc_x + 12.0f, sc_y + 10.0f, sc_w - 24.0f, 28.0f, 1.05f, UI_MODERN_TEXT_DARK, stg_header);

    float prev_x = sc_x + 14.0f, prev_y = sc_y + 46.0f, prev_w = 185.0f, prev_h = 112.0f;
    render_draw_stage_preview(g_game.selected_stage, prev_x, prev_y, prev_w, prev_h);
    render_draw_rect_outline(prev_x, prev_y, prev_w, prev_h, UI_MODERN_BORDER, 2.0f);

    float dt_x = prev_x + prev_w + 14.0f;
    const StageInfo *sinfo = &STAGE_DEFS[g_game.selected_stage];
    render_draw_text(dt_x, sc_y + 48.0f, 1.35f, UI_MODERN_TEXT_DARK, sinfo->subtitle);
    render_draw_text(dt_x, sc_y + 70.0f, 1.15f, RGBA8_C(37, 99, 235, 255), sinfo->biome);
    render_draw_text(dt_x, sc_y + 96.0f, 1.15f, RGBA8_C(217, 119, 6, 255), "WILD POKEMON HABITAT:");
    render_draw_text(dt_x, sc_y + 115.0f, 1.05f, RGBA8_C(16, 185, 129, 255), sinfo->perk);
    render_draw_text(dt_x, sc_y + 138.0f, 0.90f, UI_MODERN_TEXT_DARK, sinfo->encounters);

    float mb_x = rx + 16.0f, mb_y = sc_y + sc_h + 10.0f, mb_w = rw - 32.0f, mb_h = 122.0f;
    ui_draw_card(mb_x, mb_y, mb_w, mb_h, UI_WELL_CREAM, UI_MODERN_BORDER, 1.8f, 10.0f);
    char mhdr[64];
    if (is_coming_soon) {
        snprintf(mhdr, sizeof(mhdr), "MORE MODES COMING SOON");
    } else {
        snprintf(mhdr, sizeof(mhdr), "MODE: %s", mode_names[g_game.modes_selected_idx]);
    }
    render_draw_text(mb_x + 14.0f, mb_y + 10.0f, 1.35f, RGBA8_C(217, 119, 6, 255), mhdr);

    if (g_game.modes_selected_idx == MODE_STANDARD) {
        render_draw_text(mb_x + 14.0f, mb_y + 38.0f, 1.08f, UI_MODERN_TEXT_DARK, "- Survive growing waves of wild Pokemon and tough bosses.");
        render_draw_text(mb_x + 14.0f, mb_y + 62.0f, 1.08f, UI_MODERN_TEXT_DARK, "- Level up skills, collect items, and evolve your Pokemon.");
        render_draw_text(mb_x + 14.0f, mb_y + 86.0f, 1.08f, RGBA8_C(16, 185, 129, 255), "- Defeat the last boss at the end of the run to claim Victory!");
    } else if (g_game.modes_selected_idx == MODE_TURBO) {
        render_draw_text(mb_x + 14.0f, mb_y + 38.0f, 1.08f, UI_MODERN_TEXT_DARK, "- High-speed action - faster waves, faster movement, faster foes!");
        render_draw_text(mb_x + 14.0f, mb_y + 62.0f, 1.08f, UI_MODERN_TEXT_DARK, "- Enemies and bosses escalate rapidly toward the last boss.");
        render_draw_text(mb_x + 14.0f, mb_y + 86.0f, 1.08f, RGBA8_C(16, 185, 129, 255), "- Earn +50% bonus Pokecoins on all drops and chests!");
    } else if (g_game.modes_selected_idx == MODE_BOSSRUSH) {
        render_draw_text(mb_x + 14.0f, mb_y + 38.0f, 1.08f, UI_MODERN_TEXT_DARK, "- No regular waves - pure back-to-back Legendary Boss fights!");
        render_draw_text(mb_x + 14.0f, mb_y + 62.0f, 1.08f, UI_MODERN_TEXT_DARK, "- Start equipped with extra skills for immediate boss combat.");
        render_draw_text(mb_x + 14.0f, mb_y + 86.0f, 1.08f, RGBA8_C(16, 185, 129, 255), "- Defeat every boss and conquer the last boss to clear!");
    } else if (g_game.modes_selected_idx == MODE_ENDLESS) {
        render_draw_text(mb_x + 14.0f, mb_y + 38.0f, 1.08f, UI_MODERN_TEXT_DARK, "- True endless survival - no level cap and no skill cap!");
        render_draw_text(mb_x + 14.0f, mb_y + 62.0f, 1.08f, UI_MODERN_TEXT_DARK, "- Beating the last boss warps you to a new map with more bosses.");
        render_draw_text(mb_x + 14.0f, mb_y + 86.0f, 1.08f, RGBA8_C(16, 185, 129, 255), "- Survive as long as you can - longest time is saved on death!");
    } else {
        render_draw_text(mb_x + 14.0f, mb_y + 42.0f, 1.12f, UI_MODERN_TEXT_DARK, "Thank you for playing the game!");
        render_draw_text(mb_x + 14.0f, mb_y + 72.0f, 1.08f, RGBA8_C(16, 185, 129, 255), "We'll try to update the game if we could.");
    }

    float btn_x = rx + 20.0f, btn_y = mb_y + mb_h + 12.0f, btn_w = rw - 40.0f, btn_h = 52.0f;
    if (is_coming_soon) {
        ui_draw_card(btn_x, btn_y, btn_w, btn_h, RGBA8_C(203, 213, 225, 255), UI_MODERN_BORDER, 2.2f, 10.0f);
        ui_text_center(1, btn_x, btn_y, btn_w, btn_h, 1.35f, RGBA8_C(100, 116, 139, 255), "COMING SOON");
    } else if (has_save) {
        if (sav_cidx < 0 || sav_cidx >= TOTAL_CATALOG_POKEMON) sav_cidx = 0;
        int sm = (int)sav_time / 60, ss = (int)sav_time % 60;
        float gap = 12.0f;
        float rw_btn = btn_w * 0.62f - gap * 0.5f;
        float nw_btn = btn_w * 0.38f - gap * 0.5f;
        ui_draw_card(btn_x, btn_y, rw_btn, btn_h, UI_BTN_GREEN, UI_MODERN_BORDER, 2.5f, 10.0f);
        char rlbl[64];
        snprintf(rlbl, sizeof(rlbl), "RESUME RUN [X]");
        ui_text_center(1, btn_x, btn_y + 2.0f, rw_btn, 28.0f, 1.25f, UI_MODERN_TEXT_DARK, rlbl);
        char rsub[64];
        snprintf(rsub, sizeof(rsub), "%s Lv.%d (%02d:%02d)", POKEMON_CATALOG[sav_cidx].name, sav_lv, sm, ss);
        ui_text_center(0, btn_x, btn_y + 27.0f, rw_btn, 20.0f, 0.98f, RGBA8_C(20, 62, 34, 255), rsub);

        float nx = btn_x + rw_btn + gap;
        ui_draw_card(nx, btn_y, nw_btn, btn_h, UI_BANNER_YELLOW, UI_MODERN_BORDER, 2.2f, 10.0f);
        ui_text_center(1, nx, btn_y, nw_btn, btn_h, 1.18f, UI_MODERN_TEXT_DARK, "START NEW [Sq]");
    } else {
        ui_draw_card(btn_x, btn_y, btn_w, btn_h, UI_BTN_GREEN, UI_MODERN_BORDER, 2.5f, 10.0f);
        ui_text_center(1, btn_x, btn_y, btn_w, btn_h, 1.45f, UI_MODERN_TEXT_DARK, "START BATTLE RUN  [X]");
    }
}

static void render_menu_roster(void) {
    render_menu_world_3d();
    int sel = g_game.roster_selected_idx;
    const PokemonData *pdata = &POKEMON_CATALOG[sel];
    bool owned = save_system_is_owned(sel);

    int vis_list[TOTAL_CATALOG_POKEMON];
    int owned_count = 0, vis_n = 0, cur_pos = 0;
    for (int i = 0; i < TOTAL_CATALOG_POKEMON; i++) {
        bool is_own = save_system_is_owned(i);
        if (is_own) owned_count++;
        if (g_game.roster_tab == 0 && !is_own) continue;
        if (i == sel) cur_pos = vis_n;
        vis_list[vis_n++] = i;
    }
    if (vis_n == 0) vis_list[vis_n++] = 0;

    int slots_per_page = 20;
    int total_pages = (vis_n + slots_per_page - 1) / slots_per_page;
    int cur_page = cur_pos / slots_per_page;
    int page_start = cur_page * slots_per_page;

    char page_str[180];
    int first_show = POKEMON_CATALOG[vis_list[page_start]].dex;
    int end_vi = page_start + slots_per_page - 1;
    if (end_vi >= vis_n) end_vi = vis_n - 1;
    int last_show = POKEMON_CATALOG[vis_list[end_vi]].dex;
    if (g_game.roster_from_modes) {
        snprintf(page_str, sizeof(page_str), "Page %d/%d (#%03d-#%03d)  |  [Left/Right] [Up/Down] Move  |  [Sq] Page  |  [X] Choose  |  [O] Back",
                 cur_page + 1, total_pages, first_show, last_show);
    } else {
        snprintf(page_str, sizeof(page_str), "Page %d/%d (#%03d-#%03d)  |  [L/R] Tabs  |  [Sq] Page  |  [X] Choose  |  [O] Back",
                 cur_page + 1, total_pages, first_show, last_show);
    }

    render_draw_rect(0.0f, 0.0f, SCREEN_W, SCREEN_H, UI_BG_FLAT);
    ui_draw_top_pills("POKÉDEX", g_save.coins, owned_count, TOTAL_CATALOG_POKEMON, g_save.tickets);

    float lx = 25.0f, ly = 52.0f, lw = 385.0f, lh = 432.0f;
    static float prev_t = 0.0f;
    prev_t += 0.08f;
    int f = (int)prev_t % 4;
    uint32_t tint = owned ? 0 : RGBA8_C(16, 24, 30, 230);
    ui_draw_pokemon_trading_card(lx, ly, lw, lh, pdata, f, tint, true);

    float btn_x = lx + 16.0f, btn_y = ly + lh - 46.0f, btn_w = lw - 32.0f, btn_h = 38.0f;
    if (sel == g_game.selected_starter) {
        ui_bevel_btn(btn_x, btn_y, btn_w, btn_h, UI_GREEN_FILL, UI_GREEN_HI, UI_GREEN_LIP);
        ui_label_outlined_icon(btn_x, btn_y, btn_w, btn_h, 1.38f, "ACTIVE LEAD POKEMON", "", "");
    } else if (owned) {
        ui_bevel_btn(btn_x, btn_y, btn_w, btn_h, UI_GREEN_FILL, UI_GREEN_HI, UI_GREEN_LIP);
        ui_label_outlined_icon(btn_x, btn_y, btn_w, btn_h, 1.38f, "PRESS", "(X)", "TO CHOOSE");
    } else {
        int unlock_cost = save_system_get_unlock_ticket_cost(sel);
        if (unlock_cost < 0) {
            ui_bevel_btn(btn_x, btn_y, btn_w, btn_h, UI_GREY_FILL, UI_GREY_HI, UI_GREY_LIP);
            ui_label_outlined_icon(btn_x, btn_y, btn_w, btn_h, 1.32f, "EVOLVE TO UNLOCK", "", "");
        } else {
            bool can_unlock = (g_save.tickets >= unlock_cost);
            char ubuf[40];
            snprintf(ubuf, sizeof(ubuf), "UNLOCK (%d TKT)", unlock_cost);
            ui_bevel_btn(btn_x, btn_y, btn_w, btn_h,
                         can_unlock ? UI_GREEN_FILL : UI_GREY_FILL,
                         can_unlock ? UI_GREEN_HI   : UI_GREY_HI,
                         can_unlock ? UI_GREEN_LIP  : UI_GREY_LIP);
            ui_label_outlined_icon(btn_x, btn_y, btn_w, btn_h, 1.32f, "PRESS", "(X)", ubuf);
        }
    }

    float gx = 432.0f, gy = 52.0f, gw = 503.0f, gh = 432.0f;
    ui_draw_card(gx, gy, gw, gh, UI_MODERN_WHITE, UI_MODERN_BORDER, 2.0f, 14.0f);

    char tab0_lbl[32], tab1_lbl[32];
    snprintf(tab0_lbl, sizeof(tab0_lbl), "OWNED (%d)", owned_count);
    snprintf(tab1_lbl, sizeof(tab1_lbl), "ALL (%d)", TOTAL_CATALOG_POKEMON);
    if (g_game.roster_from_modes) {
        ui_draw_badge(gx + 12.0f, gy + 12.0f, gw - 24.0f, 27.0f, UI_BTN_GREEN, UI_MODERN_BORDER, tab0_lbl, false);
    } else {
        bool t0_act = (g_game.roster_tab == 0);
        uint32_t tab_inact_border = RGBA8_C(130, 136, 145, 255);
        ui_draw_badge(gx + 12.0f, gy + 12.0f, 233.0f, 27.0f, t0_act ? UI_BTN_GREEN : UI_TAB_INACTIVE, t0_act ? UI_MODERN_BORDER : tab_inact_border, tab0_lbl, false);
        ui_draw_badge(gx + 258.0f, gy + 12.0f, 233.0f, 27.0f, !t0_act ? UI_BTN_GREEN : UI_TAB_INACTIVE, !t0_act ? UI_MODERN_BORDER : tab_inact_border, tab1_lbl, false);
    }

    float slot_w = 88.0f, slot_h = 85.0f;
    for (int slot = 0; slot < slots_per_page; slot++) {
        int vi = page_start + slot;
        if (vi >= vis_n) break;
        int i = vis_list[vi];
        int col = slot % 5, row = slot / 5;
        float bx = gx + 14.0f + (float)col * 97.3f;
        float by = gy + 49.0f + (float)row * 94.9f;
        bool is_cur = (i == sel);
        bool is_own = save_system_is_owned(i);

        uint32_t fill = is_cur ? RGBA8_C(150, 198, 164, 255) : RGBA8_C(187, 214, 245, 255);
        ui_draw_card(bx, by, slot_w, slot_h, fill, is_cur ? UI_CARD_SEL_ORANGE : fill, is_cur ? 3.0f : 0.0f, 10.0f);

        ui_ellipse(bx + slot_w * 0.5f, by + 55.0f, 17.0f, 6.0f, is_cur ? RGBA8_C(116, 158, 130, 170) : RGBA8_C(139, 168, 204, 170));
        uint32_t tcol = is_own ? 0 : RGBA8_C(14, 22, 28, 235);
        float max_dim = (float)(POKEMON_CATALOG[i].walk.w > POKEMON_CATALOG[i].walk.h ? POKEMON_CATALOG[i].walk.w : POKEMON_CATALOG[i].walk.h);
        float base_sc = (max_dim > 40.0f) ? (40.0f / max_dim) : 1.0f;
        render_draw_pokemon_sprite(POKEMON_CATALOG[i].dex, ANIM_IDLE, bx + slot_w * 0.5f, by + 56.0f + 8.5f * base_sc, 0, 0, base_sc, 0, tcol);

        char numstr[16];
        snprintf(numstr, sizeof(numstr), "#%03d", POKEMON_CATALOG[i].dex);
        ui_text_center(2, bx, by + 62.0f, slot_w, 22.0f, 1.2f, is_own ? UI_MODERN_TEXT_DARK : UI_MODERN_TEXT_MUTED, numstr);
    }

    ui_draw_footer_pill(page_str);
}

static void render_menu_shop(void) {
    render_menu_world_3d();
    ui_gba_screen_chrome("UPGRADES SHOP", "[X] Buy Upgrade   |   [Up/Down] Navigate   |   [O] Back to Hub");

    float sx = 36.0f, sy = 52.0f, sw = 888.0f, sh = 432.0f;
    ui_draw_card(sx, sy, sw, sh, UI_MODERN_WHITE, UI_MODERN_BORDER, 2.0f, 14.0f);

    int start_i = g_game.shop_selected_idx - 2;
    if (start_i < 0) start_i = 0;
    if (start_i > TOTAL_UPGRADES - 5) start_i = TOTAL_UPGRADES - 5;
    if (start_i < 0) start_i = 0;

    for (int i = 0; i < 5 && (start_i + i) < TOTAL_UPGRADES; i++) {
        int idx = start_i + i;
        float cy = sy + 18.0f + (float)i * 78.0f;
        bool is_sel = (idx == g_game.shop_selected_idx);

        int cur_lvl = g_save.upgrades[idx];
        int max_lvl = G_UPGRADE_DEFS[idx].max_level;
        int cost = save_system_get_upgrade_cost(idx);
        bool can_buy = (g_save.coins >= cost) && (cur_lvl < max_lvl);

        ui_gba_row(sx + 18.0f, cy, sw - 36.0f, 70.0f, is_sel, idx);
        render_draw_text(sx + 54.0f, cy + 12.0f, 1.48f, UI_MODERN_TEXT_DARK, G_UPGRADE_DEFS[idx].name);
        render_draw_text(sx + 54.0f, cy + 38.0f, 1.15f, RGBA8_C(100, 116, 139, 255), G_UPGRADE_DEFS[idx].unit);

        float bx = sx + 370.0f, by = cy + 18.0f, bw = 240.0f;
        ui_gba_hp_bar(bx, by, bw, cur_lvl, max_lvl);
        char lbuf[32];
        snprintf(lbuf, sizeof(lbuf), "Level %d / %d", cur_lvl, max_lvl);
        render_draw_text_centered(bx + bw * 0.5f, by + 18.0f, 1.15f, UI_MODERN_TEXT_DARK, lbuf);

        if (cur_lvl >= max_lvl) {
            ui_draw_card(sx + sw - 210.0f, cy + 14.0f, 172.0f, 42.0f, UI_BTN_GREY, UI_MODERN_BORDER, 1.8f, 8.0f);
            ui_text_center(1, sx + sw - 210.0f, cy + 14.0f, 172.0f, 42.0f, 1.15f, UI_MODERN_TEXT_DARK, "[MAX LEVEL]");
        } else {
            uint32_t fill = can_buy ? UI_BTN_GREEN : UI_BTN_GREY;
            ui_draw_card(sx + sw - 210.0f, cy + 14.0f, 172.0f, 42.0f, fill, UI_MODERN_BORDER, 2.0f, 8.0f);
            char cbuf[32];
            snprintf(cbuf, sizeof(cbuf), "BUY ($%d)", cost);
            ui_text_center(1, sx + sw - 210.0f, cy + 14.0f, 172.0f, 42.0f, 1.20f, UI_MODERN_TEXT_DARK, cbuf);
        }
    }
}

static void render_menu_gacha(void) {
    render_menu_world_3d();
    ui_gba_screen_chrome("GACHAPON SUMMON", "[X] Standard Summon (1 TKT)   |   [Sq] Legendary Summon (10 TKT)   |   [O] Back to Hub");

    float cx = SCREEN_W * 0.5f, cy = 268.0f;
    float cw = 640.0f, ch = 424.0f;
    ui_draw_card(cx - cw * 0.5f, cy - ch * 0.5f, cw, ch, UI_MODERN_WHITE, UI_MODERN_BORDER, 2.0f, 14.0f);

    float bnr_x = cx - cw * 0.5f + 16.0f, bnr_y = cy - ch * 0.5f + 14.0f, bnr_w = cw - 32.0f, bnr_h = 32.0f;
    ui_draw_pill(bnr_x, bnr_y, bnr_w, bnr_h, UI_BANNER_YELLOW, UI_MODERN_BORDER);
    ui_text_center(1, bnr_x, bnr_y, bnr_w, bnr_h, 1.15f, UI_MODERN_TEXT_DARK, "POKéBALL CAPSULE SUMMON");

    ui_draw_card(cx - 140.0f, cy - 132.0f, 280.0f, 162.0f, UI_WELL_CYAN, UI_MODERN_BORDER, 1.8f, 12.0f);
    render_draw_shadow(cx, cy - 8.0f, 38.0f);

    if (g_game.gacha_pulling) {
        float t = g_game.gacha_timer;
        float bx = cx, by = cy - 48.0f, rot = 0.0f, sc = 0.95f;
        if (t < 1.25f) {

            float phase = fmodf(t, 0.42f) / 0.42f;
            float env = sinf(phase * 3.14159265f);
            float dir = ((int)(t / 0.42f) % 2 == 0) ? 1.0f : -1.0f;
            rot = dir * env * 0.48f;
            bx = cx + dir * env * 12.0f;
            by = cy - 48.0f - env * 10.0f;
        } else {

            float f = (t - 1.25f) / 0.35f;
            if (f > 1.0f) f = 1.0f;
            rot = sinf(t * 58.0f) * 0.28f;
            bx = cx + sinf(t * 64.0f) * 6.0f;
            by = cy - 48.0f - f * 12.0f;
            sc = 0.95f + f * 0.25f;
        }
        render_draw_ui_texture_rot(UI_TEX_GACHA_BALL, bx, by, sc, rot);
        if (t >= 1.32f) {
            float fp = (t - 1.32f) / 0.28f;
            if (fp > 1.0f) fp = 1.0f;
            render_draw_circle(cx, by, 26.0f + fp * 54.0f, RGBA8_C(255, 252, 220, (uint8_t)(fp * 215.0f)));
        }
        render_draw_text_8bit_centered(cx, cy + 56.0f, 1.35f, RGBA8_C(217, 119, 6, 255), "OPENING POKéBALL...");
    } else if (g_game.gacha_result_idx >= 0 && g_game.gacha_timer >= 1.6f) {
        int safe_idx = (g_game.gacha_result_idx >= 0 && g_game.gacha_result_idx < TOTAL_CATALOG_POKEMON) ? g_game.gacha_result_idx : 0;
        const PokemonData *rdata = &POKEMON_CATALOG[safe_idx];
        render_draw_pokemon_sprite(rdata->dex, ANIM_IDLE, cx, cy - 20.0f, 0, 0, 2.5f, 0, 0);

        char rbuf[64];
        if (g_game.gacha_is_new) {
            render_draw_text_centered(cx, cy + 44.0f, 1.40f, RGBA8_C(16, 185, 129, 255), "NEW POKéMON REGISTERED!");
            snprintf(rbuf, sizeof(rbuf), "#%03d  %s", rdata->dex, rdata->name);
            render_draw_text_centered(cx, cy + 72.0f, 1.50f, UI_MODERN_TEXT_DARK, rbuf);
        } else {
            render_draw_text_centered(cx, cy + 44.0f, 1.32f, RGBA8_C(100, 116, 139, 255), "DUPLICATE POKéMON");
            snprintf(rbuf, sizeof(rbuf), "#%03d  %s (+%d TKT Refund)", rdata->dex, rdata->name, g_game.gacha_refund_coins);
            render_draw_text_centered(cx, cy + 72.0f, 1.38f, RGBA8_C(217, 119, 6, 255), rbuf);
        }
    } else {
        static float g_idle_t = 0.0f;
        g_idle_t += 0.04f;
        float bob = sinf(g_idle_t) * 3.0f;
        render_draw_ui_texture_rot(UI_TEX_GACHA_BALL, cx, cy - 48.0f + bob, 0.95f, sinf(g_idle_t * 0.5f) * 0.06f);
    }

    if (!g_game.gacha_pulling) {
        float bw = 278.0f, bh = 50.0f, gap = 16.0f;
        float bx1 = cx - bw - gap * 0.5f;
        float bx2 = cx + gap * 0.5f;
        float by  = cy + 124.0f;

        bool can_std = (g_save.tickets >= 1);
        bool can_leg = (g_save.tickets >= 10);

        ui_draw_card(bx1, by, bw, bh, can_std ? UI_BTN_GREEN : UI_BTN_GREY, UI_MODERN_BORDER, 2.2f, 10.0f);
        ui_text_center(1, bx1, by, bw, bh, 1.15f, UI_MODERN_TEXT_DARK, "[X] STANDARD (1 TKT)");

        uint32_t leg_col = can_leg ? RGBA8_C(250, 204, 21, 255) : UI_BTN_GREY;
        ui_draw_card(bx2, by, bw, bh, leg_col, UI_MODERN_BORDER, 2.2f, 10.0f);
        ui_text_center(1, bx2, by, bw, bh, 1.15f, UI_MODERN_TEXT_DARK, "[Sq] LEGENDARY (10 TKT)");
    }
}

static void render_menu_missions(void) {
    render_menu_world_3d();
    ui_gba_screen_chrome("MISSIONS & GOALS", "[X] Claim Reward   |   [Up/Down] Navigate   |   [O] Back to Hub");

    float mx = 36.0f, my = 52.0f, mw = 888.0f, mh = 432.0f;
    ui_draw_card(mx, my, mw, mh, UI_MODERN_WHITE, UI_MODERN_BORDER, 2.0f, 14.0f);

    int start_i = g_game.missions_selected_idx - 2;
    if (start_i < 0) start_i = 0;
    if (start_i > TOTAL_GOALS - 5) start_i = TOTAL_GOALS - 5;
    if (start_i < 0) start_i = 0;

    for (int i = 0; i < 5 && (start_i + i) < TOTAL_GOALS; i++) {
        int idx = start_i + i;
        float gy = my + 18.0f + (float)i * 78.0f;
        bool is_sel = (idx == g_game.missions_selected_idx);

        int progress = save_system_get_goal_progress(idx);
        int target = save_system_get_goal_target(idx);
        int r_coins = save_system_get_goal_reward_coins(idx);
        int r_tickets = save_system_get_goal_reward_tickets(idx);
        bool done = save_system_is_goal_completed(idx);

        ui_gba_row(mx + 18.0f, gy, mw - 36.0f, 70.0f, is_sel, idx);
        char gtext[96];
        save_system_get_goal_text(idx, gtext, (int)sizeof(gtext));
        render_draw_text(mx + 54.0f, gy + 12.0f, 1.45f, UI_MODERN_TEXT_DARK, gtext);

        char rbuf[64];
        if (r_tickets > 0) {
            snprintf(rbuf, sizeof(rbuf), "Reward: $%d + %d Ticket%s", r_coins, r_tickets, r_tickets == 1 ? "" : "s");
        } else {
            snprintf(rbuf, sizeof(rbuf), "Reward: $%d", r_coins);
        }
        render_draw_text(mx + 54.0f, gy + 38.0f, 1.18f, RGBA8_C(217, 119, 6, 255), rbuf);

        float bx = mx + 460.0f, by = gy + 20.0f, bw = 190.0f;
        ui_gba_exp_bar(bx, by, bw, progress > target ? target : progress, target);
        char pbuf[32];
        if (G_GOAL_DEFS[idx].stat_type == 1) {
            snprintf(pbuf, sizeof(pbuf), "%d:%02d / %d:%02d", progress / 60, progress % 60, target / 60, target % 60);
        } else {
            snprintf(pbuf, sizeof(pbuf), "%d / %d", progress, target);
        }
        render_draw_text_centered(bx + bw * 0.5f, by + 18.0f, 1.15f, UI_MODERN_TEXT_DARK, pbuf);

        if (done) {
            ui_draw_card(mx + mw - 210.0f, gy + 14.0f, 172.0f, 42.0f, UI_BTN_GREEN, UI_MODERN_BORDER, 2.2f, 8.0f);
            ui_text_center(1, mx + mw - 210.0f, gy + 14.0f, 172.0f, 42.0f, 1.25f, UI_MODERN_TEXT_DARK, "CLAIM! [X]");
        } else {
            ui_draw_card(mx + mw - 210.0f, gy + 14.0f, 172.0f, 42.0f, UI_WELL_CREAM, UI_MODERN_BORDER, 1.5f, 8.0f);
            ui_text_center(1, mx + mw - 210.0f, gy + 14.0f, 172.0f, 42.0f, 1.15f, UI_MODERN_TEXT_DARK, "IN PROGRESS");
        }
    }
}

static void render_menu_settings(void) {
    render_menu_world_3d();
    ui_gba_screen_chrome("SETTINGS & CONTROLS", "[Up/Down] Select   |   [Left/Right] Volume   |   [X] Mute / Confirm   |   [O] Back");

    float sx = 54.0f, sy = 52.0f, sw = 852.0f, sh = 432.0f;
    ui_draw_card(sx, sy, sw, sh, UI_MODERN_WHITE, UI_MODERN_BORDER, 2.0f, 14.0f);

    float bnr0_x = sx + 20.0f, bnr0_y = sy + 14.0f, bnr0_w = sw - 40.0f, bnr0_h = 28.0f;
    ui_draw_pill(bnr0_x, bnr0_y, bnr0_w, bnr0_h, UI_BANNER_YELLOW, UI_MODERN_BORDER);
    ui_text_center(1, bnr0_x, bnr0_y, bnr0_w, bnr0_h, 1.10f, UI_MODERN_TEXT_DARK, "AUDIO SETTINGS");

    int bgm_pct = (int)floorf(audio_get_bgm_volume() * 100.0f + 0.5f);
    int sfx_pct = (int)floorf(audio_get_sfx_volume() * 100.0f + 0.5f);
    for (int i = 0; i < 2; i++) {
        float ry = sy + 48.0f + (float)i * 44.0f;
        bool is_sel = (g_game.settings_selected_idx == i);
        ui_gba_row(sx + 24.0f, ry, sw - 48.0f, 38.0f, is_sel, i);
        const char *lbl = (i == 0) ? "Background Music (BGM)" : "Sound Effects (SFX)";
        int pct = (i == 0) ? bgm_pct : sfx_pct;
        ui_text_mid(0, sx + 56.0f, ry + 18.0f, 1.28f, UI_MODERN_TEXT_DARK, lbl);

        float bar_x = sx + 430.0f, bar_y = ry + 10.0f, bar_w = 240.0f, bar_h = 16.0f;
        ui_draw_card(bar_x, bar_y, bar_w, bar_h, RGBA8_C(32, 40, 52, 255), UI_MODERN_BORDER, 1.5f, 5.0f);
        if (pct > 0) {
            render_draw_rect(bar_x + 3.0f, bar_y + 3.0f, (bar_w - 6.0f) * ((float)pct / 100.0f), bar_h - 6.0f,
                             (i == 0) ? RGBA8_C(56, 189, 248, 255) : RGBA8_C(74, 222, 128, 255));
        }
        char vbuf[32];
        snprintf(vbuf, sizeof(vbuf), pct > 0 ? "%d%%" : "MUTED", pct);
        ui_text_mid(0, bar_x + bar_w + 16.0f, ry + 18.0f, 1.22f, pct > 0 ? UI_MODERN_TEXT_DARK : RGBA8_C(220, 38, 38, 255), vbuf);
    }

    float bnr1_x = sx + 20.0f, bnr1_y = sy + 144.0f, bnr1_w = sw - 40.0f, bnr1_h = 28.0f;
    ui_draw_pill(bnr1_x, bnr1_y, bnr1_w, bnr1_h, UI_WELL_BLUE, UI_MODERN_BORDER);
    ui_text_center(1, bnr1_x, bnr1_y, bnr1_w, bnr1_h, 1.10f, UI_MODERN_TEXT_DARK, "CONTROLS");

    render_draw_text(sx + 34.0f, sy + 180.0f, 1.26f, UI_MODERN_TEXT_DARK, "- [Left/Right] [Up/Down] Left Stick / D-Pad: Move & Navigate menus");
    render_draw_text(sx + 34.0f, sy + 208.0f, 1.26f, UI_MODERN_TEXT_DARK, "- [X] Confirm   |   [O] Cancel & Back");
    render_draw_text(sx + 34.0f, sy + 236.0f, 1.26f, UI_MODERN_TEXT_DARK, "- [Tri] [Sq] [O] [X]: Cast Active Dash Skills in battle");
    render_draw_text(sx + 34.0f, sy + 264.0f, 1.26f, UI_MODERN_TEXT_DARK, "- [Sq] Reroll Cards   |   [Tri] Skip Cards (Level Up)");
    render_draw_text(sx + 34.0f, sy + 292.0f, 1.26f, UI_MODERN_TEXT_DARK, "- [L/R] / Right Stick: 3D Camera angle & zoom (L+R Reset)");
    render_draw_text(sx + 34.0f, sy + 320.0f, 1.26f, UI_MODERN_TEXT_DARK, "- START: Open Pause Menu (Save & Exit)");

    float bb_w = 340.0f, bb_h = 42.0f, bb_x = sx + (sw - bb_w) * 0.5f, bb_y = sy + 368.0f;
    bool back_sel = (g_game.settings_selected_idx == 2);
    ui_draw_card(bb_x, bb_y, bb_w, bb_h, back_sel ? UI_BTN_GREEN : UI_WELL_BLUE, UI_MODERN_BORDER, back_sel ? 2.5f : 1.8f, 10.0f);
    ui_text_center(1, bb_x, bb_y, bb_w, bb_h, 1.20f, UI_MODERN_TEXT_DARK, "RETURN TO MAIN MENU  [O]");
}

typedef struct { float y; int kind; int idx; } DrawItem;
static int cmp_draw_item(const void *a, const void *b) {
    float ya = ((const DrawItem *)a)->y, yb = ((const DrawItem *)b)->y;
    return (ya > yb) - (ya < yb);
}

static void draw_lily(float x, float y) {
    render_draw_circle_ground(x, y, 12.0f, RGBA8_C(32, 115, 60, 255));
    render_draw_circle_ground(x, y, 10.0f, RGBA8_C(68, 175, 88, 255));
    render_draw_circle_ground(x - 3.0f, y - 3.0f, 3.5f, RGBA8_C(255, 145, 195, 255));
    render_draw_circle_ground(x - 3.0f, y - 3.0f, 1.5f, RGBA8_C(255, 235, 100, 255));
}

static void draw_dir_arrow(float ax, float ay, float ang, uint32_t col, float sc) {
    float c = cosf(ang), s = sinf(ang);
    uint32_t dark = RGBA8_C(14, 24, 16, 255);
    float step = 1.6f * sc;
    if (step < 1.0f) step = 1.0f;
    for (int pass = 0; pass < 2; pass++) {
        float g = (pass == 0 ? 2.8f : 0.0f) * sc;
        uint32_t cc = pass == 0 ? dark : col;
        for (float v = -13.0f * sc - g; v <= 13.0f * sc + g; v += step)
            render_draw_line(ax + c * (26.0f * sc + g), ay + s * (26.0f * sc + g),
                             ax + c * (4.0f * sc) - s * v, ay + s * (4.0f * sc) + c * v, cc, 2.8f * sc);
        for (float v = -4.0f * sc - g; v <= 4.0f * sc + g; v += step)
            render_draw_line(ax - c * (20.0f * sc + g) - s * v, ay - s * (20.0f * sc + g) + c * v,
                             ax + c * (6.0f * sc) - s * v, ay + s * (6.0f * sc) + c * v, cc, 2.8f * sc);
    }
}

static void draw_boss_arrow(float ax, float ay, float ang, uint32_t col) {
    draw_dir_arrow(ax, ay, ang, col, 1.0f);
}

static bool project_indicator_dir(float wx, float wy, float cx, float cy,
                                  float safe_l, float safe_r, float safe_t, float safe_b,
                                  float *out_ax, float *out_ay, float *out_ang, float *out_dist_m) {
    float sx = 0.0f, sy = 0.0f, sc = 0.0f;
    render_world_to_screen(wx - cx, wy - cy, 20.0f, &sx, &sy, &sc);

    if (sc > 0.001f && sx >= 28.0f && sx <= SCREEN_W - 28.0f && sy >= 112.0f && sy <= SCREEN_H - 68.0f) {
        return false;
    }
    float wdx = wx - g_game.player.x;
    float wdy = wy - g_game.player.y;
    if (out_dist_m) *out_dist_m = sqrtf(wdx * wdx + wdy * wdy) / 10.0f;

    float mx = SCREEN_W * 0.5f, my = SCREEN_H * 0.5f;
    float vx, vy;
    if (sc > 0.001f) {
        vx = sx - mx;
        vy = sy - my;
    } else {

        float yaw_rad = render_camera_yaw() * (3.14159265f / 180.0f);
        float cyaw = cosf(yaw_rad), syaw = sinf(yaw_rad);
        vx = wdx * cyaw + wdy * syaw;
        vy = -wdx * syaw + wdy * cyaw;
        if (vy < 80.0f) vy = fabsf(vy) + 120.0f;
    }
    float vl = sqrtf(vx * vx + vy * vy);
    if (vl < 0.001f) { vx = 0.0f; vy = -1.0f; vl = 1.0f; }
    float dx = vx / vl, dy = vy / vl;

    float t_min = 1e9f;
    if (dx > 1e-4f)  { float t = (safe_r - mx) / dx; if (t > 0.0f && t < t_min) t_min = t; }
    if (dx < -1e-4f) { float t = (safe_l - mx) / dx; if (t > 0.0f && t < t_min) t_min = t; }
    if (dy > 1e-4f)  { float t = (safe_b - my) / dy; if (t > 0.0f && t < t_min) t_min = t; }
    if (dy < -1e-4f) { float t = (safe_t - my) / dy; if (t > 0.0f && t < t_min) t_min = t; }
    if (t_min > 1e8f) t_min = 120.0f;

    float ax = mx + dx * t_min;
    float ay = my + dy * t_min;
    if (ax < safe_l) ax = safe_l;
    if (ax > safe_r) ax = safe_r;
    if (ay < safe_t) ay = safe_t;
    if (ay > safe_b) ay = safe_b;

    if (out_ax)  *out_ax = ax;
    if (out_ay)  *out_ay = ay;
    if (out_ang) *out_ang = atan2f(dy, dx);
    return true;
}

static void render_playing_game(void) {
    float cx = g_game.cam_x;
    float cy = g_game.cam_y;

    if (g_game.screen_shake > 0.0f && g_game.state == STATE_PLAYING) {
        cx += ((float)(rand() % 10 - 5)) * g_game.screen_shake;
        cy += ((float)(rand() % 10 - 5)) * g_game.screen_shake;
    }

    render_world_begin(cx, cy);
    render_draw_ground(cx, cy);

    for (int p = 0; p < MAX_PROPS; p++) {
        WorldProp *prop = &g_game.props[p];
        if (!prop->active || is_tall_prop(prop->type)) continue;
        float px = prop->x - cx, py = prop->y - cy;
#ifdef __vita__
        if (px < -140.0f || px > SCREEN_W + 140.0f || py < -140.0f || py > SCREEN_H + 180.0f) continue;
#else
        if (px < -240.0f || px > SCREEN_W + 240.0f || py < -240.0f || py > SCREEN_H + 320.0f) continue;
#endif
        if (prop->type == PROP_LILY) draw_lily(px, py);
        else render_draw_world_prop(prop->type, px, py, 1.40f);
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *en = &g_game.enemies[i];
        if (!en->active || !en->is_boss || !en->has_laser_skill) continue;
        if (en->laser_warn_timer <= 0.0f && en->laser_fire_timer <= 0.0f) continue;

        float sx = en->x - cx;
        float sy = en->y - cy;
        float ca = cosf(en->laser_angle);
        float sa = sinf(en->laser_angle);
        float px = -sa;
        float py = ca;
        float len = en->laser_len;
        float hw  = en->laser_half_w;
        float ex  = sx + ca * len;
        float ey  = sy + sa * len;

        if (en->laser_warn_timer > 0.0f) {

            float prog = 1.0f - (en->laser_warn_timer / 2.00f);
            if (prog < 0.0f) prog = 0.0f;
            if (prog > 1.0f) prog = 1.0f;
            bool urgent = (en->laser_warn_timer < 0.45f);
            float flash = 0.5f + 0.5f * sinf(g_game.run_timer * (urgent ? 32.0f : 12.0f));

            uint32_t area_col = RGBA8_C(255, 55, 45, (int)(55 + 35 * flash));
            uint32_t cast_col = urgent
                ? RGBA8_C(255, (int)(110 + 110 * flash), 50, 170)
                : RGBA8_C(255, 70, 50, 130);

            render_draw_line(sx, sy, ex, ey, area_col, hw * 2.0f);

            float p_ex = sx + ca * (len * prog);
            float p_ey = sy + sa * (len * prog);
            render_draw_line(sx, sy, p_ex, p_ey, cast_col, hw * 1.90f);
        } else if (en->laser_fire_timer > 0.0f) {

            render_draw_line(sx, sy, ex, ey, RGBA8_C(255, 130, 55, 120), hw * 2.0f);
        }
    }

    for (int i = 0; i < MAX_GEMS; i++) {
        ExpGem *gem = &g_game.gems[i];
        if (!gem->active) continue;
        float sx = gem->x - cx;
        float sy = gem->y - cy;
#ifdef __vita__
        if (sx < -160.0f || sx > SCREEN_W + 160.0f || sy < -160.0f || sy > SCREEN_H + 180.0f) continue;
#else
        if (sx < -620.0f || sx > SCREEN_W + 620.0f || sy < -620.0f || sy > SCREEN_H + 620.0f) continue;
#endif

        float gbob = sinf(g_game.run_timer * 4.8f + (float)i * 0.85f) * 1.6f;
        render_draw_shadow(sx, sy + 5.0f, gem->value >= 4 ? 9.0f : (gem->value >= 3 ? 7.5f : 5.5f));
        if (gem->value >= 4) {
            render_draw_ui_texture_centered(UI_TEX_XP_MEGA, sx, sy + gbob, 1.25f);
        } else if (gem->value == 3) {
            render_draw_ui_texture_centered(UI_TEX_XP_BIG, sx, sy + gbob, 1.15f);
        } else if (gem->value == 2) {
            render_draw_ui_texture_centered(UI_TEX_XP_GEM_2, sx, sy + gbob, 1.08f);
        } else {
            render_draw_ui_texture_centered(UI_TEX_XP_GEM_1, sx, sy + gbob, 1.00f);
        }
    }

    for (int i = 0; i < MAX_PICKUPS; i++) {
        PickupItem *pk = &g_game.pickups[i];
        if (pk->active) {
            float sx = pk->x - cx;
            float sy = pk->y - cy;
            float bob = sinf(g_game.run_timer * 5.0f + (float)i * 1.5f) * 3.0f;
            render_draw_shadow(sx, sy + 10.0f, 10.0f);

            if (pk->type == 0) {

                render_draw_ui_texture_centered(UI_TEX_POTION, sx, sy + bob, 0.40f);
            } else if (pk->type == 1) {

                render_draw_ui_texture_centered(UI_TEX_BALL, sx, sy + bob, 0.55f);
            } else if (pk->type == 2) {

                render_draw_ui_texture_centered(UI_TEX_MAGNET, sx, sy + bob, 0.40f);
            } else if (pk->type == 3) {

                render_draw_ui_texture_centered(UI_TEX_POUCH, sx, sy + bob, 0.40f);
            } else if (pk->type == 4) {

                render_draw_ui_texture_centered(UI_TEX_RARE_CANDY, sx, sy + bob, 1.35f);
            } else if (pk->type == 5) {

                render_draw_ui_texture_centered(UI_TEX_GACHA_TICKET, sx, sy + bob, 1.35f);
            } else if (pk->type == 6) {

                float pulse = 0.5f + 0.5f * sinf(g_game.run_timer * 5.5f);
                render_draw_circle_ground(sx, sy + 10.0f, 24.0f + 4.0f * pulse, RGBA8_C(55, 235, 115, 78));
                render_draw_circle_ground(sx, sy + 10.0f, 15.0f + 2.0f * pulse, RGBA8_C(110, 255, 160, 118));
                render_draw_ui_texture_centered(UI_TEX_POTION, sx, sy + bob, 0.56f);
            } else if (pk->type == 7) {

                float pulse = 0.5f + 0.5f * sinf(g_game.run_timer * 6.5f);
                render_draw_circle_ground(sx, sy + 10.0f, 20.0f + 3.5f * pulse, RGBA8_C(255, 218, 55, 86));
                render_draw_circle_ground(sx, sy + 10.0f, 12.0f + 2.0f * pulse, RGBA8_C(130, 255, 170, 120));
                render_draw_ui_texture_centered(UI_TEX_REVIVE, sx, sy + bob, 1.45f);
            }
        }
    }

    float psx = g_game.player.x - cx + g_game.player.lunge_ox;
    float psy = g_game.player.y - cy + g_game.player.lunge_oy;
    static DrawItem items[MAX_ENEMIES + MAX_PROPS + 2];
    float bb_sx[8], bb_sy[8], bb_pct[8];
    int n_bb = 0;
    int n_items = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *en = &g_game.enemies[i];
        if (!en->active) continue;
        float sx = en->x - cx, sy = en->y - cy;
        if (sx < -220.0f || sx > SCREEN_W + 220.0f || sy < -220.0f || sy > SCREEN_H + 280.0f) continue;
        items[n_items].y = en->y; items[n_items].kind = 0; items[n_items].idx = i; n_items++;
    }
    items[n_items].y = g_game.player.y + g_game.player.lunge_oy; items[n_items].kind = 1; items[n_items].idx = 0; n_items++;
    for (int p = 0; p < MAX_PROPS; p++) {
        const WorldProp *prop = &g_game.props[p];
        if (!prop->active || !is_tall_prop(prop->type)) continue;
        float sx = prop->x - cx, sy = prop->y - cy;
        if (sx < -240.0f || sx > SCREEN_W + 240.0f || sy < -240.0f || sy > SCREEN_H + 320.0f) continue;
        items[n_items].y = prop->y; items[n_items].kind = 2; items[n_items].idx = p; n_items++;
    }
    qsort(items, (size_t)n_items, sizeof(DrawItem), cmp_draw_item);

    for (int k = 0; k < n_items; k++) {
        if (items[k].kind != 0) continue;
        const Enemy *en = &g_game.enemies[items[k].idx];
        if (en->is_boss) continue;
        float sx = en->x - cx, sy = en->y - cy;
        float hop = (en->jump_timer > 0.0f) ? sinf((en->jump_timer / 0.32f) * 3.14159f) * 16.0f : 0.0f;
        render_draw_oval_shadow(sx, sy, en->radius * 1.05f * (1.0f - hop * 0.015f));
    }

    for (int k = 0; k < n_items; k++) {
        const DrawItem *it = &items[k];
        if (it->kind == 0) {
            Enemy *en = &g_game.enemies[it->idx];
            float sx = en->x - cx, sy = en->y - cy;
            float hop = (en->jump_timer > 0.0f) ? sinf((en->jump_timer / 0.32f) * 3.14159f) * 16.0f : 0.0f;
            if (en->is_boss) {
                render_draw_shadow(sx, sy, en->radius * 1.3f * (1.0f - hop * 0.015f));
            } else {
                render_skip_next_shadow();
            }
            uint32_t tint = 0;
            if (en->hit_flash > 0.0f) {
                tint = RGBA8_C(255, 255, 255, 200);
            } else if (en->debuff_col != 0 && (en->slow_t > 0.0f || en->stun_t > 0.0f || en->dot_t > 0.0f)) {
                uint32_t dc = en->debuff_col;
                int dr = dc & 0xFF;
                int dg = (dc >> 8) & 0xFF;
                int db = (dc >> 16) & 0xFF;

                float pulse = 0.72f + 0.18f * sinf(g_game.run_timer * 9.0f + (float)it->idx);
                int tr = (int)(dr * pulse + 255.0f * (1.0f - pulse));
                int tg = (int)(dg * pulse + 255.0f * (1.0f - pulse));
                int tb = (int)(db * pulse + 255.0f * (1.0f - pulse));
                if (tr > 255) tr = 255; if (tg > 255) tg = 255; if (tb > 255) tb = 255;
                tint = RGBA8_C(tr, tg, tb, 255);
            }
            float sc = en->is_boss ? 1.25f : 0.74f;
            int draw_dex = (en->dex > 0) ? en->dex : POKEMON_CATALOG[en->type].dex;
            render_draw_pokemon_sprite(draw_dex, ANIM_WALK, sx, sy - hop, en->anim_frame, en->dir, sc, 0, tint);
            if (en->is_boss && n_bb < 8) {
                bb_sx[n_bb] = sx; bb_sy[n_bb] = sy - hop; bb_pct[n_bb] = en->hp / en->max_hp; n_bb++;
            }
        } else if (it->kind == 1) {

            for (int gi = 0; gi < MAX_DASH_GHOSTS; gi++) {
                const DashGhost *dg = &g_game.dash_ghosts[gi];
                if (!dg->active) continue;
                float t = dg->life / (dg->max_life > 0.01f ? dg->max_life : 0.01f);
                if (t < 0.0f) t = 0.0f;
                uint8_t ga = (uint8_t)(210.0f * t);
                uint32_t gtint = (dg->color & 0x00FFFFFFu) | ((uint32_t)ga << 24);
                render_draw_pokemon_sprite(dg->dex, dg->anim, dg->x - cx, dg->y - cy,
                                           dg->frame, dg->dir, 0.78f, 0, gtint);
            }
            float phop = (g_game.player.jump_timer > 0.0f) ? sinf((g_game.player.jump_timer / 0.34f) * 3.14159f) * 24.0f : 0.0f;
            render_draw_shadow(psx, psy, 14.0f * (1.0f - phop * 0.015f));
            uint32_t ptint = (g_game.player.iframe_timer > 0.0f && ((int)(g_game.player.iframe_timer * 15.0f) % 2 == 0)) ?
                             RGBA8_C(255, 100, 100, 180) : 0;
            render_draw_pokemon_sprite(POKEMON_CATALOG[g_game.player.char_idx].dex,
                                       g_game.player.anim_state, psx, psy - phop,
                                       g_game.player.anim_frame, g_game.player.dir, 0.78f, 0, ptint);

            if (g_game.buffs[2].stacks > 0) {
                float shine = 0.5f + 0.5f * sinf(g_game.run_timer * 6.5f);
                render_draw_circle(psx, psy - phop - 14.0f, 16.0f + 3.0f * shine, RGBA8_C(225, 235, 255, (int)(45.0f + 35.0f * shine)));
            }
            if (g_game.buffs[1].stacks > 0) {
                float shine = 0.5f + 0.5f * sinf(g_game.run_timer * 8.5f);
                render_draw_circle(psx, psy - phop - 14.0f, 15.0f + 4.0f * shine, RGBA8_C(200, 245, 255, (int)(40.0f + 30.0f * shine)));
            }
            if (g_game.buffs[0].stacks > 0) {
                float shine = 0.5f + 0.5f * sinf(g_game.run_timer * 7.0f);
                render_draw_circle(psx, psy - phop - 14.0f, 16.0f + 3.0f * shine, RGBA8_C(255, 215, 120, (int)(40.0f + 30.0f * shine)));
            }
            if (g_game.buffs[3].stacks > 0) {
                float shine = 0.5f + 0.5f * sinf(g_game.run_timer * 5.5f);
                render_draw_circle(psx, psy - phop - 14.0f, 16.0f + 3.0f * shine, RGBA8_C(120, 255, 160, (int)(40.0f + 30.0f * shine)));
            }
            if (g_game.buffs[4].stacks > 0) {
                float ft = g_game.buffs[4].timer / 0.75f;
                if (ft > 1.0f) ft = 1.0f;
                if (ft < 0.0f) ft = 0.0f;
                float pulse = 0.5f + 0.5f * sinf(g_game.run_timer * 14.0f);
                float cy_s = psy - phop - 14.0f;
                render_draw_circle(psx, cy_s, 18.0f + 8.0f * (1.0f - ft), RGBA8_C(150, 255, 195, (int)(72.0f * ft)));
                render_draw_circle(psx, cy_s, 11.0f + 4.0f * pulse, RGBA8_C(255, 255, 242, (int)(115.0f * ft)));
                float ray = 14.0f + 7.0f * pulse;
                uint32_t rcol = RGBA8_C(255, 255, 240, (int)(190.0f * ft));
                render_draw_line(psx - ray, cy_s, psx + ray, cy_s, rcol, 2.0f);
                render_draw_line(psx, cy_s - ray, psx, cy_s + ray, rcol, 2.0f);
            }
        } else {
            const WorldProp *prop = &g_game.props[it->idx];
            float px = prop->x - cx, py = prop->y - cy;
            if (prop->type == PROP_CACTUS_A || prop->type == PROP_CACTUS_B) {
                render_draw_world_prop(prop->type, px, py, 1.0f);
            } else if (prop->type == PROP_ROCK_A || prop->type == PROP_ROCK_B || prop->type == PROP_ROCK_ICE) {
                render_draw_world_prop(prop->type, px, py, 1.0f);
            } else if (prop->type == PROP_BERRY_TREE) {
                render_draw_world_prop(prop->type, px, py, 1.1f);
            } else {
                render_draw_world_prop(prop->type, px, py, 1.1f);
            }
        }
    }

    for (int zi = 0; zi < MAX_ZONES; zi++) {
        Zone *z = &g_game.zones[zi];
        if (!z->active || !z->is_decoy) continue;
        float zx = z->x - cx, zy = z->y - cy;
        float pulse = 0.88f + 0.08f * sinf(g_game.run_timer * 8.0f);
        render_draw_shadow(zx, zy, 13.5f);
        render_draw_pokemon_sprite(POKEMON_CATALOG[g_game.player.char_idx].dex, ANIM_IDLE,
            zx, zy - 3.0f, (int)(g_game.run_timer * 8.0f) % 4, g_game.player.dir,
            pulse * 0.78f, 0, RGBA8_C(195, 255, 205, 175));
    }
    for (int wi = 0; wi < MAX_WHIP_EFFECTS; wi++) {
        WhipEffect *wh = &g_game.whips[wi];
        if (!wh->active) continue;
        float prog = wh->timer / wh->max_life;
        float phop = (g_game.player.jump_timer > 0.0f) ? sinf((g_game.player.jump_timer / 0.32f) * 3.14159f) * 18.0f : 0.0f;
        render_draw_whip_ex(psx, psy - phop - 14.0f, wh->angle, prog, wh->radius * 0.78f, wh->arc, wh->seed, wh->whip_type);
    }

    int pj_ord[MAX_PROJECTILES], n_pj = 0;
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (g_game.projectiles[i].active) pj_ord[n_pj++] = i;
    }
    for (int a = 1; a < n_pj; a++) {
        int key = pj_ord[a];
        float ky = g_game.projectiles[key].y;
        int b = a - 1;
        while (b >= 0 && g_game.projectiles[pj_ord[b]].y > ky) {
            pj_ord[b + 1] = pj_ord[b];
            b--;
        }
        pj_ord[b + 1] = key;
    }
    for (int pi = 0; pi < n_pj; pi++) {
        Projectile *pj = &g_game.projectiles[pj_ord[pi]];
        if (!pj->active) continue;
        float sx = pj->x - cx, sy = pj->y - cy;
        float sc = 0.95f;
        if (pj->vfx_shape) {
            if (strcmp(pj->vfx_shape, "leaf") == 0) sc = 0.90f;
            else if (strcmp(pj->vfx_shape, "strike") == 0) sc = 0.90f;
            else if (strcmp(pj->vfx_shape, "shock") == 0) sc = (pj->damage == 0 ? 1.0f : 0.85f);
            else if (strcmp(pj->vfx_shape, "slam") == 0) sc = 0.90f;
            else if (strcmp(pj->vfx_shape, "spin") == 0) sc = 0.95f;
        }
        if (pj->draw_scale > 0.0f) sc = pj->draw_scale;
        if (pj->vfx_shape && !strcmp(pj->vfx_shape, "arc")) {
            render_draw_slash_arc(sx, sy, pj->angle, 1.0f - pj->life / (pj->max_life > 0.01f ? pj->max_life : 0.26f), pj->draw_scale * 0.78f, pj->color);
        } else if (pj->vfx_shape && !strcmp(pj->vfx_shape, "self_slam")) {
            float pr = 1.0f - pj->life / (pj->max_life > 0.01f ? pj->max_life : 0.36f);
            if (pr < 0.0f) pr = 0.0f; if (pr > 1.0f) pr = 1.0f;
            float fall = pr < 0.5f ? 1.0f - pr / 0.5f : 0.0f;
            float hgt = 80.0f * fall * fall;
            int al = pr < 0.5f ? 150 : (int)(150.0f * (1.0f - (pr - 0.5f) / 0.5f));
            float k = pj->draw_scale * 0.78f * (fall > 0.0f ? 1.0f : 1.12f);
            render_draw_shadow(sx, sy, 16.0f * pj->draw_scale);
            render_draw_pokemon_sprite((int)pj->orbit_radius, ANIM_ATTACK, sx, sy - hgt, pr < 0.5f ? 0 : 1, (int)pj->orbit_angle, k, 0, RGBA8_C(255, 255, 255, al));
        } else if (pj->vfx_shape && !strcmp(pj->vfx_shape, "egg")) {
            render_draw_egg(sx, sy, 1.0f - pj->life / (pj->max_life > 0.01f ? pj->max_life : 0.9f));
        } else
        {
            int al = pj->alpha ? pj->alpha : 255;
            int draw_frame = pj->anim_frame;
            if (pj->vfx_shape && !strncmp(pj->vfx_shape, "boom_", 5)) {
                float pr = 1.0f - pj->life / (pj->max_life > 0.01f ? pj->max_life : 0.36f);
                if (pr < 0.0f) pr = 0.0f; if (pr > 1.0f) pr = 1.0f;
                sc *= 0.55f + 0.6f * pr;
                al = (int)(255.0f * (1.0f - 0.85f * pr * pr));
            } else if (pj->vfx_shape && !strcmp(pj->vfx_shape, "bulldoze")) {
                float pr = 1.0f - pj->life / (pj->max_life > 0.01f ? pj->max_life : 0.54f);
                if (pr < 0.0f) pr = 0.0f; if (pr > 1.0f) pr = 1.0f;
                draw_frame = (int)(pr * 8.99f);
                if (draw_frame > 8) draw_frame = 8;
                if (pr > 0.85f) al = (int)((float)al * (1.0f - (pr - 0.85f) / 0.15f));
            } else if (pj->vfx_shape && (!strcmp(pj->vfx_shape, "crack") || !strcmp(pj->vfx_shape, "slamdust"))) {
                float pr = 1.0f - pj->life / (pj->max_life > 0.01f ? pj->max_life : 0.5f);
                if (pr < 0.0f) pr = 0.0f; if (pr > 1.0f) pr = 1.0f;
                if (!strcmp(pj->vfx_shape, "slamdust")) sc *= 0.7f + 0.4f * pr;
                al = (int)((float)al * (pr < 0.5f ? 1.0f : 1.0f - (pr - 0.5f) * 2.0f));
            } else {

                if (pj->max_life > 0.05f) {
                    float remaining_ratio = pj->life / pj->max_life;
                    if (remaining_ratio < 0.12f) {
                        float fade = remaining_ratio / 0.12f;
                        al = (int)((float)al * fade);
                        if (al < 0) al = 0;
                    }
                }
            }
            render_set_vfx_origin(pj->src_x, pj->src_y, pj->has_src);

            float vfx_sc = pj->has_src ? sc : (sc * 0.78f);
            render_draw_vfx_ex(pj->vfx_shape, sx, sy, pj->angle, vfx_sc, draw_frame, (uint8_t)al,
                               pj->tint_by_color ? pj->color : 0xFFFFFFFFu,
                               pj->is_boss_proj, pj->vx, pj->vy);
            render_set_vfx_origin(0.0f, 0.0f, false);
        }
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *pt = &g_game.particles[i];
        if (pt->active) render_draw_circle(pt->x - cx, pt->y - cy, pt->size, pt->color);
    }
    render_world_end();

    for (int b = 0; b < n_bb; b++) {
        float ox, oy, k;
        render_world_to_screen(bb_sx[b], bb_sy[b], 150.0f, &ox, &oy, &k);
        float bw = 96.0f * k, bh = 8.0f * k;
        render_draw_rect(ox - bw * 0.5f, oy, bw, bh, RGBA8_C(0, 0, 0, 200));
        render_draw_rect(ox - bw * 0.5f, oy, bw * bb_pct[b], bh, RGBA8_C(255, 50, 50, 255));
        render_draw_rect_outline(ox - bw * 0.5f, oy, bw, bh, RGBA8_C(255, 255, 255, 200), 1.0f);
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *en = &g_game.enemies[i];
        if (!en->active || en->is_boss || en->hp >= en->max_hp * 0.995f) continue;
        float ox, oy, k;
        render_world_to_screen(en->x - cx, en->y - cy, 32.0f, &ox, &oy, &k);
        if (ox < -40.0f || ox > SCREEN_W + 40.0f || oy < -40.0f || oy > SCREEN_H + 40.0f) continue;
        float bw = 28.0f * k, bh = 4.0f * k;
        float pct = en->hp / (en->max_hp > 0.01f ? en->max_hp : 1.0f);
        if (pct < 0.0f) pct = 0.0f; if (pct > 1.0f) pct = 1.0f;
        uint32_t hp_col = pct > 0.5f ? RGBA8_C(60, 225, 80, 240) : (pct > 0.25f ? RGBA8_C(255, 205, 45, 240) : RGBA8_C(255, 55, 55, 240));
        render_draw_rect(ox - bw * 0.5f, oy - 14.0f * k, bw, bh, RGBA8_C(10, 15, 25, 220));
        render_draw_rect(ox - bw * 0.5f, oy - 14.0f * k, bw * pct, bh, hp_col);
        render_draw_rect_outline(ox - bw * 0.5f, oy - 14.0f * k, bw, bh, RGBA8_C(0, 0, 0, 180), 1.0f);
    }
    for (int i = 0; i < MAX_POPUPS; i++) {
        DamagePopup *pop = &g_game.popups[i];
        if (pop->active) {
            char dstr[16];
            snprintf(dstr, sizeof(dstr), "%d", pop->amount);
            float ox, oy, k;
            render_world_to_screen(pop->x - cx, pop->y - cy, 0.0f, &ox, &oy, &k);
            if (k < 0.7f) k = 0.7f;
            if (k > 1.4f) k = 1.4f;
            render_draw_text(ox, oy, 1.4f * k, pop->color, dstr);
        }
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *en = &g_game.enemies[i];
        if (!en->active || !en->is_boss) continue;
        float ax = 0.0f, ay = 0.0f, ang = 0.0f, dist_m = 0.0f;
        if (!project_indicator_dir(en->x, en->y, cx, cy, 68.0f, SCREEN_W - 68.0f, 134.0f, SCREEN_H - 96.0f,
                                   &ax, &ay, &ang, &dist_m)) continue;
        uint32_t col = ((int)(g_game.run_timer * 6.0f) % 2 == 0) ? RGBA8_C(255, 60, 60, 255) : RGBA8_C(255, 215, 0, 255);
        draw_boss_arrow(ax, ay, ang, col);
        char lbl[64];
        const char *bname = (en->name && en->name[0] != '\0') ? en->name : "Boss";
        snprintf(lbl, sizeof(lbl), "%s %dm", bname, (int)dist_m);
        float bw = 166.0f, bh = 20.0f;
        float tx = ax - cosf(ang) * 44.0f - bw * 0.5f;
        float ty = ay - sinf(ang) * 34.0f - bh * 0.5f;
        if (tx < 16.0f) tx = 16.0f;
        if (tx + bw > SCREEN_W - 16.0f) tx = SCREEN_W - 16.0f - bw;
        if (ty < 114.0f) ty = 114.0f;
        if (ty + bh > SCREEN_H - 92.0f) ty = SCREEN_H - 92.0f - bh;
        render_draw_rect(tx, ty, bw, bh, RGBA8_C(10, 10, 20, 224));
        render_draw_rect_outline(tx, ty, bw, bh, RGBA8_C(255, 215, 0, 210), 1.0f);
        render_draw_box_text_centered(tx, ty, bw, bh, 0.96f, RGBA8_C(255, 220, 50, 255), lbl);
    }

    for (int i = 0; i < MAX_PICKUPS; i++) {
        PickupItem *pk = &g_game.pickups[i];
        if (!pk->active || pk->type != 6) continue;
        float ax = 0.0f, ay = 0.0f, ang = 0.0f, dist_m = 0.0f;
        if (!project_indicator_dir(pk->x, pk->y, cx, cy, 58.0f, SCREEN_W - 58.0f, 134.0f, SCREEN_H - 96.0f,
                                   &ax, &ay, &ang, &dist_m)) continue;
        uint32_t gcol = ((int)(g_game.run_timer * 5.0f) % 2 == 0) ? RGBA8_C(55, 240, 110, 255) : RGBA8_C(145, 255, 175, 255);
        draw_dir_arrow(ax, ay, ang, gcol, 0.65f);
        char lbl[32];
        snprintf(lbl, sizeof(lbl), "HEAL %dm", (int)dist_m);
        float bw = 104.0f, bh = 18.0f;
        float tx = ax - cosf(ang) * 34.0f - bw * 0.5f;
        float ty = ay - sinf(ang) * 26.0f - bh * 0.5f;
        if (tx < 16.0f) tx = 16.0f;
        if (tx + bw > SCREEN_W - 16.0f) tx = SCREEN_W - 16.0f - bw;
        if (ty < 114.0f) ty = 114.0f;
        if (ty + bh > SCREEN_H - 92.0f) ty = SCREEN_H - 92.0f - bh;
        render_draw_rect(tx, ty, bw, bh, RGBA8_C(10, 28, 18, 224));
        render_draw_rect_outline(tx, ty, bw, bh, RGBA8_C(75, 240, 125, 220), 1.0f);
        render_draw_box_text_centered(tx, ty, bw, bh, 0.88f, RGBA8_C(110, 255, 150, 255), lbl);
    }

    {
        float exp_h = 16.0f;
        float exp_max = (g_game.player.exp_to_next > 0) ? (float)g_game.player.exp_to_next : 1.0f;
        float exp_pct = (float)g_game.player.exp / exp_max;
        if (exp_pct < 0.0f) exp_pct = 0.0f;
        if (exp_pct > 1.0f) exp_pct = 1.0f;
        render_draw_rect(0.0f, 0.0f, SCREEN_W, exp_h, RGBA8_C(18, 26, 36, 255));
        render_draw_rect(0.0f, exp_h - 2.0f, SCREEN_W, 2.0f, UI_MODERN_BORDER);
        float bar_x = 56.0f, bar_y = 2.0f, bar_w = SCREEN_W - 64.0f, bar_h = exp_h - 5.0f;
        render_draw_rect(bar_x, bar_y, bar_w, bar_h, RGBA8_C(26, 38, 52, 255));
        float fw = bar_w * exp_pct;
        if (fw > 0.0f) {
            render_draw_rect(bar_x, bar_y, fw, bar_h, RGBA8_C(34, 193, 234, 255));
            render_draw_rect(bar_x, bar_y, fw, 3.0f, RGBA8_C(164, 242, 255, 255));
        }
        for (float sx_t = bar_x + 48.0f; sx_t < bar_x + bar_w; sx_t += 48.0f) {
            render_draw_rect(sx_t, bar_y, 1.5f, bar_h, RGBA8_C(16, 26, 34, 120));
        }
        char top_lv[20], top_exp[40];
        snprintf(top_lv, sizeof(top_lv), "LV %d", g_game.player.level);
        snprintf(top_exp, sizeof(top_exp), "EXP  %d / %d", g_game.player.exp, g_game.player.exp_to_next);
        render_draw_text(6.0f, 1.0f, 0.90f, RGBA8_C(255, 255, 255, 255), top_lv);
        render_draw_text_centered(SCREEN_W * 0.5f, 1.0f, 0.85f, RGBA8_C(248, 254, 255, 255), top_exp);
    }

    if (g_game.player.char_idx >= 0 && g_game.player.char_idx < TOTAL_CATALOG_POKEMON) {
        const PokemonData *cur_pd = &POKEMON_CATALOG[g_game.player.char_idx];
        float pf_x = 14.0f, pf_y = 19.0f, pf_w = 279.0f, pf_h = 66.0f;

        ui_draw_card(pf_x, pf_y, pf_w, pf_h, RGBA8_C(251, 229, 148, 255), RGBA8_C(238, 198, 72, 255), 2.0f, 10.0f);

        float port_x = pf_x + 9.0f, port_y = pf_y + 9.0f, port_s = 46.0f;
        ui_draw_card(port_x, port_y, port_s, port_s, RGBA8_C(247, 221, 134, 255), RGBA8_C(236, 196, 72, 255), 1.5f, 6.0f);
        float pmax = (float)(cur_pd->walk.w > cur_pd->walk.h ? cur_pd->walk.w : cur_pd->walk.h);
        if (pmax < 16.0f) pmax = 16.0f;
        float psc = 26.0f / pmax;
        if (psc > 0.85f) psc = 0.85f;
        render_draw_pokemon_sprite(cur_pd->dex, ANIM_IDLE, port_x + port_s * 0.5f, port_y + port_s - 5.0f, 0, 0, psc, 0, 0);

        ui_text_mid(0, pf_x + 62.0f, pf_y + 15.0f, 1.15f, UI_MODERN_TEXT_DARK, cur_pd->name);
        char lv_buf[32];
        snprintf(lv_buf, sizeof(lv_buf), "Lv%d", g_game.player.level);
        ui_text_mid(0, pf_x + pf_w - 12.0f - (float)ui_text_w(0, 1.15f, lv_buf), pf_y + 15.0f, 1.15f, UI_MODERN_TEXT_DARK, lv_buf);

        float hpx = pf_x + 61.0f, hpy = pf_y + 27.0f, hpw = pf_w - 72.0f, hph = 14.0f;
        ui_draw_card(hpx, hpy, hpw, hph, RGBA8_C(36, 40, 52, 255), RGBA8_C(20, 24, 32, 255), 1.2f, 3.0f);
        ui_text_mid(1, hpx + 4.0f, hpy + hph * 0.5f, 0.72f, RGBA8_C(236, 204, 96, 255), "HP");
        float hp_pct = g_game.player.hp / (g_game.player.max_hp > 0.01f ? g_game.player.max_hp : 1.0f);
        if (hp_pct < 0.0f) hp_pct = 0.0f; if (hp_pct > 1.0f) hp_pct = 1.0f;
        float bx = hpx + 22.0f, bw = hpw - 24.0f;
        if (hp_pct > 0.0f) {
            render_draw_rect(bx, hpy + 2.0f, bw * hp_pct, hph - 4.0f, RGBA8_C(60, 208, 112, 255));
            render_draw_rect(bx, hpy + 2.0f, bw * hp_pct, 2.0f, RGBA8_C(160, 248, 180, 255));
        }

        if (g_game.revives_left > 0) {
            char rev_str[24];
            snprintf(rev_str, sizeof(rev_str), "REV x%d", g_game.revives_left);
            render_draw_text(pf_x + 68.0f, pf_y + 47.0f, 1.02f, RGBA8_C(217, 119, 6, 255), rev_str);
        }
        char hp_str[32];
        snprintf(hp_str, sizeof(hp_str), "%d/%d", (int)ceilf(g_game.player.hp), (int)ceilf(g_game.player.max_hp));
        ui_text_mid(0, pf_x + pf_w - 12.0f - (float)ui_text_w(0, 0.96f, hp_str), pf_y + 54.0f, 0.96f, UI_MODERN_TEXT_DARK, hp_str);
    }

    {
        float pill_y = 19.0f, pill_h = 34.0f;
        int mins = (int)g_game.run_timer / 60;
        int secs = (int)g_game.run_timer % 60;
        char t_buf[16], k_buf[32], c_buf[32], tk_buf[24];
        snprintf(t_buf, sizeof(t_buf), "%02d:%02d", mins, secs);
        snprintf(k_buf, sizeof(k_buf), "KOs: %d", g_game.player.kills);
        snprintf(c_buf, sizeof(c_buf), "+%d", g_game.coins_collected);
        snprintf(tk_buf, sizeof(tk_buf), "+%d TKT", g_game.tickets_collected);

        float p1_x = 300.0f, p1_w = 86.0f;
        ui_draw_card(p1_x, pill_y, p1_w, pill_h, RGBA8_C(190, 222, 247, 255), RGBA8_C(84, 160, 226, 255), 1.8f, 9.0f);
        render_draw_circle(p1_x + 16.0f, pill_y + 17.0f, 8.5f, UI_MODERN_BORDER);
        render_draw_circle(p1_x + 16.0f, pill_y + 17.0f, 6.5f, 0xFFFFFFFF);
        render_draw_line(p1_x + 16.0f, pill_y + 17.0f, p1_x + 16.0f, pill_y + 12.0f, UI_MODERN_BORDER, 1.5f);
        render_draw_line(p1_x + 16.0f, pill_y + 17.0f, p1_x + 20.0f, pill_y + 19.0f, UI_MODERN_BORDER, 1.5f);
        ui_text_mid(2, p1_x + 28.0f, pill_y + pill_h * 0.5f, 1.1f, UI_MODERN_TEXT_DARK, t_buf);

        float p2_x = p1_x + p1_w + 5.0f, p2_w = 132.0f;
        ui_draw_card(p2_x, pill_y, p2_w, pill_h, RGBA8_C(244, 123, 108, 255), RGBA8_C(226, 88, 72, 255), 1.8f, 9.0f);
        ui_text_mid(0, p2_x + 6.0f, pill_y + pill_h * 0.5f, 2.0f, 0xFFFFFFFF, "[Ball]");
        ui_text_mid(2, p2_x + 38.0f, pill_y + pill_h * 0.5f, 1.1f, UI_MODERN_TEXT_DARK, k_buf);

        float p3_x = p2_x + p2_w + 5.0f, p3_w = 104.0f;
        ui_draw_card(p3_x, pill_y, p3_w, pill_h, RGBA8_C(140, 226, 190, 255), RGBA8_C(70, 188, 140, 255), 1.8f, 9.0f);
        render_draw_ui_texture_centered(UI_TEX_POUCH, p3_x + 22.0f, pill_y + pill_h * 0.5f, 0.42f);
        ui_text_mid(2, p3_x + 38.0f, pill_y + pill_h * 0.5f, 1.1f, UI_MODERN_TEXT_DARK, c_buf);

        float p4_x = p3_x + p3_w + 5.0f, p4_w = 96.0f;
        ui_draw_card(p4_x, pill_y, p4_w, pill_h, RGBA8_C(252, 222, 110, 255), RGBA8_C(228, 178, 52, 255), 1.8f, 9.0f);
        ui_text_mid(0, p4_x + 6.0f, pill_y + pill_h * 0.5f, 2.0f, 0xFFFFFFFF, "[Ticket]");
        ui_text_mid(2, p4_x + 36.0f, pill_y + pill_h * 0.5f, 1.1f, UI_MODERN_TEXT_DARK, tk_buf);

        float p5_x = p4_x + p4_w + 5.0f, p5_w = 74.0f;
        uint32_t rev_bg = (g_game.revives_left > 0) ? RGBA8_C(255, 240, 175, 255) : RGBA8_C(224, 230, 238, 255);
        uint32_t rev_bd = (g_game.revives_left > 0) ? RGBA8_C(217, 119, 6, 255)   : RGBA8_C(148, 163, 184, 255);
        ui_draw_card(p5_x, pill_y, p5_w, pill_h, rev_bg, rev_bd, 1.8f, 9.0f);
        render_draw_ui_texture_centered(UI_TEX_REVIVE, p5_x + 20.0f, pill_y + pill_h * 0.5f, 1.50f);
        char rv_buf[16];
        snprintf(rv_buf, sizeof(rv_buf), "x%d", g_game.revives_left);
        ui_text_mid(2, p5_x + 36.0f, pill_y + pill_h * 0.5f, 1.1f,
                    (g_game.revives_left > 0) ? RGBA8_C(180, 83, 9, 255) : RGBA8_C(100, 116, 139, 255), rv_buf);

        float bbx = 324.0f, bby = pill_y + pill_h + 6.0f, bbw = 392.0f, bbh = 29.0f;
        if (g_game.boss_active) {
            const Enemy *active_boss = NULL;
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (g_game.enemies[i].active && g_game.enemies[i].is_boss) {
                    active_boss = &g_game.enemies[i];
                    break;
                }
            }
            float bhp_pct = (active_boss && active_boss->max_hp > 0.0f) ? (active_boss->hp / active_boss->max_hp) : 1.0f;
            if (bhp_pct < 0.0f) bhp_pct = 0.0f;
            if (bhp_pct > 1.0f) bhp_pct = 1.0f;
            ui_draw_card(bbx, bby, bbw, bbh, RGBA8_C(250, 125, 98, 255), RGBA8_C(226, 88, 62, 255), 2.0f, 8.0f);
            render_draw_rect(bbx + 3.0f, bby + 3.0f, (bbw - 6.0f) * bhp_pct, bbh - 6.0f, RGBA8_C(220, 38, 38, 255));
            char b_str[96];
            const char *bn = g_game.current_boss_name[0] ? g_game.current_boss_name : g_game.next_boss_name;
            if (active_boss) {
                snprintf(b_str, sizeof(b_str), "BOSS: %s (%d/%d)", bn, (int)ceilf(active_boss->hp), (int)ceilf(active_boss->max_hp));
            } else {
                snprintf(b_str, sizeof(b_str), "BOSS: %s!", bn);
            }
            ui_text_center(2, bbx, bby, bbw, bbh, 0.95f, RGBA8_C(255, 255, 255, 255), b_str);
        } else {
            int b_mins = (int)g_game.next_boss_timer / 60;
            int b_secs = (int)g_game.next_boss_timer % 60;
            if (b_mins < 0) b_mins = 0; if (b_secs < 0) b_secs = 0;
            ui_draw_card(bbx, bby, bbw, bbh, RGBA8_C(250, 125, 98, 255), RGBA8_C(226, 88, 62, 255), 2.0f, 8.0f);
            char b_str[64];
            snprintf(b_str, sizeof(b_str), "BOSS IN: %02d:%02d (%s)", b_mins, b_secs, g_game.next_boss_name);
            ui_text_center(2, bbx, bby, bbw, bbh, 0.95f, UI_MODERN_TEXT_DARK, b_str);
            float bw_txt = (float)render_get_text_width_pix(0.95f, b_str);
            ui_warn_icon(bbx + bbw * 0.5f - bw_txt * 0.5f - 16.0f, bby + bbh * 0.5f, 8.0f, UI_MODERN_TEXT_DARK);
            ui_warn_icon(bbx + bbw * 0.5f + bw_txt * 0.5f + 16.0f, bby + bbh * 0.5f, 8.0f, UI_MODERN_TEXT_DARK);
        }
    }

    if (g_game.revive_anim_timer > 0.0f) {
        float dur = 2.1f;
        float p = 1.0f - (g_game.revive_anim_timer / dur);
        if (p < 0.0f) p = 0.0f;
        if (p > 1.0f) p = 1.0f;
        float fade = (p > 0.78f) ? (1.0f - (p - 0.78f) / 0.22f) : 1.0f;
        float rx = SCREEN_W * 0.5f, ry = SCREEN_H * 0.5f, rk = 1.0f;
        render_world_to_screen(g_game.player.x - cx, g_game.player.y - cy, 20.0f, &rx, &ry, &rk);

        render_draw_circle(rx, ry, (18.0f + 56.0f * p) * rk, RGBA8_C(255, 226, 75, (int)(92.0f * fade)));
        render_draw_circle(rx, ry, (12.0f + 34.0f * p) * rk, RGBA8_C(120, 255, 175, (int)(120.0f * fade)));

        for (int s = 0; s < 8; s++) {
            float sa = (float)s * (6.2831853f / 8.0f) + p * 4.2f;
            float sr = (20.0f + 26.0f * p) * rk;
            float sx_m = rx + cosf(sa) * sr;
            float sy_m = ry + sinf(sa) * sr * 0.6f - p * 34.0f * rk;
            render_draw_circle(sx_m, sy_m, 3.5f * rk, RGBA8_C(255, 252, 220, (int)(235.0f * fade)));
        }

        float drop = fminf(1.0f, p * 2.4f);
        float pot_y = ry - (68.0f - 36.0f * drop) * rk;
        float pot_sc = (2.15f + 0.35f * sinf(p * 12.0f)) * rk;
        render_draw_circle(rx, pot_y, 20.0f * rk, RGBA8_C(255, 232, 95, (int)(130.0f * fade)));
        render_draw_ui_texture_centered(UI_TEX_REVIVE, rx, pot_y, pot_sc);

        float rbw = 360.0f, rbh = 48.0f;
        float rbx = SCREEN_W * 0.5f - rbw * 0.5f, rby = 102.0f;
        ui_draw_card(rbx, rby, rbw, rbh, UI_MODERN_WHITE, UI_BANNER_YELLOW, 2.5f, 10.0f);
        render_draw_text_centered(rbx + rbw * 0.5f, rby + 6.0f, 1.32f, RGBA8_C(217, 119, 6, 255), "REVIVE POTION USED!");
        render_draw_text_centered(rbx + rbw * 0.5f, rby + 26.0f, 1.10f, RGBA8_C(16, 185, 129, 255), "HP FULLY RESTORED!");
    }

    static const uint32_t SLOT_COLORS[4] = {
        RGBA8_C(205, 125, 35, 255),
        RGBA8_C(240, 210, 70, 255),
        RGBA8_C(60, 215, 195, 255),
        RGBA8_C(170, 140, 215, 255)
    };
    static const uint32_t SLOT_FILLS[4] = {
        RGBA8_C(112, 68, 24, 255), RGBA8_C(125, 128, 138, 255), RGBA8_C(28, 92, 92, 255), RGBA8_C(92, 66, 128, 255)
    };
    for (int m = 0; m < g_game.player.move_count; m++) {
        const MoveState *ms = &g_game.player.moves[m];
        const MoveDef *md = &MOVE_DB[ms->id];
        float sx = 19.0f + (float)m * 46.0f;
        float sy = SCREEN_H - 51.0f;
        float sw = 40.0f, sh = 40.0f;

        float base_cd = ms->base_cd * g_game.player.cd_mult;
        float cd_pct = 1.0f - (ms->timer / (base_cd > 0.01f ? base_cd : 1.0f));
        if (cd_pct < 0.0f) cd_pct = 0.0f; if (cd_pct > 1.0f) cd_pct = 1.0f;
        bool ready = (ms->timer <= 0.02f);

        uint32_t border_col = (m < 4) ? SLOT_COLORS[m] : RGBA8_C(180, 190, 200, 255);
        ui_draw_card(sx, sy, sw, sh, (m < 4) ? SLOT_FILLS[m] : ui_darken(border_col, 0.5f), border_col, 3.0f, 8.0f);

        if (ready) {
            render_draw_type_badge(MOVE_TYPE_NAMES[md->type], sx + 7.5f, sy + 7.5f, 0.22f);
        } else {
            ui_draw_rounded_rect(sx + 5.0f, sy + 5.0f, sw - 10.0f, sh - 10.0f, 5.0f, RGBA8_C(0, 0, 0, 95));
            char cd_str[8];
            snprintf(cd_str, sizeof(cd_str), "%.1f", ms->timer);
            ui_text_center(0, sx, sy, sw, sh, 1.12f, RGBA8_C(255, 255, 255, 255), cd_str);
        }

        render_draw_rect(sx + 3.0f, sy + sh - 4.0f, sw - 6.0f, 2.0f, RGBA8_C(16, 24, 28, 255));
        render_draw_rect(sx + 3.0f, sy + sh - 4.0f, (sw - 6.0f) * cd_pct, 2.0f,
                         ready ? RGBA8_C(115, 255, 172, 255) : border_col);
    }

    if (g_game.evolution_banner_timer > 0.0f) {
        float bw = 640.0f, bh = 64.0f;
        float bx = SCREEN_W * 0.5f - bw * 0.5f;
        float by = 92.0f;
        float pulse = 0.5f + 0.5f * sinf(g_game.run_timer * 8.0f);
        uint32_t b_border = RGBA8_C(255, (int)(200.0f + 55.0f * pulse), 50, 255);
        render_draw_rect(bx, by, bw, bh, RGBA8_C(10, 18, 55, 248));
        render_draw_rect_outline(bx, by, bw, bh, b_border, 3.0f);

        char e_line1[96], e_line2[64];
        snprintf(e_line1, sizeof(e_line1), "★ CONGRATULATIONS! %s EVOLVED INTO %s! ★", g_game.evolution_old_name, g_game.evolution_new_name);
        snprintf(e_line2, sizeof(e_line2), "+60 MAX HP!   +35%% ATTACK DAMAGE!");
        render_draw_box_text_centered(bx, by + 8.0f, bw, 24.0f, 1.55f, RGBA8_C(255, 235, 80, 255), e_line1);
        render_draw_box_text_centered(bx, by + 34.0f, bw, 22.0f, 1.35f, RGBA8_C(100, 255, 140, 255), e_line2);
    }

    if (g_game.reaper_banner_timer > 0.0f) {
        float bw = 660.0f, bh = 66.0f;
        float bx = SCREEN_W * 0.5f - bw * 0.5f;
        float by = 92.0f;
        float pulse = 0.5f + 0.5f * sinf(g_game.run_timer * 10.0f);
        uint32_t b_border = RGBA8_C(255, (int)(180.0f + 75.0f * pulse), 40, 255);
        render_draw_rect(bx, by, bw, bh, RGBA8_C(28, 10, 38, 248));
        render_draw_rect_outline(bx, by, bw, bh, b_border, 3.0f);
        render_draw_box_text_centered(bx, by + 8.0f, bw, 24.0f, 1.55f, RGBA8_C(255, 225, 70, 255), "★ DIVINE JUDGMENT HAS ARRIVED - ARCEUS! ★");
        render_draw_box_text_centered(bx, by + 34.0f, bw, 22.0f, 1.25f, RGBA8_C(255, 110, 110, 255), "ALL WILD MOBS WIPED! SURVIVE & DEFEAT THE CREATOR!");
    }

    int dash_ids[4], dash_charges[4], dash_max_charges[4];
    float dash_timers[4], dash_cds[4];
    int dash_count = 0;
    for (int m = 0; m < g_game.player.move_count && dash_count < 4; m++) {
        const MoveDef *md = &MOVE_DB[g_game.player.moves[m].id];
        if (md->kind == MK_DASH) {
            dash_ids[dash_count] = g_game.player.moves[m].id;
            dash_timers[dash_count] = g_game.player.moves[m].timer;
            dash_cds[dash_count] = fmaxf(1.20f, g_game.player.moves[m].base_cd * g_game.player.cd_mult);
            dash_charges[dash_count] = g_game.player.moves[m].charges;
            dash_max_charges[dash_count] = g_game.player.moves[m].max_charges;
            dash_count++;
        }
    }
    static const char *DASH_BTN_LABELS[4] = { "[Tri]", "[Sqr]", "[O]", "[X]" };
    for (int s = 0; s < dash_count; s++) {
        const MoveDef *md = &MOVE_DB[dash_ids[s]];
        bool ready = (dash_charges[s] > 0);
        int et = (md->type >= 0 && md->type < 18) ? md->type : 0;
        uint32_t base = (et == 0) ? RGBA8_C(156, 156, 156, 255) : MOVE_TYPE_COLOR[et];
        uint32_t edge = ready ? base : RGBA8_C(140, 148, 160, 255);
        uint32_t fill = ready ? ((et == 0) ? RGBA8_C(242, 238, 232, 245) : ui_mix_white(base, 0.72f))
                              : RGBA8_C(220, 224, 232, 230);
        float bw = 68.0f, bh = 40.0f;
        float bx = SCREEN_W - 19.0f - (float)(s + 1) * bw - (float)s * 8.0f;
        float by = SCREEN_H - 51.0f;
        ui_draw_card(bx, by, bw, bh, fill, edge, 2.5f, 8.0f);

        const float icon_sc = 0.22f;
        const float icon_px = 110.0f * icon_sc;
        const float icon_y  = by + (bh - icon_px) * 0.5f;
        render_draw_type_badge(MOVE_TYPE_NAMES[et], bx + 7.0f, icon_y, icon_sc);

        if (ready) {
            const float bsc = 1.90f;
            ui_text_mid(0, bx + 7.0f + icon_px + 6.0f, by + bh * 0.5f, bsc, 0xFFFFFFFFu, DASH_BTN_LABELS[s]);
            if (dash_max_charges[s] > 1) {
                char chg_str[8];
                snprintf(chg_str, sizeof(chg_str), "x%d", dash_charges[s]);
                render_draw_text(bx + bw - 18.0f, by + 2.0f, 0.85f, RGBA8_C(20, 130, 60, 255), chg_str);
            }
        } else {
            char cd_str[12];
            snprintf(cd_str, sizeof(cd_str), "%.1f", dash_timers[s]);
            ui_text_center(0, bx + 7.0f + icon_px + 2.0f, by, bw - (7.0f + icon_px + 6.0f), bh - 2.0f, 1.05f, RGBA8_C(70, 82, 98, 255), cd_str);
            float prog = 1.0f - (dash_timers[s] / (dash_cds[s] > 0.01f ? dash_cds[s] : 1.0f));
            if (prog < 0.0f) prog = 0.0f; if (prog > 1.0f) prog = 1.0f;
            render_draw_rect(bx + 4.0f, by + bh - 4.0f, bw - 8.0f, 2.0f, RGBA8_C(180, 190, 204, 255));
            render_draw_rect(bx + 4.0f, by + bh - 4.0f, (bw - 8.0f) * prog, 2.0f, base);
        }
    }
}

static void render_draw_wrapped_box_text(float bx, float by, float bw, float scale, float line_h, int max_lines, uint32_t col, const char *text) {
    if (!text || !text[0]) return;
    char lines[6][96];
    int n_lines = 0;
    const char *p = text;
    while (*p && n_lines < max_lines) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *best_end = p;
        const char *scan = p;
        char cand[96];
        while (*scan) {
            const char *next_space = scan;
            while (*next_space && *next_space != ' ') next_space++;
            int seg_len = (int)(next_space - p);
            if (seg_len >= (int)sizeof(cand)) seg_len = (int)sizeof(cand) - 1;
            memcpy(cand, p, seg_len);
            cand[seg_len] = '\0';
            if ((float)render_get_text_width(scale, cand) <= bw) {
                best_end = next_space;
                scan = (*next_space == ' ') ? next_space + 1 : next_space;
            } else {
                if (best_end == p) {
                    int k = seg_len;
                    while (k > 1) {
                        cand[k] = '\0';
                        if ((float)render_get_text_width(scale, cand) <= bw) break;
                        k--;
                    }
                    best_end = p + k;
                }
                break;
            }
        }
        int copy_len = (int)(best_end - p);
        if (copy_len >= (int)sizeof(lines[0])) copy_len = (int)sizeof(lines[0]) - 1;
        memcpy(lines[n_lines], p, copy_len);
        lines[n_lines][copy_len] = '\0';
        n_lines++;
        p = best_end;
    }
    for (int i = 0; i < n_lines; i++) {
        ui_text_center(0, bx, by + (float)i * line_h, bw, line_h, scale, col, lines[i]);
    }
}

static void render_level_up(void) {
    render_playing_game();
    render_draw_rect(0.0f, 0.0f, SCREEN_W, SCREEN_H, UI_BG_FLAT);
    int owned_cnt = 0;
    for (int i = 0; i < TOTAL_CATALOG_POKEMON; i++) if (save_system_is_owned(i)) owned_cnt++;
    ui_draw_top_pills("LEVEL UP! CHOOSE AN UPGRADE", g_game.coins_collected, owned_cnt, TOTAL_CATALOG_POKEMON, g_game.tickets_collected);

    const float cw = 264.0f, ch = 326.0f, gap = 26.0f, sy = 96.0f;
    const float sx0 = (SCREEN_W - (3.0f * cw + 2.0f * gap)) * 0.5f;
    for (int i = 0; i < 3; i++) {
        UpgradeCard *card = &g_game.cards[i];
        float cx = sx0 + (float)i * (cw + gap);
        bool sel = (i == g_game.selected_card);
        uint32_t bg, edge, strip, well, stxt;
        char bt[32];
        bool is_move = (card->type == 0 || card->type == 2 || card->type == 3);
        if (card->type == 2)      snprintf(bt, sizeof(bt), "EVOLUTION!");
        else if (card->type == 3) snprintf(bt, sizeof(bt), "POWER-UP!");
        else if (card->cur_level > 0) snprintf(bt, sizeof(bt), "LEVEL %d", card->cur_level + 1);
        else                      snprintf(bt, sizeof(bt), "NEW MOVE / ITEM!");
        if (is_move && card->id >= 0 && card->id < MOVEDB_TOTAL) {
            int et = MOVE_DB[card->id].type; if (et < 0 || et > 17) et = 0;
            uint32_t base = MOVE_TYPE_COLOR[et];
            if (et == 0) {
                base = RGBA8_C(156, 156, 156, 255);
                strip = base; edge = RGBA8_C(176, 176, 176, 255);
                bg = RGBA8_C(242, 238, 232, 255); well = RGBA8_C(212, 204, 194, 255);
            } else {
                strip = base; edge = base;
                bg = ui_mix_white(base, 0.72f); well = ui_mix_white(base, 0.48f);
            }
            stxt = 0xFFFFFFFF;
        } else {
            bg = RGBA8_C(253,244,226,255); edge = RGBA8_C(226,166,104,255); strip = RGBA8_C(232,152,64,255); well = RGBA8_C(246,226,198,255); stxt = 0xFFFFFFFF;
        }
        (void)stxt;

        if (sel) {
            ui_draw_rounded_rect(cx - 5.0f + 2.0f, sy - 5.0f + 3.0f, cw + 10.0f, ch + 10.0f, 17.0f, UI_MODERN_SHADOW);
            ui_pix_box(cx - 6.0f, sy - 6.0f, cw + 12.0f, ch + 12.0f, UI_MODERN_WHITE, RGBA8_C(52, 52, 58, 255), 3.0f);
            ui_pix_box(cx, sy, cw, ch, bg, edge, 3.0f);
        } else {
            ui_draw_card(cx, sy, cw, ch, bg, edge, 3.0f, 14.0f);
        }

        ui_draw_badge(cx + 12.0f, sy + 12.0f, cw - 24.0f, 27.0f, strip, ui_darken(strip, 0.45f), bt, true);

        ui_text_center(0, cx + 10.0f, sy + 44.0f, cw - 20.0f, 34.0f, 1.60f, UI_MODERN_TEXT_DARK, card->title);

        float wx = cx + 12.0f, wy = sy + 80.0f, ww = cw - 24.0f, wh = 65.0f;
        ui_draw_rounded_rect(wx, wy, ww, wh, 10.0f, well);
        float icx = wx + ww * 0.5f, icy = wy + wh * 0.5f;
        if (card->type == 0 || card->type == 2 || card->type == 3) {
            const float t_sc = 0.48f;
            const float half = 55.0f * t_sc;
            render_draw_type_badge(MOVE_CATALOG[card->id].type, icx - half, icy - half, t_sc);
        } else {
            const float i_sc = 2.20f;
            const float half = 12.0f * i_sc;
            render_draw_item_icon(card->id, icx - half, icy - half, i_sc);
        }

        render_draw_wrapped_box_text(cx + 14.0f, sy + 156.0f, cw - 28.0f, 1.15f, 21.0f, 4, UI_MODERN_TEXT_DARK, card->desc);

        if (sel) {
            float bx = cx + 14.0f, by = sy + ch - 52.0f, bw = cw - 28.0f, bh = 39.0f;
            ui_bevel_btn(bx, by, bw, bh, UI_GREEN_FILL, UI_GREEN_HI, UI_GREEN_LIP);
            ui_label_outlined_icon(bx, by, bw, bh, 1.50f, "CONFIRM", "(X)", "");
        }
    }

    char r_buf[144];
    int reroll_cost = 60 + g_game.run_rerolls_bought * 40;
    if (g_game.rerolls_left > 0) {
        snprintf(r_buf, sizeof(r_buf), "[Left/Right] Select   |   [X] Confirm   |   [Sq] Reroll (Free: %d)   |   [Tri] Skip Cards", g_game.rerolls_left);
    } else {
        snprintf(r_buf, sizeof(r_buf), "[Left/Right] Select   |   [X] Confirm   |   [Sq] Reroll ($%d Run Coins)   |   [Tri] Skip Cards", reroll_cost);
    }
    ui_draw_footer_pill(r_buf);
}

static void render_chest_reward(void) {
    render_playing_game();
    render_draw_rect(0.0f, 0.0f, SCREEN_W, SCREEN_H, UI_MODERN_BG);

    int owned_cnt = 0;
    for (int i = 0; i < TOTAL_CATALOG_POKEMON; i++) {
        if (save_system_is_owned(i)) owned_cnt++;
    }
    ui_draw_top_pills("TREASURE CHEST DISCOVERY!", g_game.coins_collected, owned_cnt, TOTAL_CATALOG_POKEMON, g_game.tickets_collected);

    float cx = SCREEN_W * 0.5f;
    float cy = 100.0f;
    float bob = sinf(g_game.chest_timer * 6.0f) * 6.0f;
    render_draw_shadow(cx, cy + 24.0f, 32.0f);
    render_draw_ui_texture_centered(UI_TEX_BALL, cx, cy + bob, 1.3f);

    char c_str[64];
    snprintf(c_str, sizeof(c_str), "+%d POKÉCOINS FOUND!", g_game.chest_coins);
    render_draw_text_centered(cx, 144.0f, 1.55f, UI_MODERN_TEXT_DARK, c_str);

    int count = g_game.chest_reward_count;
    if (count < 1) count = 1;
    if (count > 5) count = 5;
    float cw = (count >= 5) ? 172.0f : 260.0f;
    float ch = 230.0f;
    float gap = (count >= 5) ? 12.0f : 24.0f;
    float total_w = (float)count * cw + (float)(count - 1) * gap;
    float start_x = (SCREEN_W - total_w) * 0.5f;
    float start_y = 180.0f;

    for (int r = 0; r < count; r++) {
        UpgradeCard *card = &g_game.chest_rewards[r];
        float rx = start_x + (float)r * (cw + gap);
        bool is_evo = (card->type == 2);

        ui_draw_card(rx, start_y, cw, ch, is_evo ? RGBA8_C(255, 248, 215, 255) : UI_MODERN_WHITE,
                     is_evo ? RGBA8_C(235, 175, 20, 255) : UI_MODERN_BORDER, 2.5f, 14.0f);

        float bnr_x = rx + 10.0f, bnr_y = start_y + 12.0f, bnr_w = cw - 20.0f, bnr_h = 24.0f;
        ui_draw_card(bnr_x, bnr_y, bnr_w, bnr_h, is_evo ? RGBA8_C(255, 195, 35, 255) : UI_BANNER_YELLOW, UI_MODERN_BORDER, 1.8f, 6.0f);
        ui_text_center(1, bnr_x, bnr_y, bnr_w, bnr_h, (count >= 5) ? 0.78f : 0.95f, UI_MODERN_TEXT_DARK,
                       is_evo ? "TIER-2 EVOLUTION!" : "POWER-UP UPGRADE!");

        float icon_cx = rx + cw * 0.5f;
        float icon_cy = start_y + 68.0f;
        ui_draw_card(icon_cx - 28.0f, icon_cy - 24.0f, 56.0f, 48.0f, UI_WELL_YELLOW, RGBA8_C(0, 0, 0, 20), 0.0f, 8.0f);
        if (card->type == 0 || card->type == 2) {
            const char *mtype = MOVE_CATALOG[card->id].type;
            render_draw_type_badge(mtype, icon_cx - 18.0f, icon_cy - 18.0f, 0.38f);
        } else {
            render_draw_item_icon(card->id % TOTAL_PASSIVES, icon_cx - 18.0f, icon_cy - 18.0f, 1.6f);
        }

        render_draw_text_centered(rx + cw * 0.5f, start_y + 100.0f, (count >= 5) ? 1.18f : 1.45f, UI_MODERN_TEXT_DARK, card->title);
        render_draw_wrapped_box_text(rx + 10.0f, start_y + 128.0f, cw - 20.0f, (count >= 5) ? 0.92f : 1.05f, 18.0f, 3, UI_MODERN_TEXT_DARK, card->desc);

        char lv_str[32];
        if (is_evo) snprintf(lv_str, sizeof(lv_str), "EVOLVED!");
        else if (card->type >= 4) snprintf(lv_str, sizeof(lv_str), "BONUS!");
        else snprintf(lv_str, sizeof(lv_str), "LEVEL %d", card->cur_level + 1);
        render_draw_text_8bit_centered(rx + cw * 0.5f, start_y + 192.0f, 1.05f, RGBA8_C(16, 185, 129, 255), lv_str);
    }

    ui_draw_footer_pill("[X] Claim Items & Resume Battle");
}

static void render_pause_menu(void) {
    render_playing_game();
    if (g_game.pause_sub_menu == 0) {

        render_hub_overview_overlay(g_game.pause_selected_idx, true);
    } else {
        render_draw_rect(0.0f, 0.0f, SCREEN_W, SCREEN_H, UI_MODERN_BG);
        float px = (SCREEN_W - 580.0f) * 0.5f, py = 54.0f, pw = 580.0f, ph = 420.0f;
        ui_draw_card(px, py, pw, ph, UI_MODERN_WHITE, UI_MODERN_BORDER, 2.0f, 14.0f);

        float bnr_x = px + 20.0f, bnr_y = py + 16.0f, bnr_w = pw - 40.0f, bnr_h = 32.0f;
        ui_draw_card(bnr_x, bnr_y, bnr_w, bnr_h, UI_BANNER_YELLOW, UI_MODERN_BORDER, 1.8f, 6.0f);
        ui_text_center(1, bnr_x, bnr_y, bnr_w, bnr_h, 1.10f, UI_MODERN_TEXT_DARK, "AUDIO & QUICK CONTROLS");

        int bgm_pct = (int)floorf(audio_get_bgm_volume() * 100.0f + 0.5f);
        int sfx_pct = (int)floorf(audio_get_sfx_volume() * 100.0f + 0.5f);
        for (int i = 0; i < 2; i++) {
            float ry = py + 60.0f + (float)i * 42.0f;
            bool is_sel = (g_game.settings_selected_idx == i);
            ui_gba_row(px + 20.0f, ry, pw - 40.0f, 36.0f, is_sel, i);
            const char *lbl = (i == 0) ? "Music (BGM)" : "Effects (SFX)";
            int pct = (i == 0) ? bgm_pct : sfx_pct;
            ui_text_mid(0, px + 50.0f, ry + 17.0f, 1.20f, UI_MODERN_TEXT_DARK, lbl);

            float bar_x = px + 235.0f, bar_y = ry + 9.0f, bar_w = 200.0f, bar_h = 16.0f;
            ui_draw_card(bar_x, bar_y, bar_w, bar_h, RGBA8_C(32, 40, 52, 255), UI_MODERN_BORDER, 1.5f, 5.0f);
            if (pct > 0) {
                render_draw_rect(bar_x + 3.0f, bar_y + 3.0f, (bar_w - 6.0f) * ((float)pct / 100.0f), bar_h - 6.0f,
                                 (i == 0) ? RGBA8_C(56, 189, 248, 255) : RGBA8_C(74, 222, 128, 255));
            }
            char vbuf[24];
            snprintf(vbuf, sizeof(vbuf), pct > 0 ? "%d%%" : "MUTE", pct);
            ui_text_mid(0, bar_x + bar_w + 12.0f, ry + 17.0f, 1.15f, pct > 0 ? UI_MODERN_TEXT_DARK : RGBA8_C(220, 38, 38, 255), vbuf);
        }

        render_draw_text(px + 34.0f, py + 152.0f, 1.28f, RGBA8_C(217, 119, 6, 255), "- Controls Reference:");
        render_draw_text(px + 44.0f, py + 182.0f, 1.22f, UI_MODERN_TEXT_DARK, "[Left/Right] [Up/Down] Left Stick / D-Pad: Move Character");
        render_draw_text(px + 44.0f, py + 212.0f, 1.22f, UI_MODERN_TEXT_DARK, "[Tri] [Sq] [O] [X]: Cast Active Dash Skills");
        render_draw_text(px + 44.0f, py + 242.0f, 1.22f, UI_MODERN_TEXT_DARK, "[L/R] / Right Stick: Rotate & Zoom 3D Camera");
        render_draw_text(px + 44.0f, py + 272.0f, 1.22f, UI_MODERN_TEXT_DARK, "START: Toggle Pause Menu");

        float bb_w = 340.0f, bb_h = 46.0f, bb_x = px + (pw - bb_w) * 0.5f, bb_y = py + 344.0f;
        bool back_sel = (g_game.settings_selected_idx == 2);
        ui_draw_card(bb_x, bb_y, bb_w, bb_h, back_sel ? UI_BTN_GREEN : UI_WELL_BLUE, UI_MODERN_BORDER, back_sel ? 2.5f : 1.8f, 10.0f);
        ui_text_center(1, bb_x, bb_y, bb_w, bb_h, 1.25f, UI_MODERN_TEXT_DARK, "BACK TO PAUSE MENU  [O]");
        ui_draw_footer_pill("[Up/Down] Select  |  [Left/Right] Volume  |  [X] Mute  |  [O] Back");
    }
}

static void render_game_over(void) {
    render_playing_game();
    render_draw_rect(0.0f, 0.0f, SCREEN_W, SCREEN_H, UI_MODERN_BG);

    int owned_cnt = 0;
    for (int i = 0; i < TOTAL_CATALOG_POKEMON; i++) if (save_system_is_owned(i)) owned_cnt++;
    ui_draw_top_pills("BATTLE RUN ENDED", g_save.coins, owned_cnt, TOTAL_CATALOG_POKEMON, g_save.tickets);

    float cx = SCREEN_W * 0.5f, cy = 268.0f;
    float cw = 580.0f, ch = 420.0f;
    ui_draw_card(cx - cw * 0.5f, cy - ch * 0.5f, cw, ch, UI_MODERN_WHITE, UI_MODERN_BORDER, 2.5f, 14.0f);

    float bnr_x = cx - cw * 0.5f + 18.0f, bnr_y = cy - ch * 0.5f + 14.0f, bnr_w = cw - 36.0f, bnr_h = 34.0f;
    ui_draw_card(bnr_x, bnr_y, bnr_w, bnr_h, RGBA8_C(253, 197, 189, 255), UI_MODERN_BORDER, 2.0f, 8.0f);
    ui_text_center(1, bnr_x, bnr_y, bnr_w, bnr_h, 1.25f, UI_MODERN_TEXT_DARK, "BATTLE RUN ENDED");

    int mins = (int)g_game.run_timer / 60;
    int secs = (int)g_game.run_timer % 60;
    char res[64];
    snprintf(res, sizeof(res), "Time Survived: %02d:%02d", mins, secs);
    render_draw_text_centered(cx, cy - 112.0f, 1.50f, UI_MODERN_TEXT_DARK, res);

    snprintf(res, sizeof(res), "POKéMON Defeated: %d", g_game.player.kills);
    render_draw_text_centered(cx, cy - 78.0f, 1.50f, UI_MODERN_TEXT_DARK, res);

    snprintf(res, sizeof(res), "Level Reached: Lv%d   |   Bosses: %d", g_game.player.level, g_game.bosses_defeated_in_run);
    render_draw_text_centered(cx, cy - 44.0f, 1.50f, UI_MODERN_TEXT_DARK, res);

    snprintf(res, sizeof(res), "Pokécoins Earned: +$%d", g_game.coins_collected);
    render_draw_text_centered(cx, cy - 8.0f, 1.70f, RGBA8_C(217, 119, 6, 255), res);

    float hx = cx - cw * 0.5f + 22.0f, hy = cy + 32.0f, hw = cw - 44.0f, hh = 64.0f;
    ui_draw_card(hx, hy, hw, hh, UI_WELL_CREAM, RGBA8_C(217, 119, 6, 255), 1.8f, 10.0f);
    render_draw_text_centered(cx, hy + 10.0f, 1.12f, RGBA8_C(195, 95, 10, 255), "TIP: Having trouble surviving? Visit the UPGRADES SHOP");
    render_draw_text_centered(cx, hy + 34.0f, 1.12f, RGBA8_C(16, 145, 95, 255), "in the Main Menu to upgrade HP, Damage, Armor & Revives!");

    float btn_w = 360.0f, btn_h = 46.0f;
    ui_draw_card(cx - btn_w * 0.5f, cy + 114.0f, btn_w, btn_h, UI_BTN_GREEN, UI_MODERN_BORDER, 2.5f, 10.0f);
    ui_text_center(1, cx - btn_w * 0.5f, cy + 114.0f, btn_w, btn_h, 1.25f, UI_MODERN_TEXT_DARK, "PRESS [X] TO RETURN TO HUB");
    ui_draw_footer_pill("[X] Return to Main Menu Hub");
}

static void render_victory(void) {
    render_playing_game();
    render_draw_rect(0.0f, 0.0f, SCREEN_W, SCREEN_H, UI_MODERN_BG);

    int owned_cnt = 0;
    for (int i = 0; i < TOTAL_CATALOG_POKEMON; i++) if (save_system_is_owned(i)) owned_cnt++;
    ui_draw_top_pills("VICTORY! STAGE CLEARED!", g_save.coins, owned_cnt, TOTAL_CATALOG_POKEMON, g_save.tickets);

    float cx = SCREEN_W * 0.5f, cy = 268.0f;
    float cw = 580.0f, ch = 390.0f;
    ui_draw_card(cx - cw * 0.5f, cy - ch * 0.5f, cw, ch, UI_MODERN_WHITE, UI_MODERN_BORDER, 2.5f, 14.0f);

    float bnr_x = cx - cw * 0.5f + 18.0f, bnr_y = cy - ch * 0.5f + 16.0f, bnr_w = cw - 36.0f, bnr_h = 34.0f;
    ui_draw_card(bnr_x, bnr_y, bnr_w, bnr_h, UI_BANNER_YELLOW, UI_MODERN_BORDER, 2.0f, 8.0f);
    ui_text_center(1, bnr_x, bnr_y, bnr_w, bnr_h, 1.25f, UI_MODERN_TEXT_DARK, "HOENN STAGE CLEARED! VICTORY!");

    int mins = (int)g_game.run_timer / 60;
    int secs = (int)g_game.run_timer % 60;
    char res[64];
    snprintf(res, sizeof(res), "Clear Time: %02d:%02d   |   Bosses Defeated: %d", mins, secs, g_game.bosses_defeated_in_run);
    render_draw_text_centered(cx, cy - 64.0f, 1.55f, UI_MODERN_TEXT_DARK, res);

    snprintf(res, sizeof(res), "Victory Bonus: +$500 Pokécoins!");
    render_draw_text_centered(cx, cy - 22.0f, 1.65f, RGBA8_C(16, 185, 129, 255), res);

    snprintf(res, sizeof(res), "Total Earned: +$%d", g_game.coins_collected + 500);
    render_draw_text_centered(cx, cy + 20.0f, 1.85f, RGBA8_C(217, 119, 6, 255), res);

    float btn_w = 360.0f, btn_h = 48.0f;
    ui_draw_card(cx - btn_w * 0.5f, cy + 96.0f, btn_w, btn_h, UI_BTN_GREEN, UI_MODERN_BORDER, 2.5f, 10.0f);
    ui_text_center(1, cx - btn_w * 0.5f, cy + 96.0f, btn_w, btn_h, 1.25f, UI_MODERN_TEXT_DARK, "PRESS [X] TO RETURN TO HUB");
    ui_draw_footer_pill("[X] Return to Main Menu Hub");
}

void game_render(void) {
    render_start_frame();

    switch (g_game.state) {
        case STATE_MENU_HUB:      render_menu_hub(); break;
        case STATE_MENU_MODES:    render_menu_modes(); break;
        case STATE_MENU_ROSTER:   render_menu_roster(); break;
        case STATE_MENU_SHOP:     render_menu_shop(); break;
        case STATE_MENU_GACHA:    render_menu_gacha(); break;
        case STATE_MENU_MISSIONS: render_menu_missions(); break;
        case STATE_MENU_SETTINGS: render_menu_settings(); break;
        case STATE_PLAYING:       render_playing_game(); break;
        case STATE_LEVEL_UP:      render_level_up(); break;
        case STATE_CHEST_REWARD:  render_chest_reward(); break;
        case STATE_PAUSED:        render_pause_menu(); break;
        case STATE_GAME_OVER:     render_game_over(); break;
        case STATE_VICTORY:       render_victory(); break;
    }

    render_end_frame();

    if (g_title_bgm_pending && g_game.state == STATE_MENU_HUB) {
        g_menu_visible_frames++;
        if (g_menu_visible_frames >= 2) {
            audio_play_bgm(BGM_TITLE);
            g_title_bgm_pending = false;
        }
    }
}
