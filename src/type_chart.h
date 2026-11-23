#ifndef TYPE_CHART_H
#define TYPE_CHART_H

#include <string.h>
#include "moves_db.h"
#include "assets_data.h"


static const float TYPE_CHART[18][18] = {

    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.7f, 0.25f,1.0f, 1.0f, 0.7f, 1.0f},
    {1.0f, 0.7f, 0.7f, 1.5f, 1.0f, 1.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 0.7f, 1.0f, 0.7f, 1.0f, 1.5f, 1.0f},
    {1.0f, 1.5f, 0.7f, 0.7f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.0f, 1.0f, 1.0f, 1.5f, 1.0f, 0.7f, 1.0f, 1.0f, 1.0f},
    {1.0f, 0.7f, 1.5f, 0.7f, 1.0f, 1.0f, 1.0f, 0.7f, 1.5f, 0.7f, 1.0f, 0.7f, 1.5f, 1.0f, 0.7f, 1.0f, 0.7f, 1.0f},
    {1.0f, 1.0f, 1.5f, 0.7f, 0.7f, 1.0f, 1.0f, 1.0f, 0.25f,1.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.7f, 1.0f, 1.0f, 1.0f},
    {1.0f, 0.7f, 0.7f, 1.5f, 1.0f, 0.7f, 1.0f, 1.0f, 1.5f, 1.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.0f, 0.7f, 1.0f},
    {1.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.0f, 0.7f, 1.0f, 0.7f, 0.7f, 0.7f, 1.5f, 0.25f,1.0f, 1.5f, 1.5f, 0.7f},
    {1.0f, 1.0f, 1.0f, 1.5f, 1.0f, 1.0f, 1.0f, 0.7f, 0.7f, 1.0f, 1.0f, 1.0f, 0.7f, 0.7f, 1.0f, 1.0f, 0.25f,1.5f},
    {1.0f, 1.5f, 1.0f, 0.7f, 1.5f, 1.0f, 1.0f, 1.5f, 1.0f, 0.25f,1.0f, 0.7f, 1.5f, 1.0f, 1.0f, 1.0f, 1.5f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.5f, 0.7f, 1.0f, 1.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 0.7f, 1.0f, 1.0f, 1.0f, 0.7f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.5f, 1.0f, 1.0f, 0.7f, 1.0f, 1.0f, 1.0f, 1.0f, 0.25f,0.7f, 1.0f},
    {1.0f, 0.7f, 1.0f, 1.5f, 1.0f, 1.0f, 0.7f, 0.7f, 1.0f, 0.7f, 1.5f, 1.0f, 1.0f, 0.7f, 1.0f, 1.5f, 0.7f, 0.7f},
    {1.0f, 1.5f, 1.0f, 1.0f, 1.0f, 1.5f, 0.7f, 1.0f, 0.7f, 1.5f, 1.0f, 1.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.7f, 1.0f},
    {0.25f,1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.0f, 1.0f, 1.5f, 1.0f, 0.7f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.0f, 0.7f, 0.25f},
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.7f, 1.0f, 1.0f, 1.0f, 1.5f, 1.0f, 1.0f, 1.5f, 1.0f, 0.7f, 1.0f, 0.7f},
    {1.0f, 0.7f, 0.7f, 1.0f, 0.7f, 1.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.0f, 1.0f, 1.0f, 0.7f, 1.5f},
    {1.0f, 0.7f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 0.7f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 1.5f, 0.7f, 1.0f}
};

static inline int type_name_to_id(const char *name) {
    if (!name || !name[0]) return -1;
    for (int i = 0; i < 18; i++) {
        if (strcmp(name, MOVE_TYPE_NAMES[i]) == 0) return i;
    }
    return -1;
}

static inline float type_effectiveness_dual(int atk_type, int def_t1, int def_t2) {
    if (atk_type < 0 || atk_type >= 18) return 1.0f;
    float mult = 1.0f;
    if (def_t1 >= 0 && def_t1 < 18) mult *= TYPE_CHART[atk_type][def_t1];
    if (def_t2 >= 0 && def_t2 < 18 && def_t2 != def_t1) mult *= TYPE_CHART[atk_type][def_t2];
    return mult;
}


static inline void pokemon_dex_get_types(int dex, int *out_t1, int *out_t2) {
    static signed char s_t1[1025];
    static signed char s_t2[1025];
    static int s_init = 0;
    if (!s_init) {
        s_init = 1;
        memset(s_t1, -1, sizeof(s_t1));
        memset(s_t2, -1, sizeof(s_t2));
        for (int i = 0; i < TOTAL_CATALOG_POKEMON; i++) {
            int d = POKEMON_CATALOG[i].dex;
            if (d > 0 && d < 1025) {
                s_t1[d] = (signed char)type_name_to_id(POKEMON_CATALOG[i].type1);
                s_t2[d] = (signed char)type_name_to_id(POKEMON_CATALOG[i].type2);
            }
        }
    }
    if (dex > 0 && dex < 1025) {
        if (out_t1) *out_t1 = (int)s_t1[dex];
        if (out_t2) *out_t2 = (int)s_t2[dex];
    } else {
        if (out_t1) *out_t1 = -1;
        if (out_t2) *out_t2 = -1;
    }
}

#endif
