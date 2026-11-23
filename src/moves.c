#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "game.h"
#include "renderer.h"
#include "audio.h"
#include "save_system.h"
#include "terrain.h"
#include "type_chart.h"

#define PI_F 3.14159265f
static inline float frand(void) { return (float)(rand() % 1000) / 1000.0f; }

static float move_dir_angle(int dir) {
    static const float a[8] = {1.570796f,0.785398f,0.0f,5.497787f,4.712389f,3.926991f,3.141593f,2.356194f};
    return a[((dir % 8) + 8) % 8];
}

static Enemy *find_nearest_enemy(float px, float py, float max_dist, const uint8_t *skip) {
    Enemy *best = NULL; float bd = max_dist * max_dist;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *en = &g_game.enemies[i];
        if (!en->active || (skip && skip[i])) continue;
        float dx = en->x - px, dy = en->y - py, d2 = dx*dx + dy*dy;
        if (d2 < bd) { bd = d2; best = en; }
    }
    return best;
}
static int enemy_index(Enemy *e) { return (int)(e - g_game.enemies); }


static void hit_enemy(Enemy *en, float dmg, float dx, float dy, float knock, const MoveDef *d, uint32_t col, bool fx) {
    Player *pl = &g_game.player;
    if (d && d->dream && en->stun_t <= 0.0f) return;

    uint32_t pop_col = col;
    if (d && d->type >= 0 && d->type < 18) {
        int en_dex = (en->dex > 0) ? en->dex : ((en->type >= 0 && en->type < TOTAL_CATALOG_POKEMON) ? POKEMON_CATALOG[en->type].dex : 0);
        int dt1 = -1, dt2 = -1;
        pokemon_dex_get_types(en_dex, &dt1, &dt2);
        float eff = type_effectiveness_dual(d->type, dt1, dt2);
        dmg *= eff;
        if (dmg < 1.0f) dmg = 1.0f;
        if (eff >= 1.45f) pop_col = RGBA8_C(255, 215, 55, 255);
        else if (eff <= 0.75f) pop_col = RGBA8_C(150, 175, 200, 255);
    }
    if (frand() < pl->crit_chance) dmg *= (pl->crit_mult > 1.0f ? pl->crit_mult : 2.0f);


    if (en->is_boss && en->dex == 493 && en->max_hp > 1000.0f) {
        float soft_cap_per_sec = en->max_hp * 0.0048f;
        if (en->dmg_window_accum + dmg > soft_cap_per_sec) {
            float allowed_full = fmaxf(0.0f, soft_cap_per_sec - en->dmg_window_accum);
            float excess = (dmg - allowed_full) * 0.12f;
            dmg = allowed_full + excess;
            if (dmg < 1.0f) dmg = 1.0f;
        }
        en->dmg_window_accum += dmg;
    }

    en->hp -= dmg; en->hit_flash = 0.20f;
    const char *p_passive = (pl->char_idx >= 0 && pl->char_idx < TOTAL_CATALOG_POKEMON && POKEMON_CATALOG[pl->char_idx].passive_desc)
                          ? POKEMON_CATALOG[pl->char_idx].passive_desc : "";

    float life_steal_pct = 0.015f * (float)pl->passives[7] + (strstr(p_passive, "+20% Life Steal") ? 0.06f : 0.0f);
    if (life_steal_pct > 0.0f && dmg > 0.0f && pl->hp > 0.0f) {
        float steal = dmg * life_steal_pct;
        if (steal < 0.08f) steal = 0.08f;
        if (steal > 1.2f) steal = 1.2f;
        pl->hp = fminf(pl->max_hp, pl->hp + steal);
    }
    if (strstr(p_passive, "+30% Knockback")) knock *= 1.30f;
    float l = sqrtf(dx*dx + dy*dy);

    if (knock > 0.0f && l > 0.1f && en->dex != 493) {
        float kb_mult = en->is_boss ? 0.10f : 0.35f;
        terrain_apply_enemy_knockback(g_game.selected_stage, &en->x, &en->y, en->radius, en->is_boss,
                                      dx / l * knock * kb_mult, dy / l * knock * kb_mult);
    }
    if (d) {
        float bm = en->is_boss ? 0.35f : 1.0f;
        bool debuffed = false;
        if (d->slow > 0.0f) {
            en->slow_t = fmaxf(en->slow_t, 3.0f);
            en->slow_f = fmaxf(en->slow_f, d->slow * bm);
            debuffed = true;
        }
        if (d->cc) {
            debuffed = true;

            en->slow_t = fmaxf(en->slow_t, 2.5f);
            en->slow_f = fmaxf(en->slow_f, 0.35f * bm);
            float proc = d->ccp > 0.0f ? fmaxf(d->ccp * 2.2f, 0.35f) : 0.35f;
            if (frand() < proc) {
                if (en->is_boss) {

                    if (en->stun_cd <= 0.0f) {
                        en->stun_t = 0.50f;
                        en->stun_cd = 5.0f;
                    }
                } else {
                    if (d->cc == 1) en->stun_t = fmaxf(en->stun_t, 0.85f);
                    else if (d->cc == 2) { en->stun_t = fmaxf(en->stun_t, 1.25f); en->slow_t = 3.0f; en->slow_f = 0.65f; }
                    else if (d->cc == 3) en->stun_t = fmaxf(en->stun_t, 2.2f);
                    else if (d->cc == 4) en->stun_t = fmaxf(en->stun_t, 1.0f);
                    else { en->slow_t = 3.0f; en->slow_f = 0.55f; }
                }
            }
        }
        if (d->burn || (strstr(p_passive, "& Burn") && frand() < 0.35f)) {
            en->dot_t = fmaxf(en->dot_t, 4.0f);
            en->dot_dps = fmaxf(dmg * 0.30f, 6.0f);
            debuffed = true;
        }
        if (d->poison || (strstr(p_passive, "+20% Poison DoT") && frand() < 0.35f)) {
            float p_mult = (d->poison > 0.0f ? (float)d->poison : 1.0f) * (strstr(p_passive, "+20% Poison DoT") ? 1.20f : 1.0f);
            en->dot_t = fmaxf(en->dot_t, 5.0f);
            en->dot_dps = fmaxf(dmg * 0.25f * p_mult, 6.0f);
            debuffed = true;
        }
        if (strstr(p_passive, "+15% Freeze") && !en->is_boss && frand() < 0.15f) {
            en->stun_t = fmaxf(en->stun_t, 1.10f);
            en->slow_t = fmaxf(en->slow_t, 2.5f);
            en->slow_f = fmaxf(en->slow_f, 0.55f);
            debuffed = true;
        }
        if (strstr(p_passive, "& Stun") && !en->is_boss && frand() < 0.15f) {
            en->stun_t = fmaxf(en->stun_t, 0.75f);
            debuffed = true;
        }
        if (d->dream) {
            float dh = dmg * (strstr(p_passive, "+25% Healing") ? 0.10f : 0.08f);
            if (dh > 3.0f) dh = 3.0f;
            pl->hp = fminf(pl->max_hp, pl->hp + dh);
        }
        if (!strcmp(d->id, "leech-seed")) {
            en->dot_t = fmaxf(en->dot_t, 3.5f);
            en->dot_dps = fmaxf(dmg * 0.22f, 4.0f);
            en->is_leeched = true;
            debuffed = true;
        }
        if (debuffed) {
            en->debuff_col = MOVE_TYPE_COLOR[d->type];
        }
    }
    game_spawn_damage_popup(en->x, en->y - 15.0f, (int)dmg, pop_col);
    if (fx) game_spawn_particles(en->x, en->y, col, 3, 80.0f);
}


float moves_circular_spin_speed(const char *shape) {
    if (!shape || shape[0] == '\0') return 0.0f;
    if (!strcmp(shape, "whirlpool")) return -10.0f;
    if (!strcmp(shape, "gust")) return 10.0f;
    if (!strcmp(shape, "bone") || !strcmp(shape, "shadow_bone") || !strcmp(shape, "stone_axe")) return 14.0f;
    static const char * const CIRCULAR_FX[] = {
        "aura_sphere", "ball", "boss_rock", "boss_steel", "bubble",
        "confusion", "cross_poison", "dark_pulse", "discharge",
        "dragon_dance", "dragon_energy", "electro_ball", "electroweb", "eweb",
        "fire_spin", "flame_wheel", "freeze_dry", "giga_drain",
        "harden", "ice_ball", "ice_burn", "ice_hammer", "infernal_parade",
        "mist_ball", "outrage", "petal_blizzard", "psychic", "psycho_cut",
        "psycut", "psyshock", "pyro_ball", "razor_leaf", "ring",
        "shadow", "shadow_ball", "shock", "snow", "spiky_shield",
        "spin", "star", "sub_orb", "swords_dance", "toxic", "toxic_wheel", "web"
    };
    for (size_t i = 0; i < sizeof(CIRCULAR_FX) / sizeof(CIRCULAR_FX[0]); i++) {
        if (!strcmp(shape, CIRCULAR_FX[i])) return 9.0f;
    }
    return 0.0f;
}


static uint32_t caster_elem_tint_color(int char_idx, int *out_elem) {
    int elem = 0;
    if (char_idx >= 0 && char_idx < TOTAL_CATALOG_POKEMON) {
        const PokemonData *pcat = &POKEMON_CATALOG[char_idx];
        for (int ti = 0; ti < 18; ti++) {
            if (pcat->type1 && !strcmp(pcat->type1, MOVE_TYPE_NAMES[ti])) {
                elem = ti;
                break;
            }
        }
    }
    if (out_elem) *out_elem = elem;
    uint32_t c = MOVE_TYPE_COLOR[elem];
    int r = (int)(c & 0xFFu);
    int g = (int)((c >> 8) & 0xFFu);
    int b = (int)((c >> 16) & 0xFFu);
    r = r + ((255 - r) * 24) / 100;
    g = g + ((255 - g) * 24) / 100;
    b = b + ((255 - b) * 24) / 100;
    return RGBA8_C(r, g, b, 255);
}

static Projectile *spawn_projectile(float x, float y, float vx, float vy, int move_id, int dmg, float life, float radius, int pierce, uint32_t color, const char *shape) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g_game.projectiles[i];
        if (p->active) continue;
        memset(p, 0, sizeof(*p));
        p->active = true; p->x = x; p->y = y; p->vx = vx; p->vy = vy; p->move_id = move_id; p->damage = dmg;
        p->life = life; p->max_life = life; p->radius = radius; p->pierce = pierce; p->hits_left = pierce + 1;
        p->angle = atan2f(vy, vx); p->color = color;
        if (shape) {
            strncpy(p->shape_name, shape, sizeof(p->shape_name) - 1);
            p->shape_name[sizeof(p->shape_name) - 1] = '\0';
            p->vfx_shape = p->shape_name;
            p->spin_speed = moves_circular_spin_speed(shape);
        } else {
            p->shape_name[0] = '\0';
            p->vfx_shape = NULL;
        }
        p->alpha = 255; p->draw_scale = 0.0f;
        return p;
    }
    return NULL;
}
static int proj_index(Projectile *p) { return p ? (int)(p - g_game.projectiles) : -1; }


static Projectile *vis(float x, float y, float ang, float life, const char *shape, uint32_t col, float vx, float vy) {
    Projectile *p = spawn_projectile(x, y, vx, vy, 0, 0, life, 0, 0, col, shape);
    if (p) p->angle = ang;
    return p;
}


static void boom_fx(float x, float y, float radius, int type, uint32_t col) {
    audio_play_sfx(SFX_EXPLOSION);
    Projectile *b = vis(x, y, 0, 0.36f, MOVE_TYPE_BOOM[type], col, 0, 0);
    if (b) {
        float sc = radius / 90.0f;
        if (sc < 0.45f) sc = 0.45f;
        if (sc > 0.80f) sc = 0.80f;
        b->draw_scale = sc;
        b->alpha = 240;
    }
}

static Zone *new_zone(int mode) {
    for (int i = 0; i < MAX_ZONES; i++) if (!g_game.zones[i].active) { memset(&g_game.zones[i], 0, sizeof(Zone)); g_game.zones[i].active = true; g_game.zones[i].mode = mode; g_game.zones[i].proj = -1; return &g_game.zones[i]; }
    return NULL;
}

static void blast(float x, float y, float r, float dmg, float knock, const MoveDef *d, uint32_t col) {
    for (int e = 0; e < MAX_ENEMIES; e++) {
        Enemy *en = &g_game.enemies[e];
        if (!en->active) continue;
        float dx = en->x - x, dy = en->y - y, rr = r + en->radius;
        if (dx*dx + dy*dy < rr*rr) hit_enemy(en, dmg, dx, dy, knock, d, col, false);
    }
    game_spawn_particles(x, y, col, 8, 120.0f);
}


typedef struct { const MoveDef *d; float dmg, speed, spread, life, size, radius, arc, width, knock, dur, rate, range; int count, pierce; uint32_t col; const char *shape; } Eff;


static const char *move_vfx_shape(const MoveDef *d) {
    const char *id = d->id;


    if (!strcmp(id, "ember")) return "ember";
    if (!strcmp(id, "water-gun")) return "drop";
    if (!strcmp(id, "bubble")) return "bubble";


    if (!strcmp(id, "tackle")) return "tackle";
    if (!strcmp(id, "quick-attack")) return "quick_attack";
    if (!strcmp(id, "swords-dance")) return "swords_dance";
    if (!strcmp(id, "agility")) return "agility";
    if (!strcmp(id, "harden")) return "harden";
    if (!strcmp(id, "body-slam")) return "stomp_impact";
    if (!strcmp(id, "hyper-voice")) return "hyper_voice";
    if (!strcmp(id, "karate-chop")) return "karate_chop";
    if (!strcmp(id, "aura-sphere")) return "aura_sphere";
    if (!strcmp(id, "axe-kick")) return "axe_kick";
    if (!strcmp(id, "high-jump-kick")) return "high_jump_kick";
    if (!strcmp(id, "hammer-arm")) return "hammer_arm";
    if (!strcmp(id, "sacred-sword")) return "sacred_sword";
    if (!strcmp(id, "egg-bomb")) return "egg";
    if (!strcmp(id, "milk-drink")) return "";
    if (!strcmp(id, "pay-day")) return "pay_day";
    if (!strcmp(id, "slash")) return "slash_crescent";
    if (!strcmp(id, "hyper-beam")) return "hyper_beam";
    if (!strcmp(id, "substitute")) return "substitute";


    if (!strcmp(id, "flamethrower")) return "flamethrower";
    if (!strcmp(id, "fire-spin")) return "fire_spin";
    if (!strcmp(id, "heat-wave")) return "heat_wave";
    if (!strcmp(id, "flame-wheel")) return "flame_wheel";
    if (!strcmp(id, "will-o-wisp")) return "wisp";
    if (!strcmp(id, "blaze-kick")) return "blaze_kick";
    if (!strcmp(id, "eruption")) return "eruption";
    if (!strcmp(id, "fire-lash")) return "fire_lash";
    if (!strcmp(id, "magma-storm")) return "magma_storm";
    if (!strcmp(id, "mystical-fire")) return "mystical_fire";


    if (!strcmp(id, "hydro-pump")) return "hydro_pump";
    if (!strcmp(id, "aqua-ring")) return "aqua_ring";
    if (!strcmp(id, "bubble-beam")) return "bubble_beam";
    if (!strcmp(id, "crabhammer")) return "tackle";
    if (!strcmp(id, "octazooka")) return "octazooka";
    if (!strcmp(id, "whirlpool")) return "whirlpool";


    if (!strcmp(id, "vine-whip")) return "vine_whip";
    if (!strcmp(id, "razor-leaf")) return "razor_leaf";
    if (!strcmp(id, "leaf-storm")) return "petal_blizzard";
    if (!strcmp(id, "giga-drain")) return "grass_knot";
    if (!strcmp(id, "leech-seed")) return "leech_seed";
    if (!strcmp(id, "solar-beam")) return "solar_beam";
    if (!strcmp(id, "spiky-shield")) return "spiky_shield";


    if (!strcmp(id, "thunder-shock")) return "thunder_shock";
    if (!strcmp(id, "discharge")) return "discharge";
    if (!strcmp(id, "thunderbolt")) return "thunderbolt";
    if (!strcmp(id, "electro-ball")) return "electro_ball";
    if (!strcmp(id, "electroweb")) return "eweb";
    if (!strcmp(id, "volt-tackle")) return "volt_tackle";
    if (!strcmp(id, "zap-cannon")) return "zap_cannon";
    if (!strcmp(id, "thunder-wave")) return "thunder_wave";


    if (!strcmp(id, "sludge")) return "sludge";
    if (!strcmp(id, "poison-sting")) return "needle";
    if (!strcmp(id, "toxic")) return "toxic";
    if (!strcmp(id, "toxic-spikes")) return "toxspikes";
    if (!strcmp(id, "sludge-bomb")) return "sludge_bomb";
    if (!strcmp(id, "cross-poison")) return "cross_poison";
    if (!strcmp(id, "poison-jab")) return "poison_jab";
    if (!strcmp(id, "sludge-wave")) return "sludge_wave";


    if (!strcmp(id, "confusion")) return "confusion";
    if (!strcmp(id, "psybeam")) return "psybeam";
    if (!strcmp(id, "psychic")) return "psychic";
    if (!strcmp(id, "hypnosis")) return "hypnosis";
    if (!strcmp(id, "dream-eater")) return "dream_eater";
    if (!strcmp(id, "psycho-cut")) return "psycut";
    if (!strcmp(id, "mist-ball")) return "mist_ball";
    if (!strcmp(id, "psyshock")) return "psyshock";
    if (!strcmp(id, "psystrike")) return "psystrike";


    if (!strcmp(id, "mud-shot")) return "mud_shot";
    if (!strcmp(id, "bulldoze")) return "bulldoze";
    if (!strcmp(id, "rock-throw")) return "rock_throw";
    if (!strcmp(id, "rock-slide")) return "rock_slide";
    if (!strcmp(id, "bonemerang")) return "bone";
    if (!strcmp(id, "spikes")) return "spikes";
    if (!strcmp(id, "drill-run")) return "drill_run";
    if (!strcmp(id, "scorching-sands")) return "scorching_sands";
    if (!strcmp(id, "magnitude")) return "crack";
    if (!strcmp(id, "stealth-rock")) return "stealthrock";
    if (!strcmp(id, "stone-axe")) return "stone_axe";
    if (!strcmp(id, "stone-edge")) return "stone_edge";


    if (!strcmp(id, "gust")) return "gust";
    if (!strcmp(id, "wing-attack")) return "wing_attack";
    if (!strcmp(id, "aerial-ace")) return "wing_attack";
    if (!strcmp(id, "air-cutter")) return "air_cutter";
    if (!strcmp(id, "air-slash")) return "air_cutter";
    if (!strcmp(id, "roost")) return "roost";


    if (!strcmp(id, "bug-bite")) return "jaws_dark_0";
    if (!strcmp(id, "string-shot")) return "web";
    if (!strcmp(id, "bug-buzz")) return "bug_buzz";


    if (!strcmp(id, "ice-beam")) return "ice_beam";
    if (!strcmp(id, "powder-snow")) return "snow";
    if (!strcmp(id, "hail")) return "hail";
    if (!strcmp(id, "aurora-beam")) return "aurora_beam";
    if (!strcmp(id, "freeze-dry")) return "freeze_dry";
    if (!strcmp(id, "ice-ball")) return "ice_ball";
    if (!strcmp(id, "ice-burn")) return "ice_burn";
    if (!strcmp(id, "ice-hammer")) return "ice_hammer";
    if (!strcmp(id, "icicle-spear")) return "icicle_spear";


    if (!strcmp(id, "lick")) return "lick";
    if (!strcmp(id, "shadow-ball")) return "shadow";
    if (!strcmp(id, "bite")) return "jaws_dark_0";
    if (!strcmp(id, "dark-pulse")) return "dark_pulse";
    if (!strcmp(id, "night-shade")) return "night_shade";
    if (!strcmp(id, "hex")) return "hex";
    if (!strcmp(id, "infernal-parade")) return "infernal_parade";
    if (!strcmp(id, "shadow-bone")) return "shadow_bone";
    if (!strcmp(id, "sucker-punch")) return "sucker_punch";
    if (!strcmp(id, "crunch")) return "crunch";


    if (!strcmp(id, "dragon-rage")) return "dragon_rage";
    if (!strcmp(id, "dragon-dance")) return "dragon_dance";
    if (!strcmp(id, "dragon-darts")) return "dragon_darts";
    if (!strcmp(id, "dragon-breath")) return "dragon_breath";
    if (!strcmp(id, "dragon-energy")) return "dragon_energy";
    if (!strcmp(id, "outrage")) return "outrage";


    if (!strcmp(id, "fairy-wind")) return "fairy_wind";
    if (!strcmp(id, "dazzling-gleam")) return "dazzling_gleam";
    if (!strcmp(id, "metal-claw")) return "metal_claw";
    if (!strcmp(id, "flash-cannon")) return "flash_cannon";
    if (!strcmp(id, "fleur-cannon")) return "fleur_cannon";
    if (!strcmp(id, "play-rough")) return "play_rough";
    if (!strcmp(id, "meteor-mash")) return "meteor_mash";
    if (!strcmp(id, "anchor-shot")) return "anchor_shot";
    if (!strcmp(id, "behemoth-blade")) return "behemoth_blade";

    return MOVE_TYPE_SHAPE[d->type];
}

static Eff eff_of(MoveState *ms, Player *pl) {
    const MoveDef *d = &MOVE_DB[ms->id]; const MoveMod *m = &ms->mod; Eff e;
    float raw_area = (pl->area_mult > 0.5f) ? pl->area_mult : 1.0f;
    float area_m = 1.0f + (raw_area - 1.0f) * 0.65f;
    if (area_m > 1.75f) area_m = 1.75f;
    float pspd_m = (pl->proj_speed_mult > 0.5f) ? pl->proj_speed_mult : 1.0f;
    e.d = d; e.dmg = ms->dmg * m->dmg * pl->atk_mult * 0.72f; e.speed = d->speed * m->speed * pspd_m; e.spread = d->spread * m->spread;
    e.life = d->life * m->life * 0.85f; e.size = d->size * m->size * area_m * 0.88f;
    e.radius = d->radius * m->radius * area_m * 0.78f * (d->kind == MK_MELEE ? 1.15f : 1.0f);
    e.arc = (m->arc >= 9.0f) ? 6.4f : d->arc; e.width = d->width * m->width * area_m * 0.80f; e.knock = d->knock + (m->dmg > 1.0f ? 10.0f * (m->dmg - 1.0f) : 0.0f);
    e.dur = d->dur * m->dur; e.rate = d->rate * m->rate; e.range = d->range * area_m * 0.80f;
    e.count = d->count + m->count + pl->bonus_amount; e.pierce = d->pierce + m->pierce;

    if (d->kind == MK_PROJECTILE || d->kind == MK_BOOMERANG || d->kind == MK_METEOR || d->kind == MK_MINE) {
        if (e.count > 6) e.count = 6;
    } else if (d->kind == MK_CONE || d->kind == MK_ORBIT || d->kind == MK_CHAIN) {
        if (e.count > 8) e.count = 8;
    } else if (d->kind == MK_NOVA) {
        if (e.count > 14) e.count = 14;
    }
    if (!strcmp(d->id, "vine-whip")) {
        e.count = 1 + (ms->level - 1) / 2 + pl->bonus_amount;
        if (e.count > MAX_WHIP_EFFECTS) e.count = MAX_WHIP_EFFECTS;
    }
    if (!strcmp(d->id, "night-shade")) {
        e.radius *= 1.15f;
    }
    if (!strcmp(d->id, "bug-buzz")) {
        e.radius *= 1.12f;
    }
    if (!strcmp(d->id, "outrage")) {
        e.radius *= 1.25f;
    }

    if (d->range > 0.0f) {
        float max_range = d->range * 1.45f;
        if (max_range > 340.0f) max_range = 340.0f;
        if (e.range > max_range) e.range = max_range;
    }
    if (d->radius > 0.0f) {
        float max_rad = d->radius * 1.55f;
        if (max_rad > 175.0f) max_rad = 175.0f;
        if (e.radius > max_rad) e.radius = max_rad;
    }
    if (d->width > 0.0f) {
        float max_w = d->width * 1.50f;
        if (max_w > 110.0f) max_w = 110.0f;
        if (e.width > max_w) e.width = max_w;
    }
    if (d->size > 0.0f) {
        float max_sz = d->size * 1.50f;
        if (max_sz > 54.0f) max_sz = 54.0f;
        if (e.size > max_sz) e.size = max_sz;
    }
    if (d->life > 0.0f) {
        float max_life = d->life * 1.35f;
        if (max_life > 2.4f) max_life = 2.4f;
        if (e.life > max_life) e.life = max_life;
    }
    if (d->speed > 0.0f) {
        float max_spd = d->speed * 1.45f;
        if (max_spd > 560.0f) max_spd = 560.0f;
        if (e.speed > max_spd) e.speed = max_spd;
    }
    if (e.knock > 110.0f) e.knock = 110.0f;
    e.col = MOVE_TYPE_COLOR[d->type]; e.shape = move_vfx_shape(d);
    return e;
}

void moves_init_state(MoveState *ms, int id) {
    memset(ms, 0, sizeof(*ms));
    ms->id = id; ms->level = 1;
    ms->base_cd = (MOVE_DB[id].kind == MK_DASH) ? 3.0f : MOVE_DB[id].cd;
    ms->dmg = (int)MOVE_DB[id].dmg; ms->count = 1; ms->timer = 0.0f;
    ms->charges = 1; ms->max_charges = 1;
    MoveMod one = {1,1,1,1,1,1,1,1,1,1,1,0,0,0,1}; ms->mod = one;
}
static void mod_apply(MoveState *ms, const MoveMod *a) {
    MoveMod *m = &ms->mod;
    m->dmg *= a->dmg; m->spread *= a->spread; m->radius *= a->radius; m->width *= a->width; m->speed *= a->speed;
    m->life *= a->life; m->size *= a->size; m->dur *= a->dur; m->rate *= a->rate; m->amount *= a->amount;
    m->count += a->count; m->pierce += a->pierce; m->stacks += a->stacks; if (a->arc >= 9.0f) m->arc = 9.0f;
    ms->base_cd *= a->cd;
}
void moves_apply_level(MoveState *ms) {
    if (ms->level > MAX_SKILL_LEVEL) {

        ms->mod.dmg *= 1.15f;
        ms->base_cd *= 0.985f;
        float min_cd = MOVE_DB[ms->id].cd * 0.42f;
        if (min_cd < 0.22f) min_cd = 0.22f;
        if (MOVE_DB[ms->id].kind == MK_DASH && min_cd < 1.05f) min_cd = 1.05f;
        if (ms->base_cd < min_cd) ms->base_cd = min_cd;
        if (MOVE_DB[ms->id].kind == MK_ORBIT) ms->timer = 0.0f;
        return;
    }
    int step = ms->level - 2; if (step < 0) step = 0; if (step > 6) step = 6;
    mod_apply(ms, &MOVE_LADDER[MOVE_DB[ms->id].kind][step]);
    if (MOVE_DB[ms->id].kind == MK_DASH) {


        if (ms->level == 2 || ms->level == 3) {
            ms->base_cd -= 0.15f;
            if (ms->base_cd < 1.20f) ms->base_cd = 1.20f;
        }
        if (ms->level >= 6) {
            ms->max_charges = 2;
            if (ms->charges < ms->max_charges) ms->charges++;
        }
    }
    if (MOVE_DB[ms->id].kind == MK_ORBIT) ms->timer = 0.0f;
}
void moves_apply_evo(MoveState *ms) {
    mod_apply(ms, &MOVE_EVO[MOVE_DB[ms->id].kind]);
    if (MOVE_DB[ms->id].kind == MK_DASH) {
        ms->max_charges = 2;
        ms->charges = 2;
        if (ms->base_cd < 1.20f) ms->base_cd = 1.20f;
    }
    if (MOVE_DB[ms->id].kind == MK_ORBIT) ms->timer = 0.0f;
}

static uint32_t s_aura_sfx_played[4] = {0};

void moves_init(void) {
    memset(g_game.projectiles, 0, sizeof(g_game.projectiles));
    memset(g_game.zones, 0, sizeof(g_game.zones));
    memset(g_game.buffs, 0, sizeof(g_game.buffs));
    memset(g_game.whips, 0, sizeof(g_game.whips));
    memset(s_aura_sfx_played, 0, sizeof(s_aura_sfx_played));
}


static bool is_status_or_passive_move(const MoveDef *d) {
    if (!d) return false;
    if (d->kind == MK_BUFF) return true;
    return !strcmp(d->id, "bug-buzz");
}

static inline void play_move_sfx(const MoveDef *d, SoundEffect explicit_sfx) {
    if (is_status_or_passive_move(d)) return;
    if ((d->kind == MK_AURA || d->kind == MK_ORBIT || d->kind == MK_TRAIL || d->kind == MK_MINE) && strcmp(d->id, "magma-storm") != 0) {

        int mid = (int)(d - MOVE_DB);
        if (mid >= 0 && mid < 128) {
            if (s_aura_sfx_played[mid >> 5] & (1u << (mid & 31))) return;
            s_aura_sfx_played[mid >> 5] |= (1u << (mid & 31));
        }
    }
    if (!strcmp(d->id, "string-shot")) {
        audio_play_sfx(SFX_ATTACK_GRASS);
        return;
    }
    if (d->kind == MK_BEAM || strstr(d->id, "beam") || strstr(d->id, "cannon") ||
        strstr(d->id, "laser") || !strcmp(d->id, "hydro-pump") || !strcmp(d->id, "octazooka") ||
        !strcmp(d->id, "psystrike") || !strcmp(d->id, "dragon-rage")) {
        audio_play_sfx(SFX_ATTACK_BEAM);
        return;
    }
    if (explicit_sfx < SFX_COUNT) audio_play_sfx(explicit_sfx);
    else audio_play_type_attack(d->type);
}

static void cast_move(MoveState *ms, Player *pl) {
    Eff e = eff_of(ms, pl); const MoveDef *d = e.d;
    int effective_kind = d->kind;
    if (!strcmp(d->id, "bubble-beam") || !strcmp(d->id, "octazooka")) effective_kind = MK_BEAM;
    float py = pl->y - 18.0f;
    float aim_oy = (effective_kind == MK_BEAM) ? pl->y : py;
    Enemy *tg = find_nearest_enemy(pl->x, aim_oy, 520.0f, NULL);
    float aim;
    bool is_moving = pl->is_moving;
    bool is_contact_melee = (d->kind == MK_MELEE || d->kind == MK_DASH || !strcmp(d->id, "body-slam") || !strcmp(d->id, "slash") || !strcmp(d->id, "tackle") || !strcmp(d->id, "lick"));
    if (d->kind == MK_DASH) {
        aim = pl->has_move_angle ? pl->move_angle : move_dir_angle(pl->dir);
        int nd = (int)floorf((1.57079632679f - aim) / 0.78539816339f + 0.5f);
        pl->dir = ((nd % 8) + 8) % 8;
    } else {
        aim = tg ? atan2f(tg->y - aim_oy, tg->x - pl->x) : move_dir_angle(pl->dir);
        if (tg && pl->jump_timer <= 0.0f) {

            if (!is_moving || is_contact_melee) {
                float dx = tg->x - pl->x, dy = tg->y - pl->y;
                pl->dir = fabsf(dx) > fabsf(dy) ? (dx > 0 ? 2 : 6) : (dy > 0 ? 0 : 4);
            }
        }
    }
    bool shoots = d->kind != MK_BUFF && d->kind != MK_ORBIT && d->kind != MK_AURA;
    if (shoots) player_play_attack_anim(pl, (d->kind == MK_MELEE || d->kind == MK_DASH) ? ANIM_ATTACK : ANIM_SHOOT, 0.26f);
    float dmg = e.dmg;

    switch (effective_kind) {
    case MK_PROJECTILE: case MK_NOVA: case MK_CONE: {
        int n = e.count; if (n < 1) n = 1;
        for (int i = 0; i < n; i++) {
            float ang;
            if (!strcmp(d->id, "hyper-voice")) {
                float cone_spread = 0.35f;
                ang = aim + (n > 1 ? ((float)i / (n - 1) - 0.5f) * cone_spread : (frand() - 0.5f) * 0.15f);
            }
            else if (d->kind == MK_NOVA) ang = ((float)i / n) * 2.0f * PI_F + frand() * 0.05f;
            else if (!strcmp(d->id, "bubble-beam")) ang = aim + (n > 1 ? ((float)i / (n - 1) - 0.5f) * 0.12f : 0.0f);
            else if (d->kind == MK_CONE) ang = aim + (n > 1 ? ((float)i / (n - 1) - 0.5f) * e.arc : 0.0f);
            else ang = aim + (n > 1 ? ((float)i - (n - 1) * 0.5f) * (e.spread + 0.05f) : (frand() - 0.5f) * e.spread);
            float spd = (!strcmp(d->id, "bubble-beam")) ? 480.0f : (!strcmp(d->id, "hyper-voice") ? 360.0f : e.speed);
            float life = (!strcmp(d->id, "bubble-beam")) ? 0.38f : e.life;
            float sz = (!strcmp(d->id, "hyper-voice")) ? (e.size + 2.0f) : (e.size + 4.0f);
            Projectile *p = spawn_projectile(pl->x, py, cosf(ang) * spd, sinf(ang) * spd, ms->id, (int)dmg, life, sz, e.pierce, e.col, e.shape);
            if (p) {
                p->def = d; p->homing = d->homing;

                if (!strcmp(e.shape, "razor_leaf") || !strcmp(e.shape, "pyro_ball")) p->spin_speed = 12.0f;
                if (!strcmp(d->id, "hyper-voice")) p->draw_scale = 0.80f;
                if (!strcmp(d->id, "pay-day")) p->draw_scale = 0.50f;
                if (!strcmp(d->id, "psychic")) { p->draw_scale = 0.70f; p->alpha = 175; }
                if (!strcmp(d->id, "bonemerang") || !strcmp(d->id, "shadow-bone")) { p->spin_speed = 14.0f; p->draw_scale = 0.85f; }
                if (!strcmp(d->id, "gust")) { p->spin_speed = 10.0f; p->draw_scale = 1.10f; }
                if (!strcmp(d->id, "thunderbolt")) { p->draw_scale = 1.15f; p->angle = ang; }
                if (!strcmp(d->id, "thunder-wave")) { p->draw_scale = 0.85f; p->angle = ang; }
                if (!strcmp(d->id, "mystical-fire")) { p->draw_scale = 1.05f; p->angle = ang; }
                if (!strcmp(d->id, "dragon-breath")) {
                    p->draw_scale = 1.15f; p->angle = ang;
                    p->color = caster_elem_tint_color(pl->char_idx, NULL);
                    p->tint_by_color = true;
                }
                if (!strcmp(d->id, "aura-sphere")) {
                    p->draw_scale = 1.05f;
                    p->color = caster_elem_tint_color(pl->char_idx, NULL);
                    p->tint_by_color = true;
                }
                if (!strcmp(d->id, "air-slash")) { p->draw_scale = 1.12f; p->angle = ang; }
                if (!strcmp(d->id, "mud-shot"))  { p->draw_scale = 1.02f; p->angle = ang; }
                if (!strcmp(d->id, "psyshock"))  { p->draw_scale = 1.05f; }
                if (!strcmp(d->id, "stone-edge")) { p->spin_speed = 0.0f; p->draw_scale = 1.25f; p->angle = ang; }
                if (!strcmp(d->id, "rock-throw")) { p->spin_speed = 0.0f; p->draw_scale = 1.15f; p->angle = 0.0f; }
            }
        }
        if (!strcmp(d->id, "powder-snow") || !strcmp(e.shape, "snow")) {
            game_spawn_particles(pl->x, py, RGBA8_C(240, 248, 255, 230), 16, 110.0f);
        }
        play_move_sfx(d, SFX_COUNT); break; }

    case MK_ORBIT: {
        int n = e.count < 1 ? 1 : e.count;
        float orb_rad = (!strcmp(d->id, "fire-spin")) ? (e.radius * 1.55f) : e.radius;
        float orb_spd = d->speed < 20 ? d->speed * 1.8f : 4.4f;

        int existing = 0;
        for (int i = 0; i < MAX_PROJECTILES; i++) {
            Projectile *p = &g_game.projectiles[i];
            if (p->active && p->is_orbit && !p->is_enemy && p->move_id == ms->id) {
                p->damage = (int)dmg;
                p->orbit_radius = orb_rad;
                p->orbit_speed = orb_spd;
                p->life = 999999.0f;
                existing++;
            }
        }
        if (existing == n) {
            ms->timer = 999.0f;
            break;
        }

        for (int i = 0; i < MAX_PROJECTILES; i++) {
            Projectile *p = &g_game.projectiles[i];
            if (p->active && p->is_orbit && !p->is_enemy && p->move_id == ms->id) p->active = false;
        }
        static float oa = 0.0f; oa += 1.4f;
        for (int k = 0; k < n; k++) {
            float a = oa + k * (2.0f * PI_F / n);
            Projectile *p = spawn_projectile(pl->x + cosf(a) * orb_rad, pl->y - 16.0f + sinf(a) * orb_rad, 0, 0, ms->id, (int)dmg, 999999.0f, e.size + 4.0f, 999999, e.col, !strcmp(e.shape, "fire") ? "spin" : e.shape);
            if (p) {
                p->def = d; p->is_orbit = true; p->orbit_angle = a; p->orbit_radius = orb_rad;
                p->orbit_speed = orb_spd;
                p->spin_speed = moves_circular_spin_speed(p->vfx_shape);
                p->angle = a + 1.5707963f;
            }
        }
        ms->timer = 999.0f;
        if (existing == 0) play_move_sfx(d, SFX_COUNT);
        break; }

    case MK_MELEE: {
        float half = e.arc * 0.5f;
        if (!strcmp(d->id, "vine-whip") || !strcmp(d->id, "fire-lash") || !strcmp(d->id, "lick")) {
            int wtype = !strcmp(d->id, "fire-lash") ? 1 : (!strcmp(d->id, "lick") ? 2 : 0);
            int lash_count = (!strcmp(d->id, "lick")) ? 1 : e.count;
            if (lash_count < 1) lash_count = 1;
            if (lash_count > MAX_WHIP_EFFECTS) lash_count = MAX_WHIP_EFFECTS;
            int made = 0;
            for (int wi = 0; wi < MAX_WHIP_EFFECTS && made < lash_count; wi++) {
                WhipEffect *wh = &g_game.whips[wi];
                wh->active = true; wh->x = pl->x; wh->y = pl->y - 14.0f;
                wh->angle = aim + ((float)made - (float)(lash_count - 1) * 0.5f) * 0.24f;
                wh->timer = 0.0f; wh->max_life = (!strcmp(d->id, "lick")) ? 0.26f : (0.34f + (float)made * 0.035f);
                wh->radius = (!strcmp(d->id, "lick")) ? (e.radius * 1.15f) : (e.radius * 1.5f);
                wh->arc = e.arc > 6.0f ? 6.2f : 2.65f;
                wh->seed = (float)(rand() % 1000) * 0.001f + (float)made * 1.7f;
                wh->dmg = (int)dmg; wh->def = d;
                wh->whip_type = wtype;
                memset(wh->hit_enemy, 0, sizeof(wh->hit_enemy)); made++;
            }
        } else {

            Enemy *tg = find_nearest_enemy(pl->x, py, e.radius + 60.0f, NULL);
            float fx_x = pl->x + cosf(aim) * e.radius * 0.55f;
            float fx_y = py + sinf(aim) * e.radius * 0.55f;

            if (tg) {
                float tdx = tg->x - pl->x;
                float tdy = tg->y - py;
                float td = sqrtf(tdx * tdx + tdy * tdy);
                if (td > 1.0f) {
                    float ldist = fminf(td - tg->radius - 4.0f, 65.0f);
                    if (ldist < 16.0f) ldist = 16.0f;
                    pl->lunge_dx = (tdx / td) * ldist;
                    pl->lunge_dy = (tdy / td) * ldist;
                    pl->lunge_dur = 0.22f;
                    pl->lunge_t = 0.22f;
                }

                fx_x = tg->x;
                fx_y = tg->y;
            } else {
                pl->lunge_dx = cosf(aim) * 35.0f;
                pl->lunge_dy = sinf(aim) * 35.0f;
                pl->lunge_dur = 0.20f;
                pl->lunge_t = 0.20f;
            }

            for (int i = 0; i < MAX_ENEMIES; i++) {
                Enemy *en = &g_game.enemies[i]; if (!en->active) continue;
                float dx = en->x - pl->x, dy = en->y - pl->y, dist = sqrtf(dx*dx + dy*dy);
                if (dist > e.radius + en->radius) continue;
                float rel = atan2f(dy, dx) - aim; while (rel > PI_F) rel -= 2*PI_F; while (rel < -PI_F) rel += 2*PI_F;
                if (e.arc > 6.0f || fabsf(rel) <= half) hit_enemy(en, dmg, dx, dy, e.knock, d, e.col, true);
            }
            if (!strcmp(d->id, "body-slam")) {
                float bxp = fx_x, byp = fx_y;
                float slam_rad = 85.0f;
                for (int i = 0; i < MAX_ENEMIES; i++) {
                    Enemy *en = &g_game.enemies[i]; if (!en->active) continue;
                    float dx = en->x - bxp, dy = en->y - byp, dist = sqrtf(dx * dx + dy * dy);
                    if (dist <= slam_rad + en->radius) {
                        hit_enemy(en, dmg * 1.25f, dx, dy, e.knock * 1.5f + 40.0f, d, e.col, true);
                    }
                }

                float slam_y = tg ? tg->y : (pl->y + sinf(aim) * e.radius * 0.55f);
                Projectile *cr = vis(bxp, slam_y, 0, 0.85f, "crack", RGBA8_C(255, 255, 255, 255), 0, 0);
                if (cr) { cr->draw_scale = 1.45f; cr->alpha = 240; }
                g_game.screen_shake = fmaxf(g_game.screen_shake, 0.35f);
            } else if (strncmp(e.shape, "slashx_", 7) == 0 || !strcmp(e.shape, "wing_attack") || !strcmp(e.shape, "cross_poison") || !strcmp(e.shape, "slash_crescent") || !strcmp(e.shape, "air_slash") || !strcmp(e.shape, "sacred_sword") || !strcmp(e.shape, "play_rough") || !strcmp(e.shape, "behemoth_blade") || !strcmp(e.shape, "metal_claw")) {
                uint32_t sc = RGBA8_C(255, 255, 255, 255);
                if (!strcmp(d->id, "wing-attack")) sc = RGBA8_C(210, 245, 255, 255);
                else if (!strcmp(d->id, "play-rough")) sc = RGBA8_C(255, 140, 220, 255);
                else if (!strcmp(d->id, "cross-poison")) sc = RGBA8_C(215, 95, 255, 255);
                else if (!strcmp(d->id, "blaze-kick")) sc = RGBA8_C(255, 150, 50, 255);
                else if (!strcmp(d->id, "metal-claw")) sc = RGBA8_C(225, 235, 250, 235);

                float sa_ = tg ? atan2f(tg->y - py, tg->x - pl->x) : aim;
                Projectile *a = vis(fx_x, fx_y, sa_, 0.30f, e.shape, sc, 0, 0);
                if (a) {
                    float s_sc = (!strcmp(d->id, "sacred-sword")) ? 1.65f :
                                 (!strcmp(d->id, "behemoth-blade")) ? 2.10f :
                                 (!strcmp(d->id, "metal-claw")) ? 1.15f :
                                 (!strcmp(d->id, "wing-attack") ? 1.35f : fmaxf(1.15f, e.radius / 44.0f));
                    a->draw_scale = s_sc;
                    a->alpha = (!strcmp(d->id, "sacred-sword") || !strcmp(d->id, "behemoth-blade")) ? 255 : 225;
                    a->anim_frame = 0;
                }
            } else {

                const char *sh = e.shape;
                uint32_t col = e.col;
                if (!strcmp(d->id, "crabhammer")) {
                    sh = "tackle";
                    col = RGBA8_C(60, 160, 255, 255);
                }
                Projectile *main_fx = vis(fx_x, fx_y, aim, 0.32f, sh, col, 0, 0);
                if (main_fx) {
                    if (!strcmp(sh, "stone_axe")) {
                        main_fx->draw_scale = 0.85f;
                        main_fx->alpha = 255;
                        main_fx->tint_by_color = false;
                    } else if (!strcmp(sh, "tackle")) {
                        main_fx->draw_scale = 1.30f;
                        main_fx->alpha = 170;
                    } else if (!strcmp(sh, "poison_jab")) {
                        main_fx->draw_scale = 1.35f;
                        main_fx->alpha = 175;
                    } else {
                        main_fx->draw_scale = fmaxf(1.05f, e.radius / 48.0f);
                        main_fx->alpha = 170;
                    }
                }
            }
        }
        g_game.screen_shake = fmaxf(g_game.screen_shake, d->knock > 100 ? 0.15f : 0.0f);
        play_move_sfx(d, SFX_ATTACK_MELEE); break; }

    case MK_BEAM: {
        float ang = aim;
        float len = e.radius, ca = cosf(ang), sa = sinf(ang);
        bool is_octa = !strcmp(d->id, "octazooka");
        bool is_bb = !strcmp(d->id, "bubble-beam");
        if (is_octa && len < 340.0f) len = 340.0f;
        else if (is_bb && len < 270.0f) len = 270.0f;
        const char *bs = e.shape;
        bool cannon = strstr(bs, "cannon") != NULL || is_bb || is_octa;

        char shape_start[48], shape_mid[48], shape_end[48];
        snprintf(shape_start, sizeof(shape_start), "%s_start", bs);
        snprintf(shape_mid, sizeof(shape_mid), "%s_mid", bs);
        snprintf(shape_end, sizeof(shape_end), "%s_end", bs);

        float b_scale = is_octa ? 1.15f : (cannon ? 1.05f : 0.95f);
        float t_start = 14.0f;
        float start_w = 48.0f * b_scale;
        float mid_w = 64.0f * b_scale;
        float end_w = 64.0f * b_scale;

        int n_mid = (int)((len - t_start - start_w - end_w) / mid_w + 0.5f);
        if (n_mid < 1) n_mid = 1;

        float vis_len = t_start + start_w + (float)n_mid * mid_w + end_w;
        float hit_len = (len > vis_len ? len : vis_len) + (is_octa ? 48.0f : 32.0f);
        float hit_hw = fmaxf(e.width * 0.5f, (is_octa || is_bb) ? (38.0f * b_scale) : (30.0f * b_scale));

        for (int i = 0; i < MAX_ENEMIES; i++) {
            Enemy *en = &g_game.enemies[i]; if (!en->active) continue;
            float dx = en->x - pl->x, dy = en->y - pl->y, t = dx * ca + dy * sa;
            if (t < -en->radius - 20.0f || t > hit_len + en->radius) continue;
            float perp = fabsf(-dx * sa + dy * ca);
            if (perp <= hit_hw + en->radius) hit_enemy(en, dmg, ca, sa, e.knock, d, e.col, false);
        }


        float t_cur = t_start;
        Projectile *p_s = vis(pl->x + ca * (t_cur + start_w * 0.5f), pl->y + sa * (t_cur + start_w * 0.5f), ang, 0.11f, shape_start, e.col, 0, 0);
        if (p_s) { p_s->draw_scale = b_scale; p_s->alpha = 255; p_s->src_x = pl->x; p_s->src_y = pl->y; p_s->has_src = true; }
        t_cur += start_w;


        for (int mi = 0; mi < n_mid; mi++) {
            Projectile *p_m = vis(pl->x + ca * (t_cur + mid_w * 0.5f), pl->y + sa * (t_cur + mid_w * 0.5f), ang, 0.11f, shape_mid, e.col, 0, 0);
            if (p_m) { p_m->draw_scale = b_scale; p_m->alpha = 255; p_m->src_x = pl->x; p_m->src_y = pl->y; p_m->has_src = true; }
            t_cur += mid_w;
        }


        Projectile *p_e = vis(pl->x + ca * (t_cur + end_w * 0.5f), pl->y + sa * (t_cur + end_w * 0.5f), ang, 0.11f, shape_end, e.col, 0, 0);
        if (p_e) { p_e->draw_scale = b_scale; p_e->alpha = 255; p_e->src_x = pl->x; p_e->src_y = pl->y; p_e->has_src = true; }

        g_game.screen_shake = fmaxf(g_game.screen_shake, 0.10f);
        play_move_sfx(d, SFX_ATTACK_BEAM);
        break;
    }

    case MK_DASH: {


        float ca = cosf(aim), sa = sinf(aim);
        float spd_mult = 1.0f;
        if (ms->level >= 2) spd_mult += 0.15f;
        if (ms->level >= 3) spd_mult += 0.15f;
        if (ms->evolved)    spd_mult += 0.12f;
        float dash_spd = 750.0f * spd_mult;
        float dash_dur = 0.19f;

        pl->dash_timer = dash_dur;
        pl->dash_vx = ca * dash_spd;
        pl->dash_vy = sa * dash_spd;
        pl->dash_dmg = (ms->level >= 4 || ms->evolved) ? dmg : 0.0f;
        pl->dash_knock = (ms->level >= 4 || ms->evolved) ? fmaxf(e.knock, 75.0f) : 0.0f;
        pl->dash_hit_rad = ((ms->level >= 5) ? 38.0f : 26.0f) * pl->area_mult;
        pl->dash_grace_iframe = (ms->level >= 6 || ms->evolved) ? 0.10f : 0.05f;
        pl->dash_evolved = (ms->level >= 8 || ms->evolved);
        pl->dash_trail_timer = 0.0f;
        pl->dash_move_id = ms->id;
        memset(pl->dash_hit_enemy, 0, sizeof(pl->dash_hit_enemy));


        static const uint32_t SANDY_PAL[10] = {
            RGBA8_C(175,  40, 255, 255),
            RGBA8_C(255,  30, 145, 255),
            RGBA8_C(255,  70,  45, 255),
            RGBA8_C(255, 165,  25, 255),
            RGBA8_C(255, 240,  45, 255),
            RGBA8_C(110, 255,  65, 255),
            RGBA8_C( 35, 255, 190, 255),
            RGBA8_C( 35, 235, 255, 255),
            RGBA8_C( 60, 175, 255, 255),
            RGBA8_C(145, 250, 255, 255)
        };
        int dex = POKEMON_CATALOG[pl->char_idx].dex;
        float est_dist = dash_spd * dash_dur;
        for (int gi = 0; gi < 10; gi++) {
            float frac = (float)gi / 10.0f;
            float gx = pl->x + ca * est_dist * frac;
            float gy = pl->y + sa * est_dist * frac;
            int slot = -1;
            float min_life = 99.0f;
            for (int k = 0; k < MAX_DASH_GHOSTS; k++) {
                if (!g_game.dash_ghosts[k].active) { slot = k; break; }
                if (g_game.dash_ghosts[k].life < min_life) { min_life = g_game.dash_ghosts[k].life; slot = k; }
            }
            if (slot >= 0) {
                DashGhost *dg = &g_game.dash_ghosts[slot];
                dg->active = true;
                dg->x = gx; dg->y = gy;
                dg->dex = dex;
                dg->anim = ANIM_ATTACK;
                dg->frame = (pl->anim_frame + gi) % 4;
                dg->dir = pl->dir;
                dg->max_life = 0.24f + (float)gi * 0.025f;
                dg->life = dg->max_life;
                dg->color = SANDY_PAL[gi];
            }
        }

        uint32_t dash_vfx_col = caster_elem_tint_color(pl->char_idx, NULL);
        Projectile *pj = vis(pl->x + ca * 42.0f, (pl->y - 14.0f) + sa * 42.0f, aim, 0.28f, e.shape, dash_vfx_col, ca * dash_spd * 0.65f, sa * dash_spd * 0.65f);
        if (pj) { pj->draw_scale = 1.40f; pj->alpha = 245; }


        pl->iframe_timer = fmaxf(pl->iframe_timer, dash_dur + pl->dash_grace_iframe);
        play_move_sfx(d, SFX_COUNT);
        break;
    }

    case MK_AURA: {
        if (!strcmp(d->id, "magma-storm")) {
            for (int k = 0; k < 3; k++) {
                Zone *z = new_zone(2);
                if (z) {
                    float a = ((float)k / 3.0f) * 2.0f * PI_F + frand() * 0.3f;
                    float dist = 35.0f + frand() * 45.0f;
                    z->x = pl->x + cosf(a) * dist;
                    z->y = pl->y + sinf(a) * dist;
                    z->radius = e.radius * 0.90f;
                    z->dmg = (int)(dmg * 0.50f);
                    z->rate = 0.28f;
                    z->tick = 0.1f;
                    z->t = 3.2f;
                    z->def = d;
                    Projectile *mp = vis(z->x, z->y, 0.0f, 3.2f, "magma_storm", RGBA8_C(255, 255, 255, 255), 0, 0);
                    if (mp) { mp->draw_scale = 1.65f; mp->alpha = 245; }
                }
            }
            play_move_sfx(d, SFX_COUNT);
            break;
        }
        for (int i = 0; i < MAX_ENEMIES; i++) {
            Enemy *en = &g_game.enemies[i]; if (!en->active) continue;
            float dx = en->x - pl->x, dy = en->y - pl->y, rr = e.radius + en->radius;
            float aura_kb = (!strcmp(d->id, "bulldoze")) ? 145.0f : (e.knock * 0.3f);
            if (dx*dx + dy*dy < rr*rr) hit_enemy(en, dmg, dx, dy, aura_kb, d, e.col, false);
        }
        if (!strcmp(d->id, "magnitude")) {
            Projectile *cr = vis(pl->x, pl->y, 0.0f, 0.70f, "crack", RGBA8_C(255, 255, 255, 255), 0, 0);
            if (cr) {
                float msc = (e.radius * 2.4f) / 54.0f;
                cr->draw_scale = (msc > 4.2f) ? msc : 4.2f;
                cr->alpha = 245;
            }
            g_game.screen_shake = fmaxf(g_game.screen_shake, 0.24f);
        } else if (!strcmp(d->id, "bulldoze")) {
            Projectile *cr = vis(pl->x, pl->y, 0.0f, 0.54f, "bulldoze", RGBA8_C(255, 255, 255, 255), 0, 0);
            if (cr) {
                float bsc = (e.radius * 2.45f) / 128.0f;
                cr->draw_scale = (bsc > 1.75f) ? bsc : 1.75f;
                cr->alpha = 255;
            }
            g_game.screen_shake = fmaxf(g_game.screen_shake, 0.18f);
        } else if (!strcmp(d->id, "night-shade")) {
            Projectile *ns_aoe = vis(pl->x, pl->y, 0.0f, 0.45f, "night_shade_aoe", RGBA8_C(255, 255, 255, 255), 0, 0);
            if (ns_aoe) {
                ns_aoe->draw_scale = fmaxf(2.25f, (e.radius * 2.20f) / 96.0f);
                ns_aoe->alpha = 220;
            }
            for (int k = 0; k < 8; k++) {
                float a = k * PI_F / 4.0f;
                Projectile *np = vis(pl->x + cosf(a) * e.radius * 0.72f, pl->y + sinf(a) * e.radius * 0.72f, 0.0f, 0.38f, "night_shade", RGBA8_C(255, 255, 255, 255), 0, 0);
                if (np) { np->draw_scale = 0.56f; np->alpha = 245; }
            }
        } else if (!strcmp(d->id, "giga-drain")) {
            Projectile *gk = vis(pl->x, pl->y, 0.0f, 0.40f, "grass_knot", RGBA8_C(255, 255, 255, 255), 0, 0);
            if (gk) {
                gk->draw_scale = fmaxf(1.85f, (e.radius * 2.10f) / 64.0f);
                gk->alpha = 180;
            }
        } else if (!strcmp(d->id, "bug-buzz")) {
            for (int k = 0; k < 8; k++) {
                float a = k * PI_F / 4.0f;
                Projectile *bp = vis(pl->x + cosf(a) * e.radius * 0.85f, pl->y - 10.0f + sinf(a) * e.radius * 0.85f, a, 0.30f, "bug_buzz", e.col, 0, 0);
                if (bp) { bp->draw_scale = 0.55f; }
            }
        } else if (!strcmp(d->id, "outrage")) {
            for (int k = 0; k < 12; k++) {
                float a = (float)k * PI_F / 6.0f + (frand() - 0.5f) * 0.28f;
                float r = e.radius * (0.35f + frand() * 0.65f);
                Projectile *op = vis(pl->x + cosf(a) * r, pl->y - 10.0f + sinf(a) * r, a, 0.35f, "outrage", e.col, 0, 0);
                if (op) { op->draw_scale = 0.42f; op->alpha = 240; }
            }
        } else if (!strcmp(d->id, "discharge")) {

            for (int k = 0; k < 8; k++) {
                float a = (float)k * PI_F / 4.0f;
                Projectile *dp = vis(pl->x + cosf(a) * e.radius * 0.96f, pl->y - 6.0f + sinf(a) * e.radius * 0.96f, a, 0.34f, "discharge", e.col, 0, 0);
                if (dp) { dp->draw_scale = 0.56f; dp->alpha = 235; }
            }
        } else {
            static const char *AURA_SHAPE[18] = {"impact","flame","bubble","leaf","shock","snow","impact","sludge","mud","wind","ring","sting","rock","shadow","dragon","crescent","steelshard","star"};
            const char *as = (e.shape && strcmp(e.shape, "impact") != 0) ? e.shape : AURA_SHAPE[d->type];
            for (int k = 0; k < 8; k++) { float a = k * PI_F / 4.0f; vis(pl->x + cosf(a) * e.radius * 0.8f, pl->y - 10.0f + sinf(a) * e.radius * 0.8f, a, 0.3f, as, e.col, 0, 0); }
        }
        play_move_sfx(d, SFX_COUNT); break; }

    case MK_BOOMERANG: {
        int n = e.count < 1 ? 1 : e.count;
        for (int i = 0; i < n; i++) {
            float ang = aim + (n > 1 ? ((float)i - (n - 1) * 0.5f) * 0.5f : 0.0f);
            float sp = e.speed < 200 ? 300.0f : e.speed;
            Projectile *p = spawn_projectile(pl->x, py, cosf(ang) * sp, sinf(ang) * sp, ms->id, (int)dmg, 2.0f * e.radius / sp, e.size + 6.0f, 99, e.col, e.shape);
            if (p) {
                p->def = d;
                p->boom = 1;
                if (!strcmp(d->id, "air-cutter")) {
                    p->spin_speed = 0.0f;
                    p->draw_scale = 1.75f;
                    p->angle = ang;
                } else {

                    p->spin_speed = (moves_circular_spin_speed(p->vfx_shape) != 0.0f) ? 14.0f : 0.0f;
                    p->angle = ang;
                    if (!strcmp(d->id, "bonemerang") || !strcmp(d->id, "shadow-bone")) {
                        p->draw_scale = 0.85f;
                    }
                }
                memset(p->hit_cd, 0, sizeof(p->hit_cd));
            }
        }
        play_move_sfx(d, SFX_COUNT); break; }

    case MK_CHAIN: {
        uint8_t hitset[MAX_ENEMIES]; memset(hitset, 0, sizeof(hitset));
        Enemy *cur = tg; float fx = pl->x, fy = py; int jumps = e.count > 0 ? e.count : 3;
        for (int j = 0; j < jumps && cur; j++) {
            hitset[enemy_index(cur)] = 1;
            float jdx = cur->x - fx, jdy = cur->y - fy;
            float jump_ang = atan2f(jdy, jdx);
            hit_enemy(cur, dmg, jdx, jdy, 0, d, e.col, true);
            for (float t = 0.0f; t <= 1.0f; t += 0.25f) {
                vis(fx + jdx * t, fy + jdy * t, jump_ang, 0.25f, e.shape, e.col, 0, 0);
            }
            fx = cur->x; fy = cur->y;
            cur = find_nearest_enemy(fx, fy, e.radius, hitset);
        }
        play_move_sfx(d, SFX_COUNT); break; }

    case MK_METEOR: {
        if (!strcmp(d->id, "egg-bomb")) {
            float ca = cosf(aim), sa = sinf(aim);
            for (int i = 0; i < 3; i++) {
                Zone *z = new_zone(0); if (!z) break;
                float dist = 70.0f + 65.0f * i;
                z->x = pl->x + ca * dist; z->y = py + sa * dist; z->t = 0.30f * (i + 1);
                z->radius = e.radius * (0.7f + 0.35f * i); z->dmg = (int)dmg * (i + 1) / 2 + (int)dmg / 2; z->def = d; z->rate = e.knock;
            }
            Projectile *eg = vis(pl->x, py, aim, 0.9f, "egg", e.col, ca * 217.0f, sa * 217.0f);
            if (eg) eg->draw_scale = 1.0f;
            play_move_sfx(d, SFX_COUNT); break;
        }
        bool is_meteor_mash = !strcmp(d->id, "meteor-mash");
        for (int i = 0; i < e.count; i++) {
            Enemy *t = find_nearest_enemy(pl->x, pl->y, 240.0f, NULL);
            Enemy *pick = t;
            if (!pick) {
                for (int k = 0; k < 4; k++) {
                    Enemy *c = &g_game.enemies[rand() % MAX_ENEMIES];
                    if (c->active) {
                        float cdx = c->x - pl->x, cdy = c->y - pl->y;
                        if (cdx * cdx + cdy * cdy <= 240.0f * 240.0f) { pick = c; break; }
                    }
                }
            }
            Zone *z = new_zone(0); if (!z) break;
            float zx = pick ? pick->x : pl->x + (frand() - 0.5f) * 150.0f, zy = pick ? pick->y : pl->y + (frand() - 0.5f) * 150.0f;
            z->x = zx; z->y = zy; z->t = is_meteor_mash ? (0.16f + 0.06f * i) : (0.35f + 0.12f * i);
            z->radius = e.radius; z->dmg = (int)dmg; z->def = d; z->rate = e.knock;
            float drop_h = is_meteor_mash ? 160.0f : 90.0f;
            float fall_v = drop_h / z->t;
            Projectile *mp = vis(zx, zy - drop_h, is_meteor_mash ? 0.0f : 1.5708f, z->t, e.shape, e.col, 0, fall_v);
            if (mp && is_meteor_mash) { mp->alpha = 250; mp->draw_scale = 1.25f; }
        }
        play_move_sfx(d, SFX_COUNT); break; }

    case MK_MINE: case MK_TURRET: case MK_TRAIL: {
        int n = d->kind == MK_TRAIL ? 1 : (e.count < 1 ? 1 : e.count);
        if (!strcmp(d->id, "toxic-spikes") || !strcmp(d->id, "spikes")) n = 1;
        for (int i = 0; i < n; i++) {
            Zone *z = new_zone(d->kind == MK_MINE ? 1 : d->kind == MK_TRAIL ? 2 : 3); if (!z) break;
            float a = frand() * 2.0f * PI_F, r = d->kind == MK_TRAIL ? 0.0f : 40.0f + frand() * 60.0f;
            z->x = pl->x + cosf(a) * r; z->y = pl->y + sinf(a) * r; z->life = e.dur; z->t = e.dur; z->radius = e.radius; z->dmg = (int)dmg;
            z->rate = d->kind == MK_TURRET ? e.rate : 0.3f; z->range = e.range; z->def = d; z->tick = 0.1f;
            z->is_decoy = !strcmp(d->id, "substitute");
            Projectile *p = z->is_decoy ? NULL : vis(z->x, z->y, 0.0f, e.dur,
                e.shape, e.col, 0, 0);
            if (p && !strcmp(e.shape, "whirlpool")) {
                p->spin_speed = -10.0f;
                p->draw_scale = 1.35f;
            }
            z->proj = proj_index(p);
        }
        if (strcmp(d->id, "substitute") != 0) play_move_sfx(d, SFX_COUNT);
        break; }

    case MK_BUFF: {
        int s = d->stat; if (s < 1 || s > 5) break;
        float amt = d->amount * ms->mod.amount; int maxst = d->stacks + ms->mod.stacks;
        if (s == 5) {
            pl->hp = fminf(pl->max_hp, pl->hp + pl->max_hp * amt * 0.55f);
            pl->iframe_timer = fmaxf(pl->iframe_timer, 0.25f);

            g_game.buffs[4].stacks = 1;
            g_game.buffs[4].per = 0.0f;
            g_game.buffs[4].timer = 0.75f;
            game_spawn_particles(pl->x, py, RGBA8_C(245, 255, 232, 255), 8, 62.0f);
            game_spawn_particles(pl->x, py, RGBA8_C(130, 255, 175, 255), 5, 48.0f);
            break;
        }
        BuffState *b = &g_game.buffs[s - 1];
        if (b->stacks < maxst) {
            b->stacks++; b->per = amt;
            if (s == 1) pl->atk_mult += amt; else if (s == 2) pl->speed *= 1.0f + amt; else if (s == 3) pl->def_mult += amt; else pl->regen += amt;
        }
        b->timer = d->dur * ms->mod.dur;
        if (!strcmp(d->id, "aqua-ring")) {

            bool found = false;
            for (int k = 0; k < MAX_PROJECTILES; k++) {
                Projectile *pj = &g_game.projectiles[k];
                if (pj->active && pj->vfx_shape && !strcmp(pj->vfx_shape, "aqua_ring")) {
                    pj->life = d->dur * ms->mod.dur;
                    pj->max_life = pj->life;
                    found = true;
                    break;
                }
            }
            if (!found) {
                Projectile *p = vis(pl->x, pl->y, 0, d->dur * ms->mod.dur, "aqua_ring", 0xFFFFFFFFu, 0, 0);
                if (p) { p->spin_speed = 0.0f; p->draw_scale = 1.0f; p->alpha = 235; }
            }
        }
        break; }
    }
}

static void buff_expire(int i, Player *pl) {
    BuffState *b = &g_game.buffs[i];
    for (int k = 0; k < b->stacks; k++) {
        if (i == 0) pl->atk_mult -= b->per;
        else if (i == 1) pl->speed /= 1.0f + b->per;
        else if (i == 2) pl->def_mult -= b->per;
        else if (i == 3) pl->regen -= b->per;
    }
    b->stacks = 0; b->timer = 0;
}

void moves_update(float dt, Player *player) {

    for (int wi = 0; wi < MAX_WHIP_EFFECTS; wi++) {
        WhipEffect *wh = &g_game.whips[wi];
        if (!wh->active) continue;
        wh->timer += dt; wh->x = player->x; wh->y = player->y - 14.0f;
        float prog = wh->timer / wh->max_life;
        if (prog >= 1.0f) { wh->active = false; continue; }
        if (wh->dmg <= 0) continue;
        float half = wh->arc * 0.5f;
        float sw = fminf(1.0f, prog / 0.70f);
        sw = 1.0f - (1.0f - sw) * (1.0f - sw);
        float sweep_end = -half + sw * wh->arc + 0.25f;
        float reach = fminf(1.0f, prog * 5.0f);
        if (prog > 0.75f) reach = 1.0f - (prog - 0.75f) / 0.25f * 0.6f;
        float max_r = wh->radius * reach;
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (wh->hit_enemy[e]) continue;
            Enemy *en = &g_game.enemies[e]; if (!en->active) continue;
            float dx = en->x - player->x, dy = en->y - player->y;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist < max_r + en->radius) {
                float rel = atan2f(dy, dx) - wh->angle;
                while (rel > PI_F) rel -= 2.0f * PI_F;
                while (rel < -PI_F) rel += 2.0f * PI_F;
                if (rel >= -half - 0.25f && rel <= sweep_end) {
                    wh->hit_enemy[e] = true;

                    for (int wi = 0; wi < MAX_WHIP_EFFECTS; wi++) {
                        if (g_game.whips[wi].active) g_game.whips[wi].hit_enemy[e] = true;
                    }
                    uint32_t hit_col = (wh->whip_type == 1) ? RGBA8_C(255, 120, 20, 255) :
                                       (wh->whip_type == 2) ? RGBA8_C(255, 90, 160, 255) :
                                                              RGBA8_C(80, 235, 80, 255);
                    uint32_t part_col = (wh->whip_type == 1) ? RGBA8_C(255, 140, 30, 255) :
                                        (wh->whip_type == 2) ? RGBA8_C(255, 150, 200, 255) :
                                                               RGBA8_C(110, 250, 90, 255);
                    hit_enemy(en, (float)wh->dmg, dx, dy, 0, wh->def, hit_col, false);
                    if (dist > 0.1f && !en->is_boss) terrain_apply_enemy_knockback(g_game.selected_stage, &en->x, &en->y, en->radius, en->is_boss, dx / dist * 58.0f, dy / dist * 58.0f);
                    game_spawn_particles(en->x, en->y, part_col, 6, 110.0f);
                }
            }
        }
    }


    float leech_heal_budget = 4.0f * dt;
    for (int e = 0; e < MAX_ENEMIES; e++) {
        Enemy *en = &g_game.enemies[e];
        if (!en->active) { en->slow_t = en->stun_t = en->stun_cd = en->dot_t = 0.0f; en->is_leeched = false; en->debuff_col = 0; continue; }
        if (en->slow_t > 0) { en->slow_t -= dt; if (en->slow_t <= 0.0f) { en->slow_t = 0.0f; en->slow_f = 0.0f; } }
        if (en->stun_t > 0) { en->stun_t -= dt; if (en->stun_t < 0.0f) en->stun_t = 0.0f; }
        if (en->stun_cd > 0) { en->stun_cd -= dt; if (en->stun_cd < 0.0f) en->stun_cd = 0.0f; }
        if (en->is_boss) {
            en->dmg_window_timer += dt;
            if (en->dmg_window_timer >= 1.0f) {
                en->dmg_window_timer -= 1.0f;
                en->dmg_window_accum = 0.0f;
            }
        }
        if (en->dot_t > 0) {
            en->dot_t -= dt;
            if (en->dot_t < 0.0f) en->dot_t = 0.0f;
            float dot_dmg = en->dot_dps * dt;
            en->hp -= dot_dmg;
            if (en->is_leeched && leech_heal_budget > 0.0f) {
                float lh = dot_dmg * 0.12f;
                if (lh > leech_heal_budget) lh = leech_heal_budget;
                leech_heal_budget -= lh;
                player->hp = fminf(player->max_hp, player->hp + lh);
            }
            if (((int)(en->dot_t * 4.0f)) != ((int)((en->dot_t + dt) * 4.0f))) {
                uint32_t pcol = en->debuff_col ? en->debuff_col : (en->is_leeched ? RGBA8_C(100, 240, 80, 255) : RGBA8_C(255, 120, 40, 255));
                game_spawn_particles(en->x, en->y - 10.0f, pcol, 1, 35.0f);
            }
        }
        if (en->slow_t <= 0.0f && en->stun_t <= 0.0f && en->dot_t <= 0.0f) {
            en->debuff_col = 0;
            en->is_leeched = false;
        }
    }
    for (int i = 0; i < 5; i++) if (g_game.buffs[i].stacks > 0) { g_game.buffs[i].timer -= dt; if (g_game.buffs[i].timer <= 0) buff_expire(i, player); }


    for (int i = 0; i < MAX_ZONES; i++) {
        Zone *z = &g_game.zones[i]; if (!z->active) continue;
        const MoveDef *d = z->def; uint32_t col = MOVE_TYPE_COLOR[d->type]; const char *sh = MOVE_TYPE_SHAPE[d->type];
        if (z->mode == 0) {
            z->t -= dt;
            if (z->t <= 0) {
                blast(z->x, z->y, z->radius, (float)z->dmg, z->rate, d, col);
                if (!strcmp(d->id, "meteor-mash")) {
                    Projectile *mh = vis(z->x, z->y, 0.0f, 0.32f, "meteor_hit", RGBA8_C(255, 255, 255, 255), 0, 0);
                    if (mh) { mh->draw_scale = 1.55f; mh->alpha = 250; }
                } else {
                    boom_fx(z->x, z->y, z->radius, d->type, col);
                }
                g_game.screen_shake = fmaxf(g_game.screen_shake, 0.1f);
                z->active = false;
            }
        } else if (z->mode == 1) {
            z->t -= dt; bool boom = z->t <= 0;
            for (int e = 0; e < MAX_ENEMIES && !boom; e++) { Enemy *en = &g_game.enemies[e]; if (!en->active) continue; float dx = en->x - z->x, dy = en->y - z->y; if (dx*dx + dy*dy < (z->radius * 0.5f + en->radius) * (z->radius * 0.5f + en->radius)) boom = true; }
            if (boom) {
                if (z->t > 0) blast(z->x, z->y, z->radius, (float)z->dmg, 60.0f, d, col);
                boom_fx(z->x, z->y, z->radius, d->type, col);
                play_move_sfx(d, SFX_COUNT);
                if (z->proj >= 0) g_game.projectiles[z->proj].active = false;
                z->active = false;
            }
        } else if (z->mode == 2) {
            z->t -= dt; z->tick -= dt;
            if (z->tick <= 0) { z->tick = z->rate; blast(z->x, z->y, z->radius, (float)z->dmg, 0, d, col); }
            if (z->t <= 0) z->active = false;
        } else {
            z->t -= dt; z->tick -= dt;
            if (z->tick <= 0) {
                Enemy *t = find_nearest_enemy(z->x, z->y, z->range, NULL);
                if (t) {
                    z->tick = z->rate;
                    float a = atan2f(t->y - z->y, t->x - z->x);
                    const char *proj_sh = (!strcmp(d->id, "will-o-wisp")) ? "wisp" : sh;
                    uint32_t proj_col = (!strcmp(d->id, "will-o-wisp")) ? RGBA8_C(70, 220, 255, 255) : col;
                    float sp_val = 340.0f;
                    bool use_tint = false;
                    int sub_elem = 0;
                    if (z->is_decoy) {
                        proj_sh = "sub_orb";
                        proj_col = caster_elem_tint_color(player->char_idx, &sub_elem);
                        sp_val = 360.0f;
                        use_tint = true;
                    }
                    Projectile *p = spawn_projectile(z->x, z->y, cosf(a) * sp_val, sinf(a) * sp_val, 0, z->dmg, 1.2f, 7.0f, 0, proj_col, proj_sh);
                    if (p) {
                        p->def = d; p->homing = 1; p->tint_by_color = use_tint;
                        if (z->is_decoy) {
                            p->draw_scale = 0.38f; p->alpha = 180;
                            audio_play_type_attack(sub_elem);
                        } else {
                            play_move_sfx(d, SFX_COUNT);
                        }
                    }
                } else z->tick = 0.15f;
            }
            if (z->t <= 0) { if (z->proj >= 0) g_game.projectiles[z->proj].active = false; z->active = false; }
        }
    }


    int dash_slot = 0;
    for (int m = 0; m < player->move_count; m++) {
        MoveState *ms = &player->moves[m];
        const MoveDef *d = &MOVE_DB[ms->id];
        if (ms->burst_left > 0) {
            ms->burst_t -= dt;
            if (ms->burst_t <= 0.0f) {
                ms->burst_left--;
                ms->burst_t = 0.14f;
                cast_move(ms, player);
            }
        } else {
            ms->timer -= dt;
        }
        if (d->kind == MK_DASH) {
            int btn = dash_slot++;
            if (ms->max_charges < 1) ms->max_charges = (ms->level >= 6 || ms->evolved) ? 2 : 1;
            float eff_cd = fmaxf(1.20f, ms->base_cd * player->cd_mult);
            if (ms->charges < ms->max_charges) {
                if (ms->timer <= 0.0f) {
                    ms->charges++;
                    if (ms->charges < ms->max_charges) ms->timer = eff_cd;
                    else ms->timer = 0.0f;
                }
            } else {
                ms->timer = 0.0f;
            }
            if (btn < 4 && g_game.skill_pressed[btn]) {
                player->dash_input_buf = 0.10f;
            }
            if (player->dash_input_buf > 0.0f && ms->charges > 0 && player->dash_timer <= 0.0f) {
                player->dash_input_buf = 0.0f;
                ms->charges--;
                if (ms->timer <= 0.0f) ms->timer = eff_cd;
                cast_move(ms, player);
            }
        } else if (ms->timer <= 0.0f) {
            if (d->kind != MK_BUFF && d->kind != MK_ORBIT && d->kind != MK_AURA) {
                Eff e = eff_of(ms, player);
                int ek = d->kind;
                if (!strcmp(d->id, "bubble-beam") || !strcmp(d->id, "octazooka")) ek = MK_BEAM;
                float ox = player->x, oy = player->y - 18.0f;
                float reach = 0.0f;
                switch (ek) {
                    case MK_MELEE:
                        reach = (!strcmp(d->id, "vine-whip") || !strcmp(d->id, "fire-lash") || !strcmp(d->id, "lick"))
                            ? (e.radius * 1.15f + 12.0f) : (e.radius + 14.0f);
                        break;
                    case MK_AURA:
                        oy = player->y;
                        reach = !strcmp(d->id, "magma-storm") ? (e.radius * 0.90f + 55.0f) : (e.radius + 10.0f);
                        break;
                    case MK_BEAM: {
                        oy = player->y;
                        float len = e.radius;
                        if (!strcmp(d->id, "octazooka") && len < 340.0f) len = 340.0f;
                        else if (!strcmp(d->id, "bubble-beam") && len < 270.0f) len = 270.0f;
                        reach = len + 12.0f;
                        break;
                    }
                    case MK_CHAIN:
                        reach = e.radius + 95.0f;
                        break;
                    case MK_BOOMERANG:
                        reach = e.radius + 12.0f;
                        break;
                    case MK_ORBIT: {
                        float orb_rad = (!strcmp(d->id, "fire-spin")) ? (e.radius * 1.55f) : e.radius;
                        reach = orb_rad + 32.0f;
                        break;
                    }
                    case MK_METEOR:
                        oy = player->y;
                        reach = !strcmp(d->id, "egg-bomb") ? 180.0f : 240.0f;
                        break;
                    case MK_CONE: {
                        float travel = e.speed * e.life * 0.88f + 10.0f;
                        reach = (travel < 340.0f) ? travel : 340.0f;
                        break;
                    }
                    case MK_NOVA: {
                        float spd = !strcmp(d->id, "hyper-voice") ? 360.0f : e.speed;
                        float travel = spd * e.life * 0.85f + 10.0f;
                        reach = (travel < 300.0f) ? travel : 300.0f;
                        break;
                    }
                    case MK_PROJECTILE: {
                        float travel = e.speed * e.life * 0.78f;
                        reach = (travel < 340.0f) ? travel : 340.0f;
                        break;
                    }
                    case MK_TURRET:
                        oy = player->y;
                        reach = e.range + 40.0f;
                        break;
                    case MK_MINE:
                    case MK_TRAIL:
                    default:
                        oy = player->y;
                        reach = 220.0f;
                        break;
                }
                if (reach > 0.0f && !find_nearest_enemy(ox, oy, reach, NULL)) {
                    ms->timer = 0.0f;
                    continue;
                }
            }
            ms->timer = ms->base_cd * (d->kind == MK_METEOR ? 1.45f : 1.0f) * player->cd_mult;
            if (d->kind == MK_BEAM) {
                Eff e = eff_of(ms, player);
                int shots = e.count > 1 ? e.count : 1;
                if (!strcmp(d->id, "psystrike") || !strcmp(d->id, "bubble-beam")) {
                    if (shots < 3) shots = 3;
                }
                ms->burst_left = shots - 1;
                ms->burst_t = 0.14f;
            }
            cast_move(ms, player);
        }
    }


    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &g_game.projectiles[i];
        if (!p->active) continue;
        p->life -= dt;
        if (p->life <= 0.0f) { p->active = false; continue; }
        p->anim_frame = (int)((p->max_life - p->life) * 14.0f);
        if (p->is_orbit) {
            p->orbit_angle += p->orbit_speed * dt;
            p->x = player->x + cosf(p->orbit_angle) * p->orbit_radius;
            p->y = (player->y - 16.0f) + sinf(p->orbit_angle) * p->orbit_radius;
            if (p->spin_speed != 0.0f) p->angle += p->spin_speed * dt;
            else p->angle = p->orbit_angle + 1.5707963f;
        } else {
            if (p->vfx_shape && !strcmp(p->vfx_shape, "aqua_ring")) {
                p->x = player->x;
                p->y = player->y;
                p->angle = 0.0f;
            } else if (p->boom && p->life < p->max_life * 0.5f) {
                float dx = player->x - p->x, dy = (player->y - 18.0f) - p->y, l = sqrtf(dx*dx + dy*dy), sp = sqrtf(p->vx*p->vx + p->vy*p->vy);
                if (l > 1.0f) { p->vx = dx / l * sp; p->vy = dy / l * sp; }
                if (p->boom == 1) { memset(p->hit_cd, 0, sizeof(p->hit_cd)); p->boom = 2; }
                if (l < 14.0f) { p->active = false; continue; }
            } else if (p->homing && !p->is_enemy && p->damage > 0) {
                Enemy *t = find_nearest_enemy(p->x, p->y, 260.0f, NULL);
                if (t) {
                    float sp = sqrtf(p->vx*p->vx + p->vy*p->vy), want = atan2f(t->y - p->y, t->x - p->x), cur = atan2f(p->vy, p->vx), df = want - cur;
                    while (df > PI_F) df -= 2*PI_F; while (df < -PI_F) df += 2*PI_F;
                    cur += df * fminf(1.0f, 5.0f * dt); p->vx = cosf(cur) * sp; p->vy = sinf(cur) * sp;
                }
            }
            p->x += p->vx * dt; p->y += p->vy * dt;
            if (p->spin_speed != 0.0f) {
                p->angle += p->spin_speed * dt;
                if (p->angle > 6.2831853f) p->angle -= 6.2831853f;
                else if (p->angle < -6.2831853f) p->angle += 6.2831853f;
            } else if (p->damage > 0 && (p->vx != 0.0f || p->vy != 0.0f)) {
                p->angle = atan2f(p->vy, p->vx);
            }
        }
        if (p->damage <= 0) continue;

        if (p->is_enemy) {
            float pdx = player->x - p->x;
            float pdy = (player->y - 14.0f) - p->y;
            float prs = p->radius + 11.0f;
            if (pdx * pdx + pdy * pdy < prs * prs) {
                if (player->iframe_timer <= 0.0f) {
                    float taken = fmaxf(1.0f, (float)p->damage - (float)player->armor);
                    player->hp -= taken;
                    player->iframe_timer = 0.35f;
                    g_game.screen_shake = 0.20f;
                    audio_play_sfx(SFX_HIT);
                    game_spawn_damage_popup(player->x, player->y - 20.0f, (int)taken, RGBA8_C(255, 60, 60, 255));
                    if (player->hp <= 0.0f) {
                        if (g_game.revives_left > 0) {
                            game_trigger_revive(player);
                        } else {
                            g_game.state = STATE_GAME_OVER;
                            g_game.grace_timer = 1.0f;
                            audio_stop_bgm();
                            audio_play_sfx(SFX_GAMEOVER);
                            g_save.stats_runs++;
                            g_save.coins += g_game.coins_collected;
                            g_save.tickets += g_game.tickets_collected;
                            g_save.stats_kills += player->kills;
                            if (g_game.run_timer > g_save.stats_best_time) g_save.stats_best_time = g_game.run_timer;
                            if (player->level > g_save.stats_best_level) g_save.stats_best_level = player->level;
                            save_system_save();
                        }
                    }
                }
                p->active = false;
            }
            continue;
        }

        int cd_dec = (int)(dt * 60.0f + 0.5f); if (cd_dec < 1) cd_dec = 1;
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (p->hit_cd[e] > 0) { p->hit_cd[e] = (p->hit_cd[e] > cd_dec && p->hit_cd[e] < 255) ? (uint8_t)(p->hit_cd[e] - cd_dec) : (p->hit_cd[e] == 255 ? 255 : 0); continue; }
            Enemy *en = &g_game.enemies[e]; if (!en->active) continue;
            float dx = en->x - p->x, dy = en->y - p->y, rs = p->radius + en->radius;
            if (dx*dx + dy*dy < rs*rs) {
                p->hit_cd[e] = p->is_orbit ? 18 : (p->boom ? 40 : 255);
                float kn = p->def ? (p->def->knock > 20 ? p->def->knock : 55.0f) : 55.0f;
                hit_enemy(en, (float)p->damage, dx, dy, kn, p->def, p->color, true);
                if (!p->is_orbit && !p->boom) { p->hits_left--; if (p->hits_left <= 0) { p->active = false; break; } }
            }
        }
    }
}

void moves_preload_move(int move_id) {
    if (move_id < 0 || move_id >= MOVEDB_TOTAL) return;
    const MoveDef *d = &MOVE_DB[move_id];
    const char *sh = move_vfx_shape(d);
    render_preload_vfx(sh);
    if (!strcmp(d->id, "meteor-mash")) render_preload_vfx("meteor_hit");
    if (!strcmp(d->id, "night-shade")) render_preload_vfx("night_shade_aoe");
    if (!strcmp(d->id, "substitute"))  render_preload_vfx("sub_orb");
    if ((d->kind == MK_BEAM || !strcmp(d->id, "bubble-beam") || !strcmp(d->id, "octazooka")) && sh && sh[0] != '\0') {
        char seg[48];
        snprintf(seg, sizeof(seg), "%s_start", sh); render_preload_vfx(seg);
        snprintf(seg, sizeof(seg), "%s_mid",   sh); render_preload_vfx(seg);
        snprintf(seg, sizeof(seg), "%s_end",   sh); render_preload_vfx(seg);
    }
}

void moves_preload_assets(void) {
    for (int m = 0; m < g_game.player.move_count; m++) {
        moves_preload_move(g_game.player.moves[m].id);
    }

    if (g_game.selected_starter >= 0 && g_game.selected_starter < TOTAL_CATALOG_POKEMON) {
        int preloaded_cnt = 0;
        for (int m = 0; m < TOTAL_MOVES && preloaded_cnt < 32; m++) {
            if (is_move_compatible(g_game.selected_starter, m)) {
                moves_preload_move(m);
                preloaded_cnt++;
            }
        }
    }
    static const char * const CORE_SHAPES[] = {
        "impact", "leaf", "ember", "water_gun", "shadow_ball", "egg",
        "boom_fire", "boom_water", "boom_electric", "boom_grass",
        "boom_ice", "boom_psychic", "boom_dark", "boom_normal",
        "slash_crescent", "crack", "slamdust", "meteor_hit", "night_shade_aoe", "sub_orb"
    };
    for (size_t e = 0; e < sizeof(CORE_SHAPES) / sizeof(CORE_SHAPES[0]); e++) {
        render_preload_vfx(CORE_SHAPES[e]);
    }
}
