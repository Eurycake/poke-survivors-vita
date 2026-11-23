#if defined(__vita__) || defined(_WIN32)
#ifdef __vita__
#include <vitasdk.h>
#include <psp2/io/stat.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#define strcasecmp _stricmp
#endif
#include "g3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "renderer.h"
#include "fx_draw.h"
#include "pokemon_meta.h"
#include "terrain.h"
#include "assets_data.h"
#include "game.h"
#include "font.h"

typedef g3d_tex vita2d_texture;
#define vita2d_load_PNG_file g3d_load_png
#define vita2d_free_texture  g3d_free_tex
static inline int vita2d_texture_get_width(const g3d_tex *t)  { return t->w; }
static inline int vita2d_texture_get_height(const g3d_tex *t) { return t->h; }

typedef struct { float pitch, yaw, zoom, fov, lean; } Cam3;
#define N_PRESETS 3
static const Cam3 CAM_PRESETS[N_PRESETS] = { {50,0,1.00f,40,0.46f}, {86,0,1.00f,30,1.00f}, {34,0,1.25f,45,0.36f} };
static const char *CAM_NAMES[N_PRESETS] = { "OCTOPATH", "CLASSIC", "LOW" };
static Cam3 g_cam = {50,0,1.00f,40,0.46f};
static int g_cam_preset = 0;
static float g_cam_msg = 0.0f, g_cam_dirty = 0.0f;
static uint32_t g_clear_col = 0xFF1C281E;

static bool g_world = false;
static float g_wcx = 0.0f, g_wcy = 0.0f;
static float g_sh_x = 0.0f, g_sh_y = 0.0f;
static bool g_sh_valid = false;
static const float AX_X[3] = {1, 0, 0}, AX_Z[3] = {0, 0, 1};

#define GROUND_Y 0.0f
static float wld_lift_fn(float x, float z);
#define FLAT_Y   0.6f

static void bb_basis(float R[3], float D[3]) {
    float up[3], nor[3];
    g3d_axes(R, up, nor);
    float eff_lean = (g_cam_preset == 1) ? g_cam.lean : (g_cam.lean * 0.66f);
    if (eff_lean > 0.48f && g_cam_preset != 1) eff_lean = 0.48f;
    float beta = eff_lean * g_cam.pitch * 0.01745329f;
    float cb = cosf(beta), sb = sinf(beta);
    D[0] = -nor[0] * sb; D[1] = -cb; D[2] = -nor[2] * sb;
}

static void vita2d_draw_rectangle(float x, float y, float w, float h, uint32_t c) {
    if (g_world) {
        g3d_layer(G3D_DECAL);
        float cc[3] = { x + g_wcx, FLAT_Y, y + g_wcy };
        g3d_plane(g3d_white(), cc, AX_X, AX_Z, 0, 0, w, h, 0, 0, 0, 1, 1, c);
    } else g3d_ui_rect(x, y, w, h, c);
}
static void vita2d_draw_fill_circle(float x, float y, float r, uint32_t c) {
    if (g_world) {
        g3d_layer(G3D_DECAL);
        float cc[3] = { x + g_wcx, FLAT_Y, y + g_wcy };
        g3d_disc(cc, AX_X, AX_Z, r, c);
    } else g3d_ui_circle(x, y, r, c);
}
static void draw_line_t(float x1, float y1, float x2, float y2, uint32_t c, float th) {
    if (th < 1.2f) th = 1.2f;
    if (g_world) {
        float dx = x2 - x1, dz = y2 - y1, l = sqrtf(dx * dx + dz * dz);
        if (l < 0.001f) return;
        g3d_layer(G3D_DECAL);
        float p1[3] = { x1 + g_wcx, FLAT_Y + 0.1f, y1 + g_wcy }, p2[3] = { x2 + g_wcx, FLAT_Y + 0.1f, y2 + g_wcy };
        float pp[3] = { -dz / l, 0, dx / l };
        g3d_ribbon(p1, p2, pp, th, c);
    } else g3d_ui_line(x1, y1, x2, y2, th, c);
}
static void vita2d_draw_texture_scale(const g3d_tex *t, float x, float y, float sx, float sy) {
    g3d_ui_tex(t, x, y, 0, 0, (float)t->w, (float)t->h, sx, sy, 0xFFFFFFFFu);
}
static void vita2d_draw_texture_part_scale(const g3d_tex *t, float x, float y, float u, float v, float w, float h, float sx, float sy) {
    g3d_ui_tex(t, x, y, u, v, w, h, sx, sy, 0xFFFFFFFFu);
}
static void vita2d_draw_texture_tint_part_scale(const g3d_tex *t, float x, float y, float u, float v, float w, float h, float sx, float sy, uint32_t c) {
    g3d_ui_tex(t, x, y, u, v, w, h, sx, sy, c);
}

static char g_data_root[256] = "";
static bool g_data_found = false;

#ifdef _WIN32
#define POKE_CACHE_SIZE 1025
#else
#define POKE_CACHE_SIZE 160
#endif
typedef struct {
    int dex;
    vita2d_texture *sheet;
    uint32_t last_used_frame;
    int fw, fh;
    int num_frames;
    int ax, ay;
} PokeCacheEntryVita;

static PokeCacheEntryVita g_poke_cache_vita[POKE_CACHE_SIZE];
static uint32_t g_render_frame_count_vita = 0;

static vita2d_texture *g_tile_grass[4];
static vita2d_texture *g_tile_dirt[3];
static vita2d_texture *g_tile_flower[3];

static vita2d_texture *g_prop_tree_a = NULL;
static vita2d_texture *g_prop_tree_b = NULL;
static vita2d_texture *g_prop_tree_autumn = NULL;
static vita2d_texture *g_prop_tree_dark = NULL;
static vita2d_texture *g_prop_rock_a = NULL;
static vita2d_texture *g_prop_rock_b = NULL;
static vita2d_texture *g_prop_cactus_a = NULL;
static vita2d_texture *g_prop_cactus_b = NULL;
static vita2d_texture *g_prop_cactus[6] = {0};
static vita2d_texture *g_prop_tuft_a = NULL;
static vita2d_texture *g_prop_tuft_b = NULL;
static vita2d_texture *g_prop_flower_pink = NULL;
static vita2d_texture *g_prop_flower_yellow = NULL;
static vita2d_texture *g_prop_pebble = NULL;
static vita2d_texture *g_prop_chest_closed = NULL;
static vita2d_texture *g_prop_chest_open = NULL;
static int g_current_stage = 0;
static vita2d_texture *g_prop_pool[4];
static vita2d_texture *g_tex_bush = NULL;
static vita2d_texture *g_tex_ground_grass = NULL;
static vita2d_texture *g_tex_ground_park = NULL;
static vita2d_texture *g_tex_ground_forest = NULL;
static vita2d_texture *g_tex_ground_path = NULL;
static vita2d_texture *g_tex_ground_sand = NULL;
static vita2d_texture *g_tex_ground_water = NULL;
static vita2d_texture *g_tex_emerald_tiles = NULL;
static vita2d_texture *g_tex_items[8] = {NULL};
static vita2d_texture *g_tex_berry_tree = NULL;
static vita2d_texture *g_tex_stage_previews[12] = {NULL};
static vita2d_texture *g_tex_ball = NULL;
static vita2d_texture *g_tex_logo = NULL;
static vita2d_texture *g_tex_xp_gem_1 = NULL;
static vita2d_texture *g_tex_xp_gem_2 = NULL;
static vita2d_texture *g_tex_xp_big = NULL;
static vita2d_texture *g_tex_xp_mega = NULL;
static vita2d_texture *g_tex_rare_candy = NULL;
static vita2d_texture *g_tex_gacha_ticket = NULL;
static vita2d_texture *g_tex_revive = NULL;
static vita2d_texture *g_tex_pedestal = NULL;
static vita2d_texture *g_tex_gacha_ball = NULL;
static vita2d_texture *g_tex_coin = NULL;
static vita2d_texture *g_tex_potion = NULL;
static vita2d_texture *g_tex_bomb = NULL;
static vita2d_texture *g_tex_magnet = NULL;
static vita2d_texture *g_tex_pouch = NULL;
static vita2d_texture *g_font_atlas = NULL;
static vita2d_texture *g_ui_icons = NULL;
static vita2d_texture *g_tex_rounded = NULL;

static const char *TYPE_NAMES[18] = {
    "normal", "fire", "water", "grass", "electric", "ice",
    "fighting", "poison", "ground", "flying", "psychic", "bug",
    "rock", "ghost", "dragon", "dark", "steel", "fairy"
};
static vita2d_texture *g_tex_types[18];

static vita2d_texture *g_fx_fire[3];
static vita2d_texture *g_fx_drop = NULL;
static vita2d_texture *g_fx_bubble[2];
static vita2d_texture *g_fx_leaf = NULL;
static vita2d_texture *g_fx_bolt[2];
static vita2d_texture *g_fx_shard = NULL;
static vita2d_texture *g_fx_rock[3];
static vita2d_texture *g_fx_star[2];
static vita2d_texture *g_fx_streak = NULL;
static vita2d_texture *g_fx_wind[2];
static vita2d_texture *g_fx_ring[2];
static vita2d_texture *g_fx_crescent = NULL;
static vita2d_texture *g_fx_impact[2];
static vita2d_texture *g_fx_ball[2];
static vita2d_texture *g_fx_shadow[3];
static vita2d_texture *g_fx_fist = NULL;
static vita2d_texture *g_fx_vine = NULL;
static vita2d_texture *g_fx_solar = NULL;
static vita2d_texture *g_tex_egg_bomb = NULL;
static vita2d_texture *g_fx_spin[4];
static vita2d_texture *g_fx_hydro = NULL;
static vita2d_texture *g_fx_shock[3];
static vita2d_texture *g_fx_strike[3];
static vita2d_texture *g_fx_slam[2];
static vita2d_texture *g_fx_ghost = NULL;
static vita2d_texture *g_fx_psybeam = NULL;
static vita2d_texture *g_fx_psystrike = NULL;
static vita2d_texture *g_fx_palm = NULL;
static vita2d_texture *g_fx_note_normal = NULL;
static vita2d_texture *g_fx_cannon_steel[2];
static vita2d_texture *g_fx_cannon_electric[2];
static vita2d_texture *g_fx_cannon_fairy[2];
static vita2d_texture *g_fx_cannon_normal[2];
static vita2d_texture *g_fx_pincer_bug[2];
static vita2d_texture *g_fx_pincer_water[2];
static vita2d_texture *g_fx_claw_steel = NULL;
static vita2d_texture *g_fx_jaws_dark[2];
static vita2d_texture *g_fx_slashx_normal[2];
static vita2d_texture *g_fx_slashx_fairy[2];
static vita2d_texture *g_fx_slashx_fire[2];
static vita2d_texture *g_fx_slashx_fighting[2];
static vita2d_texture *g_fx_slashx_steel[2];
static vita2d_texture *g_fx_slashx_poison[2];
static vita2d_texture *g_fx_kick_fighting[2];
static vita2d_texture *g_fx_hammer_ice = NULL;
static vita2d_texture *g_fx_feather_flying = NULL;
static vita2d_texture *g_fx_xmark_poison = NULL;

static bool file_exists_on_disk(const char *path) {
#ifdef __vita__
    SceIoStat stat;
    return sceIoGetstat(path, &stat) >= 0;
#else
    DWORD dwAttrib = GetFileAttributesA(path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES);
#endif
}

static void r_log(const char *msg) {
#ifdef __vita__
    FILE *f = fopen("ux0:data/poke-survivors vita/boot_log.txt", "a");
    if (!f) f = fopen("ux0:data/boot_log.txt", "a");
#else
    FILE *f = fopen("boot_log.txt", "a");
#endif
    if (f) {
        fputs(msg, f);
        fputs("\n", f);
        fclose(f);
    }
#ifdef _WIN32
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
#else
    printf("%s\n", msg);
#endif
}

static void detect_data_root(void) {
#ifdef _WIN32
    char exe_dir[512] = {0};
    if (GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir)) > 0) {
        char *s = strrchr(exe_dir, '\\');
        if (!s) s = strrchr(exe_dir, '/');
        if (s) {
            *s = '\0';
            SetCurrentDirectoryA(exe_dir);
        }
    }
#endif
    static const char *candidate_roots[] = {
        "assets",
        "./assets",
        "vita-port/assets",
        "./vita-port/assets",
        "data",
        "./data",
        "data/assets",
        "PokemonSurvivors/data",
        "../assets",
        "../data",
        "../vita-port/assets",
        "../../assets",
        "poke-survivors vita",
        "ux0:data/poke-survivors vita",
        "ux0:data/poke-survivors vita/assets",
        "ux0:data/poke-survivors-vita",
        "PokemonSurvivors",
        "ux0:data/PokemonSurvivors",
        "ux0:data/PokemonSurvivors/PokemonSurvivors",
        "ux0:data/PokemonSurvivors_Data",
        "ux0:data/PokemonSurvivors_Data/PokemonSurvivors",
        "ux0:data/PokemonSurvivors/assets",
        "ux0:data/poke-survivor",
        "ux0:data/poke-survivor/assets",
        "app0:assets",
        "app0"
    };
    char test_path[512];

    for (size_t i = 0; i < sizeof(candidate_roots)/sizeof(candidate_roots[0]); i++) {

        snprintf(test_path, sizeof(test_path), "%s/ui/font_atlas.png", candidate_roots[i]);
        if (file_exists_on_disk(test_path)) {
            strncpy(g_data_root, candidate_roots[i], sizeof(g_data_root) - 1);
            g_data_found = true;
            char buf[256];
            snprintf(buf, sizeof(buf), "[RENDER] Found data root: %s", g_data_root);
            r_log(buf);
            return;
        }
        snprintf(test_path, sizeof(test_path), "%s/mon/1.png", candidate_roots[i]);
        if (file_exists_on_disk(test_path)) {
            strncpy(g_data_root, candidate_roots[i], sizeof(g_data_root) - 1);
            g_data_found = true;
            char buf[256];
            snprintf(buf, sizeof(buf), "[RENDER] Found data root: %s", g_data_root);
            r_log(buf);
            return;
        }
        snprintf(test_path, sizeof(test_path), "%s/mon/25.png", candidate_roots[i]);
        if (file_exists_on_disk(test_path)) {
            strncpy(g_data_root, candidate_roots[i], sizeof(g_data_root) - 1);
            g_data_found = true;
            char buf[256];
            snprintf(buf, sizeof(buf), "[RENDER] Found data root: %s", g_data_root);
            r_log(buf);
            return;
        }
    }
    g_data_found = false;
    g_data_root[0] = '\0';
    r_log("[RENDER] Warning: external data root not detected!");
}

static void cam_load(void) {
#ifdef __vita__
    FILE *f = fopen("ux0:data/poke-survivors vita/camera.cfg", "r");
    if (!f) f = fopen("ux0:data/poke-survivor/camera.cfg", "r");
#else
    FILE *f = fopen("camera.cfg", "r");
#endif
    if (!f) return;
    Cam3 c; int p = 0;
    if (fscanf(f, "%f %f %f %f %f %d", &c.pitch, &c.yaw, &c.zoom, &c.fov, &c.lean, &p) == 6) {
        if (c.pitch >= 28 && c.pitch <= 88 && c.zoom >= 0.5f && c.zoom <= 2.0f && c.fov >= 25 && c.fov <= 60) {
            if (c.zoom < 0.85f) c.zoom = 0.85f;
            if (c.zoom > 1.80f) c.zoom = 1.80f;
            g_cam = c; g_cam_preset = (p >= 0 && p < N_PRESETS) ? p : 0;
        }
    }
    fclose(f);
}
static void cam_save(void) {
#ifdef __vita__
    sceIoMkdir("ux0:data/poke-survivors vita", 0777);
    FILE *f = fopen("ux0:data/poke-survivors vita/camera.cfg", "w");
#else
    FILE *f = fopen("camera.cfg", "w");
#endif
    if (!f) return;
    fprintf(f, "%.2f %.2f %.3f %.2f %.3f %d\n", g_cam.pitch, g_cam.yaw, g_cam.zoom, g_cam.fov, g_cam.lean, g_cam_preset);
    fclose(f);
}

int render_init(void) {
    static bool gl_up = false;
    r_log("[RENDER] Initializing vitaGL (3D)...");
    if (!gl_up) { g3d_init(); g3d_set_lift_fn(wld_lift_fn); gl_up = true; cam_load(); }
    r_log("[RENDER] Detecting data root...");
    detect_data_root();
    r_log("[RENDER] Loading textures...");
    render_load_all_textures();
    r_log("[RENDER] Textures loaded OK");
    return 0;
}

static void free_tex(vita2d_texture **t) {
    if (t && *t) {
        vita2d_free_texture(*t);
        *t = NULL;
    }
}

void render_fini(void) {
    for (int i = 0; i < POKE_CACHE_SIZE; i++) {
        if (g_poke_cache_vita[i].sheet) {
            vita2d_free_texture(g_poke_cache_vita[i].sheet);
            g_poke_cache_vita[i].sheet = NULL;
        }
    }
    free_tex(&g_tex_ground_sand);
    free_tex(&g_tex_ground_water);
    free_tex(&g_tex_emerald_tiles);
    for (int i = 0; i < 4; i++) free_tex(&g_tile_grass[i]);
    for (int i = 0; i < 3; i++) free_tex(&g_tile_dirt[i]);
    for (int i = 0; i < 3; i++) free_tex(&g_tile_flower[i]);

    free_tex(&g_prop_tree_a);
    free_tex(&g_prop_tree_b);
    free_tex(&g_prop_tree_autumn);
    free_tex(&g_prop_tree_dark);
    free_tex(&g_prop_rock_a);
    free_tex(&g_prop_rock_b);
    free_tex(&g_prop_cactus_a);
    free_tex(&g_prop_cactus_b);
    for (int i = 0; i < 6; i++) free_tex(&g_prop_cactus[i]);
    free_tex(&g_prop_tuft_a);
    free_tex(&g_prop_tuft_b);
    free_tex(&g_prop_flower_pink);
    free_tex(&g_prop_flower_yellow);
    free_tex(&g_prop_pebble);
    free_tex(&g_prop_chest_closed);
    free_tex(&g_prop_chest_open);
    for (int i = 0; i < 4; i++) free_tex(&g_prop_pool[i]);
    free_tex(&g_tex_bush);
    free_tex(&g_tex_ground_grass);
    free_tex(&g_tex_ground_park);
    free_tex(&g_tex_ground_forest);
    free_tex(&g_tex_ground_path);
    for (int i = 0; i < 8; i++) free_tex(&g_tex_items[i]);
    free_tex(&g_tex_berry_tree);
    for (int i = 0; i < 8; i++) free_tex(&g_tex_stage_previews[i]);
    free_tex(&g_tex_ball);
    free_tex(&g_tex_logo);
    free_tex(&g_tex_xp_gem_1);
    free_tex(&g_tex_xp_gem_2);
    free_tex(&g_tex_xp_big);
    free_tex(&g_tex_xp_mega);
    free_tex(&g_tex_rare_candy);
    free_tex(&g_tex_gacha_ticket);
    free_tex(&g_tex_revive);
    free_tex(&g_tex_pedestal);
    free_tex(&g_tex_gacha_ball);
    free_tex(&g_tex_coin);
    free_tex(&g_tex_potion);
    free_tex(&g_tex_bomb);
    free_tex(&g_tex_magnet);
    free_tex(&g_tex_pouch);
    free_tex(&g_font_atlas);
    free_tex(&g_ui_icons);
    free_tex(&g_tex_rounded);

    for (int i = 0; i < 18; i++) free_tex(&g_tex_types[i]);

    for (int i = 0; i < 3; i++) free_tex(&g_fx_fire[i]);
    free_tex(&g_fx_drop);
    for (int i = 0; i < 2; i++) free_tex(&g_fx_bubble[i]);
    free_tex(&g_fx_leaf);
    for (int i = 0; i < 2; i++) free_tex(&g_fx_bolt[i]);
    free_tex(&g_fx_shard);
    for (int i = 0; i < 3; i++) free_tex(&g_fx_rock[i]);
    for (int i = 0; i < 2; i++) free_tex(&g_fx_star[i]);
    free_tex(&g_fx_streak);
    for (int i = 0; i < 2; i++) free_tex(&g_fx_wind[i]);
    for (int i = 0; i < 2; i++) free_tex(&g_fx_ring[i]);
    free_tex(&g_fx_crescent);
    for (int i = 0; i < 2; i++) free_tex(&g_fx_impact[i]);
    for (int i = 0; i < 2; i++) free_tex(&g_fx_ball[i]);
    for (int i = 0; i < 3; i++) free_tex(&g_fx_shadow[i]);
    free_tex(&g_fx_fist);
    free_tex(&g_fx_vine);
    free_tex(&g_fx_solar);
    free_tex(&g_tex_egg_bomb);
    for (int i = 0; i < 4; i++) free_tex(&g_fx_spin[i]);
    free_tex(&g_fx_hydro);
    for (int i = 0; i < 3; i++) free_tex(&g_fx_shock[i]);
    for (int i = 0; i < 3; i++) free_tex(&g_fx_strike[i]);
    for (int i = 0; i < 2; i++) free_tex(&g_fx_slam[i]);
    free_tex(&g_fx_ghost);
    free_tex(&g_fx_psybeam);
    free_tex(&g_fx_psystrike);
    free_tex(&g_fx_palm); free_tex(&g_fx_note_normal); free_tex(&g_fx_claw_steel);
    for (int i = 0; i < 2; i++) {
        free_tex(&g_fx_cannon_steel[i]); free_tex(&g_fx_cannon_electric[i]);
        free_tex(&g_fx_cannon_fairy[i]); free_tex(&g_fx_cannon_normal[i]);
        free_tex(&g_fx_pincer_bug[i]); free_tex(&g_fx_pincer_water[i]); free_tex(&g_fx_jaws_dark[i]);
        free_tex(&g_fx_slashx_normal[i]); free_tex(&g_fx_slashx_fairy[i]);
        free_tex(&g_fx_slashx_fire[i]); free_tex(&g_fx_slashx_fighting[i]); free_tex(&g_fx_slashx_steel[i]);
        free_tex(&g_fx_slashx_poison[i]);
        free_tex(&g_fx_kick_fighting[i]);
    }

    free_tex(&g_fx_hammer_ice); free_tex(&g_fx_feather_flying); free_tex(&g_fx_xmark_poison);
}

static bool resolve_asset_path(const char *rel_path, char *out_path, size_t out_sz) {

    if (g_data_found && g_data_root[0] != '\0') {
        snprintf(out_path, out_sz, "%s/%s", g_data_root, rel_path);
        return file_exists_on_disk(out_path);
    }

    static const char *fallback_roots[] = {
        "assets",
        "./assets",
        "data",
        "./data",
        "data/assets",
        "PokemonSurvivors/data",
        "vita-port/assets",
        "./vita-port/assets",
        "../assets",
        "../data",
        "../vita-port/assets",
        "../../assets",
        "poke-survivors vita",
        "ux0:data/poke-survivors vita",
        "ux0:data/poke-survivors vita/assets",
        "ux0:data/poke-survivors-vita",
        "PokemonSurvivors",
        "ux0:data/PokemonSurvivors",
        "ux0:data/PokemonSurvivors/PokemonSurvivors",
        "ux0:data/PokemonSurvivors_Data",
        "ux0:data/PokemonSurvivors_Data/PokemonSurvivors",
        "ux0:data/PokemonSurvivors/assets",
        "ux0:data/poke-survivor",
        "ux0:data/poke-survivor/assets",
        "app0:assets",
        "app0"
    };

    for (size_t i = 0; i < sizeof(fallback_roots) / sizeof(fallback_roots[0]); i++) {
        snprintf(out_path, out_sz, "%s/%s", fallback_roots[i], rel_path);
        if (file_exists_on_disk(out_path)) return true;
    }
    return false;
}

static vita2d_texture *load_asset_png(const char *rel_path) {
    char path[512];
    if (resolve_asset_path(rel_path, path, sizeof(path))) {
        return vita2d_load_PNG_file(path);
    }

    return NULL;
}

static vita2d_texture *load_asset_padded_atlas(const char *rel_path, int tile_px, int pad_px) {
    char path[512];
    if (resolve_asset_path(rel_path, path, sizeof(path))) {
        return g3d_load_padded_atlas(path, tile_px, pad_px);
    }
    return NULL;
}

void render_load_all_textures(void) {
    char path[256];
    memset(g_poke_cache_vita, 0, sizeof(g_poke_cache_vita));
    g_render_frame_count_vita = 0;

    for (int i = 0; i < 4; i++) {
        snprintf(path, sizeof(path), "world/tile_grass_%d.png", i);
        g_tile_grass[i] = load_asset_png(path);
    }
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "world/tile_dirt_%d.png", i);
        g_tile_dirt[i] = load_asset_png(path);
    }
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "world/tile_flower_%d.png", i);
        g_tile_flower[i] = load_asset_png(path);
    }

    g_prop_tree_a = load_asset_png("world/tree_a.png");
    g_prop_tree_b = load_asset_png("world/tree_b.png");
    g_prop_tree_autumn = load_asset_png("world/tree_autumn.png");
    g_prop_tree_dark = load_asset_png("world/tree_dark.png");
    if (g_prop_tree_a) g3d_set_nearest(g_prop_tree_a);
    if (g_prop_tree_b) g3d_set_nearest(g_prop_tree_b);
    if (g_prop_tree_autumn) g3d_set_nearest(g_prop_tree_autumn);
    if (g_prop_tree_dark) g3d_set_nearest(g_prop_tree_dark);
    g_prop_rock_a = load_asset_png("world/rock_a.png");
    g_prop_rock_b = load_asset_png("world/rock_desert.png");
    g_prop_cactus_a = load_asset_png("world/cactus_a.png");
    g_prop_cactus_b = load_asset_png("world/cactus_b.png");
    for (int i = 0; i < 6; i++) {
        snprintf(path, sizeof(path), "world/cactus_%d.png", i);
        g_prop_cactus[i] = load_asset_png(path);
        if (g_prop_cactus[i]) g3d_set_nearest(g_prop_cactus[i]);
    }
    g_prop_tuft_a = load_asset_png("world/tuft_a.png");
    g_prop_tuft_b = load_asset_png("world/tuft_b.png");
    g_prop_flower_pink = load_asset_png("world/flower_pink.png");
    g_prop_flower_yellow = load_asset_png("world/flower_yellow.png");
    g_prop_pebble = load_asset_png("world/pebble.png");
    g_prop_chest_closed = load_asset_png("world/chest_closed.png");
    g_prop_chest_open = load_asset_png("world/chest_open.png");
    for (int i = 0; i < 4; i++) {
        snprintf(path, sizeof(path), "world/pool_water_%d.png", i);
        g_prop_pool[i] = load_asset_png(path);
    }
    g_tex_bush = load_asset_png("world/bush.png");
    g_tex_ground_grass = load_asset_png("world/ground_grass.png");
    g_tex_ground_park = load_asset_png("world/ground_park.png");
    g_tex_ground_forest = load_asset_png("world/ground_forest.png");
    g_tex_ground_path = load_asset_png("world/ground_path.png");
    g_tex_ground_sand = load_asset_png("world/ground_sand.png");
    g_tex_ground_water = load_asset_png("world/ground_water.png");
    g_tex_emerald_tiles = load_asset_padded_atlas("world/emerald_tiles.png", 16, 2);
    for (int i = 0; i < 8; i++) {
        char ipath[256];
        snprintf(ipath, sizeof(ipath), "ui/items/passive_%d.png", i);
        g_tex_items[i] = load_asset_png(ipath);
    }
    g_tex_berry_tree = load_asset_png("world/berry_tree.png");

    static const int STAGE_PREVIEW_FILE_IDX[7] = { 0, 1, 2, 5, 6, 7, 8 };
    for (int i = 0; i < TOTAL_STAGES && i < 7; i++) {
        int fi = STAGE_PREVIEW_FILE_IDX[i];
        char pp[96];
        snprintf(pp, sizeof(pp), "previews/stage_preview_%d.png", fi);
        g_tex_stage_previews[i] = load_asset_png(pp);
        if (!g_tex_stage_previews[i]) {
            snprintf(pp, sizeof(pp), "world/stage_preview_%d.png", fi);
            g_tex_stage_previews[i] = load_asset_png(pp);
        }
    }
    g_tex_egg_bomb = load_asset_png("fx/egg.png");

    g_tex_ball = load_asset_png("world/ball.png");
    g_tex_logo = load_asset_png("ui/logo.png");
    g_tex_xp_gem_1 = load_asset_png("world/xp_gem_1.png");
    g_tex_xp_gem_2 = load_asset_png("world/xp_gem_2.png");
    g_tex_xp_big = load_asset_png("world/xp_big.png");
    g_tex_xp_mega = load_asset_png("world/xp_mega.png");
    g_tex_rare_candy = load_asset_png("world/rare_candy.png");
    g_tex_gacha_ticket = load_asset_png("world/gacha_ticket.png");
    g_tex_revive = load_asset_png("world/revive_potion.png");
    g_tex_pedestal = load_asset_png("ui/grass_pedestal.png");
    g_tex_gacha_ball = load_asset_png("ui/gacha_ball.png");
    g_tex_coin = load_asset_png("world/meteor/coins.png");
    g_tex_potion = load_asset_png("world/poi_potion.png");
    g_tex_bomb = load_asset_png("world/trap_bomb.png");
    g_tex_magnet = load_asset_png("world/poi_xp.png");
    g_tex_pouch = load_asset_png("world/poi_pouch.png");
    if (g_tex_ball) g3d_set_nearest(g_tex_ball);
    if (g_tex_xp_gem_1) g3d_set_nearest(g_tex_xp_gem_1);
    if (g_tex_xp_gem_2) g3d_set_nearest(g_tex_xp_gem_2);
    if (g_tex_xp_big) g3d_set_nearest(g_tex_xp_big);
    if (g_tex_xp_mega) g3d_set_nearest(g_tex_xp_mega);
    if (g_tex_rare_candy) g3d_set_nearest(g_tex_rare_candy);
    if (g_tex_gacha_ticket) g3d_set_nearest(g_tex_gacha_ticket);
    if (g_tex_revive) g3d_set_nearest(g_tex_revive);
    if (g_tex_pedestal) g3d_set_nearest(g_tex_pedestal);
    if (g_tex_gacha_ball) g3d_set_nearest(g_tex_gacha_ball);
    if (g_tex_coin) g3d_set_nearest(g_tex_coin);
    if (g_tex_potion) g3d_set_nearest(g_tex_potion);
    if (g_tex_magnet) g3d_set_nearest(g_tex_magnet);
    if (g_tex_pouch) g3d_set_nearest(g_tex_pouch);
    g_font_atlas = load_asset_png("ui/font_atlas.png");
    g_ui_icons = load_asset_png("ui/ui_icons.png");
    g_tex_rounded = load_asset_png("ui/ui_rounded.png");

    for (int i = 0; i < 18; i++) {
        snprintf(path, sizeof(path), "ui/types/%s.png", TYPE_NAMES[i]);
        g_tex_types[i] = load_asset_png(path);
    }

    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "fx/fire_%d.png", i);
        g_fx_fire[i] = load_asset_png(path);
    }
    g_fx_drop = load_asset_png("fx/drop.png");
    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof(path), "fx/bubble_%d.png", i);
        g_fx_bubble[i] = load_asset_png(path);
    }
    g_fx_leaf = load_asset_png("fx/leaf.png");
    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof(path), "fx/bolt_%d.png", i);
        g_fx_bolt[i] = load_asset_png(path);
    }
    g_fx_shard = load_asset_png("fx/shard.png");
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "fx/rock_%d.png", i);
        g_fx_rock[i] = load_asset_png(path);
    }
    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof(path), "fx/star_%d.png", i);
        g_fx_star[i] = load_asset_png(path);
    }
    g_fx_streak = load_asset_png("fx/streak.png");
    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof(path), "fx/wind_%d.png", i);
        g_fx_wind[i] = load_asset_png(path);
    }
    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof(path), "fx/ring_%d.png", i);
        g_fx_ring[i] = load_asset_png(path);
    }
    g_fx_crescent = load_asset_png("fx/crescent.png");
    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof(path), "fx/impact_%d.png", i);
        g_fx_impact[i] = load_asset_png(path);
    }
    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof(path), "fx/ball_%d.png", i);
        g_fx_ball[i] = load_asset_png(path);
    }
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "fx/shadow_%d.png", i);
        g_fx_shadow[i] = load_asset_png(path);
    }
    g_fx_fist = load_asset_png("fx/fist.png");
    g_fx_vine = load_asset_png("fx/vine.png");
    g_fx_solar = load_asset_png("fx/solar.png");
    for (int i = 0; i < 4; i++) {
        snprintf(path, sizeof(path), "fx/spin_%d.png", i);
        g_fx_spin[i] = load_asset_png(path);
    }
    g_fx_hydro = load_asset_png("fx/hydro.png");
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "fx/shock_%d.png", i);
        g_fx_shock[i] = load_asset_png(path);
        snprintf(path, sizeof(path), "fx/strike_%d.png", i);
        g_fx_strike[i] = load_asset_png(path);
    }
    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof(path), "fx/slam_%d.png", i);
        g_fx_slam[i] = load_asset_png(path);
    }
    g_fx_ghost = load_asset_png("fx/ghost.png");
    g_fx_psybeam = load_asset_png("fx/psybeam.png");
    g_fx_psystrike = load_asset_png("fx/psystrike.png");
    g_fx_palm = load_asset_png("fx/palm.png");
    g_fx_note_normal = load_asset_png("fx/note_normal.png");
    for (int i = 0; i < 2; i++) {
        char fn[64];
        snprintf(fn, sizeof(fn), "fx/cannon_steel_%d.png", i); g_fx_cannon_steel[i] = load_asset_png(fn);
        snprintf(fn, sizeof(fn), "fx/cannon_electric_%d.png", i); g_fx_cannon_electric[i] = load_asset_png(fn);
        snprintf(fn, sizeof(fn), "fx/cannon_fairy_%d.png", i); g_fx_cannon_fairy[i] = load_asset_png(fn);
        snprintf(fn, sizeof(fn), "fx/cannon_normal_%d.png", i); g_fx_cannon_normal[i] = load_asset_png(fn);
        snprintf(fn, sizeof(fn), "fx/pincer_bug_%d.png", i); g_fx_pincer_bug[i] = load_asset_png(fn);
        snprintf(fn, sizeof(fn), "fx/pincer_water_%d.png", i); g_fx_pincer_water[i] = load_asset_png(fn);
        snprintf(fn, sizeof(fn), "fx/jaws_dark_%d.png", i); g_fx_jaws_dark[i] = load_asset_png(fn);
        snprintf(fn, sizeof(fn), "fx/slashx_normal_%d.png", i); g_fx_slashx_normal[i] = load_asset_png(fn);
        snprintf(fn, sizeof(fn), "fx/slashx_fairy_%d.png", i); g_fx_slashx_fairy[i] = load_asset_png(fn);
        snprintf(fn, sizeof(fn), "fx/slashx_fire_%d.png", i); g_fx_slashx_fire[i] = load_asset_png(fn);
        snprintf(fn, sizeof(fn), "fx/slashx_fighting_%d.png", i); g_fx_slashx_fighting[i] = load_asset_png(fn);
        snprintf(fn, sizeof(fn), "fx/slashx_steel_%d.png", i); g_fx_slashx_steel[i] = load_asset_png(fn);
        snprintf(fn, sizeof(fn), "fx/slashx_poison_%d.png", i); g_fx_slashx_poison[i] = load_asset_png(fn);
        snprintf(fn, sizeof(fn), "fx/kick_fighting_%d.png", i); g_fx_kick_fighting[i] = load_asset_png(fn);
    }
    g_fx_claw_steel = load_asset_png("fx/claw_steel.png");
    g_fx_hammer_ice = load_asset_png("fx/hammer_ice.png");
    g_fx_feather_flying = load_asset_png("fx/feather_flying.png");
    g_fx_xmark_poison = load_asset_png("fx/xmark_poison.png");
    if (!g_tex_egg_bomb) g_tex_egg_bomb = load_asset_png("fx/egg.png");
}

void render_start_frame(void) {
    g_world = false; g_sh_valid = false;
    g3d_frame_begin(g_clear_col);
}

void render_end_frame(void) {
    g_world = false;
    g_cam_msg = 0.0f;
    if (g_cam_dirty > 0.0f) { g_cam_dirty -= 1.0f / 60.0f; if (g_cam_dirty <= 0.0f) cam_save(); }
    g3d_frame_end();
}

void render_clear(uint32_t color) {
    g_clear_col = color;
    g3d_ui_rect(0, 0, SCREEN_W, SCREEN_H, color);
}

void render_world_begin(float cam_x, float cam_y) {
    g_world = true; g_wcx = cam_x; g_wcy = cam_y; g_sh_valid = false;
    int has_elev = (g_current_stage >= BAKED_FIRST_STAGE &&
                    g_baked_maps[g_current_stage - BAKED_FIRST_STAGE].elev != NULL);
    g3d_set_lift_z_shift(!has_elev);
    g3d_camera(cam_x + SCREEN_W * 0.5f, cam_y + SCREEN_H * 0.5f, g_cam.pitch, g_cam.yaw, g_cam.zoom, g_cam.fov);
    float e[3]; g3d_eye(e);
    float tx = cam_x + SCREEN_W * 0.5f, tz = cam_y + SCREEN_H * 0.5f;
    float dist = sqrtf((e[0] - tx) * (e[0] - tx) + e[1] * e[1] + (e[2] - tz) * (e[2] - tz));
    g3d_fog(1, dist * 1.15f, dist * 3.20f, 0.54f);
    g3d_layer(G3D_GROUND);
}
void render_world_end(void) {
    g_world = false; g_sh_valid = false;
    g3d_layer(G3D_UI);
}

void render_world_to_screen(float sx, float sy, float height, float *ox, float *oy, float *scale) {
    float px, py, s;
    float wx = sx + g_wcx, wz = sy + g_wcy;
    float l = wld_lift_fn(wx, wz);
    g3d_project(wx, height + l, wz + (g3d_get_lift_z_shift() ? l : 0.0f), &px, &py, &s);
    *ox = px; *oy = py; *scale = s;
}

void render_camera_adjust(float dt, float yaw_axis, float tilt_axis, float zoom_axis) {
    if (yaw_axis == 0.0f && tilt_axis == 0.0f && zoom_axis == 0.0f) return;
    g_cam.pitch += tilt_axis * 45.0f * dt;
    g_cam.yaw   -= yaw_axis * 50.0f * dt;
    g_cam.zoom  += zoom_axis * 0.7f * dt * g_cam.zoom;
    if (g_cam.pitch < 28.0f) g_cam.pitch = 28.0f;
    if (g_cam.pitch > 88.0f) g_cam.pitch = 88.0f;
    if (g_cam.yaw < -40.0f) g_cam.yaw = -40.0f;
    if (g_cam.yaw >  40.0f) g_cam.yaw =  40.0f;
    if (g_cam.zoom < 0.85f) g_cam.zoom = 0.85f;
    if (g_cam.zoom > 1.80f) g_cam.zoom = 1.80f;
    g_cam_dirty = 1.0f;
}
void render_camera_next_preset(void) {
    g_cam_preset = (g_cam_preset + 1) % N_PRESETS;
    g_cam = CAM_PRESETS[g_cam_preset];
    g_cam_dirty = 1.0f;
}
float render_camera_yaw(void) { return g_cam.yaw; }
float render_get_camera_zoom(void) { return g_cam.zoom > 0.1f ? g_cam.zoom : 1.0f; }

bool render_is_world_pos_visible(float wx, float wy, float margin) {
    float l = wld_lift_fn(wx, wy);
    float sx, sy, sc;
    g3d_project(wx, l, wy + (g3d_get_lift_z_shift() ? l : 0.0f), &sx, &sy, &sc);
    if (sx <= -800.0f) return false;
    return (sx >= -margin && sx <= (float)SCREEN_W + margin && sy >= -margin && sy <= (float)SCREEN_H + margin);
}

typedef struct { float x0, z0, x1, z1; uint32_t first, count; } BldRec;
static struct { int stage; BldRec *rec; uint32_t nrec, nv; unsigned char *vtx; g3d_tex *tex; unsigned char *cover; int cw, ch; } g_bld = { -1 };

static void bld_unload(void) {
    free(g_bld.rec); free(g_bld.vtx); free(g_bld.cover);
    if (g_bld.tex) g3d_free_tex(g_bld.tex);
    memset(&g_bld, 0, sizeof(g_bld)); g_bld.stage = -1;
}

static void bld_load(int stage) {
    bld_unload();
    g_bld.stage = stage;
    if (stage != STAGE_TOWN_HOENN) return;
    int si = stage - BAKED_FIRST_STAGE;
    const int asset_idx = 2;
    char rel[64], path[512];
    snprintf(rel, sizeof(rel), "world/bld_%d.bin", asset_idx);
    if (!resolve_asset_path(rel, path, sizeof(path))) return;
    FILE *f = fopen(path, "rb"); if (!f) return;
    char magic[4]; uint32_t nrec = 0, nv = 0;
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "BLD1", 4) || fread(&nrec, 4, 1, f) != 1 || fread(&nv, 4, 1, f) != 1) { fclose(f); return; }
    g_bld.rec = (BldRec *)malloc(nrec * sizeof(BldRec) + 1);
    g_bld.vtx = (unsigned char *)malloc((size_t)nv * 24 + 1);
    if (!g_bld.rec || !g_bld.vtx || fread(g_bld.rec, sizeof(BldRec), nrec, f) != nrec || fread(g_bld.vtx, 24, nv, f) != nv) {
        fclose(f); bld_unload(); return;
    }
    fclose(f);
    g_bld.nrec = nrec; g_bld.nv = nv;
    const BakedMap *bm = &g_baked_maps[si];
    g_bld.cw = bm->w; g_bld.ch = bm->h;
    g_bld.cover = (unsigned char *)calloc((size_t)bm->w * bm->h, 1);
    for (uint32_t i = 0; g_bld.cover && i < nrec; i++) {
        BldRec *b = &g_bld.rec[i];
        for (int z = (int)b->z0; z < (int)b->z1; z++)
            for (int x = (int)b->x0; x < (int)b->x1; x++)
                if (x >= 0 && z >= 0 && x < bm->w && z < bm->h) g_bld.cover[z * bm->w + x] = 1;
    }
    snprintf(rel, sizeof(rel), "world/bld_%d.png", asset_idx);
    g_bld.tex = load_asset_png(rel);
    if (!g_bld.tex) bld_unload(); else {
        g3d_set_nearest(g_bld.tex);
        char logbuf[256];
        snprintf(logbuf, sizeof(logbuf), "[RENDER] Stage %d: Loaded 3D buildings (recs=%u, verts=%u)", stage, nrec, nv);
        r_log(logbuf);
    }
}

static int bld_cov(int tx, int tz) {
    return g_bld.cover && tx >= 0 && tz >= 0 && tx < g_bld.cw && tz < g_bld.ch && g_bld.cover[tz * g_bld.cw + tx];
}

typedef struct { BldRec *rec; uint32_t nrec, nv; unsigned char *vtx; } BldSet;
static struct { int stage, w, h, gw, gh; short *grid; unsigned short *ovr; unsigned char *cov; unsigned char *cleared; BldSet rel, tre; g3d_tex *treetex; } g_wld = { -1 };
static int g_ground_phase = 0;

static void wld_set_free(BldSet *b) { free(b->rec); free(b->vtx); memset(b, 0, sizeof(*b)); }
static void wld_unload(void) {
    free(g_wld.grid); free(g_wld.ovr); free(g_wld.cov); free(g_wld.cleared); wld_set_free(&g_wld.rel); wld_set_free(&g_wld.tre);
    if (g_wld.treetex) g3d_free_tex(g_wld.treetex);
    memset(&g_wld, 0, sizeof(g_wld)); g_wld.stage = -1;
}
static int wld_read_set(const char *rel, BldSet *b) {
    char path[512]; if (!resolve_asset_path(rel, path, sizeof(path))) return 0;
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    char magic[4]; uint32_t nrec = 0, nv = 0;
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "BLD1", 4) || fread(&nrec, 4, 1, f) != 1 || fread(&nv, 4, 1, f) != 1) { fclose(f); return 0; }
    b->rec = (BldRec *)malloc(nrec * sizeof(BldRec) + 1); b->vtx = (unsigned char *)malloc((size_t)nv * 24 + 1);
    if (!b->rec || !b->vtx || fread(b->rec, sizeof(BldRec), nrec, f) != nrec || fread(b->vtx, 24, nv, f) != nv) { fclose(f); wld_set_free(b); return 0; }
    fclose(f); b->nrec = nrec; b->nv = nv; return 1;
}
static int hoenn_tree_is_thinned(float cx, float bz, float bw, float bh) {
    const float bm_m = 192.0f;
    if (cx < bm_m || cx > bw - bm_m || bz < bm_m || bz > bh - bm_m) return 0;
    int tx = (int)floorf(cx * (1.0f / 64.0f));
    int tz = (int)floorf(bz * (1.0f / 64.0f));
    int h = (tx * 7 + tz * 13) % 3;
    if (h < 0) h += 3;
    return (((tx + tz) & 1) != 0) || (h == 0);
}

static void wld_load(int stage) {
    wld_unload(); g_wld.stage = stage;
    g_wld.treetex = load_asset_png("world/trees.png");
    if (g_wld.treetex) g3d_set_nearest(g_wld.treetex);
    if (stage != STAGE_TOWN_HOENN) return;
    const int asset_idx = 2;
    char rel[64], path[512]; snprintf(rel, sizeof(rel), "world/wld_%d.bin", asset_idx);
    if (!resolve_asset_path(rel, path, sizeof(path))) return;
    FILE *f = fopen(path, "rb"); if (!f) return;
    char magic[4]; uint32_t hd[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "WLD1", 4) || fread(hd, 4, 4, f) != 4) { fclose(f); return; }
    g_wld.w = (int)hd[0]; g_wld.h = (int)hd[1]; g_wld.gw = (int)hd[2]; g_wld.gh = (int)hd[3];
    size_t ng = (size_t)g_wld.gw * g_wld.gh, nc = (size_t)g_wld.w * g_wld.h;
    g_wld.grid = (short *)malloc(ng * 2 + 2); g_wld.ovr = (unsigned short *)malloc(nc * 2 + 2); g_wld.cov = (unsigned char *)malloc(nc + 1);
    if (!g_wld.grid || !g_wld.ovr || !g_wld.cov || fread(g_wld.grid, 2, ng, f) != ng || fread(g_wld.ovr, 2, nc, f) != nc || fread(g_wld.cov, 1, nc, f) != nc) { fclose(f); wld_unload(); g_wld.stage = stage; return; }
    fclose(f);
    snprintf(rel, sizeof(rel), "world/rel_%d.bin", asset_idx); wld_read_set(rel, &g_wld.rel);
    snprintf(rel, sizeof(rel), "world/tre_%d.bin", asset_idx); wld_read_set(rel, &g_wld.tre);

    if (g_wld.tre.rec && g_wld.tre.vtx && g_wld.w > 0 && g_wld.h > 0) {
        const BakedMap *bm = &g_baked_maps[stage - BAKED_FIRST_STAGE];
        float bw = (float)(bm->w * 32), bh = (float)(bm->h * 32);
        g_wld.cleared = (unsigned char *)calloc((size_t)g_wld.w * (size_t)g_wld.h, 1);
        if (g_wld.cleared) {

            for (uint32_t ri = 0; ri < g_wld.tre.nrec; ri++) {
                BldRec *b = &g_wld.tre.rec[ri];
                unsigned char *base = g_wld.tre.vtx + (size_t)b->first * 24;
                uint32_t i = 0;
                while (i < b->count) {
                    uint8_t fl = base[i * 24 + 21];
                    if (fl == 1 && i + 6 <= b->count) {
                        float minx = 1e9f, maxx = -1e9f, zv = 0.0f;
                        for (int k = 0; k < 6; k++) {
                            float vx; memcpy(&vx, base + (i + k) * 24, 4);
                            if (vx < minx) minx = vx;
                            if (vx > maxx) maxx = vx;
                        }
                        memcpy(&zv, base + (i + 2) * 24 + 8, 4);
                        float cx = 0.5f * (minx + maxx) * 32.0f, bz = zv * 32.0f;
                        if (hoenn_tree_is_thinned(cx, bz, bw, bh)) {
                            int tx0 = (int)floorf(minx + 0.2f), tx1 = (int)floorf(maxx - 0.2f);
                            int tz1 = (int)floorf(zv), tz0 = tz1 - 1;
                            for (int tz = tz0; tz <= tz1; tz++)
                                for (int tx = tx0; tx <= tx1; tx++)
                                    if (tx >= 0 && tz >= 0 && tx < g_wld.w && tz < g_wld.h)
                                        g_wld.cleared[tz * g_wld.w + tx] = 1;
                        }
                        i += 6;
                    } else if (fl >= 1 && i + 6 <= b->count) { i += 6; } else { i += 1; }
                }
            }

            for (uint32_t ri = 0; ri < g_wld.tre.nrec; ri++) {
                BldRec *b = &g_wld.tre.rec[ri];
                unsigned char *base = g_wld.tre.vtx + (size_t)b->first * 24;
                uint32_t i = 0;
                while (i < b->count) {
                    uint8_t fl = base[i * 24 + 21];
                    if (fl == 1 && i + 6 <= b->count) {
                        float minx = 1e9f, maxx = -1e9f, zv = 0.0f;
                        for (int k = 0; k < 6; k++) {
                            float vx; memcpy(&vx, base + (i + k) * 24, 4);
                            if (vx < minx) minx = vx;
                            if (vx > maxx) maxx = vx;
                        }
                        memcpy(&zv, base + (i + 2) * 24 + 8, 4);
                        float cx = 0.5f * (minx + maxx) * 32.0f, bz = zv * 32.0f;
                        if (!hoenn_tree_is_thinned(cx, bz, bw, bh)) {
                            int tx0 = (int)floorf(minx + 0.2f), tx1 = (int)floorf(maxx - 0.2f);
                            int tz1 = (int)floorf(zv), tz0 = tz1 - 1;
                            for (int tz = tz0; tz <= tz1; tz++)
                                for (int tx = tx0; tx <= tx1; tx++)
                                    if (tx >= 0 && tz >= 0 && tx < g_wld.w && tz < g_wld.h)
                                        g_wld.cleared[tz * g_wld.w + tx] = 0;
                        }
                        i += 6;
                    } else if (fl >= 1 && i + 6 <= b->count) { i += 6; } else { i += 1; }
                }
            }

            for (uint32_t ri = 0; ri < g_wld.tre.nrec; ri++) {
                BldRec *b = &g_wld.tre.rec[ri];
                unsigned char *base = g_wld.tre.vtx + (size_t)b->first * 24;
                uint32_t i = 0, dst = 0;
                while (i < b->count) {
                    uint8_t fl = base[i * 24 + 21];
                    if (fl == 1 && i + 6 <= b->count) {
                        float minx = 1e9f, maxx = -1e9f, zv = 0.0f;
                        for (int k = 0; k < 6; k++) {
                            float vx; memcpy(&vx, base + (i + k) * 24, 4);
                            if (vx < minx) minx = vx;
                            if (vx > maxx) maxx = vx;
                        }
                        memcpy(&zv, base + (i + 2) * 24 + 8, 4);
                        float cx = 0.5f * (minx + maxx) * 32.0f, bz = zv * 32.0f;
                        int skip = hoenn_tree_is_thinned(cx, bz, bw, bh);
                        if (!skip) {
                            if (dst != i) memmove(base + dst * 24, base + i * 24, 6 * 24);
                            dst += 6;
                        }
                        i += 6;
                    } else if (fl == 0 && i + 6 <= b->count) {
                        float minx = 1e9f, maxx = -1e9f, minz = 1e9f, maxz = -1e9f;
                        for (int k = 0; k < 6; k++) {
                            float vx, vz;
                            memcpy(&vx, base + (i + k) * 24, 4);
                            memcpy(&vz, base + (i + k) * 24 + 8, 4);
                            if (vx < minx) minx = vx; if (vx > maxx) maxx = vx;
                            if (vz < minz) minz = vz; if (vz > maxz) maxz = vz;
                        }
                        int tx = (int)floorf(0.5f * (minx + maxx));
                        int tz = (int)floorf(0.5f * (minz + maxz));
                        int skip = (tx >= 0 && tz >= 0 && tx < g_wld.w && tz < g_wld.h && g_wld.cleared[tz * g_wld.w + tx]);
                        if (!skip) {
                            if (dst != i) memmove(base + dst * 24, base + i * 24, 6 * 24);
                            dst += 6;
                        }
                        i += 6;
                    } else if (fl >= 1 && i + 6 <= b->count) {
                        if (dst != i) memmove(base + dst * 24, base + i * 24, 6 * 24);
                        dst += 6; i += 6;
                    } else {
                        if (dst != i) memmove(base + dst * 24, base + i * 24, 24);
                        dst += 1; i += 1;
                    }
                }
                b->count = dst;
            }
        }
    }
    char logbuf[256];
    snprintf(logbuf, sizeof(logbuf), "[RENDER] Stage %d: Loaded 3D world (trees=%u, relief=%u, grid=%dx%d)", stage, g_wld.tre.nv, g_wld.rel.nv, g_wld.gw, g_wld.gh);
    r_log(logbuf);
}
int render_hoenn_tile_cleared(int tx, int tz) {
    return (g_wld.stage == STAGE_TOWN_HOENN && g_wld.cleared && tx >= 0 && tz >= 0 && tx < g_wld.w && tz < g_wld.h) ? g_wld.cleared[tz * g_wld.w + tx] : 0;
}
static int wld_cov(int tx, int tz) {
    return g_wld.cov && tx >= 0 && tz >= 0 && tx < g_wld.w && tz < g_wld.h ? g_wld.cov[tz * g_wld.w + tx] : 0;
}

static float wld_lift_fn(float x, float z) {
    if (g_current_stage >= BAKED_FIRST_STAGE) {
        const BakedMap *bm = &g_baked_maps[g_current_stage - BAKED_FIRST_STAGE];

        if (bm->bridges && bm->num_bridges > 0) {
            float tx_f = x / 32.0f, tz_f = z / 32.0f;
            for (int b = 0; b < bm->num_bridges; b++) {
                const BakedBridge *br = &bm->bridges[b];
                if (tx_f >= (float)br->x0 - 0.10f && tx_f <= (float)(br->x1 + 1) + 0.10f &&
                    tz_f >= (float)br->y0 - 0.10f && tz_f <= (float)(br->y1 + 1) + 0.10f) {
                    return (float)br->deck_y;
                }
            }
        }
        if (bm->elev) {
            int ew = bm->w + 1, eh = bm->h + 1;
            float fx = x / 32.0f, fz = z / 32.0f;
            if (fx < 0.0f) fx = 0.0f;
            if (fz < 0.0f) fz = 0.0f;
            if (fx > (float)(bm->w) - 0.001f) fx = (float)(bm->w) - 0.001f;
            if (fz > (float)(bm->h) - 0.001f) fz = (float)(bm->h) - 0.001f;
            int i = (int)fx, j = (int)fz;
            if (i >= ew - 1) i = ew - 2;
            if (j >= eh - 1) j = eh - 2;
            float ax = fx - (float)i, az = fz - (float)j;
            const uint8_t *e = bm->elev + (size_t)j * (size_t)ew + (size_t)i;
            float a = (float)e[0] + ((float)e[1] - (float)e[0]) * ax;
            float b = (float)e[ew] + ((float)e[ew + 1] - (float)e[ew]) * ax;
            return a + (b - a) * az;
        }
    }
    if (!g_wld.grid || g_wld.stage != g_current_stage) return 0.0f;
    if (g_ground_phase) { x += 16.0f; z += 16.0f; }
    float fx = x / 32.0f * 4.0f, fz = z / 32.0f * 4.0f;
    if (fx < 0) fx = 0; if (fz < 0) fz = 0;
    if (fx > g_wld.gw - 1.001f) fx = g_wld.gw - 1.001f; if (fz > g_wld.gh - 1.001f) fz = g_wld.gh - 1.001f;
    int i = (int)fx, j = (int)fz; float ax = fx - i, az = fz - j;
    const short *g = g_wld.grid + (size_t)j * g_wld.gw + i;
    float a = g[0] + (g[1] - g[0]) * ax, b = g[g_wld.gw] + (g[g_wld.gw + 1] - g[g_wld.gw]) * ax;
    return (a + (b - a) * az) * 2.0f;
}

void render_set_stage(int stage_id) {
    if (stage_id < 0) stage_id = 0;
    if (stage_id >= TOTAL_STAGES) stage_id = TOTAL_STAGES - 1;
    if (g_current_stage != stage_id) g3d_cache_invalidate();
    g_current_stage = stage_id;
    int has_elev = (stage_id >= BAKED_FIRST_STAGE && g_baked_maps[stage_id - BAKED_FIRST_STAGE].elev != NULL);
    g3d_set_lift_z_shift(!has_elev);
    if (g_bld.stage != stage_id) bld_load(stage_id);
    if (g_wld.stage != stage_id) wld_load(stage_id);
    if (stage_id == STAGE_TOWN_HOENN) {
        const BakedMap *bm = &g_baked_maps[stage_id - BAKED_FIRST_STAGE];
        g3d_set_border_bounds((float)(bm->w * 32), (float)(bm->h * 32), 192.0f);
    } else {
        g3d_set_border_bounds(0.0f, 0.0f, 0.0f);
    }
}

void render_draw_stage_preview(int stage_id, float x, float y, float w, float h) {
    if (stage_id < 0) stage_id = 0;
    if (stage_id >= TOTAL_STAGES) stage_id = TOTAL_STAGES - 1;
    vita2d_texture *tex = g_tex_stage_previews[stage_id];
    if (tex) {
        float tw = (float)vita2d_texture_get_width(tex);
        float th = (float)vita2d_texture_get_height(tex);
        float sx = w / tw;
        float sy = h / th;
        vita2d_draw_texture_scale(tex, x, y, sx, sy);
    } else {
        uint32_t col = (stage_id == 0) ? RGBA8(30, 80, 40, 255) :
                       (stage_id == 1) ? RGBA8(60, 90, 50, 255) :
                                         RGBA8(15, 35, 25, 255);
        vita2d_draw_rectangle(x, y, w, h, col);
    }
}

#define EMERALD_TILE_PAD 2
#define EMERALD_TILE_CELL (16 + EMERALD_TILE_PAD * 2)

void render_draw_emerald_tile(int tile_id, float x, float y) {
    if (!g_tex_emerald_tiles) return;
    if (tile_id < 0 || tile_id >= (g_tex_emerald_tiles->h / EMERALD_TILE_CELL) * 16) tile_id = 0;
    float sx = (float)((tile_id & 15) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
    float sy = (float)((tile_id >> 4) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
    vita2d_draw_texture_part_scale(g_tex_emerald_tiles, x, y, sx, sy, 16.0f, 16.0f, 2.0f, 2.0f);
}

void render_draw_ground(float cam_x, float cam_y) {
    static unsigned anim = 0; anim++;
    if (!g_world || !g_tex_emerald_tiles) {
        int start_x = (int)floorf(cam_x / 32.0f) - 1;
        int start_y = (int)floorf(cam_y / 32.0f) - 1;
        int end_x = start_x + (SCREEN_W / 32) + 3;
        int end_y = start_y + (SCREEN_H / 32) + 3;
        for (int ty = start_y; ty <= end_y; ty++) {
            for (int tx = start_x; tx <= end_x; tx++) {
                float fx = floorf((float)(tx * 32) - cam_x);
                float fy = floorf((float)(ty * 32) - cam_y);
                int tid = terrain_tile_at(g_current_stage, tx, ty, anim);
                render_draw_emerald_tile(tid, fx, fy);
            }
        }
        return;
    }

    float tx0 = cam_x + SCREEN_W * 0.5f, tz0 = cam_y + SCREEN_H * 0.5f;
    float minx = 1e9f, maxx = -1e9f, minz = 1e9f, maxz = -1e9f, h[2];
    static const float pts[6][2] = { {0,0}, {SCREEN_W,0}, {0,SCREEN_H}, {SCREEN_W,SCREEN_H}, {SCREEN_W*0.5f,0}, {SCREEN_W*0.5f,SCREEN_H} };
    for (int i = 0; i < 6; i++) {
        g3d_ground_hit(pts[i][0], pts[i][1], h);
        if (h[0] < minx) minx = h[0]; if (h[0] > maxx) maxx = h[0];
        if (h[1] < minz) minz = h[1]; if (h[1] > maxz) maxz = h[1];
    }
#ifdef __vita__
    float lim = (g_current_stage == STAGE_TOWN_HOENN) ? 600.0f : 1450.0f;
#else
    float lim = (g_current_stage == STAGE_TOWN_HOENN) ? 1050.0f : 2600.0f;
#endif
    if (minx < tx0 - lim) minx = tx0 - lim; if (maxx > tx0 + lim) maxx = tx0 + lim;
    if (minz < tz0 - lim) minz = tz0 - lim; if (maxz > tz0 + lim) maxz = tz0 + lim;
    int sx = (int)floorf(minx / 32.0f) - 2, ex = (int)floorf(maxx / 32.0f) + 2;
    int sz = (int)floorf(minz / 32.0f) - 2, ez = (int)floorf(maxz / 32.0f) + 2;
    const BakedMap *bm = (g_current_stage >= BAKED_FIRST_STAGE) ? &g_baked_maps[g_current_stage - BAKED_FIRST_STAGE] : NULL;
    int has_elev = (bm && bm->elev != NULL);
    int map_w = bm ? bm->w : (int)(WORLD_W / 32.0f);
    int map_h = bm ? bm->h : (int)(WORLD_H / 32.0f);
    if (has_elev) { sx -= 3; ex += 3; sz -= 3; ez += 3; }
    if (g_current_stage == STAGE_TOWN_HOENN) {

        sx &= ~3; sz &= ~3;
        ex = (ex + 3) | 3; ez = (ez + 3) | 3;
    }
    if (sx < 0) sx = 0;
    if (sz < 0) sz = 0;
    if (ex >= map_w) ex = map_w - 1;
    if (ez >= map_h) ez = map_h - 1;

    static int c_stage = -1, c_sx = -9999, c_sz = -9999, c_ex = -9999, c_ez = -9999, c_yaw = -9999, c_pitch = -9999, c_anim = -1;
    int yaw_q = (int)floorf(g_cam.yaw * 0.5f);
    int pitch_q = (int)floorf(g_cam.pitch * 0.5f);
    int anim_q = (g_current_stage == STAGE_TOWN_HOENN) ? (int)(anim >> 5) : (int)(anim >> 3);
    if (g3d_cache_valid() && c_stage == g_current_stage && c_sx == sx && c_sz == sz && c_ex == ex && c_ez == ez &&
        c_yaw == yaw_q && c_pitch == pitch_q && c_anim == anim_q) {
        g3d_cache_replay();
        return;
    }
    c_stage = g_current_stage; c_sx = sx; c_sz = sz; c_ex = ex; c_ez = ez; c_yaw = yaw_q; c_pitch = pitch_q; c_anim = anim_q;
    g3d_cache_begin();

    float iw = 1.0f / (float)g_tex_emerald_tiles->w, ih = 1.0f / (float)g_tex_emerald_tiles->h;
    int tile_cap = (g_tex_emerald_tiles->h / EMERALD_TILE_CELL) * 16;
    if (has_elev) {

        g3d_layer(G3D_GROUND_3D);
        int ew = bm->w + 1;
        float flow_t = (float)anim * (1.0f / 60.0f);
        for (int tz = sz; tz <= ez; tz++) {
            for (int tx = sx; tx <= ex; tx++) {
                int tid = terrain_tile_at(g_current_stage, tx, tz, anim);
                if (tid < 0 || tid >= tile_cap) tid = 0;
                float u = (float)((tid & 15) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
                float v = (float)((tid >> 4) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
                float y00 = (float)bm->elev[tz * ew + tx];
                float y10 = (float)bm->elev[tz * ew + tx + 1];
                float y11 = (float)bm->elev[(tz + 1) * ew + tx + 1];
                float y01 = (float)bm->elev[(tz + 1) * ew + tx];
                float x0 = (float)(tx * 32), z0 = (float)(tz * 32);
                float x1 = (float)((tx + 1) * 32), z1 = (float)((tz + 1) * 32);
                float u0 = u * iw, v0 = v * ih, u1 = (u + 16.0f) * iw, v1 = (v + 16.0f) * ih;
                if (tid == g_baked_tid_wf_body || tid == g_baked_tid_wf_foam || tid == g_baked_tid_wf_top) {

                    float wave = 0.5f + 0.5f * sinf(flow_t * 5.2f + (float)tx * 0.7f - (float)tz * 0.9f);
                    uint32_t wcol = RGBA8((uint32_t)(234.0f + 21.0f * wave), (uint32_t)(244.0f + 11.0f * wave), 255u, 255u);
                    float spd_v = (tid == g_baked_tid_wf_top) ? 0.95f : 1.85f;
                    g3d_quad_elev_flow(g_tex_emerald_tiles, x0, z0, x1, z1, y00, y10, y11, y01,
                                       u0, v0, u1, v1, 0.0f, flow_t * spd_v, wcol);
                } else if (tid == g_baked_tid_water || tid == 137 || tid == 150) {

                    float wave = 0.5f + 0.5f * sinf(flow_t * 3.0f + (float)tx * 0.45f + (float)tz * 0.55f);
                    uint32_t wcol = RGBA8((uint32_t)(236.0f + 16.0f * wave), (uint32_t)(244.0f + 11.0f * wave), 255u, 255u);
                    g3d_quad_elev_flow(g_tex_emerald_tiles, x0, z0, x1, z1, y00, y10, y11, y01,
                                       u0, v0, u1, v1, flow_t * 0.14f, flow_t * 0.45f, wcol);
                } else {
                    g3d_quad_elev(g_tex_emerald_tiles, x0, z0, x1, z1, y00, y10, y11, y01,
                                  u0, v0, u1, v1, 0xFFFFFFFFu);
                }
            }
        }

        if (bm->bridges && bm->num_bridges > 0) {
            for (int b = 0; b < bm->num_bridges; b++) {
                const BakedBridge *br = &bm->bridges[b];
                if (br->x1 < sx - 2 || br->x0 > ex + 2 || br->y1 < sz - 2 || br->y0 > ez + 2) continue;
                int ptid = (int)br->plank_tid;
                if (ptid < 0 || ptid >= tile_cap) ptid = 47;
                float pu = (float)((ptid & 15) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
                float pv = (float)((ptid >> 4) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
                g3d_bridge_3d(g_tex_emerald_tiles,
                              (float)(br->x0 * 32), (float)(br->y0 * 32),
                              (float)((br->x1 + 1) * 32), (float)((br->y1 + 1) * 32),
                              (float)br->deck_y, (int)br->dir,
                              pu * iw, pv * ih, (pu + 16.0f) * iw, (pv + 16.0f) * ih);
            }
        }
        g3d_cache_end();
        return;
    }
    g3d_layer(G3D_GROUND);
    g_ground_phase = 1;
    for (int tz = sz; tz <= ez; tz++) {
        for (int tx = sx; tx <= ex; tx++) {
            if (wld_cov(tx, tz) & 1) continue;
            int tid = terrain_tile_at(g_current_stage, tx, tz, anim);
            if (g_wld.ovr && tx >= 0 && tz >= 0 && tx < g_wld.w && tz < g_wld.h && g_wld.ovr[tz * g_wld.w + tx] != 0xFFFF) tid = g_wld.ovr[tz * g_wld.w + tx];
            if (g_current_stage == STAGE_TOWN_HOENN && render_hoenn_tile_cleared(tx, tz)) tid = 133;
            else if (terrain_tile_kind(g_current_stage, tx, tz) == TER_CLIFF)
                tid = terrain_under_at(g_current_stage, tx, tz);
            if (bld_cov(tx, tz)) {
#ifdef __vita__
                tid = terrain_under_at(g_current_stage, tx, tz);
#else

                int cand_tid[32], cand_cnt[32], n_cand = 0;
                for (int pass_r = 0; pass_r < 2 && n_cand == 0; pass_r++) {
                    int rad = (pass_r == 0) ? 3 : 7;
                    for (int dz = -rad; dz <= rad; dz++) {
                        for (int dx = -rad; dx <= rad; dx++) {
                            int nx = tx + dx, nz = tz + dz;
                            if (nx < 0 || nz < 0 || nx >= map_w || nz >= map_h) continue;
                            if (bld_cov(nx, nz) || (wld_cov(nx, nz) & 1)) continue;
                            int k = terrain_tile_kind(g_current_stage, nx, nz);
                            if (k == TER_CLIFF || k == TER_DEEP || k == TER_SHALLOW) continue;
                            int ntid = terrain_tile_at(g_current_stage, nx, nz, anim);
                            int slot = -1;
                            for (int c = 0; c < n_cand; c++) { if (cand_tid[c] == ntid) { slot = c; break; } }
                            if (slot >= 0) cand_cnt[slot]++;
                            else if (n_cand < 32) { cand_tid[n_cand] = ntid; cand_cnt[n_cand] = 1; n_cand++; }
                        }
                    }
                }
                if (n_cand > 0) {
                    int best_c = 0;
                    for (int c = 1; c < n_cand; c++) {
                        if (cand_cnt[c] > cand_cnt[best_c]) best_c = c;
                    }
                    tid = cand_tid[best_c];
                }
#endif
            }
            if (tid < 0 || tid >= tile_cap) tid = 0;
            float u = (float)((tid & 15) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
            float v = (float)((tid >> 4) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
            if (tid == g_baked_tid_water || tid == 137 || tid == 150 || (tid >= ET_DEEP_F0 && tid <= ET_DEEP_F0 + 3)) {
                float flow_t = (float)anim * (1.0f / 60.0f);
                float wave = 0.5f + 0.5f * sinf(flow_t * 3.0f + (float)tx * 0.45f + (float)tz * 0.55f);
                uint32_t wcol = RGBA8((uint32_t)(238.0f + 16.0f * wave), (uint32_t)(245.0f + 10.0f * wave), 255u, 255u);
                g3d_quad_elev_flow(g_tex_emerald_tiles,
                                   (float)(tx * 32) - 0.05f, (float)(tz * 32) - 0.05f,
                                   (float)((tx + 1) * 32) + 0.05f, (float)((tz + 1) * 32) + 0.05f,
                                   GROUND_Y, GROUND_Y, GROUND_Y, GROUND_Y,
                                   u * iw, v * ih, (u + 16.0f) * iw, (v + 16.0f) * ih,
                                   flow_t * 0.14f, flow_t * 0.42f, wcol);
            } else {
                float c[3] = { (float)(tx * 32), GROUND_Y, (float)(tz * 32) };
                float u0 = u * iw, u1 = (u + 16.0f) * iw;
                float v0 = v * ih, v1 = (v + 16.0f) * ih;
                const float *ax_u = AX_X, *ax_v = AX_Z;
                if (g_current_stage < BAKED_FIRST_STAGE) {
                    int tk = terrain_tile_kind(g_current_stage, tx, tz);
                    if (tk == TER_LEDGE_N) {
                        float tmp = v0; v0 = v1; v1 = tmp;
                    } else if (tk == TER_LEDGE_E) {
                        ax_u = AX_Z; ax_v = AX_X;
                    } else if (tk == TER_LEDGE_W) {
                        ax_u = AX_Z; ax_v = AX_X;
                        float tmp = v0; v0 = v1; v1 = tmp;
                    }
                }

                g3d_plane(g_tex_emerald_tiles, c, ax_u, ax_v, -0.05f, -0.05f, 32.05f, 32.05f, 0,
                          u0, v0, u1, v1, 0xFFFFFFFFu);
            }
        }
    }
    g_ground_phase = 0;
    float R_bb[3], D_bb[3];
    bb_basis(R_bb, D_bb);
#ifdef __vita__
    const int z_pad_tre = (g_current_stage == STAGE_TOWN_HOENN) ? 5 : 18;
    const int z_pad_bld = (g_current_stage == STAGE_TOWN_HOENN) ? 5 : 10;
#else
    const int z_pad_tre = (g_current_stage == STAGE_TOWN_HOENN) ? 10 : 32;
    const int z_pad_bld = (g_current_stage == STAGE_TOWN_HOENN) ? 8 : 10;
#endif
    if (g_wld.stage == g_current_stage) {
        g3d_layer(G3D_GROUND);
        if (g_wld.rel.rec) for (uint32_t i = 0; i < g_wld.rel.nrec; i++) {
            const BldRec *b = &g_wld.rel.rec[i];
            if (b->x1 < sx - 2 || b->x0 > ex + 2 || b->z1 < sz - 2 || b->z0 > ez + z_pad_tre) continue;
            g3d_tris(g_tex_emerald_tiles, g_wld.rel.vtx + (size_t)b->first * 24, (int)b->count, 32.0f);
        }
        if (g_wld.tre.rec && g_wld.treetex) for (uint32_t i = 0; i < g_wld.tre.nrec; i++) {
            const BldRec *b = &g_wld.tre.rec[i];
            if (b->x1 < sx - 2 || b->x0 > ex + 2 || b->z1 < sz - 2 || b->z0 > ez + z_pad_tre) continue;
            g3d_tris_bb(g_wld.treetex, g_wld.tre.vtx + (size_t)b->first * 24, (int)b->count, 32.0f, R_bb, D_bb, 0);
        }
    }

    if (g_bld.tex && g_bld.stage == g_current_stage) {
        g3d_layer(G3D_GROUND);
        for (uint32_t i = 0; i < g_bld.nrec; i++) {
            const BldRec *b = &g_bld.rec[i];
            if (b->x1 < sx - 2 || b->x0 > ex + 2 || b->z1 < sz - 2 || b->z0 > ez + z_pad_bld) continue;
            g3d_tris_split(g_bld.tex, g_bld.vtx + (size_t)b->first * 24, (int)b->count, 32.0f, 0);
        }
    }

    if ((g_bld.tex && g_bld.stage == g_current_stage) || (g_wld.stage == g_current_stage && g_wld.tre.rec && g_wld.treetex)) {
        g3d_layer(G3D_SHADOW);
        if (g_bld.tex && g_bld.stage == g_current_stage) {
            for (uint32_t i = 0; i < g_bld.nrec; i++) {
                const BldRec *b = &g_bld.rec[i];
                if (b->x1 < sx - 4 || b->x0 > ex + 4 || b->z1 < sz - 4 || b->z0 > ez + z_pad_bld + 2) continue;
                g3d_tris_shadow(g_bld.tex, g_bld.vtx + (size_t)b->first * 24, (int)b->count, 32.0f, 0.36f, 0.72f, RGBA8(16, 26, 40, 92));
            }
        }
        if (g_wld.stage == g_current_stage && g_wld.tre.rec && g_wld.treetex) {
            for (uint32_t i = 0; i < g_wld.tre.nrec; i++) {
                const BldRec *b = &g_wld.tre.rec[i];
                if (b->x1 < sx - 4 || b->x0 > ex + 4 || b->z1 < sz - 4 || b->z0 > ez + z_pad_tre) continue;
                g3d_tris_bb(g_wld.treetex, g_wld.tre.vtx + (size_t)b->first * 24, (int)b->count, 32.0f, R_bb, D_bb, 1);
            }
        }
    }
    {

        static const float DOWN[3] = { 0.0f, -1.0f, 0.0f };
        const int cave = g_current_stage >= STAGE_CAVE_GRANITE;
        const int cap = (g_tex_emerald_tiles->h / EMERALD_TILE_CELL) * 16;
        g3d_layer(G3D_SPRITE);
        for (int tx = sx; tx <= ex; tx++) {
            for (int tz = sz; tz <= ez; tz++) {
                if (terrain_tile_kind(g_current_stage, tx, tz) != TER_CLIFF || bld_cov(tx, tz) || wld_cov(tx, tz)) continue;
                if (tz > sz && terrain_tile_kind(g_current_stage, tx, tz - 1) == TER_CLIFF && !bld_cov(tx, tz - 1) && !wld_cov(tx, tz - 1)) continue;
                int y0 = tz, y1 = tz;
                while (tz - y0 < 60 && terrain_tile_kind(g_current_stage, tx, y0 - 1) == TER_CLIFF && !bld_cov(tx, y0 - 1) && !wld_cov(tx, y0 - 1)) y0--;
                while (y1 - tz < 60 && terrain_tile_kind(g_current_stage, tx, y1 + 1) == TER_CLIFF && !bld_cov(tx, y1 + 1) && !wld_cov(tx, y1 + 1)) y1++;
                int len = y1 - y0 + 1;
                int wall = (cave || len <= 3) ? 1 : 2;
                if (wall > len) wall = len;
                float zs = (float)((y1 + 1) * 32);

                for (int k = 0; k < wall; k++) {
                    int ty = y1 - k;
                    int tid = terrain_tile_at(g_current_stage, tx, ty, anim);
                    if (tid < 0 || tid >= cap) continue;
                    float u = (float)((tid & 15) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
                    float v = (float)((tid >> 4) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
                    float c[3] = { (float)(tx * 32), GROUND_Y + (float)((k + 1) * 32), zs };
                    g3d_plane(g_tex_emerald_tiles, c, AX_X, DOWN, -0.05f, 0.0f, 32.05f, 32.05f, 0,
                              u * iw, v * ih, (u + 16.0f) * iw, (v + 16.0f) * ih, 0xFFE8E8E8u);
                }

                for (int j = 0; j < len - wall; j++) {
                    int ty = y0 + j;
                    int tid = terrain_tile_at(g_current_stage, tx, ty, anim);
                    if (tid < 0 || tid >= cap) continue;
                    float u = (float)((tid & 15) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
                    float v = (float)((tid >> 4) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
                    float c[3] = { (float)(tx * 32), GROUND_Y + (float)(wall * 32), (float)((y0 + wall + j) * 32) };
                    g3d_plane(g_tex_emerald_tiles, c, AX_X, AX_Z, -0.05f, -0.05f, 32.05f, 32.05f, 0,
                              u * iw, v * ih, (u + 16.0f) * iw, (v + 16.0f) * ih, 0xFFFFFFFFu);
                }
                tz = y1;
            }
        }
    }
    if (g_bld.tex && g_bld.stage == g_current_stage) {
        g3d_layer(G3D_SPRITE);
        for (uint32_t i = 0; i < g_bld.nrec; i++) {
            const BldRec *b = &g_bld.rec[i];
            if (b->x1 < sx - 2 || b->x0 > ex + 2 || b->z1 < sz - 2 || b->z0 > ez + 10) continue;
            g3d_tris_split(g_bld.tex, g_bld.vtx + (size_t)b->first * 24, (int)b->count, 32.0f, 1);
        }
    }
    if (g_wld.stage == g_current_stage) {
        if (g_wld.tre.rec && g_wld.treetex) {

            g3d_layer(G3D_DECAL);
            for (uint32_t i = 0; i < g_wld.tre.nrec; i++) {
                const BldRec *b = &g_wld.tre.rec[i];
                if (b->x1 < sx - 2 || b->x0 > ex + 2 || b->z1 < sz - 2 || b->z0 > ez + z_pad_tre) continue;
                g3d_tris_bb(g_wld.treetex, g_wld.tre.vtx + (size_t)b->first * 24, (int)b->count, 32.0f, R_bb, D_bb, 3);
            }

            g3d_layer(G3D_SPRITE);
            for (uint32_t i = 0; i < g_wld.tre.nrec; i++) {
                const BldRec *b = &g_wld.tre.rec[i];
                if (b->x1 < sx - 2 || b->x0 > ex + 2 || b->z1 < sz - 2 || b->z0 > ez + z_pad_tre) continue;
                g3d_tris_bb(g_wld.treetex, g_wld.tre.vtx + (size_t)b->first * 24, (int)b->count, 32.0f, R_bb, D_bb, 2);
            }
        }
    }
    g3d_cache_end();
}

static bool g_sh_skip = false;

void render_draw_shadow(float x, float y, float radius) {
    g_sh_skip = false;
    if (g_world) {

        g_sh_x = x; g_sh_y = y; g_sh_valid = true;
        return;
    }

    vita2d_draw_fill_circle(x, y - 1.0f, radius, RGBA8(25, 20, 30, 85));
}

void render_draw_oval_shadow(float x, float y, float radius) {
    if (g_world) {
        g3d_layer(G3D_DECAL);
        g3d_shadow_oval(x + g_wcx, y + g_wcy, radius * 1.15f, radius * 0.72f, RGBA8(16, 26, 40, 108));
        return;
    }
    vita2d_draw_fill_circle(x, y - 1.0f, radius, RGBA8(25, 20, 30, 85));
}

void render_skip_next_shadow(void) {
    g_sh_skip = true;
    g_sh_valid = false;
}

void render_draw_rect(float x, float y, float w, float h, uint32_t color) {
    vita2d_draw_rectangle(x, y, w, h, color);
}

void render_draw_rounded_rect(float x, float y, float w, float h, float r, uint32_t color) {
    if (w <= 0.0f || h <= 0.0f) return;
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;
    if (r < 2.0f) {
        vita2d_draw_rectangle(x, y, w, h, color);
        return;
    }
    float c = floorf(r * 0.45f + 0.5f);
    if (c < 2.0f) c = 2.0f;
    if (c > 4.0f) c = 4.0f;
    if (c * 2.0f >= w || c * 2.0f >= h) {
        vita2d_draw_rectangle(x, y, w, h, color);
        return;
    }
    vita2d_draw_rectangle(x + c, y, w - 2.0f * c, c, color);
    vita2d_draw_rectangle(x, y + c, w, h - 2.0f * c, color);
    vita2d_draw_rectangle(x + c, y + h - c, w - 2.0f * c, c, color);
}

void render_draw_rect_outline(float x, float y, float w, float h, uint32_t color, float thickness) {
    vita2d_draw_rectangle(x, y, w, thickness, color);
    vita2d_draw_rectangle(x, y + h - thickness, w, thickness, color);
    vita2d_draw_rectangle(x, y + thickness, thickness, h - 2.0f * thickness, color);
    vita2d_draw_rectangle(x + w - thickness, y + thickness, thickness, h - 2.0f * thickness, color);
}

void render_draw_circle(float x, float y, float radius, uint32_t color) {
    if (g_world) {
        g3d_layer(G3D_DECAL);
        float R[3], D[3];
        bb_basis(R, D);
        float c[3] = { x + g_wcx - D[0] * 14.0f, 0.5f - D[1] * 14.0f, y + g_wcy - D[2] * 14.0f };
        g3d_disc(c, R, D, radius, color);
        return;
    }
    vita2d_draw_fill_circle(x, y, radius, color);
}

void render_draw_circle_ground(float x, float y, float radius, uint32_t color) {
    if (g_world) {
        g3d_layer(G3D_DECAL);
        float c[3] = { x + g_wcx, GROUND_Y + 0.65f, y + g_wcy };
        g3d_disc(c, AX_X, AX_Z, radius, color);
        return;
    }
    vita2d_draw_fill_circle(x, y, radius, color);
}

void render_draw_line(float x1, float y1, float x2, float y2, uint32_t color, float thickness) {
    if (g_world) {
        g3d_layer(G3D_DECAL);
        float R[3], D[3];
        bb_basis(R, D);
        float p1[3] = { x1 + g_wcx - D[0] * 14.0f, 0.5f - D[1] * 14.0f, y1 + g_wcy - D[2] * 14.0f };
        float p2[3] = { x2 + g_wcx - D[0] * 14.0f, 0.5f - D[1] * 14.0f, y2 + g_wcy - D[2] * 14.0f };
        float dx = p2[0] - p1[0], dz = p2[2] - p1[2], len = sqrtf(dx*dx + dz*dz);
        if (len > 0.01f) {
            float perp[3] = { -dz / len, 0.0f, dx / len };
            g3d_ribbon(p1, p2, perp, thickness < 1.0f ? 1.0f : thickness, color);
        }
        return;
    }
    draw_line_t(x1, y1, x2, y2, color, thickness);
}

static int get_or_load_poke_cache_vita(int dex) {
    if (dex <= 0) dex = 1;
    if (dex >= 1025) dex = 1024;
    for (int i = 0; i < POKE_CACHE_SIZE; i++) {
        if (g_poke_cache_vita[i].dex == dex && g_poke_cache_vita[i].sheet != NULL) {
            g_poke_cache_vita[i].last_used_frame = ++g_render_frame_count_vita;
            return i;
        }
    }

    int best_slot = 0;
    uint32_t oldest_time = 0xFFFFFFFF;
    for (int i = 0; i < POKE_CACHE_SIZE; i++) {
        if (g_poke_cache_vita[i].sheet == NULL) {
            best_slot = i;
            break;
        }
        if (g_poke_cache_vita[i].last_used_frame < oldest_time) {
            oldest_time = g_poke_cache_vita[i].last_used_frame;
            best_slot = i;
        }
    }

    if (g_poke_cache_vita[best_slot].sheet != NULL) {
        vita2d_free_texture(g_poke_cache_vita[best_slot].sheet);
        g_poke_cache_vita[best_slot].sheet = NULL;
    }

    char path[256];
    snprintf(path, sizeof(path), "mon/%d.png", dex);
    vita2d_texture *tex = load_asset_png(path);
    if (!tex) {
        tex = load_asset_png("mon/1.png");
    }

    g_poke_cache_vita[best_slot].dex = dex;
    g_poke_cache_vita[best_slot].sheet = tex;
    g_poke_cache_vita[best_slot].last_used_frame = ++g_render_frame_count_vita;

    int num_frames = (dex > 0 && dex < 1025) ? POKEMON_SPRITE_META[dex].n : 4;
    if (num_frames <= 0) num_frames = 4;
    int sh = tex ? vita2d_texture_get_height(tex) : 0;
    int sw = tex ? vita2d_texture_get_width(tex) : 0;
    int fh = (sh > 0) ? (sh / 8) : 40;
    int fw = (sw > 0) ? (sw / num_frames) : 32;

    g_poke_cache_vita[best_slot].fw = fw;
    g_poke_cache_vita[best_slot].fh = fh;
    g_poke_cache_vita[best_slot].num_frames = num_frames;
    g_poke_cache_vita[best_slot].ax = fw / 2;
    g_poke_cache_vita[best_slot].ay = fh - 4;

    return best_slot;
}

void render_draw_pokemon_sprite(int dex, int anim_type, float x, float y, int frame, int dir, float scale, int flip_h, uint32_t tint) {
    scale *= 2.0f;
    int slot = get_or_load_poke_cache_vita(dex);
    PokeCacheEntryVita *entry = &g_poke_cache_vita[slot];
    vita2d_texture *tex = entry->sheet;

    int fw = entry->fw;
    int fh = entry->fh;
    int ax = entry->ax;
    int max_frames = entry->num_frames;
    int cur_frame = (max_frames > 0) ? (((frame % max_frames) + max_frames) % max_frames) : 0;
    int cur_dir = ((dir % 8) + 8) % 8;
    int may = (dex > 0 && dex < 1025) ? POKEMON_SPRITE_META[dex].ay[cur_dir] : 0;
    int ay = (may > 0 && may <= fh) ? may : fh - 4;

    if (!tex || tex->w <= 0 || tex->h <= 0) return;
    float u0 = (float)(cur_frame * fw) / (float)tex->w, v0 = (float)(cur_dir * fh) / (float)tex->h;
    float u1 = u0 + (float)fw / (float)tex->w,           v1 = v0 + (float)fh / (float)tex->h;
    if (flip_h) { float t = u0; u0 = u1; u1 = t; }
    uint32_t col = (tint != 0 && tint != 0xFFFFFFFFu) ? tint : 0xFFFFFFFFu;

    if (g_world) {
        float gx = x, gy = y, lift = 0.0f;
        bool skip_sh = g_sh_skip;
        g_sh_skip = false;
        if (g_sh_valid && fabsf(x - g_sh_x) < 1.5f) {
            float dy = g_sh_y - y;
            if (dy > -2.0f && dy < 70.0f) { gx = g_sh_x; gy = g_sh_y; lift = dy > 3.0f ? dy : 0.0f; }
        }
        g_sh_valid = false;
        float R[3], D[3];
        bb_basis(R, D);

        float v1_foot = v0 + (float)ay / (float)tex->h;
        if (!skip_sh) {
            g3d_layer(G3D_DECAL);
            g3d_shadow_plane(tex, gx + g_wcx, gy + g_wcy, lift,
                             -(float)ax * scale, -(float)ay * scale, (float)(fw - ax) * scale, 1.0f * scale,
                             0.0f, u0, v0, u1, v1_foot, RGBA8(16, 26, 40, 94));
        }
        float c[3] = { gx + g_wcx, GROUND_Y + lift - 1.0f * scale, gy + g_wcy };
        g3d_layer(G3D_SPRITE);
        g3d_plane(tex, c, R, D, -(float)ax * scale, -(float)ay * scale, (float)(fw - ax) * scale, 0.0f, 0, u0, v0, u1, v1_foot, col);
        return;
    }
    float draw_x = x - (float)ax * scale;
    float draw_y = y - (float)ay * scale + 1.0f * scale;
    float c2[3] = { draw_x, draw_y, 0 };
    static const float UX_[3] = {1, 0, 0}, UY_[3] = {0, 1, 0};
    g3d_layer(G3D_UI);
    g3d_plane(tex, c2, UX_, UY_, 0, 0, (float)fw * scale, (float)fh * scale, 0, u0, v0, u1, v1, col);
}

#define FXG_MAX 320
static struct { char name[48]; vita2d_texture *t[12]; int n; } g_fxg[FXG_MAX];
static int g_fxg_n = 0;
static vita2d_texture *fx_generic(const char *shape, int f) {
    if (!shape || shape[0] == '\0') return NULL;
    for (int i = 0; i < g_fxg_n; i++)
        if (!strcmp(g_fxg[i].name, shape)) return g_fxg[i].n ? g_fxg[i].t[f % g_fxg[i].n] : NULL;
    if (g_fxg_n >= FXG_MAX || strlen(shape) > 44) return NULL;
    int idx = g_fxg_n++;
    snprintf(g_fxg[idx].name, sizeof(g_fxg[idx].name), "%s", shape);
    g_fxg[idx].n = 0;
    char path[128];
    for (int k = 0; k < 12; k++) {
        snprintf(path, sizeof(path), "fx/%s_%d.png", shape, k);
        vita2d_texture *t = load_asset_png(path);
        if (!t) break;
        g_fxg[idx].t[g_fxg[idx].n++] = t;
    }
    if (g_fxg[idx].n == 0) {
        snprintf(path, sizeof(path), "fx/%s.png", shape);
        vita2d_texture *t = load_asset_png(path);
        if (t) { g_fxg[idx].t[0] = t; g_fxg[idx].n = 1; }
    }
    return g_fxg[idx].n ? g_fxg[idx].t[f % g_fxg[idx].n] : NULL;
}

static vita2d_texture *resolve_builtin_vfx(const char *shape, int f) {
    if (strstr(shape, "_start") || strstr(shape, "_mid") || strstr(shape, "_end")) return NULL;

    typedef struct { const char *name; vita2d_texture **arr; int cnt; bool prefix; } VfxSlotMap;
    static const VfxSlotMap MAP[] = {
        { "palm",              &g_fx_palm,             1, false },
        { "note_normal",       &g_fx_note_normal,      1, false },
        { "hammer_ice",        &g_fx_hammer_ice,       1, false },
        { "feather_flying",    &g_fx_feather_flying,   1, false },
        { "xmark_poison",      &g_fx_xmark_poison,     1, false },
        { "claw_steel",        &g_fx_claw_steel,       1, false },
        { "drop",              &g_fx_drop,             1, false },
        { "leaf",              &g_fx_leaf,             1, false },
        { "shard",             &g_fx_shard,            1, false },
        { "streak",            &g_fx_streak,           1, false },
        { "crescent",          &g_fx_crescent,         1, false },
        { "fist",              &g_fx_fist,             1, false },
        { "vine",              &g_fx_vine,             1, false },
        { "solar",             &g_fx_solar,            1, false },
        { "hydro",             &g_fx_hydro,            1, false },
        { "ghost",             &g_fx_ghost,            1, false },
        { "psybeam",           &g_fx_psybeam,          1, false },
        { "psystrike",         &g_fx_psystrike,        1, false },
        { "fire",              g_fx_fire,              3, false },
        { "bubble",            g_fx_bubble,            2, false },
        { "bolt",              g_fx_bolt,              2, false },
        { "rock",              g_fx_rock,              3, false },
        { "star",              g_fx_star,              2, false },
        { "wind",              g_fx_wind,              2, false },
        { "ring",              g_fx_ring,              2, false },
        { "impact",            g_fx_impact,            2, false },
        { "ball",              g_fx_ball,              2, false },
        { "shadow",            g_fx_shadow,            3, false },
        { "spin",              g_fx_spin,              4, false },
        { "shock",             g_fx_shock,             3, false },
        { "strike",            g_fx_strike,            3, false },
        { "slam",              g_fx_slam,              2, false },
        { "cannon_steel",      g_fx_cannon_steel,      2, true  },
        { "cannon_electric_",  g_fx_cannon_electric,   2, true  },
        { "cannon_fairy_",     g_fx_cannon_fairy,      2, true  },
        { "cannon_normal_",    g_fx_cannon_normal,     2, true  },
        { "pincer_bug_",       g_fx_pincer_bug,        2, true  },
        { "pincer_water_",     g_fx_pincer_water,      2, true  },
        { "kick_fighting_",    g_fx_kick_fighting,     2, true  },
        { "jaws_dark_",        g_fx_jaws_dark,         2, true  },
        { "slashx_normal_",    g_fx_slashx_normal,     2, true  },
        { "slashx_fairy_",     g_fx_slashx_fairy,      2, true  },
        { "slashx_fire_",      g_fx_slashx_fire,       2, true  },
        { "slashx_fighting_",  g_fx_slashx_fighting,   2, true  },
        { "slashx_steel_",     g_fx_slashx_steel,      2, true  },
        { "slashx_poison_",    g_fx_slashx_poison,     2, true  },
    };
    for (size_t i = 0; i < sizeof(MAP) / sizeof(MAP[0]); i++) {
        bool hit = MAP[i].prefix
            ? (strncmp(shape, MAP[i].name, strlen(MAP[i].name)) == 0)
            : (strcmp(shape, MAP[i].name) == 0);
        if (hit) return MAP[i].arr[f % MAP[i].cnt];
    }
    return NULL;
}

void render_preload_pokemon(int dex) {
    get_or_load_poke_cache_vita(dex);
}

void render_preload_vfx(const char *shape) {
    if (!shape || shape[0] == '\0') return;
    if (!resolve_builtin_vfx(shape, 0)) {
        fx_generic(shape, 0);
    }
}

static float g_vfx_src_x = 0.0f, g_vfx_src_y = 0.0f;
static bool  g_vfx_has_src = false;
void render_set_vfx_origin(float ox, float oy, bool active) {
    g_vfx_src_x = ox; g_vfx_src_y = oy; g_vfx_has_src = active;
}

static inline bool is_frisbee_circle_skill(const char *shape) {
    if (!shape || shape[0] == '\0') return false;
    if (!strcmp(shape, "discharge") || !strcmp(shape, "electro_ball") ||
        !strcmp(shape, "shock") || !strcmp(shape, "thunder_wave") ||
        !strcmp(shape, "aura_sphere") ||
        !strcmp(shape, "shadow_ball") || !strcmp(shape, "shadow") ||
        !strcmp(shape, "ice_ball") || !strcmp(shape, "pyro_ball") ||
        !strcmp(shape, "mist_ball") || !strcmp(shape, "weather_ball") ||
        !strcmp(shape, "energy_ball") || !strcmp(shape, "sub_orb") ||
        !strcmp(shape, "dragon_energy") || !strcmp(shape, "outrage") ||
        !strcmp(shape, "ring") || !strcmp(shape, "spin") ||
        !strcmp(shape, "confusion") || !strcmp(shape, "psychic") ||
        !strcmp(shape, "star") || !strcmp(shape, "razor_leaf") ||
        !strcmp(shape, "petal_blizzard") || !strcmp(shape, "boss_steel") ||
        !strcmp(shape, "boss_rock")) {
        return true;
    }
    return false;
}

void render_draw_vfx_ex(const char *shape, float x, float y, float angle, float scale, int frame,
                        uint8_t alpha, uint32_t tint, bool is_boss_attack, float fwd_x, float fwd_y) {
    if (!shape || shape[0] == '\0') return;
    int f = frame < 0 ? 0 : frame;
    vita2d_texture *tex = resolve_builtin_vfx(shape, f);

    if (!tex) tex = fx_generic(shape, f);

    bool upright_locked =
        (!strncmp(shape, "boom_", 5) ||
         !strcmp(shape, "flame") || !strcmp(shape, "wisp") || !strcmp(shape, "will_o_wisp") ||
         !strcmp(shape, "flamepatch") || !strcmp(shape, "magma_storm") ||
         !strcmp(shape, "meteor_hit") || !strcmp(shape, "meteor_mash") ||
         !strcmp(shape, "strike") || !strcmp(shape, "thunder") ||
         !strcmp(shape, "crabhammer") || !strcmp(shape, "bulldoze") || !strcmp(shape, "mud") ||
         !strcmp(shape, "body_slam") || !strcmp(shape, "stomp_impact") || !strcmp(shape, "slam") || !strcmp(shape, "slamdust") ||
         !strcmp(shape, "crack") || !strcmp(shape, "magnitude") ||
         !strcmp(shape, "spikes") || !strcmp(shape, "toxic_spikes") || !strcmp(shape, "toxspikes") || !strcmp(shape, "stealthrock") ||
         !strcmp(shape, "night_shade") || !strcmp(shape, "rock") || !strcmp(shape, "rock_slide") || !strcmp(shape, "rock_throw") ||
         !strcmp(shape, "egg") || !strcmp(shape, "egg_bomb") || !strcmp(shape, "impact") ||
         !strcmp(shape, "sucker_punch") || !strcmp(shape, "fist") || !strcmp(shape, "palm") ||
         !strcmp(shape, "bite") || !strcmp(shape, "crunch") || !strcmp(shape, "pay_day") ||
         !strcmp(shape, "dazzling_gleam") || !strcmp(shape, "fairy_wind") || !strcmp(shape, "play_rough") ||
         !strcmp(shape, "feather_flying") || !strcmp(shape, "leech_seed") || !strcmp(shape, "hail") ||
         !strcmp(shape, "grass_knot"));
    if (upright_locked) {
        angle = 0.0f;
    }
    if (tex) {
        float hw = (float)tex->w * 0.5f, hh = (float)tex->h * 0.5f;
        uint32_t col = (tint != 0 && tint != 0xFFFFFFFFu)
            ? ((tint & 0x00FFFFFFu) | ((uint32_t)alpha << 24))
            : RGBA8(255, 255, 255, alpha);
        if (g_world) {

            bool is_slash = !strcmp(shape, "slash_crescent") || strncmp(shape, "slashx_", 7) == 0 ||
                            !strcmp(shape, "air_slash") || !strcmp(shape, "air_cutter") ||
                            !strcmp(shape, "psycut") || !strcmp(shape, "sacred_sword") ||
                            !strcmp(shape, "behemoth_blade") || !strcmp(shape, "metal_claw") ||
                            !strcmp(shape, "cross_poison") || !strcmp(shape, "wing_attack") ||
                            !strcmp(shape, "play_rough") || !strcmp(shape, "crescent");
            if (is_slash) {
                float eye[3]; g3d_eye(eye);
                float rtx = g_wcx + SCREEN_W * 0.5f, rtz = g_wcy + SCREEN_H * 0.5f;
                float rty = wld_lift_fn(rtx, rtz);
                float ref_d = sqrtf((eye[0] - rtx) * (eye[0] - rtx) + (eye[1] - rty) * (eye[1] - rty) + (eye[2] - rtz) * (eye[2] - rtz));
                float swx = x + g_wcx, swz = y + g_wcy;
                float swy = wld_lift_fn(swx, swz);
                float cur_d = sqrtf((eye[0] - swx) * (eye[0] - swx) + (eye[1] - swy) * (eye[1] - swy) + (eye[2] - swz) * (eye[2] - swz));
                if (cur_d > 1.0f && ref_d > 1.0f) {
                    float dm = powf(ref_d / cur_d, 1.35f);
                    if (dm < 0.50f) dm = 0.50f;
                    if (dm > 1.75f) dm = 1.75f;
                    scale *= dm;
                }
            }

            bool is_ground = !strcmp(shape, "crack") || !strcmp(shape, "bulldoze") || !strcmp(shape, "scorching_sands") || !strcmp(shape, "slamdust") ||
                             !strcmp(shape, "web") ||
                             !strcmp(shape, "eweb") || !strcmp(shape, "flame_wheel") ||
                             !strcmp(shape, "toxic") || !strcmp(shape, "toxic_wheel") ||
                             !strcmp(shape, "whirlpool") || !strcmp(shape, "aqua_ring") ||
                             !strcmp(shape, "magnitude") || !strcmp(shape, "stomp_impact") ||
                             !strcmp(shape, "body_slam") || !strcmp(shape, "slam") ||
                             !strcmp(shape, "stomp") || !strcmp(shape, "grass_knot") ||
                             !strcmp(shape, "night_shade_aoe");
            if (!strcmp(shape, "flame_wheel") || !strcmp(shape, "toxic") || !strcmp(shape, "toxic_wheel")) scale *= 1.35f;
            if (!strcmp(shape, "aqua_ring")) scale *= 1.65f;
            if (!strcmp(shape, "metal_claw")) {
                scale *= 1.0f;
            }

            if (!is_ground) {
                if (is_frisbee_circle_skill(shape)) {
                    bool is_ground_ring = !strcmp(shape, "discharge") || !strcmp(shape, "shock") ||
                                          !strcmp(shape, "thunder_wave") || !strcmp(shape, "ring") ||
                                          !strcmp(shape, "spin");
                    float lift = is_ground_ring ? 1.0f : (12.0f * scale + 4.0f);
                    uint8_t sh_a = (uint8_t)(alpha * (is_boss_attack ? 0.42f : 0.36f));
                    if (sh_a < 45 && alpha > 40) sh_a = 45;

                    g3d_layer(G3D_DECAL);
                    g3d_shadow_oval(x + g_wcx, y + g_wcy, hw * scale * 0.72f, hh * scale * 0.52f, RGBA8(16, 26, 40, sh_a));

                    g3d_layer(G3D_FX);
                    float c[3] = { x + g_wcx, GROUND_Y + lift, y + g_wcy };
                    g3d_plane(tex, c, AX_X, AX_Z, -hw * scale, -hh * scale, hw * scale, hh * scale, angle, 0, 0, 1, 1, col);
                    return;
                }

                bool is_beam = (strstr(shape, "beam") != NULL || strstr(shape, "cannon") != NULL ||
                                strstr(shape, "pump") != NULL || !strcmp(shape, "octazooka") ||
                                !strcmp(shape, "dragon_rage") || !strcmp(shape, "psystrike") ||
                                !strcmp(shape, "thunderbolt") || !strcmp(shape, "hex") ||
                                strstr(shape, "_start") != NULL || strstr(shape, "_mid") != NULL ||
                                strstr(shape, "_end") != NULL);
                if (is_beam) {
                    float wx_seg = x + g_wcx, wz_seg = y + g_wcy;

                    float base_l = g_vfx_has_src ? wld_lift_fn(g_vfx_src_x, g_vfx_src_y) : wld_lift_fn(wx_seg, wz_seg);
                    float seg_l  = wld_lift_fn(wx_seg, wz_seg);
                    float beam_y = base_l + 18.0f;
                    float h_above = beam_y - seg_l;
                    if (h_above < 0.0f) h_above = 0.0f;
                    float z_sh_on = g3d_get_lift_z_shift() ? 1.0f : 0.0f;
                    float sh[3] = { wx_seg + h_above * 0.48f, seg_l + 0.65f, wz_seg + seg_l * z_sh_on + h_above * 0.56f };
                    uint8_t sh_a = (uint8_t)(alpha * (is_boss_attack ? 0.36f : 0.28f));
                    if (sh_a < 56 && alpha > 40) sh_a = 56;
                    g3d_layer(G3D_DECAL);
                    g3d_plane_abs(g3d_white(), sh, AX_X, AX_Z, -hw * scale, -hh * scale * 0.42f, hw * scale + 0.5f, hh * scale * 0.42f, angle, 0, 0, 1, 1, RGBA8(16, 26, 40, sh_a));

                    g3d_layer(G3D_FX);
                    float c[3] = { wx_seg, beam_y, wz_seg + base_l * z_sh_on };

                    float ca = cosf(angle), sa = sinf(angle);
                    float A[3] = { ca, 0.0f, sa };
                    float f_dir[3];
                    g3d_view_dir(f_dir);
                    float to_cam[3] = { -f_dir[0], -f_dir[1], -f_dir[2] };

                    float B[3] = {
                        A[1] * to_cam[2] - A[2] * to_cam[1],
                        A[2] * to_cam[0] - A[0] * to_cam[2],
                        A[0] * to_cam[1] - A[1] * to_cam[0]
                    };
                    float magB = sqrtf(B[0]*B[0] + B[1]*B[1] + B[2]*B[2]);
                    if (magB > 0.001f) { B[0] /= magB; B[1] /= magB; B[2] /= magB; }
                    else { B[0] = -sa; B[1] = 0.0f; B[2] = ca; }

                    g3d_plane_abs(tex, c, A, B, -hw * scale, -hh * scale, hw * scale + 0.5f, hh * scale, 0.0f, 0, 0, 1, 1, col);
                    return;
                }
                float R[3], D[3];
                bb_basis(R, D);
                bool anchor_ground_impact = !strcmp(shape, "meteor_hit") || !strcmp(shape, "meteor_mash") ||
                                            !strcmp(shape, "strike") || !strcmp(shape, "boom_electric") ||
                                            !strcmp(shape, "thunder_shock") || !strcmp(shape, "thunderbolt") || !strcmp(shape, "thunder") ||
                                            !strcmp(shape, "hex") || !strcmp(shape, "flame") || !strcmp(shape, "wisp") ||
                                            !strcmp(shape, "flamepatch") || !strcmp(shape, "magma_storm") ||
                                            !strcmp(shape, "night_shade") || !strcmp(shape, "rock") || !strcmp(shape, "rock_slide") ||
                                            !strcmp(shape, "leech_seed");
                float c[3];
                uint8_t sh_a = (uint8_t)(alpha * (is_boss_attack ? 0.42f : 0.36f));
                if (sh_a < 72 && alpha > 40) sh_a = 72;
                g3d_layer(G3D_DECAL);
                if (anchor_ground_impact) {

                    float h_off = hh * scale;
                    g3d_shadow_oval(x + g_wcx, y + g_wcy, hw * scale * 0.72f, hw * scale * 0.46f, RGBA8(16, 26, 40, sh_a));
                    c[0] = x + g_wcx - D[0] * h_off;
                    c[1] = GROUND_Y + 0.5f - D[1] * h_off;
                    c[2] = y + g_wcy - D[2] * h_off;
                } else if (!strncmp(shape, "boom_", 5)) {

                    float h_off = hh * scale * 0.90f;
                    g3d_shadow_oval(x + g_wcx, y + g_wcy, hw * scale * 0.68f, hw * scale * 0.44f, RGBA8(16, 26, 40, sh_a));
                    c[0] = x + g_wcx - D[0] * h_off;
                    c[1] = GROUND_Y + 0.5f - D[1] * h_off;
                    c[2] = y + g_wcy - D[2] * h_off;
                } else {
                    float lift = 14.0f * scale + 6.0f;
                    if (!strcmp(shape, "toxic_spikes") || !strcmp(shape, "toxspikes") || !strcmp(shape, "spikes")) {
                        lift = hh * scale * 0.85f;
                    } else if (!strcmp(shape, "stealthrock")) {
                        lift = hh * scale * 0.90f + 2.0f;
                    } else {

                        float max_half = (hw > hh ? hw : hh) * scale;
                        float min_lift = max_half * 0.92f + 4.0f;
                        if (lift < min_lift) lift = min_lift;
                    }
                    g3d_shadow_oval(x + g_wcx + lift * 0.36f, y + g_wcy + lift * 0.72f, hw * scale * 0.65f, hw * scale * 0.42f, RGBA8(16, 26, 40, sh_a));
                    c[0] = x + g_wcx - D[0] * lift;
                    c[1] = GROUND_Y + 0.5f - D[1] * lift;
                    c[2] = y + g_wcy - D[2] * lift;
                }
                g3d_layer(G3D_FX);

                float cam_angle = upright_locked ? 0.0f : (angle + g_cam.yaw * 0.01745329252f);
                g3d_plane(tex, c, R, D, -hw * scale, -hh * scale, hw * scale, hh * scale, cam_angle, 0, 0, 1, 1, col);
            } else {
                g3d_layer(G3D_DECAL);
                float z_off = 0.0f;
                float ly0 = -hh * scale, ly1 = hh * scale;
                if (!strcmp(shape, "crack") || !strcmp(shape, "magnitude")) {

                    ly0 = -1.40f * hh * scale;
                    ly1 =  0.60f * hh * scale;
                } else if (!strcmp(shape, "body_slam") || !strcmp(shape, "stomp_impact") || !strcmp(shape, "slam")) {
                    z_off = 10.0f * scale;
                }
                if (upright_locked) {

                    float Rg[3], upg[3], norg[3];
                    g3d_axes(Rg, upg, norg);
                    float Sg[3] = { -norg[0], 0.0f, -norg[2] };
                    float c[3] = { x + g_wcx + Sg[0] * z_off, GROUND_Y + 0.75f, y + g_wcy + Sg[2] * z_off };
                    g3d_plane(tex, c, Rg, Sg, -hw * scale, ly0, hw * scale, ly1, 0.0f, 0, 0, 1, 1, col);
                } else {
                    float c[3] = { x + g_wcx, GROUND_Y + 0.75f, y + g_wcy + z_off };
                    g3d_plane(tex, c, AX_X, AX_Z, -hw * scale, ly0, hw * scale, ly1, angle, 0, 0, 1, 1, col);
                }
            }
            return;
        }
        g3d_ui_tex_rot(tex, x, y, scale, scale, angle, hw, hh, col);
    } else {
        render_draw_circle(x, y, 6.0f * scale, RGBA8(255, 220, 50, 255));
    }
}

void render_draw_vfx_alpha(const char *shape, float x, float y, float angle, float scale, int frame, uint8_t alpha) {
    render_draw_vfx_ex(shape, x, y, angle, scale, frame, alpha, 0xFFFFFFFFu, false, 0.0f, 0.0f);
}

void render_draw_vfx(const char *shape, float x, float y, float angle, float scale, int frame) {
    render_draw_vfx_alpha(shape, x, y, angle, scale, frame, 255);
}

void render_draw_whip(float px, float py, float angle, float progress, float radius, float arc, float seed) {
    fx_draw_whip_ex(px, py, angle, progress, radius, arc, seed, 0);
}
void render_draw_whip_ex(float px, float py, float angle, float progress, float radius, float arc, float seed, int whip_mode) {
    fx_draw_whip_ex(px, py, angle, progress, radius, arc, seed, whip_mode);
}
void render_draw_slash_arc(float px, float py, float angle, float progress, float radius, uint32_t col) {
    if (g_world) {
        float eye[3]; g3d_eye(eye);
        float rtx = g_wcx + SCREEN_W * 0.5f, rtz = g_wcy + SCREEN_H * 0.5f;
        float rty = wld_lift_fn(rtx, rtz);
        float ref_d = sqrtf((eye[0] - rtx) * (eye[0] - rtx) + (eye[1] - rty) * (eye[1] - rty) + (eye[2] - rtz) * (eye[2] - rtz));
        float swx = px + g_wcx, swz = py + g_wcy;
        float swy = wld_lift_fn(swx, swz);
        float cur_d = sqrtf((eye[0] - swx) * (eye[0] - swx) + (eye[1] - swy) * (eye[1] - swy) + (eye[2] - swz) * (eye[2] - swz));
        if (cur_d > 1.0f && ref_d > 1.0f) {
            float dm = powf(ref_d / cur_d, 1.35f);
            if (dm < 0.50f) dm = 0.50f;
            if (dm > 1.75f) dm = 1.75f;
            radius *= dm;
        }
    }
    fx_draw_slash_arc(px, py, angle, progress, radius, col);
}

void render_draw_egg(float x, float y, float u) {
    if (!g_world) { fx_draw_egg(x, y, u); return; }
    float ph = u < 0.0f ? 0.0f : (u > 0.999f ? 0.999f : u); ph *= 3.0f;
    int bn = (int)ph; float t = ph - (float)bn;
    float hop = sinf(3.14159265f * t) * (20.0f + 5.0f * (float)bn);
    float sc = 0.85f + 0.30f * ((float)bn + t);
    float gx = x + g_wcx, gz = y + g_wcy;
    if (!g_tex_egg_bomb) g_tex_egg_bomb = load_asset_png("fx/egg.png");
    if (g_tex_egg_bomb) {
        float R[3], D[3];
        bb_basis(R, D);
        float ew = (float)g_tex_egg_bomb->w * 0.5f * sc;
        float eh = (float)g_tex_egg_bomb->h * 0.5f * sc;
        float lift = hop + eh * 0.8f;
        g3d_layer(G3D_DECAL);
        g3d_shadow_oval(gx + lift * 0.36f, gz + lift * 0.72f, ew * 0.80f, eh * 0.52f, RGBA8(16, 26, 40, 92));
        g3d_layer(G3D_FX);
        float c[3] = { gx - D[0] * lift, GROUND_Y + 0.5f - D[1] * lift, gz - D[2] * lift };
        g3d_plane(g_tex_egg_bomb, c, R, D, -ew, -eh, ew, eh, u * 14.0f, 0, 0, 1, 1, 0xFFFFFFFFu);
    }
}

void render_draw_ui_texture(int tex_id, float x, float y, float scale) {
    vita2d_texture *tex = NULL;
    if (tex_id == UI_TEX_LOGO) tex = g_tex_logo;
    else if (tex_id == UI_TEX_BALL) tex = g_tex_ball;
    else if (tex_id == UI_TEX_BUSH) tex = g_tex_bush;
    else if (tex_id == UI_TEX_XP_BIG) tex = g_tex_xp_big;
    else if (tex_id == UI_TEX_COIN) tex = g_tex_coin;
    else if (tex_id == UI_TEX_POTION) tex = g_tex_potion;
    else if (tex_id == UI_TEX_CHEST_CLOSED) tex = g_prop_chest_closed;
    else if (tex_id == UI_TEX_CHEST_OPEN) tex = g_prop_chest_open;
    else if (tex_id == UI_TEX_BOMB) tex = g_tex_bomb;
    else if (tex_id == UI_TEX_MAGNET) tex = g_tex_magnet;
    else if (tex_id == UI_TEX_POUCH) tex = g_tex_pouch;
    else if (tex_id == UI_TEX_BERRY_TREE) tex = g_tex_berry_tree;
    else if (tex_id == UI_TEX_XP_GEM_1) tex = g_tex_xp_gem_1;
    else if (tex_id == UI_TEX_XP_GEM_2) tex = g_tex_xp_gem_2;
    else if (tex_id == UI_TEX_XP_MEGA) tex = g_tex_xp_mega;
    else if (tex_id == UI_TEX_RARE_CANDY) tex = g_tex_rare_candy;
    else if (tex_id == UI_TEX_GACHA_TICKET) tex = g_tex_gacha_ticket;
    else if (tex_id == UI_TEX_REVIVE) tex = g_tex_revive;
    else if (tex_id == UI_TEX_PEDESTAL) tex = g_tex_pedestal;
    else if (tex_id == UI_TEX_GACHA_BALL) tex = g_tex_gacha_ball;

    if (tex) {
        vita2d_draw_texture_scale(tex, x, y, scale, scale);
    }
}

static vita2d_texture *ui_tex_by_id(int tex_id) {
    if (tex_id == UI_TEX_LOGO) return g_tex_logo;
    if (tex_id == UI_TEX_BALL) return g_tex_ball;
    if (tex_id == UI_TEX_BUSH) return g_tex_bush;
    if (tex_id == UI_TEX_XP_BIG) return g_tex_xp_big;
    if (tex_id == UI_TEX_COIN) return g_tex_coin;
    if (tex_id == UI_TEX_POTION) return g_tex_potion;
    if (tex_id == UI_TEX_CHEST_CLOSED) return g_prop_chest_closed;
    if (tex_id == UI_TEX_CHEST_OPEN) return g_prop_chest_open;
    if (tex_id == UI_TEX_BOMB) return g_tex_bomb;
    if (tex_id == UI_TEX_MAGNET) return g_tex_magnet;
    if (tex_id == UI_TEX_POUCH) return g_tex_pouch;
    if (tex_id == UI_TEX_BERRY_TREE) return g_tex_berry_tree;
    if (tex_id == UI_TEX_XP_GEM_1) return g_tex_xp_gem_1;
    if (tex_id == UI_TEX_XP_GEM_2) return g_tex_xp_gem_2;
    if (tex_id == UI_TEX_XP_MEGA) return g_tex_xp_mega;
    if (tex_id == UI_TEX_RARE_CANDY) return g_tex_rare_candy;
    if (tex_id == UI_TEX_GACHA_TICKET) return g_tex_gacha_ticket;
    if (tex_id == UI_TEX_REVIVE) return g_tex_revive;
    if (tex_id == UI_TEX_PEDESTAL) return g_tex_pedestal;
    if (tex_id == UI_TEX_GACHA_BALL) return g_tex_gacha_ball;
    return NULL;
}

void render_draw_ui_texture_rot(int tex_id, float cx, float cy, float scale, float angle) {
    vita2d_texture *tex = ui_tex_by_id(tex_id);
    if (!tex) return;
    g3d_ui_tex_rot(tex, cx, cy, scale, scale, angle, (float)tex->w * 0.5f, (float)tex->h * 0.5f, 0xFFFFFFFFu);
}

void render_draw_loading_overlay(const char *msg, float anim_phase, uint8_t bg_alpha) {
    (void)anim_phase;
    render_draw_rect(0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H, RGBA8(12, 18, 34, bg_alpha));
    if (msg && msg[0]) {
        render_draw_text_centered(SCREEN_W * 0.5f, SCREEN_H * 0.5f - 10.0f, 1.65f, RGBA8(255, 225, 80, 255), msg);
    }
}

void render_draw_ui_texture_centered(int tex_id, float cx, float cy, float scale) {
    vita2d_texture *tex = ui_tex_by_id(tex_id);

    if (tex && g_world) {
        float w = (float)tex->w * scale, h = (float)tex->h * scale;
        float gx = cx, gy = cy, lift = 0.0f;
        if (g_sh_valid && fabsf(cx - g_sh_x) < 1.5f) {
            float dy = g_sh_y - cy;
            if (dy > -2.0f && dy < 70.0f) { gx = g_sh_x; gy = g_sh_y; lift = dy - h * 0.5f; if (lift < 0.0f) lift = 0.0f; }
        }
        g_sh_valid = false;
        float R[3], D[3];
        bb_basis(R, D);

        g3d_layer(G3D_DECAL);
        g3d_shadow_oval(gx + g_wcx, gy + g_wcy, w * 0.38f, w * 0.24f, RGBA8(16, 26, 40, 92));
        g3d_layer(G3D_SPRITE);
        float c[3] = { gx + g_wcx, GROUND_Y + lift, gy + g_wcy };
        g3d_plane(tex, c, R, D, -w * 0.5f, -h, w * 0.5f, 0, 0, 0, 0, 1, 1, 0xFFFFFFFFu);
        return;
    }
    if (tex) {
        float w = (float)vita2d_texture_get_width(tex);
        float h = (float)vita2d_texture_get_height(tex);
        float x = cx - w * scale * 0.5f;
        float y = cy - h * scale * 0.5f;
        vita2d_draw_texture_scale(tex, x, y, scale, scale);
    }
}

void render_draw_type_badge(const char *type_name, float x, float y, float scale) {
    if (!type_name) return;
    for (int i = 0; i < 18; i++) {
        if (strcasecmp(type_name, TYPE_NAMES[i]) == 0) {
            if (g_tex_types[i]) {
                vita2d_draw_texture_scale(g_tex_types[i], x, y, scale, scale);
            }
            return;
        }
    }
}

void render_draw_world_prop(int prop_id, float x, float y, float scale) {
    vita2d_texture *tex = NULL;
    float anchor_y = 0.5f;
    float pscale = scale;

    switch (prop_id) {
        case PROP_TREE_A:        tex = g_prop_tree_a; anchor_y = 0.88f; pscale = scale * 1.50f; break;
        case PROP_TREE_B:        tex = g_prop_tree_b; anchor_y = 0.88f; pscale = scale * 1.50f; break;
        case PROP_TREE_AUTUMN:   tex = g_prop_tree_autumn; anchor_y = 0.88f; pscale = scale * 1.50f; break;
        case PROP_TREE_DARK:     tex = g_prop_tree_dark; anchor_y = 0.90f; pscale = scale * 1.55f; break;
        case PROP_ROCK_A:        tex = g_prop_rock_a; anchor_y = 0.90f; pscale = scale * 1.00f; break;
        case PROP_ROCK_B:        tex = g_prop_rock_b; anchor_y = 0.88f; pscale = scale * 1.05f; break;
        case PROP_ROCK_ICE:      tex = g_prop_rock_b; anchor_y = 0.88f; pscale = scale * 1.05f; break;
        case PROP_CACTUS_A:
        case PROP_CACTUS_B: {
            uint32_t ch = (uint32_t)((int)(x + g_wcx) * 73856093u) ^ (uint32_t)((int)(y + g_wcy) * 19349663u);
            tex = g_prop_cactus[ch % 6u];
            if (!tex) tex = (prop_id == PROP_CACTUS_A) ? g_prop_cactus_a : g_prop_cactus_b;
            anchor_y = 0.94f;
            pscale = scale * 1.15f;
            break;
        }
        case PROP_TUFT_A:        tex = g_prop_tuft_a; anchor_y = 0.50f; pscale = scale * 0.85f; break;
        case PROP_TUFT_B:        tex = g_prop_tuft_b; anchor_y = 0.50f; pscale = scale * 0.85f; break;
        case PROP_FLOWER_PINK:   tex = g_prop_flower_pink; anchor_y = 0.50f; pscale = scale * 0.85f; break;
        case PROP_FLOWER_YELLOW: tex = g_prop_flower_yellow; anchor_y = 0.50f; pscale = scale * 0.85f; break;
        case PROP_PEBBLE:        tex = g_prop_pebble; anchor_y = 0.85f; pscale = scale * 1.00f; break;
        case PROP_CHEST:         tex = g_prop_chest_closed; anchor_y = 0.80f; pscale = scale * 1.00f; break;
        case PROP_WATER_POOL:    tex = g_prop_pool[0]; anchor_y = 0.50f; pscale = scale * 1.00f; break;
        case PROP_BERRY_TREE:    tex = g_tex_berry_tree; anchor_y = 0.90f; pscale = scale * 0.95f; break;
        default:                 tex = g_tex_bush; anchor_y = 0.90f; pscale = scale * 1.00f; break;
    }

    if (g_world && tex && prop_id <= PROP_TREE_DARK) {
        float wx = x + g_wcx, wz = y + g_wcy;
        float w = (float)tex->w * pscale, h = (float)tex->h * pscale;
        float R[3], D[3];
        bb_basis(R, D);

        bool is_border_tree = (wx < 170.0f || wx > WORLD_W - 170.0f || wz < 170.0f || wz > WORLD_H - 170.0f);
        if (is_border_tree) {
            g3d_layer(G3D_DECAL);
            g3d_shadow_oval_sun(wx, wz, w * 0.36f, w * 0.30f, h * 0.65f, RGBA8(16, 26, 40, 92));
            g3d_layer(G3D_SPRITE);
            float cc[3] = { wx, GROUND_Y - 3.5f, wz };
            g3d_plane(tex, cc, R, D, -w * 0.5f, -h, w * 0.5f, 0.0f, 0.0f, 0, 0, 1, 1, 0xFFFFFFFFu);
            return;
        }

        float base_y = wld_lift_fn(wx, wz);
        float zs = g3d_get_lift_z_shift() ? 1.0f : 0.0f;
        float bz_w = wz + base_y * zs;
        float rad = (prop_id == PROP_TREE_B ? 27.0f : 31.0f) * scale;
        float h3d = (prop_id == PROP_TREE_DARK ? 86.0f : 78.0f) * scale;
        float tu0 = (prop_id == PROP_TREE_B) ? 0.38f : 0.40f;
        float tu1 = (prop_id == PROP_TREE_B) ? 0.58f : 0.56f;
        float tv0 = (prop_id == PROP_TREE_B) ? 0.87f : 0.85f;
        float tv1 = (prop_id == PROP_TREE_B) ? 0.93f : 0.92f;
        float v_cut = (prop_id == PROP_TREE_B) ? 0.83f : 0.80f;
        float vh = h * v_cut;
        uint32_t brk_tint = RGBA8(255, 232, 206, 255);
        uint32_t seed = (uint32_t)((int)(wx * 0.25f) * 73856093u) ^ (uint32_t)((int)(wz * 0.25f) * 19349663u);

        g3d_layer(G3D_DECAL);
        g3d_shadow_oval_sun(wx, wz, w * 0.38f, w * 0.32f, h * 0.72f, RGBA8(16, 26, 40, 92));
        g3d_tree_base_3d(tex, wx, base_y, bz_w, rad, h3d, tu0, tv0, tu1, tv1, brk_tint, seed);
        g3d_layer(G3D_SPRITE);
        float cc[3] = { wx, GROUND_Y + h * (1.0f - v_cut) * 0.44f, wz };
        g3d_plane(tex, cc, R, D, -w * 0.5f, -vh, w * 0.5f, 0.0f, 0.0f, 0, 0, 1, v_cut, 0xFFFFFFFFu);
        return;
    }

    if (g_world && (prop_id == PROP_ROCK_A || prop_id == PROP_ROCK_B || prop_id == PROP_ROCK_ICE || prop_id == PROP_PEBBLE)) {

        const g3d_tex *rt = g_tex_emerald_tiles ? g_tex_emerald_tiles : g3d_white();
        float iw = (rt && rt->w > 0) ? (1.0f / (float)rt->w) : 1.0f;
        float ih = (rt && rt->h > 0) ? (1.0f / (float)rt->h) : 1.0f;
        int top_tid = 44, side_tid = 47, crystal = 0;
        float rx = 16.5f * pscale, rz = 14.5f * pscale, rh = 19.0f * pscale;
        uint32_t tint = RGBA8(250, 236, 214, 255);
        if (prop_id == PROP_ROCK_A) {
            top_tid = 44; side_tid = 114;
            rx = 19.5f * pscale; rz = 17.0f * pscale; rh = 23.0f * pscale;
            tint = RGBA8(204, 212, 224, 255);
        } else if (prop_id == PROP_ROCK_B) {
            top_tid = 44; side_tid = 47;
            rx = 16.5f * pscale; rz = 14.5f * pscale; rh = 19.5f * pscale;
            if (g_current_stage == STAGE_CAVE_GRANITE)      tint = RGBA8(228, 198, 152, 255);
            else if (g_current_stage == STAGE_CAVE_CHIMNEY) tint = RGBA8(232, 166, 134, 255);
            else                                            tint = RGBA8(255, 238, 216, 255);
        } else if (prop_id == PROP_ROCK_ICE) {
            top_tid = 44; side_tid = 113; crystal = 1;
            rx = 15.5f * pscale; rz = 14.0f * pscale; rh = 25.0f * pscale;
            tint = RGBA8(152, 224, 255, 255);
        } else {
            top_tid = 44; side_tid = 47;
            rx = 7.5f * pscale; rz = 6.5f * pscale; rh = 7.5f * pscale;
            tint = RGBA8(226, 216, 200, 255);
        }
        float tu0 = 0.0f, tv0 = 0.0f, tu1 = 1.0f, tv1 = 1.0f;
        float su0 = 0.0f, sv0 = 0.0f, su1 = 1.0f, sv1 = 1.0f;
        if (g_tex_emerald_tiles) {
            float tu = (float)((top_tid & 15) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
            float tv = (float)((top_tid >> 4) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
            float su = (float)((side_tid & 15) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
            float sv = (float)((side_tid >> 4) * EMERALD_TILE_CELL + EMERALD_TILE_PAD);
            tu0 = tu * iw; tv0 = tv * ih; tu1 = (tu + 16.0f) * iw; tv1 = (tv + 16.0f) * ih;
            su0 = su * iw; sv0 = sv * ih; su1 = (su + 16.0f) * iw; sv1 = (sv + 16.0f) * ih;
        }
        float wx = x + g_wcx, wz = y + g_wcy;
        uint32_t seed = (uint32_t)((int)(wx * 0.25f) * 73856093u) ^ (uint32_t)((int)(wz * 0.25f) * 19349663u);
        g3d_layer(G3D_DECAL);
        g3d_shadow_oval_sun(wx, wz, rx * 0.95f, rz * 0.95f, rh, RGBA8(16, 26, 40, 92));
        g3d_layer(G3D_SPRITE);
        g3d_rock_3d(rt, wx, wz, rx, rz, rh, tu0, tv0, tu1, tv1, su0, sv0, su1, sv1, tint, seed, crystal);
        return;
    }

    if (tex && g_world) {
        float w = (float)tex->w * pscale, h = (float)tex->h * pscale;
        float c[3] = { x + g_wcx, GROUND_Y, y + g_wcy };
        if (prop_id == PROP_WATER_POOL) {
            g3d_layer(G3D_DECAL);
            g3d_plane(tex, c, AX_X, AX_Z, -w * 0.5f, -h * 0.5f, w * 0.5f, h * 0.5f, 0, 0, 0, 1, 1, 0xFFFFFFFFu);
        } else {
            const float T = 32.0f;
            float R[3], D[3];
            bb_basis(R, D);
            if (prop_id == PROP_TUFT_A || prop_id == PROP_TUFT_B) {

                float hw = 0.52f * T, gh = 0.85f * T;
                float cc[3] = { c[0], -1.0f, c[2] };
                g3d_layer(G3D_SPRITE);
                if (g_wld.treetex) {
                    g3d_plane(g_wld.treetex, cc, R, D, -hw, -gh, hw, 0, 0, 80.0f / 128.0f, 32.0f / 128.0f, 96.0f / 128.0f, 48.0f / 128.0f, 0xFFFFFFFFu);
                } else {
                    g3d_plane(tex, cc, R, D, -hw, -gh, hw, 0, 0, 0, 0, 1, 1, 0xFFFFFFFFu);
                }
            } else if (prop_id == PROP_FLOWER_PINK || prop_id == PROP_FLOWER_YELLOW) {
                uint32_t hh = (uint32_t)(int)(x * 0.03125f) * 73856093u ^ (uint32_t)(int)(y * 0.03125f) * 19349663u;
                float L = 0.65f * T, hw = 0.36f * T;
                g3d_layer(G3D_SPRITE);
                for (int i = 0; i < 2; i++) {
                    float ox = (-0.20f + 0.40f * (float)((hh >> (i * 8)) & 0xFFu) / 255.0f) * T;
                    float oz = (-0.10f + 0.45f * (float)((hh >> (7 + i * 9)) & 0xFFu) / 255.0f) * T;
                    float cc[3] = { c[0] + ox, -1.0f, c[2] + oz };
                    g3d_plane(tex, cc, R, D, -hw, -L, hw, 0, 0, 0, 0, 1, 1, 0xFFFFFFFFu);
                }
            } else {

                float v1 = 1.0f;
                if (prop_id == PROP_BERRY_TREE && tex->h > 4)      v1 = (float)(tex->h - 4) / (float)tex->h;
                else if (prop_id > PROP_BERRY_TREE && tex->h > 2)  v1 = (float)(tex->h - 2) / (float)tex->h;
                float vh = h * v1;
                float sink = (prop_id <= PROP_TREE_DARK) ? 3.5f : ((prop_id == PROP_BERRY_TREE) ? 3.0f : 2.0f);

                g3d_layer(G3D_DECAL);
                g3d_shadow_oval_sun(c[0], c[2], w * 0.32f, w * 0.26f, h * 0.55f, RGBA8(16, 26, 40, 90));
                g3d_layer(G3D_SPRITE);
                float cc[3] = { c[0], GROUND_Y - sink, c[2] };
                g3d_plane(tex, cc, R, D, -w * 0.5f, -vh, w * 0.5f, 0, 0, 0, 0, 1, v1, 0xFFFFFFFFu);
            }
        }
        return;
    }
    if (tex) {
        float dx = x - (float)vita2d_texture_get_width(tex) * 0.5f * pscale;
        float dy = y - (float)vita2d_texture_get_height(tex) * anchor_y * pscale;
        vita2d_draw_texture_scale(tex, dx, dy, pscale, pscale);
    }
}

static int match_ps_btn_token(const char *s, int *out_slot) {
    if (strncmp(s, "[Sq]", 4) == 0 || strncmp(s, "(Sq)", 4) == 0) { *out_slot = 96; return 4; }
    if (s[0] == '[') {
        if (strncmp(s, "[Left/Right]", 12) == 0)   { *out_slot = 200; return 12; }
        if (strncmp(s, "[Left / Right]", 14) == 0) { *out_slot = 200; return 14; }
        if (strncmp(s, "[Up/Down]", 9) == 0)       { *out_slot = 201; return 9; }
        if (strncmp(s, "[Up / Down]", 11) == 0)    { *out_slot = 201; return 11; }
        if (strncmp(s, "[L/R]", 5) == 0)           { *out_slot = 202; return 5; }
        if (strncmp(s, "[L / R]", 7) == 0)         { *out_slot = 202; return 7; }
        if (strncmp(s, "[Left]", 6) == 0)          { *out_slot = 109; return 6; }
        if (strncmp(s, "[Right]", 7) == 0)         { *out_slot = 110; return 7; }
        if (strncmp(s, "[Up]", 4) == 0)            { *out_slot = 111; return 4; }
        if (strncmp(s, "[DDown]", 7) == 0)         { *out_slot = 112; return 7; }
        if (strncmp(s, "[L]", 3) == 0)             { *out_slot = 113; return 3; }
        if (strncmp(s, "[R]", 3) == 0)             { *out_slot = 114; return 3; }
        if (strncmp(s, "[Tri]", 5) == 0)       { *out_slot = 95; return 5; }
        if (strncmp(s, "[Triangle]", 10) == 0) { *out_slot = 95; return 10; }
        if (strncmp(s, "[Sqr]", 5) == 0)       { *out_slot = 96; return 5; }
        if (strncmp(s, "[Square]", 8) == 0)    { *out_slot = 96; return 8; }
        if (strncmp(s, "[O]", 3) == 0)         { *out_slot = 97; return 3; }
        if (strncmp(s, "[Circle]", 8) == 0)    { *out_slot = 97; return 8; }
        if (strncmp(s, "[X]", 3) == 0)         { *out_slot = 98; return 3; }
        if (strncmp(s, "[Cross]", 7) == 0)     { *out_slot = 98; return 7; }
        if (strncmp(s, "[Coin]", 6) == 0)      { *out_slot = 99; return 6; }
        if (strncmp(s, "[Ball]", 6) == 0)      { *out_slot = 100; return 6; }
        if (strncmp(s, "[Ticket]", 8) == 0)    { *out_slot = 101; return 8; }
        if (strncmp(s, "[TKT]", 5) == 0)       { *out_slot = 101; return 5; }
        if (strncmp(s, "[Trophy]", 8) == 0)    { *out_slot = 102; return 8; }
        if (strncmp(s, "[Star]", 6) == 0)      { *out_slot = 103; return 6; }
        if (strncmp(s, "[Wrench]", 8) == 0)    { *out_slot = 104; return 8; }
        if (strncmp(s, "[Arrow]", 7) == 0)     { *out_slot = 105; return 7; }
        if (strncmp(s, "[Down]", 6) == 0)      { *out_slot = 106; return 6; }
        if (strncmp(s, "[Cursor]", 8) == 0)    { *out_slot = 107; return 8; }
        if (strncmp(s, "[SettingsX]", 11) == 0){ *out_slot = 108; return 11; }
    } else if (s[0] == '(') {
        if (strncmp(s, "(Tri)", 5) == 0)       { *out_slot = 95; return 5; }
        if (strncmp(s, "(Triangle)", 10) == 0) { *out_slot = 95; return 10; }
        if (strncmp(s, "(Sqr)", 5) == 0)       { *out_slot = 96; return 5; }
        if (strncmp(s, "(Square)", 8) == 0)    { *out_slot = 96; return 8; }
        if (strncmp(s, "(O)", 3) == 0)         { *out_slot = 97; return 3; }
        if (strncmp(s, "(Circle)", 8) == 0)    { *out_slot = 97; return 8; }
        if (strncmp(s, "(X)", 3) == 0)         { *out_slot = 98; return 3; }
        if (strncmp(s, "(Cross)", 7) == 0)     { *out_slot = 98; return 7; }
        if (strncmp(s, "(Coin)", 6) == 0)      { *out_slot = 99; return 6; }
        if (strncmp(s, "(Ball)", 6) == 0)      { *out_slot = 100; return 6; }
        if (strncmp(s, "(Ticket)", 8) == 0)    { *out_slot = 101; return 8; }
        if (strncmp(s, "(TKT)", 5) == 0)       { *out_slot = 101; return 5; }
        if (strncmp(s, "(Trophy)", 8) == 0)    { *out_slot = 102; return 8; }
        if (strncmp(s, "(Star)", 6) == 0)      { *out_slot = 103; return 6; }
        if (strncmp(s, "(Wrench)", 8) == 0)    { *out_slot = 104; return 8; }
        if (strncmp(s, "(Arrow)", 7) == 0)     { *out_slot = 105; return 7; }
        if (strncmp(s, "(Down)", 6) == 0)      { *out_slot = 106; return 6; }
    }
    return 0;
}

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

typedef struct { stbtt_fontinfo info; unsigned char *data; int ok; } TtFont;
typedef struct { uint32_t key; short u, v, w, h, xo, yo; float adv; unsigned char used; } GlyphE;
#define GA_W 1024
#define GA_H 1024
#define GC_N 8192
static TtFont g_tt[3];
static g3d_tex *g_gl_atlas = NULL;
static int g_ga_x = 0, g_ga_y = 0, g_ga_rowh = 0, g_tt_init_done = 0;
static GlyphE g_gc[GC_N];

static void tt_init(void) {
    if (g_tt_init_done) return;
    g_tt_init_done = 1;
    static const char *rel[3] = { "fonts/Chakra Petch/ChakraPetch-SemiBold.ttf", "fonts/m6x11/m6x11plus.ttf", "fonts/m6x11/m6x11plus.ttf" };
    for (int i = 0; i < 3; i++) {
        char path[512];
        if (!resolve_asset_path(rel[i], path, sizeof(path))) continue;
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        unsigned char *buf = (n > 0) ? (unsigned char *)malloc((size_t)n) : NULL;
        if (buf && fread(buf, 1, (size_t)n, f) == (size_t)n &&
            stbtt_InitFont(&g_tt[i].info, buf, stbtt_GetFontOffsetForIndex(buf, 0))) {
            g_tt[i].data = buf; g_tt[i].ok = 1;
        } else if (buf) free(buf);
        fclose(f);
    }
    if (!g_tt[2].ok) g_tt[2] = g_tt[1];
    if (g_tt[0].ok && g_tt[1].ok) g_gl_atlas = g3d_tex_create_rgba(GA_W, GA_H);
}

static GlyphE *tt_glyph(int font, float em, int outline, int cp) {
    int emq = (int)(em * 4.0f + 0.5f);
    uint32_t key = ((uint32_t)(cp & 0xFFFF) << 16) | ((uint32_t)(emq & 0x1FFF) << 3) | ((uint32_t)font << 1) | (uint32_t)outline;
    uint32_t h = (key * 2654435761u) >> 19;
    for (int n = 0; n < GC_N; n++) {
        GlyphE *e = &g_gc[(h + (uint32_t)n) & (GC_N - 1)];
        if (e->used) { if (e->key == key) return e; continue; }
        TtFont *tf = &g_tt[font];
        float sc = stbtt_ScaleForMappingEmToPixels(&tf->info, (float)emq / 4.0f);
        int adv, lsb, x0, y0, x1, y1;
        stbtt_GetCodepointHMetrics(&tf->info, cp, &adv, &lsb);
        stbtt_GetCodepointBitmapBox(&tf->info, cp, sc, sc, &x0, &y0, &x1, &y1);
        e->used = 1; e->key = key; e->adv = (float)adv * sc; e->w = e->h = 0;
        int bw = x1 - x0, bh = y1 - y0;
        if (bw <= 0 || bh <= 0) return e;
        int pad = outline ? 1 : 0, tw = bw + 2 * pad, th = bh + 2 * pad;
        if (g_ga_x + tw + 1 > GA_W) { g_ga_x = 0; g_ga_y += g_ga_rowh + 1; g_ga_rowh = 0; }
        if (g_ga_y + th > GA_H) {
            memset(g_gc, 0, sizeof(g_gc)); g_ga_x = g_ga_y = g_ga_rowh = 0;
            return NULL;
        }
        unsigned char *bm = (unsigned char *)calloc((size_t)bw * bh, 1);
        unsigned char *px = (unsigned char *)calloc((size_t)tw * th, 4);
        if (!bm || !px) { free(bm); free(px); return NULL; }
        stbtt_MakeCodepointBitmap(&tf->info, bm, bw, bh, bw, sc, sc, cp);
        for (int yy = 0; yy < th; yy++) for (int xx = 0; xx < tw; xx++) {
            int bx = xx - pad, by = yy - pad, a = 0, d = 0;
            if (bx >= 0 && bx < bw && by >= 0 && by < bh) a = bm[by * bw + bx];
            if (outline) {
                for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) {
                    int qx = bx + ox, qy = by + oy;
                    if (qx >= 0 && qx < bw && qy >= 0 && qy < bh && bm[qy * bw + qx] > d) d = bm[qy * bw + qx];
                }
            } else d = a;
            unsigned char *o = &px[(yy * tw + xx) * 4];
            float t = (float)a / 255.0f;
            o[0] = (unsigned char)(outline ? 24.0f + (255.0f - 24.0f) * t : 255.0f);
            o[1] = (unsigned char)(outline ? 30.0f + (255.0f - 30.0f) * t : 255.0f);
            o[2] = (unsigned char)(outline ? 38.0f + (255.0f - 38.0f) * t : 255.0f);
            o[3] = (unsigned char)d;
        }
        g3d_tex_update(g_gl_atlas, g_ga_x, g_ga_y, tw, th, px);
        e->u = (short)g_ga_x; e->v = (short)g_ga_y; e->w = (short)tw; e->h = (short)th;
        e->xo = (short)(x0 - pad); e->yo = (short)(y0 - pad);
        g_ga_x += tw + 1; if (th > g_ga_rowh) g_ga_rowh = th;
        free(bm); free(px);
        return e;
    }
    return NULL;
}

static float tt_run(int mode, float x, float y, float scale, uint32_t color, const char *text, int draw) {
    if (!text) return 0.0f;
    float s = scale * 0.70f, em, cx = x;
    int outline = 0, font = (mode == 0) ? 0 : (mode == 1 ? 2 : 1);
    if (mode == 0) em = 21.0f * s;
    else {
        float px = ((mode == 1) ? 28.0f : 32.0f) * s, n = px / 18.0f, r = floorf(n + 0.5f);
        em = (r >= 1.0f && fabsf(n - r) < 0.18f) ? r * 18.0f : px;
        outline = 0;
    }

    em = floorf(em * 0.5f + 0.5f) * 2.0f;
    if (em < 8.0f) em = 8.0f;
    float base_y = floorf(y + ((mode == 0) ? 20.0f : 21.0f) * s + 0.5f);
    for (const char *p = text; *p; ) {
        if (p[0] == '\\' && (p[1] == '[' || p[1] == '(')) {
            p++;
            GlyphE *ge = tt_glyph(font, em, outline, (unsigned char)*p);
            if (ge) {
                if (draw && ge->w > 0)
                    vita2d_draw_texture_tint_part_scale(g_gl_atlas, floorf(cx + 0.5f) + (float)ge->xo, base_y + (float)ge->yo, (float)ge->u, (float)ge->v, (float)ge->w, (float)ge->h, 1.0f, 1.0f, color);
                cx += ge->adv;
            }
            p++;
            continue;
        }
        int slot = -1, tl = match_ps_btn_token(p, &slot);
        if (tl > 0) {
            if (slot == 200 || slot == 201 || slot == 202) {
                int s_a = (slot == 200) ? 109 : (slot == 201 ? 111 : 113);
                int s_b = (slot == 200) ? 110 : (slot == 201 ? 112 : 114);
                if (draw && g_ui_icons) {
                    float sz = 19.0f * s;
                    float isc = sz / 64.0f;
                    float iy = y + 12.0f * s - sz * 0.5f;
                    vita2d_draw_texture_tint_part_scale(g_ui_icons, cx, iy, (float)((s_a - 95) * 64), 0.0f, 64.0f, 64.0f, isc, isc, 0xFFFFFFFFu);
                    vita2d_draw_texture_tint_part_scale(g_ui_icons, cx + 17.5f * s, iy, (float)((s_b - 95) * 64), 0.0f, 64.0f, 64.0f, isc, isc, 0xFFFFFFFFu);
                }
                cx += 35.0f * s; p += tl; continue;
            }
            if (draw) {
                if (g_ui_icons && slot >= 95 && slot <= 114) {
                    float sz = 18.0f * s;
                    float isc = sz / 64.0f;
                    float iy = y + 12.0f * s - sz * 0.5f;
                    vita2d_draw_texture_tint_part_scale(g_ui_icons, cx, iy, (float)((slot - 95) * 64), 0.0f, 64.0f, 64.0f, isc, isc, 0xFFFFFFFFu);
                } else if (g_font_atlas) {
                    vita2d_draw_texture_tint_part_scale(g_font_atlas, cx, y - 1.0f * s, (float)((slot % 16) * 18), (float)((slot / 16) * 26), 18.0f, 26.0f, s, s, 0xFFFFFFFFu);
                }
            }
            cx += 16.0f * s; p += tl; continue;
        }
        unsigned char c0 = (unsigned char)*p; int cp = c0;
        if (c0 >= 0xC0 && (p[1] & 0xC0) == 0x80) {
            if (c0 < 0xE0) { cp = ((c0 & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
            else if ((p[2] & 0xC0) == 0x80) { cp = ((c0 & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
            else p++;
        } else p++;
        GlyphE *g = tt_glyph(font, em, outline, cp);
        if (!g) continue;
        if (draw && g->w > 0)
            vita2d_draw_texture_tint_part_scale(g_gl_atlas, floorf(cx + 0.5f) + (float)g->xo, base_y + (float)g->yo, (float)g->u, (float)g->v, (float)g->w, (float)g->h, 1.0f, 1.0f, color);
        cx += g->adv;
    }
    return cx - x;
}

void render_draw_text_pix(float x, float y, float scale, uint32_t color, const char *text) {
    tt_init();
    if (g_gl_atlas) tt_run(2, x, y, scale, color, text, 1);
}

void render_preload_hud_fonts(void) {
    static int done = 0;
    if (done) return;
    tt_init();
    if (!g_gl_atlas) return;
    done = 1;
    static const char *chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz:/+-().!%,'$?[]<>| ";
    static const char *nums  = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!+-:/.% ";

    static const float s0[] = { 0.82f, 0.96f, 1.10f, 1.24f, 1.38f, 1.52f, 1.66f, 1.80f };
    static const float s1[] = { 0.72f, 0.82f, 0.92f, 1.02f, 1.12f, 1.25f };
    static const float s2[] = { 0.72f, 0.82f, 0.92f, 1.02f, 1.12f, 1.22f };
    for (size_t i = 0; i < sizeof(s0)/sizeof(s0[0]); i++) tt_run(0, 0, 0, s0[i], 0xFFFFFFFFu, chars, 0);
    for (size_t i = 0; i < sizeof(s1)/sizeof(s1[0]); i++) tt_run(1, 0, 0, s1[i], 0xFFFFFFFFu, nums, 0);
    for (size_t i = 0; i < sizeof(s2)/sizeof(s2[0]); i++) tt_run(2, 0, 0, s2[i], 0xFFFFFFFFu, chars, 0);
}

static void warmup_grid_tex(const g3d_tex *t, int *slot) {
    if (!t || t->w <= 0 || t->h <= 0) return;
    int idx = (*slot)++;
    float x = (float)((idx % 48) * 20);
    float y = (float)(((idx / 48) % 24) * 20);
    float sw = (t->w > 16) ? 16.0f : (float)t->w;
    float sh = (t->h > 16) ? 16.0f : (float)t->h;
    g3d_ui_tex(t, x, y, 0.0f, 0.0f, sw, sh, 1.0f, 1.0f, 0xFFFFFFFFu);
}

void render_warmup_match(float spawn_x, float spawn_y) {
    g3d_cache_invalidate();
    float cam_x = spawn_x - SCREEN_W * 0.5f;
    float cam_y = spawn_y - SCREEN_H * 0.5f;

    render_start_frame();
    render_clear(RGBA8(12, 18, 34, 255));
    render_world_begin(cam_x, cam_y);
    render_draw_ground(cam_x, cam_y);

    g3d_layer(G3D_DECAL);
    g3d_shadow_oval(spawn_x, spawn_y, 18.0f, 12.0f, RGBA8(16, 26, 40, 108));
    g3d_shadow_oval_sun(spawn_x + 32.0f, spawn_y, 24.0f, 20.0f, 48.0f, RGBA8(16, 26, 40, 92));
    render_world_end();

    int slot = 0;
    for (int i = 0; i < POKE_CACHE_SIZE; i++) {
        if (g_poke_cache_vita[i].dex > 0) warmup_grid_tex(g_poke_cache_vita[i].sheet, &slot);
    }

    render_draw_loading_overlay("PREPARING BATTLEFIELD...", 0.35f, 248);
    render_end_frame();

    render_start_frame();
    render_clear(RGBA8(12, 18, 34, 255));
    slot = 0;
    for (int i = 0; i < g_fxg_n; i++) {
        for (int k = 0; k < g_fxg[i].n; k++) warmup_grid_tex(g_fxg[i].t[k], &slot);
    }
    for (int i = 0; i < 3; i++) warmup_grid_tex(g_fx_fire[i], &slot);
    warmup_grid_tex(g_fx_drop, &slot);
    warmup_grid_tex(g_tex_egg_bomb, &slot);
    warmup_grid_tex(g_tex_emerald_tiles, &slot);
    warmup_grid_tex(g_prop_tree_a, &slot);
    warmup_grid_tex(g_prop_tree_b, &slot);
    warmup_grid_tex(g_prop_tree_dark, &slot);
    warmup_grid_tex(g_tex_berry_tree, &slot);
    warmup_grid_tex(g_tex_xp_gem_1, &slot);
    warmup_grid_tex(g_tex_xp_gem_2, &slot);
    warmup_grid_tex(g_tex_xp_big, &slot);
    warmup_grid_tex(g_tex_xp_mega, &slot);
    warmup_grid_tex(g_tex_coin, &slot);
    warmup_grid_tex(g_tex_potion, &slot);
    warmup_grid_tex(g_bld.tex, &slot);
    warmup_grid_tex(g_wld.treetex, &slot);
    warmup_grid_tex(g_gl_atlas, &slot);
    warmup_grid_tex(g_ui_icons, &slot);
    for (int i = 0; i < 18; i++) warmup_grid_tex(g_tex_types[i], &slot);
    for (int i = 0; i < 8; i++) warmup_grid_tex(g_tex_items[i], &slot);
    for (int i = 0; i < TOTAL_STAGES && i < 7; i++) warmup_grid_tex(g_tex_stage_previews[i], &slot);

    render_draw_loading_overlay("PREPARING BATTLEFIELD...", 1.15f, 248);
    render_end_frame();
}
int render_get_text_width_pix(float scale, const char *text) {
    tt_init();
    return g_gl_atlas ? (int)tt_run(2, 0, 0, scale, 0, text, 0) : 0;
}
void render_draw_text_pix_centered(float cx, float y, float scale, uint32_t color, const char *text) {
    render_draw_text_pix(cx - (float)render_get_text_width_pix(scale, text) * 0.5f, y, scale, color, text);
}

void render_draw_text(float x, float y, float scale, uint32_t color, const char *text) {
    tt_init();
    if (g_gl_atlas) { tt_run(0, x, y, scale, color, text, 1); return; }
    if (!text) return;
    int len = (int)strlen(text);
    float cur_x = x;
    float cur_y = y;

    if (g_font_atlas) {
        float s = scale * 0.70f;
        for (int i = 0; i < len; ) {
            int btn_slot = -1;
            int tok_len = match_ps_btn_token(text + i, &btn_slot);
            if (tok_len > 0) {
                int col = btn_slot % 16;
                int row = btn_slot / 16;
                float src_x = (float)(col * 18);
                float src_y = (float)(row * 26);

                vita2d_draw_texture_tint_part_scale(g_font_atlas, cur_x, cur_y - 1.0f * s, src_x, src_y, 18.0f, 26.0f, s, s, 0xFFFFFFFFu);
                cur_x += 16.0f * s;
                i += tok_len;
                continue;
            }
            char c = text[i++];
            if (c < 32 || c > 126) c = '?';
            int idx = c - 32;
            int col = idx % 16;
            int row = idx / 16;
            float src_x = (float)(col * 18);
            float src_y = (float)(row * 26);
            float adv = (float)FONT_ADVANCE[idx] * s;
            vita2d_draw_texture_tint_part_scale(g_font_atlas, cur_x, cur_y, src_x, src_y, 18.0f, 26.0f, s, s, color);
            cur_x += adv;
        }
        return;
    }

    for (int i = 0; i < len; ) {
        int btn_slot = -1;
        int tok_len = match_ps_btn_token(text + i, &btn_slot);
        if (tok_len > 0) {
            float bw = 12.0f * scale;
            float bh = 12.0f * scale;
            vita2d_draw_rectangle(cur_x, cur_y, bw, bh, RGBA8(28, 34, 48, 245));
            if (btn_slot == 95) {
                render_draw_line(cur_x + 6.0f * scale, cur_y + 2.0f * scale, cur_x + 2.0f * scale, cur_y + 10.0f * scale, RGBA8(0, 230, 150, 255), 1.0f * scale);
                render_draw_line(cur_x + 2.0f * scale, cur_y + 10.0f * scale, cur_x + 10.0f * scale, cur_y + 10.0f * scale, RGBA8(0, 230, 150, 255), 1.0f * scale);
                render_draw_line(cur_x + 10.0f * scale, cur_y + 10.0f * scale, cur_x + 6.0f * scale, cur_y + 2.0f * scale, RGBA8(0, 230, 150, 255), 1.0f * scale);
            } else if (btn_slot == 96) {
                render_draw_rect_outline(cur_x + 2.0f * scale, cur_y + 2.0f * scale, 8.0f * scale, 8.0f * scale, RGBA8(255, 115, 180, 255), 1.0f * scale);
            } else if (btn_slot == 97) {
                render_draw_circle(cur_x + 6.0f * scale, cur_y + 6.0f * scale, 4.0f * scale, RGBA8(255, 75, 75, 255));
                render_draw_circle(cur_x + 6.0f * scale, cur_y + 6.0f * scale, 3.0f * scale, RGBA8(28, 34, 48, 255));
            } else if (btn_slot == 98) {
                render_draw_line(cur_x + 3.0f * scale, cur_y + 3.0f * scale, cur_x + 9.0f * scale, cur_y + 9.0f * scale, RGBA8(90, 150, 255, 255), 1.0f * scale);
                render_draw_line(cur_x + 9.0f * scale, cur_y + 3.0f * scale, cur_x + 3.0f * scale, cur_y + 9.0f * scale, RGBA8(90, 150, 255, 255), 1.0f * scale);
            }
            cur_x += 14.0f * scale;
            i += tok_len;
            continue;
        }
        char c = text[i++];
        if (c < 32 || c > 126) c = '?';
        int glyph_idx = c - 32;

        for (int row = 0; row < 8; row++) {
            uint8_t row_bits = FONT_8X8[glyph_idx][row];
            for (int col = 0; col < 8; col++) {
                if ((row_bits >> col) & 1) {
                    vita2d_draw_rectangle(cur_x + (float)col * scale, cur_y + (float)row * scale, scale, scale, color);
                }
            }
        }
        cur_x += 8.0f * scale;
    }
}

void render_draw_text_shadow(float x, float y, float scale, uint32_t color, const char *text) {
    if (g_font_atlas) {
        render_draw_text(x, y, scale, color, text);
    } else {
        render_draw_text(x + scale, y + scale, scale, RGBA8(10, 10, 10, 220), text);
        render_draw_text(x, y, scale, color, text);
    }
}

void render_draw_text_centered(float cx, float y, float scale, uint32_t color, const char *text) {
    int w = render_get_text_width(scale, text);
    render_draw_text_shadow(cx - ((float)w * 0.5f), y, scale, color, text);
}

int render_get_text_width(float scale, const char *text) {
    tt_init();
    if (g_gl_atlas) return (int)tt_run(0, 0, 0, scale, 0, text, 0);
    if (!text) return 0;
    int len = (int)strlen(text);
    if (g_font_atlas) {
        float s = scale * 0.70f;
        float total_w = 0.0f;
        for (int i = 0; i < len; ) {
            int btn_slot = -1;
            int tok_len = match_ps_btn_token(text + i, &btn_slot);
            if (tok_len > 0) {
                total_w += 16.0f * s;
                i += tok_len;
                continue;
            }
            char c = text[i++];
            if (c < 32 || c > 126) c = '?';
            total_w += (float)FONT_ADVANCE[c - 32] * s;
        }
        return (int)total_w;
    }
    float total_w = 0.0f;
    for (int i = 0; i < len; ) {
        int btn_slot = -1;
        int tok_len = match_ps_btn_token(text + i, &btn_slot);
        if (tok_len > 0) {
            total_w += 14.0f * scale;
            i += tok_len;
            continue;
        }
        total_w += 8.0f * scale;
        i++;
    }
    return (int)total_w;
}

void render_draw_text_8bit(float x, float y, float scale, uint32_t color, const char *text) {
    tt_init();
    if (g_gl_atlas) { tt_run(1, x, y, scale, color, text, 1); return; }
    if (!text) return;
    int len = (int)strlen(text);
    float cur_x = x;
    float cur_y = y;

    if (g_font_atlas) {
        float s = scale * 0.70f;
        for (int i = 0; i < len; ) {
            int btn_slot = -1;
            int tok_len = match_ps_btn_token(text + i, &btn_slot);
            if (tok_len > 0) {
                int col = btn_slot % 16;
                int row = btn_slot / 16;
                float src_x = (float)(col * 18);
                float src_y = (float)(row * 26);
                vita2d_draw_texture_tint_part_scale(g_font_atlas, cur_x, cur_y - 1.0f * s, src_x, src_y, 18.0f, 26.0f, s, s, 0xFFFFFFFFu);
                cur_x += 16.0f * s;
                i += tok_len;
                continue;
            }
            char c = text[i++];
            if (c < 32 || c > 126) c = '?';
            int idx = (c - 32) + 112;
            int col = idx % 16;
            int row = idx / 16;
            float src_x = (float)(col * 18);
            float src_y = (float)(row * 26);
            float adv = (float)FONT_ADVANCE_8BIT[c - 32] * s;
            vita2d_draw_texture_tint_part_scale(g_font_atlas, cur_x, cur_y, src_x, src_y, 18.0f, 26.0f, s, s, color);
            cur_x += adv;
        }
        return;
    }
    render_draw_text(x, y, scale, color, text);
}

int render_get_text_width_8bit(float scale, const char *text) {
    tt_init();
    if (g_gl_atlas) return (int)tt_run(1, 0, 0, scale, 0, text, 0);
    if (!text) return 0;
    int len = (int)strlen(text);
    if (g_font_atlas) {
        float s = scale * 0.70f;
        float total_w = 0.0f;
        for (int i = 0; i < len; ) {
            int btn_slot = -1;
            int tok_len = match_ps_btn_token(text + i, &btn_slot);
            if (tok_len > 0) {
                total_w += 16.0f * s;
                i += tok_len;
                continue;
            }
            char c = text[i++];
            if (c < 32 || c > 126) c = '?';
            total_w += (float)FONT_ADVANCE_8BIT[c - 32] * s;
        }
        return (int)total_w;
    }
    return render_get_text_width(scale, text);
}

void render_draw_text_8bit_centered(float cx, float y, float scale, uint32_t color, const char *text) {
    int w = render_get_text_width_8bit(scale, text);
    render_draw_text_8bit(cx - ((float)w * 0.5f), y, scale, color, text);
}

const uint32_t* render_get_framebuffer(void) {
    return NULL;
}

bool renderer_has_data_assets(void) {
    return g_data_found && (g_tex_ground_grass != NULL || g_font_atlas != NULL);
}

void render_draw_data_missing_screen(void) {
    vita2d_draw_rectangle(0.0f, 0.0f, SCREEN_W, SCREEN_H, RGBA8(12, 18, 48, 255));
    vita2d_draw_rectangle(30.0f, 25.0f, SCREEN_W - 60.0f, SCREEN_H - 50.0f, RGBA8(18, 30, 75, 240));
    vita2d_draw_rectangle(30.0f, 25.0f, SCREEN_W - 60.0f, 55.0f, RGBA8(190, 40, 40, 255));

    render_draw_text(50.0f, 42.0f, 2.0f, RGBA8(255, 255, 255, 255), "POKE-SURVIVORS VITA - DATA ASSETS REQUIRED");

    render_draw_text(50.0f, 105.0f, 1.8f, RGBA8(255, 220, 60, 255), "Game data files were not found on your memory card.");
    render_draw_text(50.0f, 140.0f, 1.5f, RGBA8(220, 230, 255, 255), "Please copy the 'poke-survivors vita' data folder to:");
    render_draw_text(70.0f, 172.0f, 1.8f, RGBA8(80, 255, 120, 255), "ux0:data/poke-survivors vita/");

    render_draw_text(50.0f, 220.0f, 1.5f, RGBA8(255, 215, 0, 255), "Verify this exact folder structure in VitaShell:");
    render_draw_text(70.0f, 255.0f, 1.4f, RGBA8(180, 220, 255, 255), "ux0:data/poke-survivors vita/mon/       (982 Pokemon sprites)");
    render_draw_text(70.0f, 285.0f, 1.4f, RGBA8(180, 220, 255, 255), "ux0:data/poke-survivors vita/ui/        (Font & interface)");
    render_draw_text(70.0f, 315.0f, 1.4f, RGBA8(180, 220, 255, 255), "ux0:data/poke-survivors vita/world/     (Map tiles & props)");
    render_draw_text(70.0f, 345.0f, 1.4f, RGBA8(180, 220, 255, 255), "ux0:data/poke-survivors vita/fx/        (Battle VFX)");

    vita2d_draw_rectangle(30.0f, SCREEN_H - 95.0f, SCREEN_W - 60.0f, 70.0f, RGBA8(10, 18, 45, 255));
    render_draw_text(60.0f, SCREEN_H - 72.0f, 1.6f, RGBA8(255, 220, 50, 255),
                     "[X] Retry Asset Detection          [O] Exit to LiveArea");
}

void render_draw_item_icon(int item_id, float x, float y, float scale) {
    if (item_id >= 0 && item_id < 8 && g_tex_items[item_id]) {
        vita2d_draw_texture_scale(g_tex_items[item_id], x, y, scale, scale);
    }
}

#endif
