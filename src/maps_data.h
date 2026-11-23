#ifndef MAPS_DATA_H
#define MAPS_DATA_H
#include <stdint.h>

#define BAKED_STAGE_COUNT 4
#define BAKED_FIRST_STAGE 3
typedef struct {
    uint8_t x0, y0, x1, y1;
    uint8_t deck_y;
    uint8_t dir;
    uint16_t plank_tid;
} BakedBridge;
typedef struct {
    int w, h;
    int start_tx, start_ty;
    const uint16_t *tile;
    const uint8_t *kind;
    const uint16_t *under;
    const uint8_t *elev;
    const BakedBridge *bridges;
    int num_bridges;
} BakedMap;
extern const BakedMap g_baked_maps[BAKED_STAGE_COUNT];
extern const uint16_t g_baked_tid_water;
extern const uint16_t g_baked_tid_wf_top;
extern const uint16_t g_baked_tid_wf_body;
extern const uint16_t g_baked_tid_wf_foam;
#endif
