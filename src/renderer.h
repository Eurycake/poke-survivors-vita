#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>
#include <stdbool.h>

#define SCREEN_W 960
#define SCREEN_H 544


int render_init(void);
void render_fini(void);


void render_start_frame(void);
void render_end_frame(void);
void render_clear(uint32_t color);


void render_world_begin(float cam_x, float cam_y);
void render_world_end(void);

void render_world_to_screen(float sx, float sy, float height, float *ox, float *oy, float *scale);

void render_camera_adjust(float dt, float yaw_axis, float tilt_axis, float zoom_axis);
void render_camera_next_preset(void);
float render_camera_yaw(void);
float render_get_camera_zoom(void);
bool render_is_world_pos_visible(float wx, float wy, float margin);


void render_draw_rect(float x, float y, float w, float h, uint32_t color);
void render_draw_rect_outline(float x, float y, float w, float h, uint32_t color, float thickness);
void render_draw_circle(float x, float y, float radius, uint32_t color);
void render_draw_circle_ground(float x, float y, float radius, uint32_t color);
void render_draw_line(float x1, float y1, float x2, float y2, uint32_t color, float thickness);
void render_draw_shadow(float x, float y, float radius);
void render_draw_oval_shadow(float x, float y, float radius);
void render_skip_next_shadow(void);


void render_load_all_textures(void);
void render_preload_pokemon(int dex);
void render_preload_vfx(const char *shape);
void render_set_stage(int stage_id);
int render_hoenn_tile_cleared(int tx, int tz);
void render_draw_ground(float cam_x, float cam_y);
void render_draw_emerald_tile(int tile_id, float x, float y);
void render_draw_stage_preview(int stage_id, float x, float y, float w, float h);


void render_draw_pokemon_sprite(int dex, int anim_type, float x, float y, int frame, int dir, float scale, int flip_h, uint32_t tint);


void render_set_vfx_origin(float ox, float oy, bool active);
void render_draw_vfx(const char *shape, float x, float y, float angle, float scale, int frame);
void render_draw_vfx_alpha(const char *shape, float x, float y, float angle, float scale, int frame, uint8_t alpha);
void render_draw_vfx_ex(const char *shape, float x, float y, float angle, float scale, int frame,
                        uint8_t alpha, uint32_t tint, bool is_boss_attack, float fwd_x, float fwd_y);
void render_draw_slash_arc(float px, float py, float angle, float progress, float radius, uint32_t col);
void render_draw_egg(float x, float y, float progress);
void render_draw_whip(float px, float py, float angle, float progress, float radius, float arc, float seed);
void render_draw_whip_ex(float px, float py, float angle, float progress, float radius, float arc, float seed, int whip_mode);


enum {
    UI_TEX_LOGO = 1,
    UI_TEX_BALL = 2,
    UI_TEX_BUSH = 3,
    UI_TEX_XP_BIG = 4,
    UI_TEX_COIN = 5,
    UI_TEX_POTION = 6,
    UI_TEX_CHEST_CLOSED = 7,
    UI_TEX_CHEST_OPEN = 8,
    UI_TEX_BOMB = 9,
    UI_TEX_MAGNET = 10,
    UI_TEX_POUCH = 11,
    UI_TEX_BERRY_TREE = 12,
    UI_TEX_XP_GEM_1 = 13,
    UI_TEX_XP_GEM_2 = 14,
    UI_TEX_XP_MEGA = 15,
    UI_TEX_RARE_CANDY = 16,
    UI_TEX_GACHA_TICKET = 17,
    UI_TEX_REVIVE = 18,
    UI_TEX_PEDESTAL = 19,
    UI_TEX_GACHA_BALL = 22
};

void render_draw_ui_texture(int tex_id, float x, float y, float scale);
void render_draw_ui_texture_centered(int tex_id, float cx, float cy, float scale);
void render_draw_ui_texture_rot(int tex_id, float cx, float cy, float scale, float angle);
void render_draw_loading_overlay(const char *msg, float anim_phase, uint8_t bg_alpha);
void render_draw_rounded_rect(float x, float y, float w, float h, float r, uint32_t color);
void render_preload_hud_fonts(void);
void render_warmup_match(float spawn_x, float spawn_y);
void render_draw_world_prop(int prop_id, float x, float y, float scale);
void render_draw_type_badge(const char *type_name, float x, float y, float scale);
void render_draw_item_icon(int item_id, float x, float y, float scale);


void render_draw_text(float x, float y, float scale, uint32_t color, const char *text);
void render_draw_text_shadow(float x, float y, float scale, uint32_t color, const char *text);
void render_draw_text_centered(float cx, float y, float scale, uint32_t color, const char *text);
int render_get_text_width(float scale, const char *text);

void render_draw_text_8bit(float x, float y, float scale, uint32_t color, const char *text);
void render_draw_text_8bit_centered(float cx, float y, float scale, uint32_t color, const char *text);
int render_get_text_width_8bit(float scale, const char *text);
void render_draw_text_pix(float x, float y, float scale, uint32_t color, const char *text);
void render_draw_text_pix_centered(float cx, float y, float scale, uint32_t color, const char *text);
int render_get_text_width_pix(float scale, const char *text);
const uint32_t* render_get_framebuffer(void);


bool renderer_has_data_assets(void);
void render_draw_data_missing_screen(void);

#endif
