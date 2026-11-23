#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "save_system.h"
#include "assets_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __vita__
#include <psp2/io/stat.h>
static const char *SAVE_FILE_PATH = "ux0:data/poke-survivors vita/save.dat";
#else
static const char *SAVE_FILE_PATH = "save.dat";
#endif

SaveData g_save;

void save_system_init(void) {
    memset(&g_save, 0, sizeof(SaveData));
    g_save.magic = SAVE_MAGIC;
    g_save.version = SAVE_VERSION;
    g_save.coins = 0;
    g_save.tickets = 0;
    g_save.selected_starter = 0;


    static const int DEFAULT_UNLOCKED[] = {
        1, 4, 7, 172,
        152, 155, 158,
        252, 255, 258,
        387, 390, 393,
        495, 498, 501,
        650, 653, 656,
        722, 725, 728,
        810, 813, 816,
        906, 909, 912
    };
    for (int i = 0; i < (int)(sizeof(DEFAULT_UNLOCKED)/sizeof(DEFAULT_UNLOCKED[0])); i++) {
        int d = DEFAULT_UNLOCKED[i];
        if (d >= 0 && d < 1024) {
            g_save.owned_mask[d / 32] |= (1u << (d % 32));
        }
    }
    g_save.stats_best_level = 1;

    save_system_load();
}

void save_system_load(void) {
    FILE *f = fopen(SAVE_FILE_PATH, "rb");
#ifdef __vita__
    if (!f) f = fopen("ux0:data/poke-survivor/save.dat", "rb");
#endif
    if (!f) {
        save_system_save();
        return;
    }

    SaveData temp;
    memset(&temp, 0, sizeof(SaveData));
    size_t read_bytes = fread(&temp, 1, sizeof(SaveData), f);
    fclose(f);

    if (read_bytes == sizeof(SaveData) && temp.magic == SAVE_MAGIC && temp.version == SAVE_VERSION) {
        memcpy(&g_save, &temp, sizeof(SaveData));
    } else if (read_bytes >= sizeof(SaveData) - sizeof(temp.goal_tier) && temp.magic == SAVE_MAGIC && temp.version == 2) {

        memcpy(&g_save, &temp, sizeof(SaveData) - sizeof(temp.goal_tier));
        g_save.version = SAVE_VERSION;
        memset(g_save.goal_tier, 0, sizeof(g_save.goal_tier));
        for (int i = 0; i < TOTAL_GOALS; i++) {
            if (g_save.claimed_goals_mask & (1u << i)) g_save.goal_tier[i] = 1;
        }
        save_system_save();
    } else {
        save_system_save();
    }
}

void save_system_save(void) {
#ifdef __vita__
    sceIoMkdir("ux0:data/poke-survivors vita", 0777);
#endif
    FILE *f = fopen(SAVE_FILE_PATH, "wb");
    if (f) {
        fwrite(&g_save, 1, sizeof(SaveData), f);
        fclose(f);
    }
}

bool save_system_is_owned(int pokemon_catalog_idx) {
    if (pokemon_catalog_idx < 0 || pokemon_catalog_idx >= TOTAL_CATALOG_POKEMON) return false;
    int dex = POKEMON_CATALOG[pokemon_catalog_idx].dex;
    if (dex < 0 || dex >= 1024) return false;
    return (g_save.owned_mask[dex / 32] & (1u << (dex % 32))) != 0;
}

void save_system_unlock_pokemon(int pokemon_catalog_idx) {
    if (pokemon_catalog_idx >= 0 && pokemon_catalog_idx < TOTAL_CATALOG_POKEMON) {
        int dex = POKEMON_CATALOG[pokemon_catalog_idx].dex;
        if (dex >= 0 && dex < 1024) {
            g_save.owned_mask[dex / 32] |= (1u << (dex % 32));
            save_system_save();
        }
    }
}

void save_system_unlock_dex(int dex) {
    if (dex >= 0 && dex < 1024) {
        g_save.owned_mask[dex / 32] |= (1u << (dex % 32));
        save_system_save();
    }
}

int save_system_get_upgrade_cost(int upgrade_idx) {
    if (upgrade_idx < 0 || upgrade_idx >= TOTAL_UPGRADES) return 999999;
    int cur_lvl = g_save.upgrades[upgrade_idx];
    if (cur_lvl >= G_UPGRADE_DEFS[upgrade_idx].max_level) return 0;
    float base = G_UPGRADE_DEFS[upgrade_idx].base_cost;
    float grow = G_UPGRADE_DEFS[upgrade_idx].growth;
    float cost = base * powf(grow, (float)cur_lvl);
    return ((int)roundf(cost / 10.0f)) * 10;
}

bool save_system_buy_upgrade(int upgrade_idx) {
    if (upgrade_idx < 0 || upgrade_idx >= TOTAL_UPGRADES) return false;
    int cur_lvl = g_save.upgrades[upgrade_idx];
    if (cur_lvl >= G_UPGRADE_DEFS[upgrade_idx].max_level) return false;
    int cost = save_system_get_upgrade_cost(upgrade_idx);
    if (g_save.coins >= cost) {
        g_save.coins -= cost;
        g_save.upgrades[upgrade_idx]++;
        save_system_save();
        return true;
    }
    return false;
}

int save_system_get_goal_progress(int goal_idx) {
    if (goal_idx < 0 || goal_idx >= TOTAL_GOALS) return 0;
    int type = G_GOAL_DEFS[goal_idx].stat_type;
    switch (type) {
        case 0: return g_save.stats_kills;
        case 1: return (int)g_save.stats_best_time;
        case 2: return g_save.stats_best_level;
        case 3: return g_save.stats_bosses;
        case 4: return g_save.stats_chests;
        case 5: return g_save.stats_pulls;
        case 6: return g_save.stats_runs;
        default: return 0;
    }
}

int save_system_get_goal_tier(int goal_idx) {
    if (goal_idx < 0 || goal_idx >= TOTAL_GOALS) return 0;
    return (int)g_save.goal_tier[goal_idx];
}

int save_system_get_goal_target(int goal_idx) {
    if (goal_idx < 0 || goal_idx >= TOTAL_GOALS) return 1;
    int tier = (int)g_save.goal_tier[goal_idx];
    int base = G_GOAL_DEFS[goal_idx].target;
    int st   = G_GOAL_DEFS[goal_idx].stat_type;
    if (tier <= 0) return base;
    if (tier > 20) tier = 20;
    double val;
    if (st == 1) {

        val = (double)base * pow(1.45, (double)tier);
        int s = ((int)((val + 15.0) / 30.0)) * 30;
        return s > base ? s : base + 30 * tier;
    } else if (st == 2) {

        val = (double)base * pow(1.50, (double)tier);
        int lv = ((int)((val + 2.5) / 5.0)) * 5;
        return lv > base ? lv : base + 5 * tier;
    } else {

        val = (double)base * pow(2.2, (double)tier);
        if (val > 2000000000.0) return 2000000000;
        int v = (int)llround(val);
        if (v >= 1000) v = ((v + 50) / 100) * 100;
        else if (v >= 100) v = ((v + 12) / 25) * 25;
        else if (v >= 20) v = ((v + 2) / 5) * 5;
        return v > base ? v : base + tier;
    }
}

int save_system_get_goal_reward_coins(int goal_idx) {
    if (goal_idx < 0 || goal_idx >= TOTAL_GOALS) return 0;
    int tier = (int)g_save.goal_tier[goal_idx];
    if (tier > 20) tier = 20;
    double val = (double)G_GOAL_DEFS[goal_idx].reward_coins * pow(1.65, (double)tier);
    if (val > 500000.0) val = 500000.0;
    int c = ((int)((val + 25.0) / 50.0)) * 50;
    return c > 0 ? c : G_GOAL_DEFS[goal_idx].reward_coins;
}

int save_system_get_goal_reward_tickets(int goal_idx) {
    if (goal_idx < 0 || goal_idx >= TOTAL_GOALS) return 0;
    int tier = (int)g_save.goal_tier[goal_idx];
    int base = G_GOAL_DEFS[goal_idx].reward_tickets;
    if (tier == 0) return base;
    return base + 1 + tier / 2;
}

void save_system_get_goal_text(int goal_idx, char *out, int out_sz) {
    if (!out || out_sz <= 0) return;
    if (goal_idx < 0 || goal_idx >= TOTAL_GOALS) { out[0] = '\0'; return; }
    int tier = (int)g_save.goal_tier[goal_idx];
    int target = save_system_get_goal_target(goal_idx);
    int st = G_GOAL_DEFS[goal_idx].stat_type;
    switch (st) {
        case 0: snprintf(out, (size_t)out_sz, "Defeat %d Pokemon  (Lv.%d)", target, tier + 1); break;
        case 1: snprintf(out, (size_t)out_sz, "Survive %d:%02d in a run  (Lv.%d)", target / 60, target % 60, tier + 1); break;
        case 2: snprintf(out, (size_t)out_sz, "Reach Level %d  (Lv.%d)", target, tier + 1); break;
        case 3: snprintf(out, (size_t)out_sz, "Defeat %d Boss%s  (Lv.%d)", target, target == 1 ? "" : "es", tier + 1); break;
        case 4: snprintf(out, (size_t)out_sz, "Open %d Treasure Chests  (Lv.%d)", target, tier + 1); break;
        case 5: snprintf(out, (size_t)out_sz, "Perform %d Gacha Pulls  (Lv.%d)", target, tier + 1); break;
        default: snprintf(out, (size_t)out_sz, "%s  (Lv.%d)", G_GOAL_DEFS[goal_idx].text, tier + 1); break;
    }
}

bool save_system_is_goal_completed(int goal_idx) {
    if (goal_idx < 0 || goal_idx >= TOTAL_GOALS) return false;
    return save_system_get_goal_progress(goal_idx) >= save_system_get_goal_target(goal_idx);
}

bool save_system_is_goal_claimed(int goal_idx) {
    (void)goal_idx;
    return false;
}

bool save_system_claim_goal(int goal_idx) {
    if (goal_idx < 0 || goal_idx >= TOTAL_GOALS) return false;
    if (!save_system_is_goal_completed(goal_idx)) return false;

    g_save.coins += save_system_get_goal_reward_coins(goal_idx);
    g_save.tickets += save_system_get_goal_reward_tickets(goal_idx);
    if (g_save.goal_tier[goal_idx] < 65000) g_save.goal_tier[goal_idx]++;
    g_save.claimed_goals_mask |= (1u << goal_idx);
    save_system_save();
    return true;
}

bool save_system_is_evolved_dex(int dex) {
    static uint8_t s_evo_target[1025];
    static int s_init = 0;
    if (!s_init) {
        s_init = 1;
        memset(s_evo_target, 0, sizeof(s_evo_target));
        for (int d = 1; d < 1025; d++) {
            int t = POKEMON_EVO_TARGET[d];
            if (t > 0 && t < 1025 && t != d) s_evo_target[t] = 1;
        }
    }
    return (dex > 0 && dex < 1025) ? (s_evo_target[dex] != 0) : false;
}

bool save_system_is_legendary_dex(int dex) {

    if ((dex >= 144 && dex <= 146) || dex == 150 || dex == 151) return true;
    if ((dex >= 243 && dex <= 245) || (dex >= 249 && dex <= 251)) return true;
    if (dex >= 377 && dex <= 386) return true;
    if (dex >= 480 && dex <= 494) return true;
    if (dex >= 638 && dex <= 649) return true;
    if (dex >= 716 && dex <= 721) return true;
    if ((dex >= 772 && dex <= 773) || (dex >= 785 && dex <= 809)) return true;
    if ((dex >= 888 && dex <= 898) || dex == 905) return true;
    if (dex >= 1001 && dex <= 1025) return true;
    return false;
}

bool save_system_is_rare_dex(int dex) {

    static const int RARE_BASE[] = {
        1, 4, 7, 25, 123, 131, 133, 142, 143, 147, 152, 155, 158, 175, 246,
        252, 255, 258, 280, 359, 371, 374, 387, 390, 393, 442, 443, 447, 479,
        495, 498, 501, 570, 610, 633, 636, 650, 653, 656, 679, 704, 722, 725,
        728, 778, 782, 810, 813, 816, 884, 885, 906, 909, 912, 996
    };
    for (size_t i = 0; i < sizeof(RARE_BASE) / sizeof(RARE_BASE[0]); i++) {
        if (RARE_BASE[i] == dex) return true;
    }
    return false;
}

int save_system_get_unlock_ticket_cost(int pokemon_catalog_idx) {
    if (pokemon_catalog_idx < 0 || pokemon_catalog_idx >= TOTAL_CATALOG_POKEMON) return -1;
    int dex = POKEMON_CATALOG[pokemon_catalog_idx].dex;
    if (save_system_is_evolved_dex(dex)) return -1;
    if (save_system_is_legendary_dex(dex)) return 20;
    if (save_system_is_rare_dex(dex)) return 4;
    return 3;
}

bool save_system_unlock_with_tickets(int pokemon_catalog_idx) {
    if (save_system_is_owned(pokemon_catalog_idx)) return false;
    int cost = save_system_get_unlock_ticket_cost(pokemon_catalog_idx);
    if (cost < 0 || g_save.tickets < cost) return false;
    g_save.tickets -= cost;
    save_system_unlock_pokemon(pokemon_catalog_idx);
    return true;
}

int save_system_gacha_pull_ex(int legendary_only, bool *out_is_new, int *out_refund_tickets) {
    g_save.stats_pulls++;

    int non_leg[TOTAL_CATALOG_POKEMON], n_non = 0;
    int leg[TOTAL_CATALOG_POKEMON],     n_leg = 0;
    for (int i = 0; i < TOTAL_CATALOG_POKEMON; i++) {
        int dex = POKEMON_CATALOG[i].dex;
        if (save_system_is_evolved_dex(dex)) continue;
        if (save_system_is_legendary_dex(dex)) leg[n_leg++] = i;
        else non_leg[n_non++] = i;
    }

    int roll = 0;
    if (legendary_only) {
        roll = (n_leg > 0) ? leg[rand() % n_leg] : 0;
    } else {

        if (n_leg > 0 && (rand() % 100) < 5) roll = leg[rand() % n_leg];
        else if (n_non > 0) roll = non_leg[rand() % n_non];
        else if (n_leg > 0) roll = leg[rand() % n_leg];
    }

    bool owned = save_system_is_owned(roll);
    if (!owned) {
        save_system_unlock_pokemon(roll);
        if (out_is_new) *out_is_new = true;
        if (out_refund_tickets) *out_refund_tickets = 0;
    } else {

        int refund_tkt = legendary_only ? 8 : 1;
        g_save.tickets += refund_tkt;
        if (out_is_new) *out_is_new = false;
        if (out_refund_tickets) *out_refund_tickets = refund_tkt;
    }

    save_system_save();
    return roll;
}

int save_system_gacha_pull(bool *out_is_new, int *out_refund_coins) {
    return save_system_gacha_pull_ex(0, out_is_new, out_refund_coins);
}
