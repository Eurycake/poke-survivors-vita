#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>
#include "assets_data.h"
#include "moves_db.h"

#define SCREEN_W 960
#define SCREEN_H 544

#define MAX_ENEMIES      200
#define MAX_PROJECTILES  160
#define MAX_GEMS          50
#define MAX_PARTICLES    250
#define MAX_POPUPS        60
#define MAX_PICKUPS       40
#define MAX_WANDERERS      6
#define MAX_WHIP_EFFECTS   8
#define MAX_SKILL_LEVEL    8

typedef enum {
    MODE_STANDARD    = 0,
    MODE_TURBO       = 1,
    MODE_BOSSRUSH    = 2,
    MODE_ENDLESS     = 3,
    MODE_COMING_SOON = 4,
    TOTAL_RUN_MODES  = 5
} RunModeEnum;


extern float g_world_w, g_world_h;
#define WORLD_W g_world_w
#define WORLD_H g_world_h
#define MAX_PROPS 3400

typedef enum {
    STATE_MENU_HUB = 0,
    STATE_MENU_MODES,
    STATE_MENU_ROSTER,
    STATE_MENU_SHOP,
    STATE_MENU_GACHA,
    STATE_MENU_MISSIONS,
    STATE_MENU_SETTINGS,
    STATE_PLAYING,
    STATE_LEVEL_UP,
    STATE_CHEST_REWARD,
    STATE_PAUSED,
    STATE_GAME_OVER,
    STATE_VICTORY
} GameStateEnum;

typedef enum {
    REGION_MEADOW = 0,
    REGION_DESERT,
    REGION_FOREST,
    REGION_LAKESIDE,
    REGION_CAVE,
    REGION_MAGMA,
    REGION_ICE,
    REGION_TOWN
} RegionId;

typedef struct {
    float move_x, move_y;
    bool btn_confirm, btn_confirm_pressed;
    bool btn_cancel,  btn_cancel_pressed;
    bool btn_square,  btn_square_pressed;
    bool btn_triangle,btn_triangle_pressed;
    bool btn_pause,   btn_pause_pressed;
    bool btn_skill_pressed[4];
    bool touch_pressed;
    float touch_x, touch_y;
    bool btn_up,      btn_up_pressed;
    bool btn_down,    btn_down_pressed;
    bool btn_left,    btn_left_pressed;
    bool btn_right,   btn_right_pressed;
    bool btn_l_pressed, btn_r_pressed;
    float cam_yaw, cam_tilt, cam_zoom;
    bool cam_preset_pressed;
} InputState;

typedef struct {
    float x, y;
    float vx, vy;
    int move_id;
    int damage;
    float life;
    float max_life;
    float radius;
    int pierce;
    int hits_left;
    float angle;
    float orbit_angle;
    float orbit_radius;
    float orbit_speed;
    float spin_speed;
    bool is_orbit;
    bool active;
    uint32_t color;
    char shape_name[48];
    const char *vfx_shape;
    int anim_frame;
    float draw_scale;
    uint8_t alpha;
    uint8_t hit_cd[MAX_ENEMIES];
    const MoveDef *def;
    int homing, boom;
    bool is_enemy;
    bool is_boss_proj;
    bool tint_by_color;
    float src_x, src_y;
    bool has_src;
} Projectile;

#define MAX_ZONES 48
typedef struct {
    bool active;
    int mode;
    float x, y, t, life, tick, radius, rate, range;
    int dmg, proj;
    const MoveDef *def;
    bool is_decoy;
} Zone;

typedef struct { float timer; int stacks; float per; } BuffState;

typedef struct {
    bool active;
    float x, y;
    float angle;
    float timer;
    float max_life;
    float radius;
    float arc;
    float seed;
    int dmg;
    const MoveDef *def;
    int whip_type;
    bool hit_enemy[MAX_ENEMIES];
} WhipEffect;

typedef struct {
    float x, y;
    int value;
    int exp;
    bool magnetized;
    bool active;
} ExpGem;

typedef struct {
    float x, y;
    int type;
    float timer;
    bool magnetized;
    bool active;
} PickupItem;

typedef enum {
    PROP_TREE_A = 0,
    PROP_TREE_B,
    PROP_TREE_AUTUMN,
    PROP_TREE_DARK,
    PROP_ROCK_A,
    PROP_ROCK_B,
    PROP_TUFT_A,
    PROP_TUFT_B,
    PROP_FLOWER_PINK,
    PROP_FLOWER_YELLOW,
    PROP_PEBBLE,
    PROP_CHEST,
    PROP_WATER_POOL,
    PROP_BERRY_TREE,
    PROP_CACTUS_A,
    PROP_CACTUS_B,
    PROP_LILY,
    PROP_ROCK_ICE,
    TOTAL_PROPS
} PropType;

typedef enum {
    STAGE_ROUTE_29 = 0,
    STAGE_NATIONAL_PARK,
    STAGE_ILEX_FOREST,
    STAGE_TOWN_HOENN,
    STAGE_CAVE_GRANITE,
    STAGE_CAVE_CHIMNEY,
    STAGE_CAVE_ICE,
    TOTAL_STAGES
} StageId;

typedef struct {
    float x, y;
    int type;
    bool active;
} WorldProp;

typedef struct {
    float x, y;
    int amount;
    float timer;
    float max_timer;
    uint32_t color;
    bool active;
} DamagePopup;

typedef struct {
    float x, y;
    float vx, vy;
    float life, max_life;
    float size;
    uint32_t color;
    bool active;
} Particle;

#define MAX_DASH_GHOSTS 32
typedef struct {
    bool active;
    float x, y;
    int dex, anim, frame, dir;
    float life, max_life;
    uint32_t color;
} DashGhost;

typedef struct {
    int id;
    int level;
    float timer;
    float base_cd;
    int dmg;
    int count;
    bool evolved;
    MoveMod mod;
    int burst_left;
    float burst_t;
    int charges;
    int max_charges;
} MoveState;

typedef struct {
    float x, y;
    int type;
    int dex;
    float hp, max_hp;
    float speed;
    float dmg;
    float radius;
    float anim_timer;
    int anim_frame;
    int dir;
    float hit_flash;
    float jump_timer;
    float jump_vx;
    float jump_vy;
    bool active;
    bool is_boss;
    bool is_leeched;
    float slow_t, slow_f, stun_t, stun_cd, dot_t, dot_dps;
    uint32_t debuff_col;
    float attack_cd, attack_timer;
    int elem_type;
    uint8_t attack_phase;
    float alive_time;
    float dmg_window_timer;
    float dmg_window_accum;
    int arceus_plate;
    float plate_timer;
    float contact_timer;
    float boss_dash_cd;
    float boss_dash_timer;
    float boss_dash_vx, boss_dash_vy;
    bool  has_laser_skill;
    float laser_cd;
    float laser_warn_timer;
    float laser_fire_timer;
    float laser_angle;
    float laser_len;
    float laser_half_w;
    const char *name;
} Enemy;

typedef struct {
    float x, y;
    int char_idx;
    float hp, max_hp;
    float speed;
    float atk_mult;
    float def_mult;
    int   armor;
    float regen;
    float magnet_radius;
    float cd_mult;
    float area_mult;
    float proj_speed_mult;
    int   bonus_amount;
    float crit_chance;
    float crit_mult;
    float growth_mult;
    int level;
    int exp, exp_to_next;
    int kills;
    float iframe_timer;

    int anim_state;
    int anim_frame;
    float anim_timer;
    float attack_anim_timer;
    int dir;
    float move_angle;
    bool has_move_angle;
    bool is_moving;
    float jump_timer;
    float jump_vx;
    float jump_vy;

    float lunge_t;
    float lunge_dur;
    float lunge_dx;
    float lunge_dy;
    float lunge_ox;
    float lunge_oy;


    float dash_timer;
    float dash_vx, dash_vy;
    float dash_dmg;
    float dash_knock;
    float dash_hit_rad;
    float dash_grace_iframe;
    float dash_trail_timer;
    float dash_input_buf;
    int   dash_move_id;
    bool  dash_evolved;
    bool  dash_hit_enemy[MAX_ENEMIES];

    int move_count;
    MoveState moves[6];
    int passives[TOTAL_PASSIVES];
} Player;

typedef struct {
    int type;
    int id;
    int cur_level;
    char title[48];
    char desc[80];
} UpgradeCard;

typedef struct {
    float x, y;
    float vx, vy;
    float target_x, target_y;
    int dex;
    int dir;
    int frame;
    int anim_type;
    float speed;
    float anim_timer;
    float wander_timer;
    float wait_timer;
} MenuWanderer;

typedef struct {
    GameStateEnum state;
    float run_timer;
    int run_mode;
    float spawn_timer;
    float wave_difficulty;

    int coins_collected;
    int tickets_collected;
    int rerolls_left;
    int run_rerolls_bought;
    int revives_left;
    float revive_anim_timer;
    int bosses_defeated_in_run;

    Player player;
    Enemy enemies[MAX_ENEMIES];
    Projectile projectiles[MAX_PROJECTILES];
    WhipEffect whips[MAX_WHIP_EFFECTS];
    Zone zones[MAX_ZONES];
    bool skill_pressed[4];
    BuffState buffs[5];
    ExpGem gems[MAX_GEMS];
    PickupItem pickups[MAX_PICKUPS];
    WorldProp props[MAX_PROPS];
    DamagePopup popups[MAX_POPUPS];
    Particle particles[MAX_PARTICLES];
    DashGhost dash_ghosts[MAX_DASH_GHOSTS];

    UpgradeCard cards[3];
    int selected_card;
    int selected_starter;
    int selected_stage;
    float grace_timer;


    int chest_coins;
    UpgradeCard chest_rewards[5];
    int chest_reward_count;
    float chest_timer;


    int hub_selected_idx;
    int modes_selected_idx;
    int roster_selected_idx;
    int roster_tab;
    bool roster_from_modes;
    bool roster_on_tabs;
    int gacha_selected_idx;
    int shop_selected_idx;
    int shop_scroll_offset;
    int missions_selected_idx;
    int settings_selected_idx;
    int pause_selected_idx;
    int pause_sub_menu;


    bool gacha_pulling;
    float gacha_timer;
    int gacha_result_idx;
    bool gacha_is_new;
    int gacha_refund_coins;


    MenuWanderer wanderers[MAX_WANDERERS];
    float menu_cam_x, menu_cam_y;
    int tagline_index;


    float evolution_banner_timer;
    char evolution_old_name[32];
    char evolution_new_name[32];
    int evolution_old_dex;
    int evolution_new_dex;


    float next_boss_timer;
    float next_heal_timer;
    float wave_clock;
    int   endless_cycle;
    bool  swarm_triggered;
    bool  encircle_triggered;
    bool  final_reaper_spawned;
    float reaper_banner_timer;
    int next_boss_dex;
    char next_boss_name[32];
    char current_boss_name[32];
    bool boss_active;
    uint32_t used_boss_mask;
    int last_boss_idx;


    float bgm_rotate_timer;
    int last_battle_bgm;

    float cam_x, cam_y;
    float screen_shake;
} GameState;

extern GameState g_game;


#define RUN_SAVE_MAGIC   0x524E5356
#define RUN_SAVE_VERSION 1

typedef struct {
    uint32_t magic;
    uint32_t version;
    int run_mode;
    int selected_stage;
    float run_timer;
    float wave_clock;
    int endless_cycle;
    bool swarm_triggered;
    bool encircle_triggered;
    bool final_reaper_spawned;
    float spawn_timer;
    int coins_collected;
    int tickets_collected;
    int rerolls_left;
    int run_rerolls_bought;
    int revives_left;
    int bosses_defeated_in_run;
    float next_boss_timer;
    float next_heal_timer;
    int next_boss_dex;
    char next_boss_name[32];
    char current_boss_name[32];
    bool boss_active;
    uint32_t used_boss_mask;
    int last_boss_idx;
    Player player;
    Enemy enemies[MAX_ENEMIES];
    ExpGem gems[MAX_GEMS];
    PickupItem pickups[MAX_PICKUPS];
} RunSaveData;

bool game_has_saved_run(int mode);
bool game_get_saved_run_info(int mode, int *out_char_idx, int *out_level, float *out_time, int *out_stage);
bool game_save_current_run(void);
bool game_resume_saved_run(int mode);
void game_delete_saved_run(int mode);
void game_endless_warp_random_map(void);


void game_init(void);
void game_reset_run(int starter_idx, int mode);
void game_play_random_battle_bgm(void);
void game_update(float dt, const InputState *input);
void game_render(void);

void player_init(Player *p, int starter_idx);
void player_update(Player *p, float dt, const InputState *input);
void player_recalc_stats(Player *p);
int  player_calc_xp_needed(int level);
void player_play_attack_anim(Player *p, int anim_type, float duration);
void player_check_evolution(Player *p);

void enemies_init(void);
void enemies_update(float dt, Player *player);
void enemies_spawn_wave(float game_time, const Player *player);
void enemies_spawn_swarm_event(const Player *player);
void enemies_spawn_encircle_ring(const Player *player);
void enemies_wipe_normal_mobs(void);
float enemies_get_current_spawn_delay(float wave_time);
void enemies_spawn_boss(int boss_cat_idx, const Player *player);
void enemies_spawn_boss_dex(int dex, const Player *player);
void enemies_preload_stage_start(int stage_id, int mode, int first_boss_dex);
int game_pick_next_boss(char *out_name, size_t max_len);

void moves_init(void);
void moves_update(float dt, Player *player);
void moves_preload_move(int move_id);
void moves_preload_assets(void);

void moves_init_state(MoveState *ms, int id);
void moves_apply_level(MoveState *ms);
void moves_apply_evo(MoveState *ms);
float moves_circular_spin_speed(const char *shape);
void game_spawn_damage_popup(float x, float y, int amount, uint32_t color);
void game_spawn_particles(float x, float y, uint32_t color, int count, float speed);
void game_spawn_gem(float x, float y, int value);
void game_spawn_pickup(float x, float y, int type);
void game_trigger_chest_reward(void);
void game_trigger_revive(Player *player);

#endif
