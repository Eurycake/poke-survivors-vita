#ifndef __vita__
#include <windows.h>
#include <mmsystem.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio.h"

#pragma comment(lib, "winmm.lib")

#define SAMPLE_RATE 44100
#define BUFFER_FRAMES 1024
#define NUM_BUFFERS 8
#define MAX_ACTIVE_VOICES 32

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
    float volume;
    bool active;
    bool loop;
} Voice;

static DWORD g_lastSfxMs[SFX_COUNT];

static HWAVEOUT g_hWaveOut = NULL;
static HANDLE g_hAudioEvent = NULL;
static WAVEHDR g_waveHeaders[NUM_BUFFERS];
static int16_t g_audioBuffers[NUM_BUFFERS][BUFFER_FRAMES * 2];
static HANDLE g_hAudioThread = NULL;
static volatile bool g_audioRunning = false;
static CRITICAL_SECTION g_audioLock;

static SoundData g_sfxData[SFX_COUNT];
static Voice g_voices[MAX_ACTIVE_VOICES];

static SoundData g_bgmCache[BGM_COUNT];
static Voice g_bgmVoice;
static MusicTrack g_currentBgm = BGM_NONE;
static volatile float g_bgmMasterVol = 1.0f;
static volatile float g_sfxMasterVol = 1.0f;

static const char *SFX_PATHS[SFX_COUNT] = {
    "assets/audio/sfx_select.wav",
    "assets/audio/sfx_confirm.wav",
    "assets/audio/sfx_hit.wav",
    "assets/audio/sfx_shot_normal.wav",
    "assets/audio/sfx_shot_fire.wav",
    "assets/audio/sfx_shot_water.wav",
    "assets/audio/sfx_shot_grass.wav",
    "assets/audio/sfx_shot_electric.wav",
    "assets/audio/sfx_shot_ice.wav",
    "assets/audio/sfx_shot_fighting.wav",
    "assets/audio/sfx_shot_poison.wav",
    "assets/audio/sfx_shot_ground.wav",
    "assets/audio/sfx_shot_flying.wav",
    "assets/audio/sfx_shot_psychic.wav",
    "assets/audio/sfx_shot_bug.wav",
    "assets/audio/sfx_shot_rock.wav",
    "assets/audio/sfx_shot_ghost.wav",
    "assets/audio/sfx_shot_dragon.wav",
    "assets/audio/sfx_shot_steel.wav",
    "assets/audio/sfx_shot_dark.wav",
    "assets/audio/sfx_shot_fairy.wav",
    "assets/audio/sfx_shot_beam.wav",
    "assets/audio/sfx_shot_buff.wav",
    "assets/audio/sfx_shot_aura.wav",
    "assets/audio/sfx_shot_melee.wav",
    "assets/audio/sfx_shot_normal.wav",
    "assets/audio/sfx_coin.wav",
    "assets/audio/sfx_chest.wav",
    "assets/audio/sfx_emerald_levelup.wav",
    "assets/audio/sfx_emerald_levelup.wav",
    "assets/audio/sfx_break.wav",
    "assets/audio/sfx_gameover.wav",
    "assets/audio/sfx_heal.wav",
    "assets/audio/sfx_boss.wav",
    "assets/audio/sfx_bossDown.wav"
};

static const char *BGM_PATHS[BGM_COUNT] = {
    NULL,
    "assets/audio/mus_title.wav",
    "assets/audio/mus_route101.wav",
    "assets/audio/mus_route111.wav",
    "assets/audio/mus_route120.wav",
    "assets/audio/mus_battle_wild.wav",
    "assets/audio/mus_boss_regi.wav",
    "assets/audio/mus_boss_rayquaza.wav",
    "assets/audio/mus_boss_mew.wav",
    "assets/audio/mus_victory.wav"
};

static const int8_t PC_IMA_INDEX[16] = { -1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8 };
static const int16_t PC_IMA_STEP[89] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,
    118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,
    963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,
    5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,
    24623,27086,29794,32767
};

static inline int32_t pc_ima_step(uint8_t nib, int32_t *pred, int *idx) {
    int32_t step = PC_IMA_STEP[*idx], diff = step >> 3;
    if (nib & 4) diff += step; if (nib & 2) diff += step >> 1; if (nib & 1) diff += step >> 2;
    if (nib & 8) *pred -= diff; else *pred += diff;
    if (*pred > 32767) *pred = 32767; else if (*pred < -32768) *pred = -32768;
    *idx += PC_IMA_INDEX[nib & 15];
    if (*idx < 0) *idx = 0; else if (*idx > 88) *idx = 88;
    return *pred;
}

static bool load_wav(const char *path, SoundData *out) {
    memset(out, 0, sizeof(SoundData));
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        char alt[512];
        if (strncmp(path, "assets/", 7) == 0) {
            snprintf(alt, sizeof(alt), "data/%s", path + 7);
            fp = fopen(alt, "rb");
            if (!fp) {
                snprintf(alt, sizeof(alt), "../data/%s", path + 7);
                fp = fopen(alt, "rb");
            }
        }
        if (!fp) {
            snprintf(alt, sizeof(alt), "vita-port/%s", path);
            fp = fopen(alt, "rb");
        }
        if (!fp) {
            snprintf(alt, sizeof(alt), "../%s", path);
            fp = fopen(alt, "rb");
        }
        if (!fp) {
            snprintf(alt, sizeof(alt), "../vita-port/%s", path);
            fp = fopen(alt, "rb");
        }
    }
    if (!fp) return false;

    char riff[4];
    uint32_t riff_sz = 0;
    char wave[4];
    if (fread(riff, 1, 4, fp) != 4 || memcmp(riff, "RIFF", 4) != 0 ||
        fread(&riff_sz, 4, 1, fp) != 1 ||
        fread(wave, 1, 4, fp) != 4 || memcmp(wave, "WAVE", 4) != 0) {
        fclose(fp);
        return false;
    }

    uint16_t fmt_tag = 0, channels = 0, block_align = 0, bits_per_sample = 0, spb = 0;
    uint32_t sample_rate = 0;
    uint8_t *data_bytes = NULL;
    uint32_t data_sz = 0;

    char chunk_id[4];
    uint32_t chunk_sz = 0;
    while (fread(chunk_id, 1, 4, fp) == 4 && fread(&chunk_sz, 4, 1, fp) == 1) {
        if (memcmp(chunk_id, "fmt ", 4) == 0) {
            fread(&fmt_tag, 2, 1, fp);
            fread(&channels, 2, 1, fp);
            fread(&sample_rate, 4, 1, fp);
            fseek(fp, 4, SEEK_CUR);
            fread(&block_align, 2, 1, fp);
            fread(&bits_per_sample, 2, 1, fp);
            if (chunk_sz >= 20) {
                uint16_t cb = 0; fread(&cb, 2, 1, fp);
                fread(&spb, 2, 1, fp);
                if (chunk_sz > 20) fseek(fp, chunk_sz - 20, SEEK_CUR);
            } else if (chunk_sz > 16) fseek(fp, chunk_sz - 16, SEEK_CUR);
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            data_bytes = (uint8_t*)malloc(chunk_sz);
            if (data_bytes) {
                fread(data_bytes, 1, chunk_sz, fp);
                data_sz = chunk_sz;
            }
            break;
        } else {
            fseek(fp, chunk_sz, SEEK_CUR);
        }
    }
    fclose(fp);

    if (data_bytes && fmt_tag == 0x0011 && channels == 2 && block_align >= 16 && spb > 0) {
        uint32_t nblk = data_sz / block_align;
        int16_t *pcm = (int16_t *)malloc((size_t)nblk * spb * 2 * sizeof(int16_t));
        if (!pcm) { free(data_bytes); return false; }
        uint32_t total = 0;
        for (uint32_t b = 0; b < nblk; b++) {
            const uint8_t *blk = data_bytes + (size_t)b * block_align;
            int32_t pL = (int16_t)(blk[0] | (blk[1] << 8)), pR = (int16_t)(blk[4] | (blk[5] << 8));
            int iL = blk[2] > 88 ? 88 : blk[2], iR = blk[6] > 88 ? 88 : blk[6];
            pcm[total * 2 + 0] = (int16_t)pL; pcm[total * 2 + 1] = (int16_t)pR; total++;
            for (uint32_t pos = 8; pos + 8 <= block_align; pos += 8) {
                for (uint32_t j = 0; j < 4; j++) {
                    uint8_t byteL = blk[pos + j], byteR = blk[pos + 4 + j];
                    pcm[(total + j*2)*2 + 0] = (int16_t)pc_ima_step(byteL & 0x0F, &pL, &iL);
                    pcm[(total + j*2 + 1)*2 + 0] = (int16_t)pc_ima_step(byteL >> 4, &pL, &iL);
                    pcm[(total + j*2)*2 + 1] = (int16_t)pc_ima_step(byteR & 0x0F, &pR, &iR);
                    pcm[(total + j*2 + 1)*2 + 1] = (int16_t)pc_ima_step(byteR >> 4, &pR, &iR);
                }
                total += 8;
            }
        }
        free(data_bytes);
        out->samples = pcm;
        out->channels = 2;
        out->total_frames = total;
        out->sample_rate = (sample_rate > 0) ? sample_rate : 22050;
        return total > 0;
    }

    if (!data_bytes || fmt_tag != 1 || bits_per_sample != 16) {
        if (data_bytes) free(data_bytes);
        return false;
    }

    out->samples = (int16_t*)data_bytes;
    out->channels = (int)channels;
    out->total_frames = data_sz / (channels * sizeof(int16_t));
    out->sample_rate = (sample_rate > 0) ? sample_rate : 22050;
    return true;
}

static void free_sound(SoundData *sd) {
    if (sd->samples) {
        free(sd->samples);
        sd->samples = NULL;
    }
    sd->total_frames = 0;
    sd->channels = 0;
}

static inline float soft_limit(float x) {
    if (x > 26000.0f) {
        float d = x - 26000.0f;
        return 26000.0f + 6200.0f * (d / (6200.0f + d));
    } else if (x < -26000.0f) {
        float d = -x - 26000.0f;
        return -(26000.0f + 6200.0f * (d / (6200.0f + d)));
    }
    return x;
}

static inline void sample_voice(const SoundData *sd, uint64_t pos_fp, float vol, float *out_l, float *out_r) {
    uint32_t i0 = (uint32_t)(pos_fp >> 16);
    uint32_t i1 = (i0 + 1 < sd->total_frames) ? (i0 + 1) : i0;
    float frac = (float)(pos_fp & 0xFFFFu) * (1.0f / 65536.0f);
    if (sd->channels == 2) {
        float l0 = (float)sd->samples[i0 * 2 + 0], l1 = (float)sd->samples[i1 * 2 + 0];
        float r0 = (float)sd->samples[i0 * 2 + 1], r1 = (float)sd->samples[i1 * 2 + 1];
        *out_l += (l0 + (l1 - l0) * frac) * vol;
        *out_r += (r0 + (r1 - r0) * frac) * vol;
    } else {
        float s0 = (float)sd->samples[i0], s1 = (float)sd->samples[i1];
        float s = (s0 + (s1 - s0) * frac) * vol;
        *out_l += s;
        *out_r += s;
    }
}

static void mix_buffer(int16_t *buf) {
    EnterCriticalSection(&g_audioLock);

    for (int f = 0; f < BUFFER_FRAMES; f++) {
        float mix_l = 0.0f;
        float mix_r = 0.0f;

        if (g_bgmVoice.active && g_bgmVoice.data && g_bgmVoice.data->total_frames > 0) {
            const SoundData *sd = g_bgmVoice.data;
            float bv = g_bgmVoice.volume * g_bgmMasterVol;
            if (bv > 0.001f) sample_voice(sd, g_bgmVoice.pos_fp, bv, &mix_l, &mix_r);
            g_bgmVoice.pos_fp += g_bgmVoice.step_fp;
            if ((uint32_t)(g_bgmVoice.pos_fp >> 16) >= sd->total_frames) {
                if (g_bgmVoice.loop) {
                    g_bgmVoice.pos_fp = 0;
                } else {
                    g_bgmVoice.active = false;
                }
            }
        }

        float smv = g_sfxMasterVol;
        for (int v = 0; v < MAX_ACTIVE_VOICES; v++) {
            Voice *vc = &g_voices[v];
            if (!vc->active || !vc->data || (uint32_t)(vc->pos_fp >> 16) >= vc->data->total_frames) {
                vc->active = false;
                continue;
            }
            if (smv > 0.001f) sample_voice(vc->data, vc->pos_fp, vc->volume * smv, &mix_l, &mix_r);
            vc->pos_fp += vc->step_fp;
            if ((uint32_t)(vc->pos_fp >> 16) >= vc->data->total_frames) {
                vc->active = false;
            }
        }

        buf[f * 2 + 0] = (int16_t)soft_limit(mix_l);
        buf[f * 2 + 1] = (int16_t)soft_limit(mix_r);
    }

    LeaveCriticalSection(&g_audioLock);
}

static DWORD WINAPI AudioThreadProc(LPVOID lpParam) {
    (void)lpParam;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    for (int i = 0; i < NUM_BUFFERS; i++) {
        WAVEHDR *hdr = &g_waveHeaders[i];
        int16_t *buf = g_audioBuffers[i];
        mix_buffer(buf);

        memset(hdr, 0, sizeof(WAVEHDR));
        hdr->lpData = (LPSTR)buf;
        hdr->dwBufferLength = BUFFER_FRAMES * 2 * sizeof(int16_t);

        if (waveOutPrepareHeader(g_hWaveOut, hdr, sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
            waveOutWrite(g_hWaveOut, hdr, sizeof(WAVEHDR));
        }
    }

    while (g_audioRunning) {
        WaitForSingleObject(g_hAudioEvent, 10);

        if (!g_audioRunning) break;

        for (int i = 0; i < NUM_BUFFERS; i++) {
            WAVEHDR *hdr = &g_waveHeaders[i];
            if (hdr->dwFlags & WHDR_DONE) {
                int16_t *buf = g_audioBuffers[i];
                mix_buffer(buf);
                waveOutWrite(g_hWaveOut, hdr, sizeof(WAVEHDR));
            }
        }
    }

    return 0;
}

void audio_init(void) {
    timeBeginPeriod(1);
    InitializeCriticalSection(&g_audioLock);

    for (int i = 0; i < SFX_COUNT; i++) {
        if (SFX_PATHS[i]) {
            load_wav(SFX_PATHS[i], &g_sfxData[i]);
        }
    }

    memset(g_voices, 0, sizeof(g_voices));
    memset(g_bgmCache, 0, sizeof(g_bgmCache));
    memset(&g_bgmVoice, 0, sizeof(g_bgmVoice));
    g_currentBgm = BGM_NONE;

    if (BGM_PATHS[BGM_BOSS_REGI])     load_wav(BGM_PATHS[BGM_BOSS_REGI], &g_bgmCache[BGM_BOSS_REGI]);
    if (BGM_PATHS[BGM_BOSS_RAYQUAZA]) load_wav(BGM_PATHS[BGM_BOSS_RAYQUAZA], &g_bgmCache[BGM_BOSS_RAYQUAZA]);
    if (BGM_PATHS[BGM_BOSS_MEW])      load_wav(BGM_PATHS[BGM_BOSS_MEW], &g_bgmCache[BGM_BOSS_MEW]);

    WAVEFORMATEX wfx;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    g_hAudioEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (waveOutOpen(&g_hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)g_hAudioEvent, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        g_hWaveOut = NULL;
        return;
    }

    memset(g_waveHeaders, 0, sizeof(g_waveHeaders));
    g_audioRunning = true;
    g_hAudioThread = CreateThread(NULL, 0, AudioThreadProc, NULL, 0, NULL);
}

void audio_fini(void) {
    g_audioRunning = false;
    if (g_hAudioEvent) {
        SetEvent(g_hAudioEvent);
    }
    if (g_hAudioThread) {
        WaitForSingleObject(g_hAudioThread, 1000);
        CloseHandle(g_hAudioThread);
        g_hAudioThread = NULL;
    }

    if (g_hWaveOut) {
        waveOutReset(g_hWaveOut);
        for (int i = 0; i < NUM_BUFFERS; i++) {
            if (g_waveHeaders[i].dwFlags & WHDR_PREPARED) {
                waveOutUnprepareHeader(g_hWaveOut, &g_waveHeaders[i], sizeof(WAVEHDR));
            }
        }
        waveOutClose(g_hWaveOut);
        g_hWaveOut = NULL;
    }

    if (g_hAudioEvent) {
        CloseHandle(g_hAudioEvent);
        g_hAudioEvent = NULL;
    }

    for (int i = 0; i < SFX_COUNT; i++) {
        free_sound(&g_sfxData[i]);
    }
    for (int i = 0; i < BGM_COUNT; i++) {
        free_sound(&g_bgmCache[i]);
    }

    DeleteCriticalSection(&g_audioLock);
    timeEndPeriod(1);
}

void audio_play_sfx(SoundEffect sfx) {
    if (sfx < 0 || sfx >= SFX_COUNT) return;
    if (!g_sfxData[sfx].samples) return;

    DWORD now_ms = timeGetTime();
    DWORD self_gap = 40;
    if (sfx == SFX_LEVELUP || sfx == SFX_EVOLUTION || sfx == SFX_GAMEOVER || sfx == SFX_BOSS_SPAWN || sfx == SFX_BOSS_DOWN) self_gap = 20;
    else if (sfx == SFX_HIT) self_gap = 95;
    else if (sfx == SFX_GEM) self_gap = 75;
    else if (sfx == SFX_SELECT || sfx == SFX_CONFIRM) self_gap = 30;
    if ((DWORD)(now_ms - g_lastSfxMs[sfx]) < self_gap) return;
    g_lastSfxMs[sfx] = now_ms;

    EnterCriticalSection(&g_audioLock);

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

    int best_slot = -1;
    if (same_count >= 4) {
        best_slot = oldest_same;
    } else {
        uint64_t max_progress = 0;
        for (int i = 0; i < MAX_ACTIVE_VOICES; i++) {
            if (!g_voices[i].active) {
                best_slot = i;
                break;
            }
            if (g_voices[i].pos_fp > max_progress) {
                max_progress = g_voices[i].pos_fp;
                best_slot = i;
            }
        }
    }

    if (best_slot >= 0) {
        Voice *v = &g_voices[best_slot];
        v->data = target_sd;
        v->pos_fp = 0;
        v->step_fp = (uint32_t)(((uint64_t)target_sd->sample_rate << 16) / SAMPLE_RATE);
        if (sfx == SFX_LEVELUP || sfx == SFX_EVOLUTION || sfx == SFX_BOSS_SPAWN || sfx == SFX_BOSS_DOWN) v->volume = 0.78f;
        else if (sfx == SFX_HIT || sfx == SFX_GEM) v->volume = 0.34f;
        else if (sfx == SFX_SELECT || sfx == SFX_CONFIRM) v->volume = 0.55f;
        else if ((sfx >= SFX_ATTACK_NORMAL && sfx <= SFX_ATTACK_MELEE) || sfx == SFX_ATTACK) v->volume = 0.55f;
        else v->volume = 0.55f;
        v->active = true;
        v->loop = false;
    }

    LeaveCriticalSection(&g_audioLock);
}

void audio_play_type_attack(int move_type) {
    switch (move_type) {
        case 0:  audio_play_sfx(SFX_ATTACK_NORMAL); break;
        case 1:  audio_play_sfx(SFX_ATTACK_FIRE); break;
        case 2:  audio_play_sfx(SFX_ATTACK_WATER); break;
        case 3:  audio_play_sfx(SFX_ATTACK_GRASS); break;
        case 4:  audio_play_sfx(SFX_ATTACK_ELECTRIC); break;
        case 5:  audio_play_sfx(SFX_ATTACK_ICE); break;
        case 6:  audio_play_sfx(SFX_ATTACK_FIGHTING); break;
        case 7:  audio_play_sfx(SFX_ATTACK_POISON); break;
        case 8:  audio_play_sfx(SFX_ATTACK_GROUND); break;
        case 9:  audio_play_sfx(SFX_ATTACK_FLYING); break;
        case 10: audio_play_sfx(SFX_ATTACK_PSYCHIC); break;
        case 11: audio_play_sfx(SFX_ATTACK_BUG); break;
        case 12: audio_play_sfx(SFX_ATTACK_ROCK); break;
        case 13: audio_play_sfx(SFX_ATTACK_GHOST); break;
        case 14: audio_play_sfx(SFX_ATTACK_DRAGON); break;
        case 15: audio_play_sfx(SFX_ATTACK_DARK); break;
        case 16: audio_play_sfx(SFX_ATTACK_STEEL); break;
        case 17: audio_play_sfx(SFX_ATTACK_FAIRY); break;
        default: audio_play_sfx(SFX_ATTACK_NORMAL); break;
    }
}

void audio_play_bgm(MusicTrack track) {
    if (track == g_currentBgm) return;
    if (track <= BGM_NONE || track >= BGM_COUNT || !BGM_PATHS[track]) {
        audio_stop_bgm();
        return;
    }

    if (!g_bgmCache[track].samples) {
        load_wav(BGM_PATHS[track], &g_bgmCache[track]);
    }

    EnterCriticalSection(&g_audioLock);
    g_bgmVoice.active = false;
    if (g_bgmCache[track].samples) {
        g_bgmVoice.data = &g_bgmCache[track];
        g_bgmVoice.pos_fp = 0;
        g_bgmVoice.step_fp = (uint32_t)(((uint64_t)g_bgmCache[track].sample_rate << 16) / SAMPLE_RATE);
        g_bgmVoice.volume = 0.75f;
        g_bgmVoice.active = true;
        g_bgmVoice.loop = (track != BGM_VICTORY);
        g_currentBgm = track;
    } else {
        g_currentBgm = BGM_NONE;
    }
    LeaveCriticalSection(&g_audioLock);
}

void audio_stop_bgm(void) {
    EnterCriticalSection(&g_audioLock);
    g_bgmVoice.active = false;
    g_currentBgm = BGM_NONE;
    LeaveCriticalSection(&g_audioLock);
}

void audio_set_bgm_volume(float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    g_bgmMasterVol = vol;
}
void audio_set_sfx_volume(float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    g_sfxMasterVol = vol;
}
float audio_get_bgm_volume(void) { return g_bgmMasterVol; }
float audio_get_sfx_volume(void) { return g_sfxMasterVol; }

#endif
