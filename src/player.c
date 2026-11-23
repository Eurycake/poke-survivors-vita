#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "game.h"
#include "audio.h"
#include "save_system.h"
#include "terrain.h"
#ifndef _WIN32
#include <strings.h>
#define _stricmp strcasecmp
#endif

static int get_direction_from_vec(float vx, float vy, int old_dir) {
    if (fabsf(vx) < 0.05f && fabsf(vy) < 0.05f) return old_dir;
    float a = atan2f(vy, vx);

    int nd = (int)floorf((1.57079632679f - a) / 0.78539816339f + 0.5f);
    return ((nd % 8) + 8) % 8;
}

int player_calc_xp_needed(int level) {
    if (level < 1) level = 1;
    if (level <= 15) {
        return 14 + (level * 18);
    } else if (level <= 35) {
        return 284 + ((level - 15) * 32);
    } else {
        return 924 + ((level - 35) * 48);
    }
}

void player_recalc_stats(Player *p) {
    const PokemonData *base = &POKEMON_CATALOG[p->char_idx];


    float shop_hp_pct = (float)g_save.upgrades[0] * 0.05f;
    if (shop_hp_pct > 0.50f) shop_hp_pct = 0.50f;
    float shop_atk    = (float)g_save.upgrades[1] * 0.04f;
    float shop_spd    = (float)g_save.upgrades[2] * 0.03f;
    float shop_cd     = (float)g_save.upgrades[3] * 0.03f;
    int   shop_armor  = (g_save.upgrades[4] + 1) / 2;
    float shop_regen  = (float)g_save.upgrades[5] * 0.20f;
    float shop_mag    = (float)g_save.upgrades[6] * 8.0f;
    float shop_xp     = (float)g_save.upgrades[7] * 0.05f;
    float shop_crit   = (float)g_save.upgrades[9] * 0.03f;

    float prev_max_hp = p->max_hp;


    float starter_hp_base = (float)base->base_hp;
    if (starter_hp_base < 60.0f) starter_hp_base = 60.0f;
    float base_plus_shop_hp = floorf(starter_hp_base * (1.0f + shop_hp_pct) + 0.5f);
    p->max_hp = base_plus_shop_hp * (1.0f + (float)p->passives[4] * 0.20f);


    p->regen = ((float)p->passives[2] * 0.40f) + shop_regen;


    p->armor = p->passives[4] + shop_armor;
    if (p->armor > 10) p->armor = 10;
    p->def_mult = 1.0f - (float)p->armor * 0.05f;
    if (p->def_mult < 0.25f) p->def_mult = 0.25f;


    float starter_spd_base = base->base_speed;
    p->speed = starter_spd_base * (1.0f + (float)p->passives[1] * 0.10f + shop_spd);


    float starter_atk_base = base->base_atk;
    p->atk_mult = starter_atk_base * (1.0f + (float)p->passives[0] * 0.10f + shop_atk);


    p->cd_mult = 1.0f - ((float)p->passives[5] * 0.08f + shop_cd);


    p->area_mult = 1.0f + (float)p->passives[3] * 0.10f;


    p->proj_speed_mult = 1.0f + (float)p->passives[1] * 0.10f;


    p->bonus_amount = (p->passives[7] >= 5) ? 3 : ((p->passives[7] >= 4) ? 2 : ((p->passives[7] >= 2) ? 1 : 0));


    p->magnet_radius = 45.0f + (float)p->passives[3] * 25.0f + shop_mag;


    p->crit_chance = 0.05f + (float)p->passives[6] * 0.05f + shop_crit;
    p->crit_mult   = 2.00f + (float)p->passives[6] * 0.20f;


    p->growth_mult = 1.0f + (float)p->passives[7] * 0.08f + shop_xp;


    const char *pd = base->passive_desc ? base->passive_desc : "";
    if (strstr(pd, "+2.5 HP/sec"))              p->regen += 2.5f;
    if (strstr(pd, "-15% Move Cooldown") || strstr(pd, "-15% Attack Cooldown")) p->cd_mult *= 0.85f;
    if (strstr(pd, "+30% Radius"))              p->area_mult *= 1.30f;
    if (strstr(pd, "+25% Attack Damage"))       p->atk_mult *= 1.25f;
    if (strstr(pd, "+20% Melee Damage"))        p->atk_mult *= 1.20f;
    if (strstr(pd, "+25% Crit"))                { p->crit_chance += 0.25f; p->atk_mult *= 1.15f; }
    if (strstr(pd, "Fast Aura Disc & Crit"))    { p->crit_chance += 0.15f; p->proj_speed_mult *= 1.25f; }
    if (strstr(pd, "+20% Move Speed"))          p->speed *= 1.20f;
    if (strstr(pd, "+25% Move Speed"))          { p->speed *= 1.25f; p->armor += 1; }
    if (strstr(pd, "High Speed"))               { p->speed *= 1.22f; p->proj_speed_mult *= 1.20f; }
    if (strstr(pd, "+35% Damage Reduction"))    { p->armor += 5; p->def_mult *= 0.65f; }
    if (strstr(pd, "Shell Armor") || strstr(pd, "Armor Shell") || strstr(pd, "Frost Armor") || strstr(pd, "Venom Armor") || strstr(pd, "Rock Armor")) p->armor += 2;
    if (strstr(pd, "Fairy Radiance"))           { p->regen += 1.2f; p->area_mult *= 1.15f; }
    if (strstr(pd, "Seeking Shadow Spheres"))   { p->proj_speed_mult *= 1.20f; p->armor += 1; }
    if (strstr(pd, "Learns All Move Types"))    { p->cd_mult *= 0.88f; p->atk_mult *= 1.15f; }
    int dex = base->dex;
    if (dex == 493) p->cd_mult *= 0.85f;


    if (g_game.run_mode == MODE_TURBO) {
        p->speed *= 1.50f;
        p->proj_speed_mult *= 1.50f;
    }


    if (p->max_hp > 500.0f) p->max_hp = 500.0f;
    if (p->regen > 5.0f) p->regen = 5.0f;
    if (p->armor > 10) p->armor = 10;
    float spd_cap = (g_game.run_mode == MODE_TURBO) ? 350.0f : 280.0f;
    if (p->speed > spd_cap) p->speed = spd_cap;
    if (p->atk_mult > 3.0f) p->atk_mult = 3.0f;
    if (p->cd_mult < 0.30f) p->cd_mult = 0.30f;
    if (p->area_mult > 2.5f) p->area_mult = 2.5f;
    if (p->proj_speed_mult > 2.5f) p->proj_speed_mult = 2.5f;
    if (p->bonus_amount > 3) p->bonus_amount = 3;
    if (p->magnet_radius > 250.0f) p->magnet_radius = 250.0f;
    if (p->crit_chance > 0.50f) p->crit_chance = 0.50f;
    if (p->crit_mult > 3.0f) p->crit_mult = 3.0f;
    if (p->growth_mult > 2.0f) p->growth_mult = 2.0f;

    if (prev_max_hp > 0.0f && p->max_hp > prev_max_hp && p->hp > 0.0f) {
        p->hp = fminf(p->max_hp, p->hp + (p->max_hp - prev_max_hp));
    }
}

void player_check_evolution(Player *p) {
    if (p->char_idx < 0 || p->char_idx >= TOTAL_CATALOG_POKEMON) return;
    int cur_dex = POKEMON_CATALOG[p->char_idx].dex;
    if (cur_dex <= 0 || cur_dex >= 1025) return;
    int target_dex = POKEMON_EVO_TARGET[cur_dex];
    if (target_dex <= 0 || target_dex >= 1025) return;


    int req_lvl = 15;
    if (POKEMON_EVO_TARGET[target_dex] > 0) {

        req_lvl = 10;
    } else {
        bool is_stage2 = false;
        for (int d = 1; d < 1025; d++) {
            if (POKEMON_EVO_TARGET[d] == cur_dex) {
                is_stage2 = true;
                break;
            }
        }
        if (is_stage2) {
            req_lvl = 22;
        } else {
            req_lvl = 15;
        }
    }

    if (POKEMON_EVO_LEVEL[cur_dex] > 0 && POKEMON_EVO_LEVEL[cur_dex] < req_lvl) {
        req_lvl = POKEMON_EVO_LEVEL[cur_dex];
    }

    if (p->level >= req_lvl) {
        int new_cat_idx = POKEMON_DEX_TO_CATALOG[target_dex];
        if (new_cat_idx >= 0 && new_cat_idx < TOTAL_CATALOG_POKEMON) {
            snprintf(g_game.evolution_old_name, sizeof(g_game.evolution_old_name), "%s", POKEMON_CATALOG[p->char_idx].name);
            snprintf(g_game.evolution_new_name, sizeof(g_game.evolution_new_name), "%s", POKEMON_CATALOG[new_cat_idx].name);
            g_game.evolution_old_dex = cur_dex;
            g_game.evolution_new_dex = target_dex;
            g_game.evolution_banner_timer = 4.0f;

            p->char_idx = new_cat_idx;
            save_system_unlock_dex(target_dex);


            player_recalc_stats(p);
            p->max_hp = fminf(500.0f, p->max_hp + 45.0f);
            p->atk_mult = fminf(3.0f, p->atk_mult * 1.20f);
            p->hp = p->max_hp;

            g_game.screen_shake = 0.5f;
            game_spawn_particles(p->x, p->y, RGBA8_C(255, 230, 80, 255), 45, 220.0f);
            audio_play_sfx(SFX_EVOLUTION);
        }
    }
}

void player_play_attack_anim(Player *p, int anim_type, float duration) {
    p->anim_state = anim_type;
    p->attack_anim_timer = duration;
    p->anim_frame = 0;
    p->anim_timer = 0.0f;
}

void player_init(Player *p, int starter_idx) {
    memset(p, 0, sizeof(Player));
    p->char_idx = starter_idx;
    p->x = WORLD_W * 0.5f;
    p->y = WORLD_H * 0.5f;
    p->dir = 0;
    p->move_angle = 1.570796f;
    p->has_move_angle = false;
    p->level = 1;
    p->exp = 0;
    p->exp_to_next = player_calc_xp_needed(1);
    p->anim_state = ANIM_IDLE;

    player_recalc_stats(p);
    p->hp = p->max_hp;
    p->iframe_timer = 0.0f;
    p->lunge_t = 0.0f;
    p->lunge_dur = 0.0f;
    p->lunge_dx = 0.0f;
    p->lunge_dy = 0.0f;
    p->lunge_ox = 0.0f;
    p->lunge_oy = 0.0f;


    const PokemonData *pcat = &POKEMON_CATALOG[starter_idx];
    int t1 = -1, t2 = -1;
    for (int ti = 0; ti < 18; ti++) {
        if (pcat->type1 && _stricmp(pcat->type1, MOVE_TYPE_NAMES[ti]) == 0) t1 = ti;
        if (pcat->type2 && _stricmp(pcat->type2, MOVE_TYPE_NAMES[ti]) == 0) t2 = ti;
    }
    int elem_moves[MOVEDB_TOTAL];
    int elem_count = 0;
    int elem_dashes[MOVEDB_TOTAL];
    int dash_count = 0;
    int quick_attack_id = 1;
    for (int mi = 0; mi < MOVEDB_TOTAL; mi++) {
        const MoveDef *md = &MOVE_DB[mi];
        if (strcmp(md->id, "quick-attack") == 0) quick_attack_id = mi;
        if (md->kind == MK_DASH) {
            if (md->type == t1 || (t2 >= 0 && md->type == t2) || pcat->dex == 151 || pcat->dex == 493) {
                elem_dashes[dash_count++] = mi;
            }
            continue;
        }
        if (md->kind == MK_BUFF || md->dmg <= 0.0f) continue;
        if (strcmp(md->id, "dream-eater") == 0 || strcmp(md->id, "substitute") == 0) continue;
        if (md->type == t1 || (t2 >= 0 && md->type == t2) || pcat->dex == 151 || pcat->dex == 493) {
            elem_moves[elem_count++] = mi;
        }
    }
    int start_m = pcat->starting_move;
    if (elem_count > 0) {
        start_m = elem_moves[rand() % elem_count];
    } else if (start_m < 0 || start_m >= MOVEDB_TOTAL || MOVE_DB[start_m].kind == MK_DASH) {
        start_m = 0;
    }
    moves_init_state(&p->moves[0], start_m);


    int dash_m = quick_attack_id;
    if (dash_count > 0) {
        dash_m = elem_dashes[rand() % dash_count];
    }
    moves_init_state(&p->moves[1], dash_m);
    p->move_count = 2;
}

void player_update(Player *p, float dt, const InputState *input) {

    if (input->move_x * input->move_x + input->move_y * input->move_y > 0.01f) {
        p->move_angle = atan2f(input->move_y, input->move_x);
        p->has_move_angle = true;
    }


    if (p->regen > 0.0f && p->hp < p->max_hp) {
        p->hp += p->regen * dt;
        if (p->hp > p->max_hp) p->hp = p->max_hp;
    }


    if (p->dash_input_buf > 0.0f) {
        p->dash_input_buf -= dt;
        if (p->dash_input_buf < 0.0f) p->dash_input_buf = 0.0f;
    }


    if (p->dash_timer > 0.0f) {
        float step_dt = (dt < p->dash_timer) ? dt : p->dash_timer;
        p->dash_timer -= dt;
        p->is_moving = true;

        float edge_m = (g_game.selected_stage >= BAKED_FIRST_STAGE) ? 32.0f : 148.0f;
        float nx = p->x + p->dash_vx * step_dt;
        float ny = p->y + p->dash_vy * step_dt;
        if (nx < edge_m) nx = edge_m;
        if (nx > WORLD_W - edge_m) nx = WORLD_W - edge_m;
        if (ny < edge_m) ny = edge_m;
        if (ny > WORLD_H - edge_m) ny = WORLD_H - edge_m;

        p->x = nx;
        p->y = ny;


        if (p->dash_dmg > 0.0f) {
            for (int e = 0; e < MAX_ENEMIES; e++) {
                if (p->dash_hit_enemy[e]) continue;
                Enemy *en = &g_game.enemies[e];
                if (!en->active) continue;
                float dx = en->x - p->x, dy = en->y - p->y;
                float rr = p->dash_hit_rad + en->radius;
                if (dx * dx + dy * dy <= rr * rr) {
                    p->dash_hit_enemy[e] = true;
                    float dmg = p->dash_dmg;
                    if (((float)(rand() % 1000) / 1000.0f) < p->crit_chance) dmg *= p->crit_mult;
                    en->hp -= dmg;
                    en->hit_flash = 0.20f;
                    float dl = sqrtf(p->dash_vx * p->dash_vx + p->dash_vy * p->dash_vy);
                    if (p->dash_knock > 0.0f && dl > 1.0f && !en->is_boss) {
                        terrain_apply_enemy_knockback(g_game.selected_stage, &en->x, &en->y, en->radius, en->is_boss,
                                                      (p->dash_vx / dl) * p->dash_knock * 0.40f,
                                                      (p->dash_vy / dl) * p->dash_knock * 0.40f);
                    }
                    game_spawn_damage_popup(en->x, en->y - 16.0f, (int)dmg, RGBA8_C(80, 245, 255, 255));
                    game_spawn_particles(en->x, en->y, RGBA8_C(80, 245, 255, 255), 5, 110.0f);
                }
            }
        }


        if (p->dash_evolved) {
            p->dash_trail_timer -= step_dt;
            if (p->dash_trail_timer <= 0.0f) {
                p->dash_trail_timer += 0.05f;
                for (int zi = 0; zi < MAX_ZONES; zi++) {
                    if (!g_game.zones[zi].active) {
                        Zone *z = &g_game.zones[zi];
                        memset(z, 0, sizeof(Zone));
                        z->active = true;
                        z->mode = 2;
                        z->x = p->x;
                        z->y = p->y;
                        z->life = 2.2f;
                        z->t = 2.2f;
                        z->radius = 38.0f * p->area_mult;
                        z->dmg = (int)fmaxf(14.0f, p->dash_dmg * 0.55f);
                        z->rate = 0.25f;
                        z->tick = 0.05f;
                        int mid = (p->dash_move_id >= 0 && p->dash_move_id < MOVEDB_TOTAL) ? p->dash_move_id : 1;
                        z->def = &MOVE_DB[mid];
                        z->proj = -1;
                        uint32_t tcol = MOVE_TYPE_COLOR[z->def->type];
                        game_spawn_particles(z->x, z->y, tcol, 6, 95.0f);
                        break;
                    }
                }
            }
        }


        if (p->dash_timer <= 0.0f) {
            p->dash_timer = 0.0f;
            float edge_m = (g_game.selected_stage >= BAKED_FIRST_STAGE) ? 32.0f : 148.0f;
            int stg = g_game.selected_stage;
            if (terrain_box_blocked(stg, p->x, p->y, 10.0f, 16.0f, 6.0f)) {
                float dl = sqrtf(p->dash_vx * p->dash_vx + p->dash_vy * p->dash_vy);
                float dx_u = (dl > 1.0f) ? (p->dash_vx / dl) : 0.0f;
                float dy_u = (dl > 1.0f) ? (p->dash_vy / dl) : 1.0f;
                bool cleared = false;

                for (int s = 1; s <= 12; s++) {
                    float tx = p->x + dx_u * (float)(s * 14);
                    float ty = p->y + dy_u * (float)(s * 14);
                    if (tx < edge_m || tx > WORLD_W - edge_m || ty < edge_m || ty > WORLD_H - edge_m) break;
                    if (!terrain_box_blocked(stg, tx, ty, 10.0f, 16.0f, 6.0f)) {
                        p->x = tx; p->y = ty;
                        cleared = true;
                        break;
                    }
                }

                for (int r = 1; r <= 8 && !cleared; r++) {
                    for (int a = 0; a < 16; a++) {
                        float ang = (float)a * (6.2831853f / 16.0f);
                        float tx = p->x + cosf(ang) * (float)(r * 16);
                        float ty = p->y + sinf(ang) * (float)(r * 16);
                        if (tx < edge_m || tx > WORLD_W - edge_m || ty < edge_m || ty > WORLD_H - edge_m) continue;
                        if (!terrain_box_blocked(stg, tx, ty, 10.0f, 16.0f, 6.0f)) {
                            p->x = tx; p->y = ty;
                            cleared = true;
                            break;
                        }
                    }
                }
            }
            p->iframe_timer = fmaxf(p->iframe_timer, p->dash_grace_iframe);
            if (p->dash_evolved && p->move_count > 0 && MOVE_DB[p->moves[0].id].kind != MK_DASH) {
                p->moves[0].timer = 0.0f;
            }
        }
    } else if (p->jump_timer > 0.0f) {
        p->jump_timer -= dt;
        p->x += p->jump_vx * dt;
        p->y += p->jump_vy * dt;
        p->is_moving = true;
    } else {
        static float s_ice_dx = 0.0f, s_ice_dy = 0.0f;
        static bool s_on_ice = false;
        static const float DIR_VX[8] = { 0.0f, 0.7071f, 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f };
        static const float DIR_VY[8] = { 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f, 0.0f, 0.7071f };

        float prev_x = p->x, prev_y = p->y;
        float vx = input->move_x;
        float vy = input->move_y;
        float len = sqrtf(vx * vx + vy * vy);
        bool on_ice_now = (terrain_is_smooth_ice(g_game.selected_stage, p->x, p->y) != 0);

        if (on_ice_now) {
            if (!s_on_ice) {
                if (len > 0.01f) {
                    s_ice_dx = vx / len;
                    s_ice_dy = vy / len;
                } else {
                    int d = (p->dir >= 0 && p->dir < 8) ? p->dir : 0;
                    s_ice_dx = DIR_VX[d];
                    s_ice_dy = DIR_VY[d];
                }
                s_on_ice = true;
            } else if (len > 0.01f) {

                float steer = 2.4f * dt;
                if (steer > 0.35f) steer = 0.35f;
                s_ice_dx += (vx / len - s_ice_dx) * steer;
                s_ice_dy += (vy / len - s_ice_dy) * steer;
                float sl = sqrtf(s_ice_dx * s_ice_dx + s_ice_dy * s_ice_dy);
                if (sl > 0.01f) { s_ice_dx /= sl; s_ice_dy /= sl; }
            }
            float slide_spd = p->speed * 1.08f;
            float nx = p->x + s_ice_dx * slide_spd * dt;
            float ny = p->y + s_ice_dy * slide_spd * dt;
            bool moved_x = false, moved_y = false;
            if (!terrain_box_blocked(g_game.selected_stage, nx, p->y, 7.0f, 8.0f, 5.0f)) {
                p->x = nx; moved_x = true;
            }
            if (!terrain_box_blocked(g_game.selected_stage, p->x, ny, 7.0f, 8.0f, 5.0f)) {
                p->y = ny; moved_y = true;
            }
            if (!moved_x && !moved_y) {
                s_on_ice = false;
            }
            p->dir = get_direction_from_vec(s_ice_dx, s_ice_dy, p->dir);
            p->is_moving = (moved_x || moved_y);
        } else {
            s_on_ice = false;
            if (len > 0.01f) {
                vx /= len;
                vy /= len;
                float step = p->speed * dt;
                float nx = p->x + vx * step;
                float ny = p->y + vy * step;

                int tx_hit = terrain_at(g_game.selected_stage, nx + (vx > 0.0f ? 8.0f : -8.0f), p->y);
                int ty_hit = terrain_at(g_game.selected_stage, p->x, ny + (vy > 0.0f ? 6.0f : -9.0f));

                if (!terrain_box_blocked(g_game.selected_stage, nx, p->y, 7.0f, 8.0f, 5.0f)) {
                    p->x = nx;
                } else if ((tx_hit == TER_LEDGE_E && vx > 0.20f) || (tx_hit == TER_LEDGE_W && vx < -0.20f)) {
                    float sx_dir = (vx > 0.0f) ? 1.0f : -1.0f;
                    int land = terrain_at(g_game.selected_stage, p->x + sx_dir * 68.0f, p->y);
                    if (!terrain_is_blocked(land)) {
                        p->jump_timer = 0.34f;
                        p->jump_vx = sx_dir * 200.0f;
                        p->jump_vy = 0.0f;
                        p->dir = (sx_dir > 0.0f) ? 2 : 6;
                    }
                } else if (fabsf(vy) < 0.25f) {

                    bool free_up = !terrain_box_blocked(g_game.selected_stage, nx, p->y - 12.0f, 7.0f, 8.0f, 5.0f);
                    bool free_dn = !terrain_box_blocked(g_game.selected_stage, nx, p->y + 12.0f, 7.0f, 8.0f, 5.0f);
                    if (free_up && !free_dn && !terrain_box_blocked(g_game.selected_stage, p->x, p->y - step * 0.75f, 7.0f, 8.0f, 5.0f))
                        p->y -= step * 0.75f;
                    else if (free_dn && !free_up && !terrain_box_blocked(g_game.selected_stage, p->x, p->y + step * 0.75f, 7.0f, 8.0f, 5.0f))
                        p->y += step * 0.75f;
                }

                if (p->jump_timer <= 0.0f) {
                    if (!terrain_box_blocked(g_game.selected_stage, p->x, ny, 7.0f, 8.0f, 5.0f)) {
                        p->y = ny;
                    } else if ((ty_hit == TER_LEDGE_S && vy > 0.20f) || (ty_hit == TER_LEDGE_N && vy < -0.20f)) {
                        float sy_dir = (vy > 0.0f) ? 1.0f : -1.0f;
                        int land = terrain_at(g_game.selected_stage, p->x, p->y + sy_dir * 68.0f);
                        if (!terrain_is_blocked(land)) {
                            p->jump_timer = 0.34f;
                            p->jump_vx = 0.0f;
                            p->jump_vy = sy_dir * 200.0f;
                            p->dir = (sy_dir > 0.0f) ? 0 : 4;
                        }
                    } else if (fabsf(vx) < 0.25f) {

                        bool free_lt = !terrain_box_blocked(g_game.selected_stage, p->x - 12.0f, ny, 7.0f, 8.0f, 5.0f);
                        bool free_rt = !terrain_box_blocked(g_game.selected_stage, p->x + 12.0f, ny, 7.0f, 8.0f, 5.0f);
                        if (free_lt && !free_rt && !terrain_box_blocked(g_game.selected_stage, p->x - step * 0.75f, p->y, 7.0f, 8.0f, 5.0f))
                            p->x -= step * 0.75f;
                        else if (free_rt && !free_lt && !terrain_box_blocked(g_game.selected_stage, p->x + step * 0.75f, p->y, 7.0f, 8.0f, 5.0f))
                            p->x += step * 0.75f;
                    }
                }
                (void)prev_x; (void)prev_y;
                if (p->jump_timer <= 0.0f) p->dir = get_direction_from_vec(vx, vy, p->dir);
                p->is_moving = true;
            } else {
                p->is_moving = false;
            }
        }
    }


    if (p->attack_anim_timer > 0.0f) {
        p->attack_anim_timer -= dt;
        p->anim_timer += dt * 14.0f;
        if (p->anim_timer >= 1.0f) {
            p->anim_timer -= 1.0f;
            p->anim_frame++;
        }
        if (p->attack_anim_timer <= 0.0f) {
            p->anim_state = p->is_moving ? ANIM_WALK : ANIM_IDLE;
            p->anim_frame = 0;
        }
    } else {
        p->anim_state = p->is_moving ? ANIM_WALK : ANIM_IDLE;
        float rate = p->is_moving ? 10.0f : 5.0f;
        p->anim_timer += dt * rate;
        if (p->anim_timer >= 1.0f) {
            p->anim_timer -= 1.0f;
            p->anim_frame++;
        }
    }


    if (p->dash_timer <= 0.0f) {
        for (int i = 0; i < MAX_PROPS; i++) {
            const WorldProp *pr = &g_game.props[i];
            if (!pr->active) continue;
            float dx = p->x - pr->x, dy = p->y - pr->y;
            if (dx > 40.0f || dx < -40.0f || dy > 40.0f || dy < -40.0f) continue;
            float r;
            if (pr->type <= PROP_TREE_DARK) r = 10.0f;
            else if (pr->type == PROP_ROCK_A) r = 12.0f;
            else if (pr->type == PROP_ROCK_B || pr->type == PROP_ROCK_ICE) r = 9.0f;
            else if (pr->type == PROP_CACTUS_A || pr->type == PROP_CACTUS_B) r = 8.0f;
            else if (pr->type == PROP_BERRY_TREE) r = 10.0f;
            else continue;
            r += 8.0f;
            float d2 = dx * dx + dy * dy;
            if (d2 < r * r) {
                if (d2 < 0.001f) { dx = 0.0f; dy = 1.0f; d2 = 1.0f; }
                float d = sqrtf(d2), push = (r - d) / d;
                float px_new = p->x + dx * push;
                float py_new = p->y + dy * push;
                if (!terrain_box_blocked(g_game.selected_stage, px_new, p->y, 7.0f, 8.0f, 5.0f)) p->x = px_new;
                if (!terrain_box_blocked(g_game.selected_stage, p->x, py_new, 7.0f, 8.0f, 5.0f)) p->y = py_new;
            }
        }
    }


    float edge_m = (g_game.selected_stage >= BAKED_FIRST_STAGE) ? 16.0f : 148.0f;
    if (p->x < edge_m) p->x = edge_m;
    if (p->y < edge_m) p->y = edge_m;
    if (p->x > WORLD_W - edge_m) p->x = WORLD_W - edge_m;
    if (p->y > WORLD_H - edge_m) p->y = WORLD_H - edge_m;


    if (p->iframe_timer > 0.0f) {
        p->iframe_timer -= dt;
        if (p->iframe_timer < 0.0f) p->iframe_timer = 0.0f;
    }


    if (p->lunge_t > 0.0f) {
        p->lunge_t -= dt;
        if (p->lunge_t <= 0.0f) {
            p->lunge_t = 0.0f;
            p->lunge_ox = 0.0f;
            p->lunge_oy = 0.0f;
        } else {
            float dur = p->lunge_dur > 0.01f ? p->lunge_dur : 0.16f;
            float progress = 1.0f - (p->lunge_t / dur);
            if (progress < 0.0f) progress = 0.0f;
            if (progress > 1.0f) progress = 1.0f;
            float pulse = sinf(progress * 3.14159265f);
            p->lunge_ox = p->lunge_dx * pulse;
            p->lunge_oy = p->lunge_dy * pulse;

            p->dir = get_direction_from_vec(p->lunge_dx, p->lunge_dy, p->dir);
        }
    } else {
        p->lunge_ox = 0.0f;
        p->lunge_oy = 0.0f;
    }
}
