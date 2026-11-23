#ifndef AUDIO_H
#define AUDIO_H

typedef enum {
    SFX_SELECT = 0,
    SFX_CONFIRM,
    SFX_HIT,
    SFX_ATTACK_NORMAL,
    SFX_ATTACK_FIRE,
    SFX_ATTACK_WATER,
    SFX_ATTACK_GRASS,
    SFX_ATTACK_ELECTRIC,
    SFX_ATTACK_ICE,
    SFX_ATTACK_FIGHTING,
    SFX_ATTACK_POISON,
    SFX_ATTACK_GROUND,
    SFX_ATTACK_FLYING,
    SFX_ATTACK_PSYCHIC,
    SFX_ATTACK_BUG,
    SFX_ATTACK_ROCK,
    SFX_ATTACK_GHOST,
    SFX_ATTACK_DRAGON,
    SFX_ATTACK_STEEL,
    SFX_ATTACK_DARK,
    SFX_ATTACK_FAIRY,
    SFX_ATTACK_BEAM,
    SFX_ATTACK_BUFF,
    SFX_ATTACK_AURA,
    SFX_ATTACK_MELEE,
    SFX_ATTACK,
    SFX_GEM,
    SFX_CHEST,
    SFX_LEVELUP,
    SFX_EVOLUTION,
    SFX_EXPLOSION,
    SFX_GAMEOVER,
    SFX_HEAL,
    SFX_BOSS_SPAWN,
    SFX_BOSS_DOWN,
    SFX_COUNT
} SoundEffect;

typedef enum {
    BGM_NONE = 0,
    BGM_TITLE,
    BGM_ROUTE_MEADOW,
    BGM_ROUTE_DESERT,
    BGM_ROUTE_LAKESIDE,
    BGM_BATTLE_WILD,
    BGM_BOSS_REGI,
    BGM_BOSS_RAYQUAZA,
    BGM_BOSS_MEW,
    BGM_VICTORY,
    BGM_COUNT
} MusicTrack;

void audio_init(void);
void audio_fini(void);
void audio_play_sfx(SoundEffect sfx);
void audio_play_type_attack(int move_type);
void audio_play_bgm(MusicTrack track);
void audio_stop_bgm(void);
void audio_set_bgm_volume(float vol);
void audio_set_sfx_volume(float vol);
float audio_get_bgm_volume(void);
float audio_get_sfx_volume(void);

#endif
