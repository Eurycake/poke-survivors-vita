#ifdef __vita__
#include <vitasdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "audio.h"

#define SAMPLE_RATE 48000
#define DMA_FRAMES 512
#define NUM_AUDIO_BUFS 4
#define MAX_ACTIVE_VOICES 24
#define SFX_QUEUE_CAP 64
#define BGM_MAX_BLK_ALIGN 4096
#define BGM_MAX_BLK_SAMPLES 4096

typedef struct {
    int16_t *samples;
    uint32_t total_frames;
    int channels;
    uint32_t sample_rate;
} SoundData;

typedef struct {
    const SoundData *data;
    uint64_t pos_fp;
    uint32_t step_fp;
    uint32_t fade_in_frames;
    int32_t vol_q8;
    bool active;
} Voice;

static inline bool is_attack_sfx(SoundEffect s) {
    return (s >= SFX_ATTACK_NORMAL && s <= SFX_ATTACK_MELEE) || s == SFX_ATTACK;
}

typedef struct {
    uint8_t *raw;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t sample_rate;
    uint16_t block_align;
    uint16_t samples_per_block;
} BgmAsset;

typedef struct {
    const BgmAsset *asset;
    uint32_t data_pos;
    int16_t blk_L[BGM_MAX_BLK_SAMPLES];
    int16_t blk_R[BGM_MAX_BLK_SAMPLES];
    uint32_t blk_frames;
    uint32_t blk_cursor;
    int32_t prev_L, prev_R;
    int32_t curr_L, curr_R;
    uint32_t step_fp;
    uint32_t phase_fp;
    bool loop;
    int fade_samples;
} BgmPlayer;

static SceUID g_mixer_thread = -1;
static SceUID g_output_thread = -1;
static SceUID g_semEmpty = -1;
static SceUID g_semFull = -1;
static volatile int g_audio_running = 0;

static SoundData g_sfxData[SFX_COUNT];
static BgmAsset g_bgmAssets[BGM_COUNT];
static Voice g_voices[MAX_ACTIVE_VOICES];
static SceUInt64 g_lastSfxUs[SFX_COUNT];

static volatile SoundEffect g_sfxQueue[SFX_QUEUE_CAP];
static volatile uint32_t g_sfxQHead = 0;
static volatile uint32_t g_sfxQTail = 0;

__attribute__((aligned(64))) static int16_t g_dmaBuf[NUM_AUDIO_BUFS][DMA_FRAMES * 2];

static BgmPlayer g_bgm;
static volatile MusicTrack g_currentBgm = BGM_NONE;
static volatile MusicTrack g_pendingBgm = BGM_NONE;
static volatile bool g_bgmChangeReq = false;
static volatile int32_t g_bgmMasterQ8 = 256;
static volatile int32_t g_sfxMasterQ8 = 256;

static const int8_t IMA_INDEX_TABLE[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

static const int16_t IMA_STEP_TABLE[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static inline int32_t ima_step_nibble(uint8_t nib, int32_t *pred, int *idx) {
    int32_t step = IMA_STEP_TABLE[*idx];
    int32_t diff = step >> 3;
    if (nib & 4) diff += step;
    if (nib & 2) diff += (step >> 1);
    if (nib & 1) diff += (step >> 2);
    if (nib & 8) *pred -= diff;
    else         *pred += diff;
    if (*pred > 32767) *pred = 32767;
    else if (*pred < -32768) *pred = -32768;
    *idx += IMA_INDEX_TABLE[nib & 15];
    if (*idx < 0) *idx = 0;
    else if (*idx > 88) *idx = 88;
    return *pred;
}

static const char *SFX_PATHS[SFX_COUNT] = {
    "audio/sfx_select.wav",
    "audio/sfx_confirm.wav",
    "audio/sfx_hit.wav",
    "audio/sfx_shot_normal.wav",
    "audio/sfx_shot_fire.wav",
    "audio/sfx_shot_water.wav",
    "audio/sfx_shot_grass.wav",
    "audio/sfx_shot_electric.wav",
    "audio/sfx_shot_ice.wav",
    "audio/sfx_shot_fighting.wav",
    "audio/sfx_shot_poison.wav",
    "audio/sfx_shot_ground.wav",
    "audio/sfx_shot_flying.wav",
    "audio/sfx_shot_psychic.wav",
    "audio/sfx_shot_bug.wav",
    "audio/sfx_shot_rock.wav",
    "audio/sfx_shot_ghost.wav",
    "audio/sfx_shot_dragon.wav",
    "audio/sfx_shot_steel.wav",
    "audio/sfx_shot_dark.wav",
    "audio/sfx_shot_fairy.wav",
    "audio/sfx_shot_beam.wav",
    "audio/sfx_shot_buff.wav",
    "audio/sfx_shot_aura.wav",
    "audio/sfx_shot_melee.wav",
    "audio/sfx_shot_normal.wav",
    "audio/sfx_coin.wav",
    "audio/sfx_chest.wav",
    "audio/sfx_emerald_levelup.wav",
    "audio/sfx_emerald_levelup.wav",
    "audio/sfx_break.wav",
    "audio/sfx_gameover.wav",
    "audio/sfx_heal.wav",
    "audio/sfx_boss.wav",
    "audio/sfx_bossDown.wav"
};

static const char *BGM_PATHS[BGM_COUNT] = {
    NULL,
    "audio/mus_title.wav",
    "audio/mus_route101.wav",
    "audio/mus_route111.wav",
    "audio/mus_route120.wav",
    "audio/mus_battle_wild.wav",
    "audio/mus_boss_regi.wav",
    "audio/mus_boss_rayquaza.wav",
    "audio/mus_boss_mew.wav",
    "audio/mus_victory.wav"
};

static uint8_t *read_entire_audio_file(const char *rel, size_t *out_sz) {
    static const char *roots[] = {
        "ux0:data/poke-survivors vita",
        "ux0:data/poke-survivors vita/assets",
        "ux0:data/poke-survivors-vita",
        "ux0:data/PokemonSurvivors",
        "ux0:data/PokemonSurvivors/PokemonSurvivors",
        "ux0:data/PokemonSurvivors/assets",
        "ux0:data/PokemonSurvivors_Data",
        "ux0:data/PokemonSurvivors_Data/PokemonSurvivors",
        "ux0:data/poke-survivor",
        "app0:assets",
        "assets",
        "data"
    };
    char full[256];
    SceUID fd = -1;
    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        snprintf(full, sizeof(full), "%s/%s", roots[i], rel);
        fd = sceIoOpen(full, SCE_O_RDONLY, 0);
        if (fd >= 0) break;
    }
    if (fd < 0) fd = sceIoOpen(rel, SCE_O_RDONLY, 0);
    if (fd < 0) return NULL;

    SceOff sz = sceIoLseek(fd, 0, SCE_SEEK_END);
    sceIoLseek(fd, 0, SCE_SEEK_SET);
    if (sz < 44 || sz > 12 * 1024 * 1024) { sceIoClose(fd); return NULL; }

    uint8_t *raw = (uint8_t *)malloc((size_t)sz);
    if (!raw) { sceIoClose(fd); return NULL; }
    size_t got = 0;
    while (got < (size_t)sz) {
        size_t rem = (size_t)sz - got;
        int r = sceIoRead(fd, raw + got, (SceSize)(rem > 65536 ? 65536 : rem));
        if (r <= 0) break;
        got += (size_t)r;
    }
    sceIoClose(fd);
    if (got != (size_t)sz) { free(raw); return NULL; }
    *out_sz = got;
    return raw;
}

static bool load_wav_pcm(const char *rel, SoundData *out) {
    memset(out, 0, sizeof(*out));
    size_t got = 0;
    uint8_t *raw = read_entire_audio_file(rel, &got);
    if (!raw) return false;
    if (memcmp(raw, "RIFF", 4) != 0 || memcmp(raw + 8, "WAVE", 4) != 0) {
        free(raw);
        return false;
    }

    uint16_t fmt_tag = 0, channels = 0, bits = 0;
    uint32_t sample_rate = 0, data_sz = 0;
    uint8_t *data_bytes = NULL;
    size_t pos = 12;
    while (pos + 8 <= got) {
        uint32_t csz;
        memcpy(&csz, raw + pos + 4, 4);
        if (memcmp(raw + pos, "fmt ", 4) == 0 && pos + 8 + 16 <= got) {
            memcpy(&fmt_tag, raw + pos + 8, 2);
            memcpy(&channels, raw + pos + 10, 2);
            memcpy(&sample_rate, raw + pos + 12, 4);
            memcpy(&bits, raw + pos + 22, 2);
        } else if (memcmp(raw + pos, "data", 4) == 0) {
            size_t avail = got - (pos + 8);
            data_sz = (csz < avail) ? csz : (uint32_t)avail;
            data_bytes = (uint8_t *)malloc(data_sz);
            if (data_bytes) memcpy(data_bytes, raw + pos + 8, data_sz);
            break;
        }
        pos += 8 + csz + (csz & 1);
    }
    free(raw);

    if (!data_bytes || fmt_tag != 1 || bits != 16 || channels == 0 || sample_rate == 0) {
        free(data_bytes);
        return false;
    }

    out->samples = (int16_t *)data_bytes;
    out->channels = (int)channels;
    out->sample_rate = sample_rate;
    out->total_frames = data_sz / (channels * sizeof(int16_t));
    return out->total_frames > 0;
}

static bool load_bgm_adpcm(const char *rel, BgmAsset *out) {
    memset(out, 0, sizeof(*out));
    size_t got = 0;
    uint8_t *raw = read_entire_audio_file(rel, &got);
    if (!raw) return false;
    if (got < 64 || memcmp(raw, "RIFF", 4) != 0 || memcmp(raw + 8, "WAVE", 4) != 0) {
        free(raw);
        return false;
    }

    uint16_t fmt_tag = 0, ch = 0, blk_align = 0, spb = 0;
    uint32_t sr = 0, data_off = 0, data_sz = 0;
    size_t pos = 12;
    while (pos + 8 <= got && pos < 512) {
        uint32_t csz;
        memcpy(&csz, raw + pos + 4, 4);
        if (memcmp(raw + pos, "fmt ", 4) == 0 && pos + 8 + 16 <= got) {
            memcpy(&fmt_tag, raw + pos + 8, 2);
            memcpy(&ch, raw + pos + 10, 2);
            memcpy(&sr, raw + pos + 12, 4);
            memcpy(&blk_align, raw + pos + 20, 2);
            if (csz >= 20 && pos + 8 + 20 <= got) {
                memcpy(&spb, raw + pos + 26, 2);
            }
        } else if (memcmp(raw + pos, "data", 4) == 0) {
            data_off = (uint32_t)(pos + 8);
            size_t avail = got - data_off;
            data_sz = (csz < avail) ? csz : (uint32_t)avail;
            break;
        }
        pos += 8 + csz + (csz & 1);
    }

    if (data_off == 0 || fmt_tag != 0x0011 || ch != 2 || sr == 0 ||
        blk_align == 0 || blk_align > BGM_MAX_BLK_ALIGN ||
        spb == 0 || spb > BGM_MAX_BLK_SAMPLES || data_sz < blk_align) {
        free(raw);
        return false;
    }

    out->raw = raw;
    out->data_offset = data_off;
    out->data_size = data_sz;
    out->sample_rate = sr;
    out->block_align = blk_align;
    out->samples_per_block = spb;
    return true;
}

static void free_sound(SoundData *sd) {
    if (sd->samples) { free(sd->samples); sd->samples = NULL; }
    sd->total_frames = 0;
    sd->channels = 0;
    sd->sample_rate = 0;
}

static void free_bgm_asset(BgmAsset *ba) {
    if (ba->raw) { free(ba->raw); ba->raw = NULL; }
    memset(ba, 0, sizeof(*ba));
}

static void close_bgm(void) {
    memset(&g_bgm, 0, sizeof(g_bgm));
    g_currentBgm = BGM_NONE;
}

static bool open_bgm_cached(const BgmAsset *asset, bool loop) {
    close_bgm();
    if (!asset || !asset->raw) return false;
    g_bgm.asset = asset;
    g_bgm.data_pos = 0;
    g_bgm.blk_frames = 0;
    g_bgm.blk_cursor = 0;
    g_bgm.prev_L = 0; g_bgm.prev_R = 0;
    g_bgm.curr_L = 0; g_bgm.curr_R = 0;
    g_bgm.step_fp = (uint32_t)(((uint64_t)asset->sample_rate << 16) / SAMPLE_RATE);
    g_bgm.phase_fp = 0x10000u;
    g_bgm.loop = loop;
    g_bgm.fade_samples = 128;
    return true;
}

static bool decode_next_bgm_block_mem(void) {
    const BgmAsset *a = g_bgm.asset;
    if (!a || !a->raw) return false;

    if (g_bgm.data_pos + a->block_align > a->data_size) {
        if (g_bgm.loop) {
            g_bgm.data_pos = 0;
            g_bgm.fade_samples = 64;
        } else {
            close_bgm();
            return false;
        }
    }

    const uint8_t *blk = a->raw + a->data_offset + g_bgm.data_pos;
    g_bgm.data_pos += a->block_align;

    int32_t predL = (int16_t)(blk[0] | (blk[1] << 8));
    int idxL = blk[2]; if (idxL > 88) idxL = 88;
    int32_t predR = (int16_t)(blk[4] | (blk[5] << 8));
    int idxR = blk[6]; if (idxR > 88) idxR = 88;

    g_bgm.blk_L[0] = (int16_t)predL;
    g_bgm.blk_R[0] = (int16_t)predR;

    uint32_t out_f = 1;
    for (uint32_t pos = 8; pos + 8 <= a->block_align && out_f + 8 <= a->samples_per_block; pos += 8) {
        for (uint32_t j = 0; j < 4; j++) {
            uint8_t b = blk[pos + j];
            g_bgm.blk_L[out_f + j * 2 + 0] = (int16_t)ima_step_nibble(b & 0x0Fu, &predL, &idxL);
            g_bgm.blk_L[out_f + j * 2 + 1] = (int16_t)ima_step_nibble(b >> 4,    &predL, &idxL);
        }
        for (uint32_t j = 0; j < 4; j++) {
            uint8_t b = blk[pos + 4 + j];
            g_bgm.blk_R[out_f + j * 2 + 0] = (int16_t)ima_step_nibble(b & 0x0Fu, &predR, &idxR);
            g_bgm.blk_R[out_f + j * 2 + 1] = (int16_t)ima_step_nibble(b >> 4,    &predR, &idxR);
        }
        out_f += 8;
    }

    g_bgm.blk_frames = out_f;
    g_bgm.blk_cursor = 0;
    return out_f > 0;
}

static inline void sample_bgm_frame(int32_t *out_l, int32_t *out_r) {
    if (!g_bgm.asset) return;

    while (g_bgm.phase_fp >= 0x10000u) {
        g_bgm.phase_fp -= 0x10000u;
        if (g_bgm.blk_cursor >= g_bgm.blk_frames) {
            if (!decode_next_bgm_block_mem()) return;
        }
        g_bgm.prev_L = g_bgm.curr_L;
        g_bgm.prev_R = g_bgm.curr_R;
        g_bgm.curr_L = g_bgm.blk_L[g_bgm.blk_cursor];
        g_bgm.curr_R = g_bgm.blk_R[g_bgm.blk_cursor];
        g_bgm.blk_cursor++;
    }

    int32_t frac = (int32_t)(g_bgm.phase_fp & 0xFFFFu);
    int32_t sL = g_bgm.prev_L + (((g_bgm.curr_L - g_bgm.prev_L) * frac) >> 16);
    int32_t sR = g_bgm.prev_R + (((g_bgm.curr_R - g_bgm.prev_R) * frac) >> 16);

    int32_t g_q8 = (192 * g_bgmMasterQ8) >> 8;
    if (g_bgm.fade_samples > 0) {
        g_q8 = (g_q8 * (129 - g_bgm.fade_samples)) >> 7;
        g_bgm.fade_samples--;
    }

    *out_l += (sL * g_q8) >> 8;
    *out_r += (sR * g_q8) >> 8;
    g_bgm.phase_fp += g_bgm.step_fp;
}

static inline int16_t soft_limit_q(int32_t x) {
    if (x > 24000) {
        int32_t y = 24000 + ((x - 24000) >> 2);
        return (int16_t)(y > 32767 ? 32767 : y);
    } else if (x < -24000) {
        int32_t y = -24000 + ((x + 24000) >> 2);
        return (int16_t)(y < -32768 ? -32768 : y);
    }
    return (int16_t)x;
}

static void sample_voice(Voice *vc, int32_t *out_l, int32_t *out_r) {
    const SoundData *sd = vc->data;
    uint32_t idx = (uint32_t)(vc->pos_fp >> 16);
    if (idx >= sd->total_frames) {
        vc->active = false;
        return;
    }
    uint32_t idx1 = (idx + 1 < sd->total_frames) ? (idx + 1) : idx;
    int32_t frac = (int32_t)(vc->pos_fp & 0xFFFFu);
    int32_t l0, l1, r0, r1;
    if (sd->channels >= 2) {
        l0 = sd->samples[idx * 2 + 0];  r0 = sd->samples[idx * 2 + 1];
        l1 = sd->samples[idx1 * 2 + 0]; r1 = sd->samples[idx1 * 2 + 1];
    } else {
        l0 = r0 = sd->samples[idx];
        l1 = r1 = sd->samples[idx1];
    }
    int32_t sl = l0 + (((l1 - l0) * frac) >> 16);
    int32_t sr = r0 + (((r1 - r0) * frac) >> 16);
    int32_t g_q8 = (vc->vol_q8 * g_sfxMasterQ8) >> 8;
    if (vc->fade_in_frames < 32u) {
        g_q8 = (g_q8 * (int32_t)(vc->fade_in_frames + 1u)) >> 5;
        vc->fade_in_frames++;
    }
    *out_l += (sl * g_q8) >> 8;
    *out_r += (sr * g_q8) >> 8;

    vc->pos_fp += vc->step_fp;
    if ((uint32_t)(vc->pos_fp >> 16) >= sd->total_frames) {
        vc->active = false;
    }
}

static void drain_sfx_queue(void) {
    uint32_t head = g_sfxQHead;
    uint32_t tail = __atomic_load_n(&g_sfxQTail, __ATOMIC_ACQUIRE);
    while (head != tail) {
        SoundEffect sfx = g_sfxQueue[head & (SFX_QUEUE_CAP - 1)];
        head++;
        if (sfx >= 0 && sfx < SFX_COUNT && g_sfxData[sfx].samples) {
            const SoundData *target_sd = &g_sfxData[sfx];
            int same_count = 0;
            int oldest_same = -1;
            uint64_t oldest_same_pos = 0;
            for (int i = 0; i < MAX_ACTIVE_VOICES; i++) {
                if (g_voices[i].active && g_voices[i].data == target_sd) {
                    same_count++;
                    if (oldest_same < 0 || g_voices[i].pos_fp >= oldest_same_pos) {
                        oldest_same_pos = g_voices[i].pos_fp;
                        oldest_same = i;
                    }
                }
            }
            int best = -1;
            if (same_count >= 4) {
                best = oldest_same;
            } else {
                uint64_t max_pos = 0;
                for (int i = 0; i < MAX_ACTIVE_VOICES; i++) {
                    if (!g_voices[i].active) { best = i; break; }
                    if (g_voices[i].pos_fp >= max_pos) { max_pos = g_voices[i].pos_fp; best = i; }
                }
            }
            if (best >= 0) {
                Voice *v = &g_voices[best];
                v->data = target_sd;
                v->pos_fp = 0;
                v->step_fp = (uint32_t)(((uint64_t)target_sd->sample_rate << 16) / SAMPLE_RATE);
                v->fade_in_frames = 0;
                v->vol_q8 = (sfx == SFX_HIT) ? 165 :
                            (sfx == SFX_LEVELUP || sfx == SFX_EVOLUTION || sfx == SFX_BOSS_SPAWN || sfx == SFX_BOSS_DOWN) ? 245 :
                            is_attack_sfx(sfx) ? 195 : 215;
                v->active = true;
            }
        }
    }
    __atomic_store_n(&g_sfxQHead, head, __ATOMIC_RELEASE);
}

static void mix_dma_buffer(int16_t *dst) {
    if (g_bgmChangeReq) {
        MusicTrack target = g_pendingBgm;
        g_bgmChangeReq = false;
        if (target > BGM_NONE && target < BGM_COUNT && g_bgmAssets[target].raw) {
            if (open_bgm_cached(&g_bgmAssets[target], target != BGM_VICTORY)) {
                g_currentBgm = target;
            }
        } else {
            close_bgm();
        }
    }

    drain_sfx_queue();
    for (int f = 0; f < DMA_FRAMES; f++) {
        int32_t mix_l = 0, mix_r = 0;
        sample_bgm_frame(&mix_l, &mix_r);
        for (int v = 0; v < MAX_ACTIVE_VOICES; v++) {
            if (g_voices[v].active) sample_voice(&g_voices[v], &mix_l, &mix_r);
        }
        dst[f * 2 + 0] = soft_limit_q(mix_l);
        dst[f * 2 + 1] = soft_limit_q(mix_r);
    }
}

static int audio_mixer_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    int wr = 2;
    while (g_audio_running) {
        if (sceKernelWaitSema(g_semEmpty, 1, NULL) < 0) break;
        if (!g_audio_running) break;

        mix_dma_buffer(g_dmaBuf[wr]);
        __sync_synchronize();
        wr = (wr + 1) % NUM_AUDIO_BUFS;
        sceKernelSignalSema(g_semFull, 1);
    }
    return 0;
}

static int audio_output_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    int port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, DMA_FRAMES, SAMPLE_RATE, SCE_AUDIO_OUT_MODE_STEREO);
    if (port < 0) return 0;

    int vols[2] = { SCE_AUDIO_VOLUME_0DB, SCE_AUDIO_VOLUME_0DB };
    sceAudioOutSetVolume(port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vols);

    int rd = 0;
    int prev_rd = -1;
    while (g_audio_running) {
        if (sceKernelWaitSema(g_semFull, 1, NULL) < 0) break;
        if (!g_audio_running) break;

        __sync_synchronize();
        sceAudioOutOutput(port, g_dmaBuf[rd]);
        if (prev_rd >= 0) {
            sceKernelSignalSema(g_semEmpty, 1);
        }
        prev_rd = rd;
        rd = (rd + 1) % NUM_AUDIO_BUFS;
    }

    sceAudioOutOutput(port, NULL);
    sceAudioOutReleasePort(port);
    return 0;
}

void audio_init(void) {
    memset(g_sfxData, 0, sizeof(g_sfxData));
    memset(g_bgmAssets, 0, sizeof(g_bgmAssets));
    memset(g_voices, 0, sizeof(g_voices));
    memset(g_lastSfxUs, 0, sizeof(g_lastSfxUs));
    memset(g_dmaBuf, 0, sizeof(g_dmaBuf));
    memset(&g_bgm, 0, sizeof(g_bgm));
    g_sfxQHead = 0;
    g_sfxQTail = 0;
    g_currentBgm = BGM_NONE;
    g_pendingBgm = BGM_NONE;
    g_bgmChangeReq = false;

    for (int i = 0; i < SFX_COUNT; i++) {
        if (SFX_PATHS[i]) {
            load_wav_pcm(SFX_PATHS[i], &g_sfxData[i]);
        }
    }
    for (int i = 0; i < BGM_COUNT; i++) {
        if (BGM_PATHS[i]) {
            load_bgm_adpcm(BGM_PATHS[i], &g_bgmAssets[i]);
        }
    }

    mix_dma_buffer(g_dmaBuf[0]);
    mix_dma_buffer(g_dmaBuf[1]);
    g_semEmpty = sceKernelCreateSema("aud_empty", 0, NUM_AUDIO_BUFS - 2, NUM_AUDIO_BUFS, NULL);
    g_semFull  = sceKernelCreateSema("aud_full",  0, 2, NUM_AUDIO_BUFS, NULL);

    g_audio_running = 1;
    g_mixer_thread  = sceKernelCreateThread("audio_mixer",  audio_mixer_thread,  0x42, 0x8000, 0, SCE_KERNEL_CPU_MASK_USER_ALL, NULL);
    g_output_thread = sceKernelCreateThread("audio_output", audio_output_thread, 0x40, 0x4000, 0, SCE_KERNEL_CPU_MASK_USER_ALL, NULL);
    if (g_mixer_thread >= 0)  sceKernelStartThread(g_mixer_thread, 0, NULL);
    if (g_output_thread >= 0) sceKernelStartThread(g_output_thread, 0, NULL);
}

void audio_fini(void) {
    g_audio_running = 0;
    if (g_semEmpty >= 0) sceKernelSignalSema(g_semEmpty, NUM_AUDIO_BUFS);
    if (g_semFull >= 0)  sceKernelSignalSema(g_semFull, NUM_AUDIO_BUFS);
    if (g_output_thread >= 0) {
        sceKernelWaitThreadEnd(g_output_thread, NULL, NULL);
        sceKernelDeleteThread(g_output_thread);
        g_output_thread = -1;
    }
    if (g_mixer_thread >= 0) {
        sceKernelWaitThreadEnd(g_mixer_thread, NULL, NULL);
        sceKernelDeleteThread(g_mixer_thread);
        g_mixer_thread = -1;
    }
    if (g_semEmpty >= 0) { sceKernelDeleteSema(g_semEmpty); g_semEmpty = -1; }
    if (g_semFull >= 0)  { sceKernelDeleteSema(g_semFull);  g_semFull  = -1; }
    for (int i = 0; i < SFX_COUNT; i++) free_sound(&g_sfxData[i]);
    for (int i = 0; i < BGM_COUNT; i++) free_bgm_asset(&g_bgmAssets[i]);
}

void audio_play_sfx(SoundEffect sfx) {
    if (sfx < 0 || sfx >= SFX_COUNT || sfx == SFX_GEM) return;
    if (!g_sfxData[sfx].samples) return;
    SceUInt64 now = sceKernelGetProcessTimeWide();
    uint64_t self_gap = (sfx == SFX_LEVELUP || sfx == SFX_EVOLUTION || sfx == SFX_GAMEOVER || sfx == SFX_BOSS_SPAWN || sfx == SFX_BOSS_DOWN) ? 20000ULL :
                        (sfx == SFX_HIT) ? 90000ULL : 40000ULL;
    if (now - g_lastSfxUs[sfx] < self_gap) return;
    g_lastSfxUs[sfx] = now;

    uint32_t tail = g_sfxQTail;
    uint32_t head = __atomic_load_n(&g_sfxQHead, __ATOMIC_ACQUIRE);
    if ((tail - head) < SFX_QUEUE_CAP) {
        g_sfxQueue[tail & (SFX_QUEUE_CAP - 1)] = sfx;
        __atomic_store_n(&g_sfxQTail, tail + 1, __ATOMIC_RELEASE);
    }
}

void audio_play_type_attack(int move_type) {
    static const SoundEffect map[18] = {
        SFX_ATTACK_NORMAL, SFX_ATTACK_FIRE, SFX_ATTACK_WATER, SFX_ATTACK_GRASS,
        SFX_ATTACK_ELECTRIC, SFX_ATTACK_ICE, SFX_ATTACK_FIGHTING, SFX_ATTACK_POISON,
        SFX_ATTACK_GROUND, SFX_ATTACK_FLYING, SFX_ATTACK_PSYCHIC, SFX_ATTACK_BUG,
        SFX_ATTACK_ROCK, SFX_ATTACK_GHOST, SFX_ATTACK_DRAGON, SFX_ATTACK_DARK,
        SFX_ATTACK_STEEL, SFX_ATTACK_FAIRY
    };
    audio_play_sfx((move_type >= 0 && move_type < 18) ? map[move_type] : SFX_ATTACK_NORMAL);
}

void audio_play_bgm(MusicTrack track) {
    if (track == g_pendingBgm && (g_bgmChangeReq || track == g_currentBgm)) return;
    g_pendingBgm = track;
    g_bgmChangeReq = true;
}

void audio_stop_bgm(void) {
    audio_play_bgm(BGM_NONE);
}

void audio_set_bgm_volume(float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    g_bgmMasterQ8 = (int32_t)(vol * 256.0f + 0.5f);
}
void audio_set_sfx_volume(float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    g_sfxMasterQ8 = (int32_t)(vol * 256.0f + 0.5f);
}
float audio_get_bgm_volume(void) { return (float)g_bgmMasterQ8 / 256.0f; }
float audio_get_sfx_volume(void) { return (float)g_sfxMasterQ8 / 256.0f; }

#endif
