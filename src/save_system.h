#ifndef SAVE_SYSTEM_H
#define SAVE_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>

#define SAVE_MAGIC 0x454B4F50
#define SAVE_VERSION 3

#define TOTAL_UPGRADES 12
#define TOTAL_GOALS 12

typedef struct {
    const char *id;
    const char *name;
    const char *unit;
    int max_level;
    float base_cost;
    float growth;
} UpgradeDef;

static const UpgradeDef G_UPGRADE_DEFS[TOTAL_UPGRADES] = {
    { "hp",     "Vitality",            "+5% Max HP (Max +50%)",      10,  220.0f, 1.42f },
    { "atk",    "Strength",            "+4% Damage",                 10,  280.0f, 1.44f },
    { "spd",    "Agility",             "+3% Move Speed",              8,  240.0f, 1.44f },
    { "cd",     "Reflexes",            "-3% Cooldown",                8,  300.0f, 1.46f },
    { "def",    "Armor",               "+2% Defense",                 8,  280.0f, 1.45f },
    { "regen",  "Synthesis",           "+0.2 HP/sec",                 5,  360.0f, 1.54f },
    { "magnet", "Magnet",              "+12% Pickup Range",           6,  200.0f, 1.44f },
    { "xp",     "Wisdom",              "+5% Experience",              8,  320.0f, 1.46f },
    { "coins",  "Fortune",             "+8% Coins",                   8,  360.0f, 1.48f },
    { "crit",   "Critical Hit",        "+3% Crit Chance",            10,  280.0f, 1.44f },
    { "reroll", "Card Rerolls",        "+1 Reroll per Run",           5,  250.0f, 1.45f },
    { "revive", "Revive Potion",       "+1 Revive (Restocks, Max 2)", 2, 2000.0f, 1.00f },
};

typedef struct {
    const char *id;
    const char *text;
    int stat_type;
    int target;
    int reward_coins;
    int reward_tickets;
} GoalDef;

static const GoalDef G_GOAL_DEFS[TOTAL_GOALS] = {
    { "m_kill100", "Defeat 100 Pokemon",             0, 100,  150, 0 },
    { "m_kill500", "Defeat 500 Pokemon",             0, 500,  400, 1 },
    { "m_time3",   "Survive 3:00 in a single run",   1, 180,  200, 0 },
    { "m_time6",   "Survive 6:00 in a single run",   1, 360,  500, 1 },
    { "m_lv10",    "Reach Level 10",                 2, 10,   200, 0 },
    { "m_lv25",    "Reach Level 25",                 2, 25,   500, 0 },
    { "m_boss1",   "Defeat a Boss",                  3, 1,    300, 1 },
    { "m_chest5",  "Open 5 Treasure Chests",         4, 5,    200, 0 },
    { "m_pull3",   "Perform 3 Gacha Pulls",          5, 3,    150, 0 },
    { "a_kill2k",  "Defeat 2,000 Pokemon",           0, 2000, 800, 2 },
    { "a_time10",  "Survive 10:00 in a run",         1, 600, 1000, 2 },
    { "a_boss5",   "Defeat 5 Bosses",                3, 5,   1200, 3 }
};

typedef struct {
    uint32_t magic;
    uint32_t version;
    int coins;
    int tickets;
    int selected_starter;
    uint32_t owned_mask[32];
    int upgrades[TOTAL_UPGRADES];
    int stats_runs;
    int stats_kills;
    float stats_best_time;
    int stats_best_level;
    int stats_bosses;
    int stats_chests;
    int stats_pulls;
    uint32_t claimed_goals_mask;
    uint16_t goal_tier[TOTAL_GOALS];
} SaveData;

extern SaveData g_save;

void save_system_init(void);
void save_system_load(void);
void save_system_save(void);

bool save_system_is_owned(int pokemon_catalog_idx);
void save_system_unlock_pokemon(int pokemon_catalog_idx);
void save_system_unlock_dex(int dex);

int save_system_get_upgrade_cost(int upgrade_idx);
bool save_system_buy_upgrade(int upgrade_idx);

int save_system_get_goal_progress(int goal_idx);
int save_system_get_goal_target(int goal_idx);
int save_system_get_goal_reward_coins(int goal_idx);
int save_system_get_goal_reward_tickets(int goal_idx);
int save_system_get_goal_tier(int goal_idx);
void save_system_get_goal_text(int goal_idx, char *out, int out_sz);
bool save_system_is_goal_completed(int goal_idx);
bool save_system_is_goal_claimed(int goal_idx);
bool save_system_claim_goal(int goal_idx);

bool save_system_is_evolved_dex(int dex);
bool save_system_is_legendary_dex(int dex);
bool save_system_is_rare_dex(int dex);
int  save_system_get_unlock_ticket_cost(int pokemon_catalog_idx);
bool save_system_unlock_with_tickets(int pokemon_catalog_idx);

int save_system_gacha_pull(bool *out_is_new, int *out_refund_coins);
int save_system_gacha_pull_ex(int legendary_only, bool *out_is_new, int *out_refund_tickets);

#endif
