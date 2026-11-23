#include <stddef.h>
#ifndef ASSETS_DATA_H
#define ASSETS_DATA_H

#include <stdint.h>
#include <stdbool.h>

#define ANIM_IDLE   0
#define ANIM_WALK   1
#define ANIM_ATTACK 2
#define ANIM_SHOOT  3

#define RGBA8_C(r,g,b,a) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(g)<<8)|((uint32_t)(r)))

typedef struct {
    int w, h, n;
    float ax, ay;
} AnimMeta;

typedef struct {
    int dex;
    const char *name;
    const char *type1;
    const char *type2;
    int base_hp;
    float base_speed;
    float base_atk;
    AnimMeta walk;
    AnimMeta idle;
    AnimMeta attack;
    AnimMeta shoot;
    int starting_move;
    const char *passive_desc;
} PokemonData;

#include <string.h>
#include "evolution_data.inc"


static const PokemonData POKEMON_CATALOG[] = {
#include "pokemon_catalog.inc"
};

#define TOTAL_CATALOG_POKEMON 982
#define STARTER_COUNT 982


typedef struct {
    int id;
    const char *name;
    const char *type;
    const char *desc;
    float base_cd;
    int base_dmg;
    int max_level;
    uint32_t color;
    const char *vfx_name;
} MoveInfo;

#include "moves_db.h"
static const MoveInfo MOVE_CATALOG[] = {
#include "moves_catalog.inc"
};
#define TOTAL_MOVES MOVEDB_TOTAL


static inline int get_allied_coverage_type(int t) {

    switch (t) {
        case 1:  return 8;
        case 2:  return 5;
        case 3:  return 7;
        case 4:  return 16;
        case 5:  return 2;
        case 6:  return 12;
        case 7:  return 15;
        case 8:  return 12;
        case 9:  return 14;
        case 10: return 17;
        case 11: return 3;
        case 12: return 8;
        case 13: return 15;
        case 14: return 1;
        case 15: return 13;
        case 16: return 12;
        case 17: return 10;
        default: return 0;
    }
}

static inline bool is_move_strict_element(int starter_idx, int move_id) {
    if (starter_idx < 0 || starter_idx >= TOTAL_CATALOG_POKEMON) return true;
    if (POKEMON_CATALOG[starter_idx].dex == 151 || POKEMON_CATALOG[starter_idx].dex == 493) return true;
    int t = MOVE_DB[move_id].type;
    static const char *T_NAMES[18] = {
        "Normal", "Fire", "Water", "Grass", "Electric", "Ice", "Fighting", "Poison",
        "Ground", "Flying", "Psychic", "Bug", "Rock", "Ghost", "Dragon", "Dark", "Steel", "Fairy"
    };
    if (t >= 0 && t < 18) {
        const char *t1 = POKEMON_CATALOG[starter_idx].type1;
        const char *t2 = POKEMON_CATALOG[starter_idx].type2;
        if (t1 && strcmp(t1, T_NAMES[t]) == 0) return true;
        if (t2 && strcmp(t2, T_NAMES[t]) == 0) return true;
    }
    return false;
}

static inline bool is_move_compatible(int starter_idx, int move_id) {
    if (is_move_strict_element(starter_idx, move_id)) return true;

    if (MOVE_DB[move_id].type == 0) return true;
    return false;
}


typedef struct {
    int id;
    const char *name;
    const char *desc;
    int max_level;
} PassiveInfo;

static const PassiveInfo PASSIVE_CATALOG[] = {
    { 0, "Muscle Band",  "+10% Might (Damage) per rank (Cap 300%)",        5 },
    { 1, "Choice Scarf", "+10% Move & Proj Speed/rank (Cap 200%/250%)",    5 },
    { 2, "Leftovers",    "+0.4 HP/s Recovery per rank (Cap 5.0 HP/s)",     5 },
    { 3, "Item Magnet",  "+25px Magnet & +10% Area/rank (Cap 250px/250%)", 5 },
    { 4, "Focus Band",   "+20% Max HP & +1 Armor/rank (Cap 500HP/10Arm)",  5 },
    { 5, "Quick Claw",   "-8% Skill Cooldown per rank (Floor 0.30x)",      5 },
    { 6, "Scope Lens",   "+5% Crit Chance & +0.2x Crit Mult/rank",         5 },
    { 7, "Shell Bell",   "+8% XP Growth, +1 Amount (Lv3/5) & Lifesteal",   5 }
};

#define TOTAL_PASSIVES 8
#define MAX_EQUIPPED_PASSIVES 6


static inline int get_move_paired_passive(int move_id) {
    if (move_id < 0 || move_id >= TOTAL_MOVES) return 0;
    int k = MOVE_DB[move_id].kind;
    switch (k) {
        case MK_MELEE:
        case MK_BEAM:       return 0;
        case MK_ORBIT:
        case MK_BOOMERANG:
        case MK_DASH:       return 1;
        case MK_AURA:
        case MK_TRAIL:      return 2;
        case MK_NOVA:
        case MK_METEOR:
        case MK_MINE:       return 3;
        case MK_BUFF:       return 4;
        case MK_PROJECTILE:
        case MK_CONE:       return 5;
        case MK_CHAIN:
        case MK_TURRET:     return 6;
        default:            return 0;
    }
}

#endif
