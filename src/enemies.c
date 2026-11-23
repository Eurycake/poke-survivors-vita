#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "game.h"
#include "renderer.h"
#include "audio.h"
#include "save_system.h"
#include "terrain.h"

static int get_enemy_dir(float vx, float vy) {
    float a = atan2f(vy, vx);
    int nd = (int)floorf((1.57079632679f - a) / 0.78539816339f + 0.5f);
    return ((nd % 8) + 8) % 8;
}

void enemies_init(void) {
    memset(g_game.enemies, 0, sizeof(g_game.enemies));
}

typedef struct {
    int dex;
    const char *name;
    float base_hp;
    float base_speed;
    float base_atk;
    float radius;
} WildPoke;


static const WildPoke MEADOW_TIER1[] = {
    { 265, "Wurmple",   18.0f, 110.0f,  7.0f, 15.0f },
    { 263, "Zigzagoon", 22.0f, 125.0f,  8.0f, 16.0f },
    { 261, "Poochyena", 24.0f, 122.0f,  9.0f, 16.0f },
    {  10, "Caterpie",  18.0f, 112.0f,  7.0f, 15.0f },
    {  16, "Pidgey",    20.0f, 126.0f,  8.0f, 15.0f },
    {  19, "Rattata",   21.0f, 128.0f,  8.0f, 15.0f }
};
static const WildPoke MEADOW_TIER2[] = {
    { 276, "Taillow",   32.0f, 138.0f, 11.0f, 15.0f },
    { 273, "Seedot",    35.0f, 115.0f, 10.0f, 16.0f },
    { 280, "Ralts",     34.0f, 120.0f, 10.0f, 16.0f },
    { 179, "Mareep",    36.0f, 114.0f, 10.0f, 16.0f },
    { 187, "Hoppip",    30.0f, 128.0f, 10.0f, 15.0f },
    { 401, "Kricketot", 33.0f, 118.0f, 10.0f, 16.0f }
};
static const WildPoke MEADOW_TIER3[] = {
    { 285, "Shroomish", 48.0f, 118.0f, 13.0f, 17.0f },
    { 309, "Electrike", 45.0f, 135.0f, 14.0f, 17.0f },
    { 316, "Gulpin",    55.0f, 112.0f, 12.0f, 18.0f },
    { 123, "Scyther",   52.0f, 142.0f, 16.0f, 19.0f },
    { 127, "Pinsir",    56.0f, 126.0f, 16.0f, 19.0f },
    { 214, "Heracross", 58.0f, 128.0f, 16.0f, 19.0f }
};
static const WildPoke MEADOW_TIER4[] = {
    { 264, "Linoone",   70.0f, 140.0f, 17.0f, 19.0f },
    { 286, "Breloom",   78.0f, 128.0f, 20.0f, 19.0f },
    { 277, "Swellow",   68.0f, 145.0f, 18.0f, 18.0f },
    { 181, "Ampharos",  82.0f, 122.0f, 19.0f, 20.0f },
    { 282, "Gardevoir", 76.0f, 130.0f, 20.0f, 19.0f },
    { 407, "Roserade",  74.0f, 134.0f, 19.0f, 19.0f }
};
static const WildPoke MEADOW_TIER5[] = {
    { 310, "Manectric", 95.0f, 145.0f, 23.0f, 20.0f },
    { 289, "Slaking",  140.0f, 115.0f, 28.0f, 24.0f },
    { 254, "Sceptile", 110.0f, 142.0f, 25.0f, 21.0f },
    { 212, "Scizor",   115.0f, 136.0f, 26.0f, 21.0f },
    { 468, "Togekiss", 112.0f, 132.0f, 24.0f, 21.0f },
    { 470, "Leafeon",  108.0f, 138.0f, 24.0f, 20.0f }
};


static const WildPoke CAVE_TIER1[] = {
    {  41, "Zubat",    18.0f, 125.0f,  7.0f, 15.0f },
    {  74, "Geodude",  24.0f, 105.0f,  9.0f, 16.0f },
    { 304, "Aron",     26.0f, 108.0f,  9.0f, 16.0f },
    {  95, "Onix",     28.0f, 104.0f,  9.0f, 18.0f },
    { 246, "Larvitar", 24.0f, 112.0f,  9.0f, 16.0f },
    { 524, "Roggenrola",25.0f,102.0f,  9.0f, 16.0f }
};
static const WildPoke CAVE_TIER2[] = {
    { 296, "Makuhita", 36.0f, 112.0f, 11.0f, 17.0f },
    { 299, "Nosepass", 38.0f, 100.0f, 10.0f, 17.0f },
    { 302, "Sableye",  32.0f, 125.0f, 11.0f, 16.0f },
    { 104, "Cubone",   34.0f, 116.0f, 11.0f, 16.0f },
    { 247, "Pupitar",  40.0f, 112.0f, 12.0f, 17.0f },
    { 529, "Drilbur",  34.0f, 126.0f, 12.0f, 16.0f }
};
static const WildPoke CAVE_TIER3[] = {
    {  42, "Golbat",   50.0f, 140.0f, 13.0f, 17.0f },
    {  75, "Graveler", 58.0f, 108.0f, 14.0f, 18.0f },
    { 305, "Lairon",   56.0f, 112.0f, 14.0f, 18.0f },
    { 111, "Rhyhorn",  60.0f, 110.0f, 15.0f, 19.0f },
    { 303, "Mawile",   52.0f, 122.0f, 14.0f, 17.0f },
    { 525, "Boldore",  62.0f, 104.0f, 14.0f, 19.0f }
};
static const WildPoke CAVE_TIER4[] = {
    { 297, "Hariyama", 82.0f, 118.0f, 20.0f, 20.0f },
    { 476, "Probopass",76.0f, 105.0f, 18.0f, 19.0f },
    { 169, "Crobat",   72.0f, 150.0f, 18.0f, 18.0f },
    { 112, "Rhydon",   88.0f, 112.0f, 20.0f, 21.0f },
    { 530, "Excadrill",80.0f, 132.0f, 21.0f, 20.0f },
    { 715, "Noivern",  76.0f, 146.0f, 20.0f, 20.0f }
};
static const WildPoke CAVE_TIER5[] = {
    {  76, "Golem",   130.0f, 105.0f, 27.0f, 24.0f },
    { 306, "Aggron",  125.0f, 115.0f, 27.0f, 23.0f },
    { 208, "Steelix", 140.0f, 100.0f, 26.0f, 26.0f },
    { 464, "Rhyperior",142.0f,106.0f, 28.0f, 25.0f },
    { 526, "Gigalith",134.0f, 104.0f, 27.0f, 24.0f },
    { 248, "Tyranitar",138.0f,118.0f, 28.0f, 24.0f }
};


static const WildPoke MAGMA_TIER1[] = {
    { 322, "Numel",    22.0f, 110.0f,  8.0f, 16.0f },
    { 218, "Slugma",   20.0f, 100.0f,  8.0f, 15.0f },
    {  74, "Geodude",  24.0f, 105.0f,  9.0f, 16.0f },
    { 228, "Houndour", 22.0f, 126.0f,  9.0f, 16.0f },
    {  37, "Vulpix",   20.0f, 122.0f,  8.0f, 15.0f },
    {  58, "Growlithe",23.0f, 124.0f,  9.0f, 16.0f }
};
static const WildPoke MAGMA_TIER2[] = {
    { 324, "Torkoal",  40.0f,  95.0f, 11.0f, 17.0f },
    {  75, "Graveler", 36.0f, 108.0f, 11.0f, 17.0f },
    {  66, "Machop",   38.0f, 110.0f, 11.0f, 17.0f },
    {  77, "Ponyta",   34.0f, 134.0f, 12.0f, 17.0f },
    { 126, "Magmar",   40.0f, 122.0f, 13.0f, 18.0f },
    { 240, "Magby",    32.0f, 120.0f, 11.0f, 16.0f }
};
static const WildPoke MAGMA_TIER3[] = {
    { 323, "Camerupt", 55.0f, 112.0f, 14.0f, 19.0f },
    { 219, "Magcargo", 52.0f, 100.0f, 13.0f, 18.0f },
    {  67, "Machoke",  58.0f, 115.0f, 14.0f, 18.0f },
    { 229, "Houndoom", 54.0f, 136.0f, 16.0f, 19.0f },
    {  38, "Ninetales",52.0f, 134.0f, 15.0f, 19.0f },
    {  59, "Arcanine", 62.0f, 136.0f, 16.0f, 20.0f }
};
static const WildPoke MAGMA_TIER4[] = {
    {  76, "Golem",    80.0f, 105.0f, 19.0f, 20.0f },
    {  68, "Machamp",  85.0f, 120.0f, 21.0f, 20.0f },
    { 324, "Torkoal",  78.0f,  98.0f, 18.0f, 19.0f },
    {  78, "Rapidash", 74.0f, 144.0f, 20.0f, 20.0f },
    { 609, "Chandelure",74.0f,128.0f, 21.0f, 19.0f },
    { 758, "Salazzle", 72.0f, 142.0f, 20.0f, 19.0f }
};
static const WildPoke MAGMA_TIER5[] = {
    { 323, "Camerupt",135.0f, 110.0f, 27.0f, 24.0f },
    { 306, "Aggron",  125.0f, 115.0f, 27.0f, 23.0f },
    { 208, "Steelix", 140.0f, 100.0f, 26.0f, 26.0f },
    { 467, "Magmortar",128.0f,122.0f, 27.0f, 23.0f },
    { 637, "Volcarona",118.0f,138.0f, 26.0f, 23.0f },
    { 555, "Darmanitan",126.0f,132.0f,27.0f, 22.0f }
};


static const WildPoke ICE_TIER1[] = {
    { 363, "Spheal",   22.0f, 108.0f,  8.0f, 15.0f },
    { 361, "Snorunt",  20.0f, 116.0f,  8.0f, 15.0f },
    { 220, "Swinub",   22.0f, 114.0f,  8.0f, 15.0f },
    { 225, "Delibird", 21.0f, 122.0f,  8.0f, 15.0f },
    { 582, "Vanillite",20.0f, 116.0f,  8.0f, 15.0f },
    { 613, "Cubchoo",  22.0f, 112.0f,  9.0f, 15.0f }
};
static const WildPoke ICE_TIER2[] = {
    {  86, "Seel",     35.0f, 116.0f, 10.0f, 16.0f },
    { 215, "Sneasel",  32.0f, 138.0f, 12.0f, 16.0f },
    {  41, "Zubat",    30.0f, 130.0f, 10.0f, 15.0f },
    { 459, "Snover",   36.0f, 112.0f, 11.0f, 17.0f },
    { 583, "Vanillish",34.0f, 118.0f, 11.0f, 17.0f },
    { 615, "Cryogonal",34.0f, 134.0f, 12.0f, 17.0f }
};
static const WildPoke ICE_TIER3[] = {
    { 364, "Sealeo",   54.0f, 112.0f, 13.0f, 18.0f },
    {  87, "Dewgong",  52.0f, 118.0f, 13.0f, 18.0f },
    {  42, "Golbat",   48.0f, 138.0f, 13.0f, 17.0f },
    { 478, "Froslass", 50.0f, 136.0f, 15.0f, 18.0f },
    { 712, "Bergmite", 56.0f, 108.0f, 13.0f, 18.0f },
    { 875, "Eiscue",   54.0f, 120.0f, 14.0f, 18.0f }
};
static const WildPoke ICE_TIER4[] = {
    { 362, "Glalie",   78.0f, 122.0f, 19.0f, 19.0f },
    { 221, "Piloswine",84.0f, 115.0f, 19.0f, 20.0f },
    { 124, "Jynx",     72.0f, 126.0f, 19.0f, 18.0f },
    { 460, "Abomasnow",86.0f, 116.0f, 20.0f, 21.0f },
    { 461, "Weavile",  74.0f, 148.0f, 21.0f, 19.0f },
    { 584, "Vanilluxe",78.0f, 124.0f, 19.0f, 19.0f }
};
static const WildPoke ICE_TIER5[] = {
    { 365, "Walrein", 132.0f, 112.0f, 25.0f, 24.0f },
    { 131, "Lapras",  135.0f, 116.0f, 24.0f, 24.0f },
    {  91, "Cloyster",120.0f, 118.0f, 25.0f, 22.0f },
    { 473, "Mamoswine",138.0f,120.0f, 27.0f, 24.0f },
    { 713, "Avalugg", 145.0f, 102.0f, 26.0f, 25.0f },
    { 614, "Beartic", 128.0f, 122.0f, 26.0f, 23.0f }
};


static const WildPoke DESERT_TIER1[] = {
    {  27, "Sandshrew", 20.0f, 115.0f,  8.0f, 15.0f },
    { 328, "Trapinch",  24.0f, 112.0f,  9.0f, 16.0f },
    { 331, "Cacnea",    22.0f, 118.0f,  8.0f, 16.0f },
    {  50, "Diglett",   18.0f, 128.0f,  8.0f, 15.0f },
    { 449, "Hippopotas",25.0f, 108.0f,  9.0f, 16.0f },
    { 551, "Sandile",   22.0f, 122.0f,  9.0f, 16.0f }
};
static const WildPoke DESERT_TIER2[] = {
    { 343, "Baltoy",    34.0f, 122.0f, 10.0f, 16.0f },
    { 322, "Numel",     38.0f, 114.0f, 11.0f, 17.0f },
    { 207, "Gligar",    36.0f, 132.0f, 11.0f, 17.0f },
    { 104, "Cubone",    35.0f, 118.0f, 11.0f, 16.0f },
    { 552, "Krokorok",  38.0f, 126.0f, 12.0f, 17.0f },
    { 559, "Scraggy",   36.0f, 120.0f, 11.0f, 16.0f }
};
static const WildPoke DESERT_TIER3[] = {
    { 329, "Vibrava",   48.0f, 134.0f, 14.0f, 18.0f },
    { 332, "Cacturne",  52.0f, 122.0f, 15.0f, 18.0f },
    { 323, "Camerupt",  58.0f, 115.0f, 16.0f, 19.0f },
    { 444, "Gabite",    54.0f, 134.0f, 16.0f, 19.0f },
    { 450, "Hippowdon", 64.0f, 110.0f, 16.0f, 21.0f },
    { 561, "Sigilyph",  50.0f, 136.0f, 15.0f, 19.0f }
};
static const WildPoke DESERT_TIER4[] = {
    {  28, "Sandslash", 72.0f, 130.0f, 18.0f, 19.0f },
    { 344, "Claydol",   80.0f, 124.0f, 19.0f, 20.0f },
    { 227, "Skarmory",  75.0f, 136.0f, 19.0f, 20.0f },
    { 472, "Gliscor",   78.0f, 138.0f, 20.0f, 20.0f },
    { 553, "Krookodile",82.0f, 134.0f, 21.0f, 21.0f },
    { 623, "Golurk",    88.0f, 114.0f, 21.0f, 22.0f }
};
static const WildPoke DESERT_TIER5[] = {
    { 330, "Flygon",   105.0f, 142.0f, 24.0f, 22.0f },
    { 248, "Tyranitar",135.0f, 120.0f, 27.0f, 24.0f },
    { 445, "Garchomp", 125.0f, 144.0f, 26.0f, 23.0f },
    { 530, "Excadrill",118.0f, 136.0f, 26.0f, 21.0f },
    { 464, "Rhyperior",138.0f, 108.0f, 27.0f, 24.0f },
    { 635, "Hydreigon",122.0f, 138.0f, 27.0f, 23.0f }
};


static const WildPoke FOREST_TIER1[] = {
    { 167, "Spinarak",  18.0f, 112.0f,  7.0f, 15.0f },
    { 163, "Hoothoot",  20.0f, 120.0f,  8.0f, 15.0f },
    {  43, "Oddish",    22.0f, 114.0f,  8.0f, 15.0f },
    {  92, "Gastly",    19.0f, 126.0f,  8.0f, 15.0f },
    { 708, "Phantump",  21.0f, 116.0f,  8.0f, 15.0f },
    { 570, "Zorua",     21.0f, 128.0f,  9.0f, 15.0f }
};
static const WildPoke FOREST_TIER2[] = {
    { 355, "Duskull",   34.0f, 118.0f, 10.0f, 16.0f },
    { 353, "Shuppet",   32.0f, 126.0f, 11.0f, 16.0f },
    { 274, "Nuzleaf",   36.0f, 124.0f, 11.0f, 17.0f },
    {  93, "Haunter",   34.0f, 132.0f, 12.0f, 17.0f },
    { 198, "Murkrow",   33.0f, 134.0f, 11.0f, 16.0f },
    { 200, "Misdreavus",34.0f, 128.0f, 11.0f, 16.0f }
};
static const WildPoke FOREST_TIER3[] = {
    { 302, "Sableye",   46.0f, 125.0f, 13.0f, 17.0f },
    { 357, "Tropius",   58.0f, 118.0f, 14.0f, 22.0f },
    {  44, "Gloom",     50.0f, 116.0f, 13.0f, 17.0f },
    { 168, "Ariados",   48.0f, 124.0f, 14.0f, 18.0f },
    { 429, "Mismagius", 52.0f, 136.0f, 15.0f, 18.0f },
    { 430, "Honchkrow", 56.0f, 132.0f, 16.0f, 19.0f }
};
static const WildPoke FOREST_TIER4[] = {
    { 356, "Dusclops",  76.0f, 122.0f, 18.0f, 20.0f },
    { 354, "Banette",   72.0f, 132.0f, 19.0f, 19.0f },
    { 275, "Shiftry",   78.0f, 130.0f, 19.0f, 20.0f },
    { 571, "Zoroark",   74.0f, 144.0f, 21.0f, 20.0f },
    { 778, "Mimikyu",   72.0f, 138.0f, 20.0f, 18.0f },
    { 858, "Hatterene", 80.0f, 118.0f, 20.0f, 20.0f }
};
static const WildPoke FOREST_TIER5[] = {
    { 709, "Trevenant", 115.0f, 122.0f, 25.0f, 22.0f },
    {  94, "Gengar",    100.0f, 142.0f, 26.0f, 21.0f },
    { 724, "Decidueye", 108.0f, 138.0f, 25.0f, 22.0f },
    { 477, "Dusknoir",  126.0f, 118.0f, 26.0f, 23.0f },
    { 609, "Chandelure",112.0f, 132.0f, 26.0f, 21.0f },
    { 461, "Weavile",   106.0f, 148.0f, 26.0f, 20.0f }
};


static const WildPoke LAKE_TIER1[] = {
    { 183, "Marill",    20.0f, 116.0f,  8.0f, 15.0f },
    {  54, "Psyduck",   22.0f, 118.0f,  8.0f, 16.0f },
    { 283, "Surskit",   19.0f, 128.0f,  8.0f, 15.0f },
    {  60, "Poliwag",   20.0f, 122.0f,  8.0f, 15.0f },
    { 129, "Magikarp",  18.0f, 118.0f,  7.0f, 15.0f },
    { 194, "Wooper",    22.0f, 112.0f,  8.0f, 15.0f }
};
static const WildPoke LAKE_TIER2[] = {
    { 341, "Corphish",  34.0f, 120.0f, 11.0f, 16.0f },
    { 339, "Barboach",  32.0f, 122.0f, 10.0f, 16.0f },
    { 278, "Wingull",   30.0f, 135.0f, 10.0f, 16.0f },
    {  61, "Poliwhirl", 35.0f, 124.0f, 11.0f, 17.0f },
    { 116, "Horsea",    31.0f, 126.0f, 11.0f, 15.0f },
    { 418, "Buizel",    34.0f, 134.0f, 11.0f, 16.0f }
};
static const WildPoke LAKE_TIER3[] = {
    { 318, "Carvanha",  44.0f, 142.0f, 15.0f, 17.0f },
    { 279, "Pelipper",  52.0f, 124.0f, 13.0f, 19.0f },
    { 271, "Lombre",    48.0f, 122.0f, 13.0f, 18.0f },
    { 117, "Seadra",    50.0f, 130.0f, 14.0f, 18.0f },
    { 195, "Quagsire",  58.0f, 114.0f, 14.0f, 19.0f },
    { 419, "Floatzel",  52.0f, 142.0f, 15.0f, 19.0f }
};
static const WildPoke LAKE_TIER4[] = {
    { 272, "Ludicolo",  74.0f, 128.0f, 18.0f, 20.0f },
    { 342, "Crawdaunt", 78.0f, 124.0f, 20.0f, 20.0f },
    { 340, "Whiscash",  82.0f, 120.0f, 18.0f, 20.0f },
    {  62, "Poliwrath", 84.0f, 124.0f, 20.0f, 20.0f },
    { 121, "Starmie",   74.0f, 142.0f, 20.0f, 19.0f },
    { 319, "Sharpedo",  72.0f, 146.0f, 21.0f, 20.0f }
};
static const WildPoke LAKE_TIER5[] = {
    { 130, "Gyarados", 125.0f, 135.0f, 26.0f, 24.0f },
    { 260, "Swampert", 120.0f, 128.0f, 26.0f, 22.0f },
    { 350, "Milotic",  115.0f, 132.0f, 24.0f, 22.0f },
    { 230, "Kingdra",  118.0f, 136.0f, 25.0f, 22.0f },
    { 134, "Vaporeon", 124.0f, 126.0f, 24.0f, 21.0f },
    { 658, "Greninja", 110.0f, 148.0f, 26.0f, 21.0f }
};


static const WildPoke TOWN_TIER1[] = {
    {  25, "Pikachu",   21.0f, 130.0f,  9.0f, 15.0f },
    { 133, "Eevee",     22.0f, 122.0f,  8.0f, 15.0f },
    {  52, "Meowth",    20.0f, 128.0f,  8.0f, 15.0f },
    { 300, "Skitty",    20.0f, 120.0f,  8.0f, 15.0f },
    { 311, "Plusle",    19.0f, 132.0f,  8.0f, 15.0f },
    { 312, "Minun",     19.0f, 132.0f,  8.0f, 15.0f }
};
static const WildPoke TOWN_TIER2[] = {
    {  58, "Growlithe", 36.0f, 126.0f, 11.0f, 16.0f },
    {  39, "Jigglypuff",38.0f, 112.0f, 10.0f, 16.0f },
    {  81, "Magnemite", 34.0f, 122.0f, 11.0f, 16.0f },
    { 100, "Voltorb",   32.0f, 138.0f, 11.0f, 16.0f },
    { 175, "Togepi",    34.0f, 114.0f, 10.0f, 15.0f },
    { 209, "Snubbull",  36.0f, 118.0f, 11.0f, 16.0f }
};
static const WildPoke TOWN_TIER3[] = {
    {  26, "Raichu",    52.0f, 140.0f, 15.0f, 18.0f },
    {  53, "Persian",   50.0f, 142.0f, 14.0f, 18.0f },
    {  82, "Magneton",  54.0f, 126.0f, 15.0f, 18.0f },
    { 101, "Electrode", 48.0f, 148.0f, 15.0f, 18.0f },
    { 301, "Delcatty",  52.0f, 132.0f, 14.0f, 18.0f },
    { 281, "Kirlia",    50.0f, 128.0f, 14.0f, 17.0f }
};
static const WildPoke TOWN_TIER4[] = {
    {  59, "Arcanine",  82.0f, 138.0f, 20.0f, 21.0f },
    { 135, "Jolteon",   72.0f, 148.0f, 20.0f, 19.0f },
    { 196, "Espeon",    74.0f, 140.0f, 20.0f, 19.0f },
    { 197, "Umbreon",   86.0f, 126.0f, 18.0f, 19.0f },
    { 282, "Gardevoir", 76.0f, 132.0f, 20.0f, 20.0f },
    { 448, "Lucario",   80.0f, 140.0f, 21.0f, 20.0f }
};
static const WildPoke TOWN_TIER5[] = {
    { 143, "Snorlax",  145.0f, 108.0f, 26.0f, 25.0f },
    { 181, "Ampharos", 120.0f, 126.0f, 25.0f, 22.0f },
    { 462, "Magnezone",126.0f, 122.0f, 26.0f, 22.0f },
    { 468, "Togekiss", 118.0f, 134.0f, 25.0f, 21.0f },
    { 700, "Sylveon",  116.0f, 130.0f, 25.0f, 21.0f },
    { 475, "Gallade",  118.0f, 142.0f, 27.0f, 21.0f }
};

static int ter_is_near_lake(int stage, float x, float y) {
    if (stage == STAGE_NATIONAL_PARK) return 0;
    int tx = (int)floorf(x / (float)TER_TILE);
    int ty = (int)floorf(y / (float)TER_TILE);
    if (stage == STAGE_ROUTE_29 && tx >= 89) return 0;
    for (int dy = -4; dy <= 4; dy++) {
        for (int dx = -4; dx <= 4; dx++) {
            int t = terrain_tile_kind(stage, tx + dx, ty + dy);
            if (t == TER_SHALLOW || t == TER_DEEP) return 1;
        }
    }
    return 0;
}

static WildPoke pick_regional_wild_pokemon(float ex, float ey, float game_time) {

    int stage = g_game.selected_stage;
    int ter = terrain_at(stage, ex, ey);
    int region = REGION_MEADOW;
    if (stage == STAGE_CAVE_GRANITE) {
        region = REGION_CAVE;
    } else if (stage == STAGE_CAVE_CHIMNEY) {
        region = REGION_MAGMA;
    } else if (stage == STAGE_CAVE_ICE) {
        region = REGION_ICE;
    } else if (stage == STAGE_TOWN_HOENN) {
        region = (ter == TER_SHALLOW || ter == TER_DEEP || ter_is_near_lake(stage, ex, ey)) ? REGION_LAKESIDE : REGION_TOWN;
    } else if (stage == STAGE_NATIONAL_PARK || ter == TER_SAND) {
        region = REGION_DESERT;
    } else if (ter == TER_SHALLOW || ter == TER_DEEP || ter_is_near_lake(stage, ex, ey)) {
        region = REGION_LAKESIDE;
    } else if (stage == STAGE_ILEX_FOREST || ter == TER_FOREST) {
        region = REGION_FOREST;
    } else {
        region = REGION_MEADOW;
    }


    int max_tier = 0;
    if (game_time >= 480.0f) max_tier = 4;
    else if (game_time >= 360.0f) max_tier = 3;
    else if (game_time >= 240.0f) max_tier = 2;
    else if (game_time >= 120.0f) max_tier = 1;
    else max_tier = 0;


    int tier = max_tier;
    int roll = rand() % 100;
    if (max_tier > 0 && roll < 30) tier = max_tier - 1;
    if (max_tier > 1 && roll < 10) tier = max_tier - 2;

    int idx = rand() % 6;

    switch (region) {
        case REGION_TOWN:
            if (tier == 4) return TOWN_TIER5[idx];
            if (tier == 3) return TOWN_TIER4[idx];
            if (tier == 2) return TOWN_TIER3[idx];
            if (tier == 1) return TOWN_TIER2[idx];
            return TOWN_TIER1[idx];

        case REGION_CAVE:
            if (tier == 4) return CAVE_TIER5[idx];
            if (tier == 3) return CAVE_TIER4[idx];
            if (tier == 2) return CAVE_TIER3[idx];
            if (tier == 1) return CAVE_TIER2[idx];
            return CAVE_TIER1[idx];

        case REGION_MAGMA:
            if (tier == 4) return MAGMA_TIER5[idx];
            if (tier == 3) return MAGMA_TIER4[idx];
            if (tier == 2) return MAGMA_TIER3[idx];
            if (tier == 1) return MAGMA_TIER2[idx];
            return MAGMA_TIER1[idx];

        case REGION_ICE:
            if (tier == 4) return ICE_TIER5[idx];
            if (tier == 3) return ICE_TIER4[idx];
            if (tier == 2) return ICE_TIER3[idx];
            if (tier == 1) return ICE_TIER2[idx];
            return ICE_TIER1[idx];

        case REGION_DESERT:
            if (tier == 4) return DESERT_TIER5[idx];
            if (tier == 3) return DESERT_TIER4[idx];
            if (tier == 2) return DESERT_TIER3[idx];
            if (tier == 1) return DESERT_TIER2[idx];
            return DESERT_TIER1[idx];

        case REGION_FOREST:
            if (tier == 4) return FOREST_TIER5[idx];
            if (tier == 3) return FOREST_TIER4[idx];
            if (tier == 2) return FOREST_TIER3[idx];
            if (tier == 1) return FOREST_TIER2[idx];
            return FOREST_TIER1[idx];

        case REGION_LAKESIDE:
            if (tier == 4) return LAKE_TIER5[idx];
            if (tier == 3) return LAKE_TIER4[idx];
            if (tier == 2) return LAKE_TIER3[idx];
            if (tier == 1) return LAKE_TIER2[idx];
            return LAKE_TIER1[idx];

        case REGION_MEADOW:
        default:
            if (tier == 4) return MEADOW_TIER5[idx];
            if (tier == 3) return MEADOW_TIER4[idx];
            if (tier == 2) return MEADOW_TIER3[idx];
            if (tier == 1) return MEADOW_TIER2[idx];
            return MEADOW_TIER1[idx];
    }
}

typedef struct {
    int   min_enemies;
    float spawn_delay;
    float hp_mult;
    int   speed_tier;
} WaveStageDef;


static const WaveStageDef GDD_WAVE_TABLE[15] = {
     {   8, 1.00f, 1.00f, 0 },
     {  14, 0.85f, 1.25f, 0 },
     {  20, 0.72f, 1.50f, 1 },
     {  26, 0.62f, 1.80f, 1 },
     {  34, 0.52f, 2.10f, 1 },
     {  42, 0.46f, 2.45f, 2 },
     {  48, 0.40f, 2.80f, 2 },
     {  56, 0.35f, 3.20f, 2 },
     {  64, 0.30f, 3.60f, 3 },
     {  72, 0.26f, 4.05f, 3 },
     {  78, 0.22f, 4.50f, 3 },
     {  84, 0.20f, 5.00f, 2 },
     {  90, 0.17f, 5.50f, 2 },
     {  95, 0.15f, 6.00f, 4 },
     { 100, 0.12f, 6.60f, 4 }
};

static int get_wave_stage_idx(float wave_time) {
    int idx = (int)(wave_time / 60.0f);
    if (idx < 0) idx = 0;
    if (idx > 14) idx = 14;
    return idx;
}

float enemies_get_current_spawn_delay(float wave_time) {
    if (g_game.final_reaper_spawned) return 999.0f;
    return GDD_WAVE_TABLE[get_wave_stage_idx(wave_time)].spawn_delay;
}

static float get_speed_tier_mult(int speed_tier) {
    switch (speed_tier) {
        case 0: return 0.84f;
        case 1: return 1.00f;
        case 2: return 1.18f;
        case 3: return 0.86f + (float)(rand() % 43) / 100.0f;
        case 4: return 1.34f;
        default: return 1.00f;
    }
}

void enemies_wipe_normal_mobs(void) {
    for (int e = 0; e < MAX_ENEMIES; e++) {
        Enemy *en = &g_game.enemies[e];
        if (en->active && !en->is_boss) {
            game_spawn_particles(en->x, en->y, RGBA8_C(255, 225, 80, 255), 6, 130.0f);
            en->active = false;
        }
    }
}

void enemies_spawn_swarm_event(const Player *player) {
    if (!player || g_game.final_reaper_spawned) return;
    int active_count = 0;
    for (int e = 0; e < MAX_ENEMIES; e++) if (g_game.enemies[e].active) active_count++;

    float zoom = render_get_camera_zoom();
    if (zoom < 0.1f) zoom = 1.0f;
    float cam_radius = 480.0f / zoom;
    int swarm_count = 18;
    float base_ang = ((float)(rand() % 360)) * (3.14159265f / 180.0f);
    float cycle_mult = (g_game.run_mode == MODE_ENDLESS && g_game.endless_cycle > 0) ? powf(2.0f, (float)g_game.endless_cycle) : 1.0f;
    float hyp_spd = (g_game.run_mode == MODE_TURBO) ? 1.40f : 1.0f;

    for (int i = 0; i < swarm_count && active_count < 120; i++) {
        int slot = -1;
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!g_game.enemies[e].active) { slot = e; break; }
        }
        if (slot < 0) break;
        Enemy *en = &g_game.enemies[slot];
        memset(en, 0, sizeof(Enemy));
        en->active = true;
        en->is_boss = false;

        float ang = base_ang + ((float)(i - swarm_count / 2)) * 0.08f;
        float dist = cam_radius + 40.0f + (float)(i % 4) * 24.0f;
        float ex = player->x + cosf(ang) * dist;
        float ey = player->y + sinf(ang) * dist;
        if (ex < 220.0f) ex = 220.0f; if (ex > WORLD_W - 220.0f) ex = WORLD_W - 220.0f;
        if (ey < 220.0f) ey = 220.0f; if (ey > WORLD_H - 220.0f) ey = WORLD_H - 220.0f;
        en->x = ex; en->y = ey;

        WildPoke mon = pick_regional_wild_pokemon(en->x, en->y, g_game.wave_clock);
        en->dex = mon.dex;
        en->name = mon.name;
        en->type = 9;
        en->max_hp = mon.base_hp * 1.35f * cycle_mult;
        en->hp = en->max_hp;
        en->speed = mon.base_speed * 1.25f * hyp_spd;
        en->dmg = mon.base_atk * 0.70f * cycle_mult;
        en->radius = mon.radius;
        en->dir = get_enemy_dir(player->x - en->x, player->y - en->y);
        en->attack_cd = 5.0f;
        en->attack_timer = 2.5f;
        active_count++;
    }
}

void enemies_spawn_encircle_ring(const Player *player) {
    if (!player || g_game.final_reaper_spawned) return;
    static const WildPoke TANK_RING[4] = {
        { 306, "Aggron",  135.0f, 102.0f, 26.0f, 23.0f },
        {  76, "Golem",   140.0f,  98.0f, 25.0f, 24.0f },
        { 289, "Slaking", 150.0f, 105.0f, 27.0f, 24.0f },
        { 365, "Walrein", 138.0f, 100.0f, 25.0f, 24.0f }
    };
    float cycle_mult = (g_game.run_mode == MODE_ENDLESS && g_game.endless_cycle > 0) ? powf(2.0f, (float)g_game.endless_cycle) : 1.0f;
    float hyp_spd = (g_game.run_mode == MODE_TURBO) ? 1.40f : 1.0f;
    int stage = g_game.selected_stage;

    for (int i = 0; i < 16; i++) {
        int slot = -1;
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!g_game.enemies[e].active) { slot = e; break; }
        }
        if (slot < 0) break;
        float ang = (float)i * (6.2831853f / 16.0f);
        float dist = 270.0f;
        float ex = player->x + cosf(ang) * dist;
        float ey = player->y + sinf(ang) * dist;
        if (ex < 180.0f) ex = 180.0f; if (ex > WORLD_W - 180.0f) ex = WORLD_W - 180.0f;
        if (ey < 180.0f) ey = 180.0f; if (ey > WORLD_H - 180.0f) ey = WORLD_H - 180.0f;
        if (terrain_is_blocked(terrain_at(stage, ex, ey))) continue;

        Enemy *en = &g_game.enemies[slot];
        memset(en, 0, sizeof(Enemy));
        en->active = true;
        en->is_boss = false;
        en->x = ex; en->y = ey;
        WildPoke tk = TANK_RING[i % 4];
        en->dex = tk.dex;
        en->name = tk.name;
        en->type = 10;
        en->max_hp = tk.base_hp * 2.80f * cycle_mult;
        en->hp = en->max_hp;
        en->speed = tk.base_speed * 0.85f * hyp_spd;
        en->dmg = tk.base_atk * 0.85f * cycle_mult;
        en->radius = tk.radius;
        en->dir = get_enemy_dir(player->x - en->x, player->y - en->y);
        en->attack_cd = 6.0f;
        en->attack_timer = 3.0f;
    }
}

void enemies_spawn_wave(float game_time, const Player *player) {
    if (g_game.run_mode == MODE_BOSSRUSH || g_game.final_reaper_spawned) {

        return;
    }

    int w_idx = get_wave_stage_idx(game_time);
    const WaveStageDef *wdef = &GDD_WAVE_TABLE[w_idx];


    int hard_cap = (w_idx == 5) ? 120 : 100;
    int active_count = 0;
    for (int e = 0; e < MAX_ENEMIES; e++) {
        if (g_game.enemies[e].active) active_count++;
    }
    int target_min = wdef->min_enemies;
    if (target_min > hard_cap) target_min = hard_cap;
    if (active_count >= target_min) return;


    int deficit = target_min - active_count;
    int count = (deficit > 5) ? 5 : deficit;
    if (count < 1) count = 1;

    float cycle_mult = (g_game.run_mode == MODE_ENDLESS && g_game.endless_cycle > 0) ? powf(2.0f, (float)g_game.endless_cycle) : 1.0f;
    float hyp_spd = (g_game.run_mode == MODE_TURBO) ? 1.40f : 1.0f;

    for (int i = 0; i < count; i++) {
        int slot = -1;
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!g_game.enemies[e].active) {
                slot = e;
                break;
            }
        }
        if (slot == -1) break;

        Enemy *en = &g_game.enemies[slot];
        memset(en, 0, sizeof(Enemy));
        en->active = true;
        en->is_boss = false;

        float zoom = render_get_camera_zoom();
        if (zoom < 0.1f) zoom = 1.0f;

        float spawn_ang = ((float)(rand() % 360)) * (3.14159265f / 180.0f);
        float base_dist = (520.0f / zoom) + (float)(rand() % 45);
        float spawn_dist = base_dist;
        float ex = player->x + cosf(spawn_ang) * spawn_dist;
        float ey = player->y + sinf(spawn_ang) * spawn_dist;
        bool baked_stage = g_game.selected_stage >= BAKED_FIRST_STAGE;
        bool found = false;
        for (int tries = 0; tries < (baked_stage ? 32 : 16); tries++) {
            float cand_a = spawn_ang + (float)(tries & 7) * 0.785398f;
            float cdist = base_dist + (float)(tries >> 3) * (120.0f / zoom);
            float cx = player->x + cosf(cand_a) * cdist;
            float cy = player->y + sinf(cand_a) * cdist;
            if (cx < 220.0f) cx = 220.0f;
            if (cx > WORLD_W - 220.0f) cx = WORLD_W - 220.0f;
            if (cy < 220.0f) cy = 220.0f;
            if (cy > WORLD_H - 220.0f) cy = WORLD_H - 220.0f;
            if (render_is_world_pos_visible(cx, cy, 38.0f)) continue;
            int ct = terrain_at(g_game.selected_stage, cx, cy);
            if (!terrain_is_blocked(ct) && !terrain_is_water(ct) &&
                (!baked_stage || terrain_line_clear(g_game.selected_stage, player->x, player->y, cx, cy))) {
                ex = cx;
                ey = cy;
                found = true;
                break;
            }
        }
        if (!found) {
            for (int ftry = 0; ftry < 16; ftry++) {
                float fa = spawn_ang + (float)ftry * 0.392699f;
                float fd = (base_dist + 90.0f / zoom) + (float)(ftry * 25);
                float fx = player->x + cosf(fa) * fd;
                float fy = player->y + sinf(fa) * fd;
                if (fx < 220.0f) fx = 220.0f;
                if (fx > WORLD_W - 220.0f) fx = WORLD_W - 220.0f;
                if (fy < 220.0f) fy = 220.0f;
                if (fy > WORLD_H - 220.0f) fy = WORLD_H - 220.0f;
                if (!render_is_world_pos_visible(fx, fy, 35.0f) &&
                    !terrain_is_blocked(terrain_at(g_game.selected_stage, fx, fy))) {
                    ex = fx; ey = fy;
                    break;
                }
            }
        }
        if (ex < 220.0f) ex = 220.0f;
        if (ex > WORLD_W - 220.0f) ex = WORLD_W - 220.0f;
        if (ey < 220.0f) ey = 220.0f;
        if (ey > WORLD_H - 220.0f) ey = WORLD_H - 220.0f;
        en->x = ex;
        en->y = ey;

        WildPoke mon = pick_regional_wild_pokemon(en->x, en->y, game_time);
        en->dex = mon.dex;
        en->name = mon.name;
        en->type = 8;


        float lv_hp_mult  = 1.0f + fmaxf(0.0f, (float)(player->level - 1)) * 0.16f;
        float lv_dmg_mult = 1.0f + fmaxf(0.0f, (float)(player->level - 1)) * 0.045f;
        if (lv_dmg_mult > 2.20f) lv_dmg_mult = 2.20f;
        float eff_hp_mult = fmaxf(wdef->hp_mult, lv_hp_mult);
        en->max_hp = mon.base_hp * eff_hp_mult * cycle_mult;
        en->hp = en->max_hp;
        en->speed = mon.base_speed * get_speed_tier_mult(wdef->speed_tier) * hyp_spd;
        en->dmg = (mon.base_atk * 0.45f + (float)w_idx * 0.55f) * lv_dmg_mult * cycle_mult;
        if (en->dmg > 26.0f * cycle_mult) en->dmg = 26.0f * cycle_mult;
        en->radius = mon.radius;
        en->hit_flash = 0.0f;
        en->slow_t = en->stun_t = en->stun_cd = en->dot_t = 0.0f;
        en->is_leeched = false;
        en->jump_timer = 0.0f;
        en->jump_vx = 0.0f;
        en->jump_vy = 0.0f;
        en->anim_timer = 0.0f;
        en->anim_frame = rand() % 4;
        en->dir = get_enemy_dir(player->x - en->x, player->y - en->y);
        RegionId reg = (en->x < WORLD_W * 0.5f) ? ((en->y < WORLD_H * 0.5f) ? REGION_DESERT : REGION_MEADOW)
                                                : ((en->y < WORLD_H * 0.5f) ? REGION_FOREST : REGION_LAKESIDE);
        en->elem_type = (int)reg;
        en->attack_cd = 4.5f + (float)(rand() % 25) / 10.0f;
        en->attack_timer = 1.5f + (float)(rand() % 35) / 10.0f;
    }
}

void enemies_spawn_boss_dex(int dex, const Player *player) {
    if (dex <= 0 || dex >= 1025) dex = 377;
    int slot = -1;
    for (int e = 0; e < MAX_ENEMIES; e++) {
        if (!g_game.enemies[e].active) {
            slot = e;
            break;
        }
    }
    if (slot == -1) {
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!g_game.enemies[e].is_boss) {
                slot = e;
                break;
            }
        }
    }
    if (slot == -1) slot = 0;

    Enemy *en = &g_game.enemies[slot];
    memset(en, 0, sizeof(Enemy));
    en->active = true;
    en->is_boss = true;
    en->attack_cd = 3.0f;
    en->attack_timer = 2.0f;
    en->dex = dex;

    const char *boss_name = "Boss";
    float boss_hp = 3600.0f;
    float boss_spd = 115.0f;
    float boss_atk = 36.0f;
    float boss_rad = 38.0f;

    switch (dex) {
        case 377: boss_name = "Regirock";  boss_hp = 4200.0f; boss_spd = 105.0f; boss_atk = 34.0f; boss_rad = 36.0f; break;
        case 378: boss_name = "Regice";    boss_hp = 4000.0f; boss_spd = 110.0f; boss_atk = 34.0f; boss_rad = 36.0f; break;
        case 379: boss_name = "Registeel"; boss_hp = 4400.0f; boss_spd = 108.0f; boss_atk = 36.0f; boss_rad = 36.0f; break;
        case 95:  boss_name = "Onix";      boss_hp = 3500.0f; boss_spd = 112.0f; boss_atk = 30.0f; boss_rad = 38.0f; break;
        case 130: boss_name = "Gyarados";  boss_hp = 4600.0f; boss_spd = 125.0f; boss_atk = 38.0f; boss_rad = 42.0f; break;
        case 6:   boss_name = "Charizard"; boss_hp = 4500.0f; boss_spd = 130.0f; boss_atk = 40.0f; boss_rad = 38.0f; break;
        case 94:  boss_name = "Gengar";    boss_hp = 3800.0f; boss_spd = 132.0f; boss_atk = 42.0f; boss_rad = 34.0f; break;
        case 144: boss_name = "Articuno";  boss_hp = 5000.0f; boss_spd = 126.0f; boss_atk = 38.0f; boss_rad = 40.0f; break;
        case 145: boss_name = "Zapdos";    boss_hp = 5000.0f; boss_spd = 136.0f; boss_atk = 42.0f; boss_rad = 40.0f; break;
        case 146: boss_name = "Moltres";   boss_hp = 5000.0f; boss_spd = 128.0f; boss_atk = 40.0f; boss_rad = 40.0f; break;
        case 149: boss_name = "Dragonite"; boss_hp = 4900.0f; boss_spd = 134.0f; boss_atk = 40.0f; boss_rad = 40.0f; break;
        case 150: boss_name = "Mewtwo";    boss_hp = 8500.0f; boss_spd = 142.0f; boss_atk = 50.0f; boss_rad = 44.0f; break;
        case 151: boss_name = "Mew";       boss_hp = 5200.0f; boss_spd = 145.0f; boss_atk = 42.0f; boss_rad = 34.0f; break;
        case 243: boss_name = "Raikou";    boss_hp = 5100.0f; boss_spd = 144.0f; boss_atk = 42.0f; boss_rad = 38.0f; break;
        case 244: boss_name = "Entei";     boss_hp = 5400.0f; boss_spd = 134.0f; boss_atk = 44.0f; boss_rad = 40.0f; break;
        case 245: boss_name = "Suicune";   boss_hp = 5300.0f; boss_spd = 136.0f; boss_atk = 40.0f; boss_rad = 38.0f; break;
        case 248: boss_name = "Tyranitar"; boss_hp = 5600.0f; boss_spd = 118.0f; boss_atk = 44.0f; boss_rad = 42.0f; break;
        case 249: boss_name = "Lugia";     boss_hp = 6600.0f; boss_spd = 138.0f; boss_atk = 44.0f; boss_rad = 44.0f; break;
        case 250: boss_name = "Ho-Oh";     boss_hp = 6600.0f; boss_spd = 136.0f; boss_atk = 46.0f; boss_rad = 44.0f; break;
        case 373: boss_name = "Salamence"; boss_hp = 5200.0f; boss_spd = 140.0f; boss_atk = 43.0f; boss_rad = 42.0f; break;
        case 376: boss_name = "Metagross"; boss_hp = 5800.0f; boss_spd = 120.0f; boss_atk = 45.0f; boss_rad = 42.0f; break;
        case 380: boss_name = "Latias";    boss_hp = 5100.0f; boss_spd = 146.0f; boss_atk = 41.0f; boss_rad = 38.0f; break;
        case 381: boss_name = "Latios";    boss_hp = 5200.0f; boss_spd = 148.0f; boss_atk = 44.0f; boss_rad = 38.0f; break;
        case 382: boss_name = "Kyogre";    boss_hp = 7000.0f; boss_spd = 132.0f; boss_atk = 47.0f; boss_rad = 46.0f; break;
        case 383: boss_name = "Groudon";   boss_hp = 7200.0f; boss_spd = 126.0f; boss_atk = 48.0f; boss_rad = 46.0f; break;
        case 384: boss_name = "Rayquaza";  boss_hp = 6800.0f; boss_spd = 140.0f; boss_atk = 46.0f; boss_rad = 46.0f; break;
        case 386: boss_name = "Deoxys";    boss_hp = 5900.0f; boss_spd = 152.0f; boss_atk = 49.0f; boss_rad = 38.0f; break;
        case 445: boss_name = "Garchomp";  boss_hp = 5400.0f; boss_spd = 144.0f; boss_atk = 45.0f; boss_rad = 40.0f; break;
        case 483: boss_name = "Dialga";    boss_hp = 7400.0f; boss_spd = 130.0f; boss_atk = 49.0f; boss_rad = 46.0f; break;
        case 484: boss_name = "Palkia";    boss_hp = 7300.0f; boss_spd = 134.0f; boss_atk = 49.0f; boss_rad = 46.0f; break;
        case 487: boss_name = "Giratina";  boss_hp = 7800.0f; boss_spd = 136.0f; boss_atk = 50.0f; boss_rad = 48.0f; break;
        case 491: boss_name = "Darkrai";   boss_hp = 5800.0f; boss_spd = 146.0f; boss_atk = 48.0f; boss_rad = 38.0f; break;
        case 493: boss_name = "Arceus";    boss_hp = 10000.0f; boss_spd = 138.0f; boss_atk = 56.0f; boss_rad = 48.0f; break;
        default:  boss_name = "Legendary"; break;
    }

    en->name = boss_name;


    static const float GDD_BOSS_MULTS[7] = { 500.0f, 800.0f, 1200.0f, 2000.0f, 2500.0f, 3500.0f, 4500.0f };
    int b_stage = g_game.bosses_defeated_in_run % 8;
    int eff_lvl = (player->level > 1) ? player->level : 1;
    float cycle_mult = (g_game.run_mode == MODE_ENDLESS && g_game.endless_cycle > 0) ? powf(2.0f, (float)g_game.endless_cycle) : 1.0f;
    float hyp_spd = (g_game.run_mode == MODE_TURBO) ? 1.40f : 1.0f;

    if (dex == 493) {

        enemies_wipe_normal_mobs();
        g_game.final_reaper_spawned = true;
        g_game.reaper_banner_timer = 5.0f;
        int reaper_lvl = (eff_lvl < 25) ? 25 : eff_lvl;
        en->max_hp = 10000.0f * (float)reaper_lvl * cycle_mult;
        en->attack_cd = 2.30f;
        en->attack_timer = 1.50f;
        en->arceus_plate = 10;
        en->plate_timer = 15.0f;
    } else {
        float gdd_mult = GDD_BOSS_MULTS[b_stage < 7 ? b_stage : 6];

        float mon_ratio = boss_hp / 5000.0f;
        if (mon_ratio < 0.80f) mon_ratio = 0.80f;
        if (mon_ratio > 1.35f) mon_ratio = 1.35f;
        en->max_hp = gdd_mult * (float)eff_lvl * mon_ratio * cycle_mult;
        if (en->max_hp < boss_hp * 0.75f) en->max_hp = boss_hp * 0.75f;
        if (g_game.run_mode == MODE_BOSSRUSH) {
            en->max_hp = (2800.0f + (float)g_game.bosses_defeated_in_run * 1800.0f) * mon_ratio;
        }

        if (b_stage == 4 && g_game.run_mode != MODE_BOSSRUSH && !g_game.encircle_triggered) {
            g_game.encircle_triggered = true;
            enemies_spawn_encircle_ring(player);
        }
    }

    float lvl_dmg = (1.0f + (float)(player->level - 1) * 0.025f) * cycle_mult;
    float lvl_spd = 1.0f + (float)(player->level - 1) * 0.008f;
    if (lvl_spd > 1.25f) lvl_spd = 1.25f;

    en->hp = en->max_hp;
    en->speed = boss_spd * lvl_spd * hyp_spd;
    en->dmg = (boss_atk * 0.65f) * lvl_dmg;
    if (en->dmg > 32.0f * cycle_mult) en->dmg = 32.0f * cycle_mult;
    en->radius = boss_rad * (dex == 493 ? 1.45f : 1.35f);
    en->hit_flash = 0.0f;
    en->jump_timer = 0.0f;
    en->jump_vx = 0.0f;
    en->jump_vy = 0.0f;
    en->anim_timer = 0.0f;
    en->anim_frame = 0;


    en->boss_dash_cd = 8.0f;
    en->boss_dash_timer = 0.0f;
    en->boss_dash_vx = 0.0f;
    en->boss_dash_vy = 0.0f;


    en->has_laser_skill = (dex != 95 && dex != 94 && dex != 445);
    en->laser_cd = (dex == 493) ? 5.5f : 7.5f;
    en->laser_warn_timer = 0.0f;
    en->laser_fire_timer = 0.0f;
    en->laser_angle = 0.0f;
    en->laser_len = (dex == 493) ? 620.0f : 520.0f;
    en->laser_half_w = (dex == 493) ? 42.0f : 32.0f;

    audio_play_sfx(SFX_BOSS_SPAWN);
    if (dex == 493 || dex == 150 || dex == 151 || dex == 94 || dex == 380 || dex == 381 || dex == 386 || dex == 487 || dex == 491)
        audio_play_bgm(BGM_BOSS_MEW);
    else if (dex == 384 || dex == 382 || dex == 383 || dex == 6 || dex == 130 || dex == 144 || dex == 145 || dex == 146 ||
             dex == 149 || dex == 249 || dex == 250 || dex == 373 || dex == 445 || dex == 483 || dex == 484)
        audio_play_bgm(BGM_BOSS_RAYQUAZA);
    else
        audio_play_bgm(BGM_BOSS_REGI);

    float zoom = render_get_camera_zoom();
    if (zoom < 0.1f) zoom = 1.0f;
    float base_ang = ((float)(rand() % 360)) * (3.14159265f / 180.0f);
    float spawn_dist = (820.0f / zoom) + (float)(rand() % 80);
    float bx = player->x, by = player->y;
    bool found_spot = false;
    float best_dist2 = -1.0f;
    const int stage = g_game.selected_stage;
    const float margin = (stage >= BAKED_FIRST_STAGE) ? 96.0f : 220.0f;
    const float clear_r = en->radius + 28.0f;
    const float clear_r2 = clear_r * clear_r;
    static const float DIST_MULTS[5] = { 1.0f, 0.80f, 1.20f, 0.62f, 0.45f };

    for (int ring = 0; ring < 5 && !found_spot; ring++) {
        float dist = spawn_dist * DIST_MULTS[ring];
        for (int tries = 0; tries < 32; tries++) {
            float ang = base_ang + (float)tries * (6.2831853f / 32.0f);
            float tx = player->x + cosf(ang) * dist;
            float ty = player->y + sinf(ang) * dist;
            if (tx < margin || tx > WORLD_W - margin || ty < margin || ty > WORLD_H - margin) continue;
            if (ring < 3 && render_is_world_pos_visible(tx, ty, 50.0f)) continue;


            bool terrain_ok = true;
            for (int oy = -2; oy <= 2 && terrain_ok; oy++) {
                for (int ox = -2; ox <= 2; ox++) {
                    int ct = terrain_at(stage, tx + (float)ox * 32.0f, ty + (float)oy * 32.0f);
                    if (terrain_is_blocked(ct) || terrain_is_ledge(ct) || terrain_is_water(ct)) {
                        terrain_ok = false;
                        break;
                    }
                }
            }
            if (!terrain_ok) continue;


            bool prop_ok = true;
            for (int p = 0; p < MAX_PROPS; p++) {
                const WorldProp *wp = &g_game.props[p];
                if (!wp->active) continue;
                if (wp->type > PROP_TREE_DARK && wp->type != PROP_ROCK_A && wp->type != PROP_ROCK_B &&
                    wp->type != PROP_ROCK_ICE && wp->type != PROP_CACTUS_A && wp->type != PROP_CACTUS_B &&
                    wp->type != PROP_BERRY_TREE) continue;
                float pdx = wp->x - tx, pdy = wp->y - ty;
                if (pdx * pdx + pdy * pdy < clear_r2) { prop_ok = false; break; }
            }
            if (!prop_ok) continue;

            float dd2 = (tx - player->x) * (tx - player->x) + (ty - player->y) * (ty - player->y);
            if (dd2 > best_dist2) {
                best_dist2 = dd2;
                bx = tx;
                by = ty;
                found_spot = true;
            }
        }
    }
    if (!found_spot) {

        bx = player->x + cosf(base_ang) * 260.0f;
        by = player->y + sinf(base_ang) * 260.0f;
        if (bx < margin) bx = margin;
        if (bx > WORLD_W - margin) bx = WORLD_W - margin;
        if (by < margin) by = margin;
        if (by > WORLD_H - margin) by = WORLD_H - margin;
        int ct = terrain_at(stage, bx, by);
        if (terrain_is_blocked(ct) || terrain_is_ledge(ct) || terrain_is_water(ct)) {
            bx = player->x;
            by = player->y;
        }
    }
    en->x = bx;
    en->y = by;
    en->dir = get_enemy_dir(player->x - en->x, player->y - en->y);

    g_game.screen_shake = 0.5f;
    audio_play_sfx(SFX_EXPLOSION);
}

void enemies_spawn_boss(int boss_cat_idx, const Player *player) {
    int boss_dex = 377;
    if (g_game.run_timer >= 600.0f || boss_cat_idx == 18) {
        boss_dex = 150;
    } else if (g_game.run_timer >= 480.0f || boss_cat_idx == 17) {
        boss_dex = 384;
    } else if (g_game.run_timer >= 360.0f || boss_cat_idx == 16) {
        boss_dex = 379;
    } else if (g_game.run_timer >= 240.0f || boss_cat_idx == 15) {
        boss_dex = 378;
    }
    enemies_spawn_boss_dex(boss_dex, player);
}

static inline bool enemy_can_jump_ledge_tile(int t, float dirx, float diry) {
    return (t == TER_LEDGE_S && diry > 0.15f) ||
           (t == TER_LEDGE_N && diry < -0.15f) ||
           (t == TER_LEDGE_E && dirx > 0.15f) ||
           (t == TER_LEDGE_W && dirx < -0.15f);
}

static bool enemy_cliff_is_downhill(int stage, float cx, float cy, float lx, float ly, float sy_dir) {
    if (stage >= BAKED_FIRST_STAGE) {
        const BakedMap *bm = &g_baked_maps[stage - BAKED_FIRST_STAGE];
        if (bm->elev) {
            int tx0 = (int)(cx * (1.0f / 32.0f)), ty0 = (int)(cy * (1.0f / 32.0f));
            int tx1 = (int)(lx * (1.0f / 32.0f)), ty1 = (int)(ly * (1.0f / 32.0f));
            if (tx0 < 0) tx0 = 0;
            if (ty0 < 0) ty0 = 0;
            if (tx0 > bm->w) tx0 = bm->w;
            if (ty0 > bm->h) ty0 = bm->h;
            if (tx1 < 0) tx1 = 0;
            if (ty1 < 0) ty1 = 0;
            if (tx1 > bm->w) tx1 = bm->w;
            if (ty1 > bm->h) ty1 = bm->h;
            int ew = bm->w + 1;
            int e0 = (int)bm->elev[ty0 * ew + tx0];
            int e1 = (int)bm->elev[ty1 * ew + tx1];

            return (e1 < e0 - 1);
        }
    }

    return (sy_dir > 0.0f);
}

void enemies_update(float dt, Player *player) {

    static int near_solid_props[256];
    int n_near_props = 0;
    for (int p = 0; p < MAX_PROPS && n_near_props < 256; p++) {
        const WorldProp *pr = &g_game.props[p];
        if (!pr->active) continue;
        if (pr->type > PROP_TREE_DARK && pr->type != PROP_ROCK_A && pr->type != PROP_ROCK_B &&
            pr->type != PROP_ROCK_ICE && pr->type != PROP_CACTUS_A && pr->type != PROP_CACTUS_B &&
            pr->type != PROP_BERRY_TREE) continue;
        float pdx = pr->x - player->x, pdy = pr->y - player->y;
        if (pdx < -820.0f || pdx > 820.0f || pdy < -620.0f || pdy > 620.0f) continue;
        near_solid_props[n_near_props++] = p;
    }


#ifdef __vita__
    enum { PF_R = 18, PF_N = PF_R * 2 + 1 };
#else
    enum { PF_R = 26, PF_N = PF_R * 2 + 1 };
#endif
    static int16_t pf_dist[PF_N * PF_N];
    static int pf_stage = -1, pf_px = -9999, pf_py = -9999;
    static float pf_timer = 0.0f;
    int stage = g_game.selected_stage;
    int ptx = (int)floorf(player->x / (float)TER_TILE);
    int pty = (int)floorf(player->y / (float)TER_TILE);
    pf_timer -= dt;
#ifdef __vita__
    int dtx = ptx - pf_px, dty = pty - pf_py;
    if (dtx < 0) dtx = -dtx; if (dty < 0) dty = -dty;
    if (stage != pf_stage || dtx >= 3 || dty >= 3 || pf_timer <= 0.0f) {
        pf_stage = stage; pf_px = ptx; pf_py = pty; pf_timer = 0.28f;
#else
    if (stage != pf_stage || ptx != pf_px || pty != pf_py || pf_timer <= 0.0f) {
        pf_stage = stage; pf_px = ptx; pf_py = pty; pf_timer = 0.15f;
#endif
        uint8_t blk[PF_N * PF_N];
        for (int idx = 0; idx < PF_N * PF_N; idx++) { pf_dist[idx] = -1; blk[idx] = 0; }
        for (int gy = 0; gy < PF_N; gy++) {
            int ty = pty - PF_R + gy;
            for (int gx = 0; gx < PF_N; gx++) {
                int tx = ptx - PF_R + gx;
                if (terrain_is_blocked(terrain_tile_kind(stage, tx, ty))) blk[gy * PF_N + gx] = 1;
            }
        }
        for (int np = 0; np < n_near_props; np++) {
            const WorldProp *pr = &g_game.props[near_solid_props[np]];
            int gx = (int)floorf(pr->x / (float)TER_TILE) - (ptx - PF_R);
            int gy = (int)floorf(pr->y / (float)TER_TILE) - (pty - PF_R);
            if (gx >= 0 && gx < PF_N && gy >= 0 && gy < PF_N) blk[gy * PF_N + gx] = 1;
        }
        int q[PF_N * PF_N], qh = 0, qt = 0;
        int c_idx = PF_R * PF_N + PF_R;
        blk[c_idx] = 0;
        pf_dist[c_idx] = 0;
        q[qt++] = c_idx;
        static const int d4x[4] = { 1, -1, 0, 0 }, d4y[4] = { 0, 0, 1, -1 };
        while (qh < qt) {
            int cur = q[qh++];
            int cx = cur % PF_N, cy = cur / PF_N;
            int16_t nd = pf_dist[cur] + 1;
            for (int d = 0; d < 4; d++) {
                int nx = cx + d4x[d], ny = cy + d4y[d];
                if (nx < 0 || nx >= PF_N || ny < 0 || ny >= PF_N) continue;
                int ni = ny * PF_N + nx;
                if (blk[ni] || pf_dist[ni] >= 0) continue;
                pf_dist[ni] = nd;
                q[qt++] = ni;
            }
        }
    }


    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *en = &g_game.enemies[i];
        if (!en->active) continue;

        if (en->jump_timer > 0.0f) {
            en->jump_timer -= dt;
            en->x += en->jump_vx * dt;
            en->y += en->jump_vy * dt;
            en->anim_timer += dt * 10.0f;
            if (en->anim_timer >= 1.0f) { en->anim_timer -= 1.0f; en->anim_frame++; }
            continue;
        }

        float dx = player->x - en->x;
        float dy = player->y - en->y;
        float dist = sqrtf(dx * dx + dy * dy);


        float zoom = render_get_camera_zoom();
        if (zoom < 0.1f) zoom = 1.0f;
        if (en->is_boss && en->dex == 493) {
            float wrap_threshold = 760.0f / zoom;
            if (dist > wrap_threshold && en->laser_warn_timer <= 0.0f && en->laser_fire_timer <= 0.0f) {
                float wrap_dist = (460.0f / zoom) + 30.0f;
                float wrap_ang = player->has_move_angle ? (player->move_angle + ((float)(rand() % 120 - 60)) * 0.0174533f)
                                                        : (((float)(rand() % 360)) * 0.0174533f);
                float wx = player->x + cosf(wrap_ang) * wrap_dist;
                float wy = player->y + sinf(wrap_ang) * wrap_dist;
                float margin = (stage >= BAKED_FIRST_STAGE) ? 96.0f : 220.0f;
                if (wx < margin) wx = margin; if (wx > WORLD_W - margin) wx = WORLD_W - margin;
                if (wy < margin) wy = margin; if (wy > WORLD_H - margin) wy = WORLD_H - margin;
                if (!terrain_is_blocked(terrain_at(stage, wx, wy))) {
                    game_spawn_particles(en->x, en->y, RGBA8_C(255, 230, 120, 255), 14, 150.0f);
                    en->x = wx;
                    en->y = wy;
                    game_spawn_particles(en->x, en->y, RGBA8_C(255, 230, 120, 255), 18, 170.0f);
                    dx = player->x - en->x;
                    dy = player->y - en->y;
                    dist = sqrtf(dx * dx + dy * dy);
                }
            }
        }


        if (en->is_boss) {
            if (en->boss_dash_timer > 0.0f) {
                en->boss_dash_timer -= dt;
                float nx = en->x + en->boss_dash_vx * dt;
                float ny = en->y + en->boss_dash_vy * dt;
                float margin = (stage >= BAKED_FIRST_STAGE) ? 32.0f : 80.0f;
                if (nx < margin) nx = margin; if (nx > WORLD_W - margin) nx = WORLD_W - margin;
                if (ny < margin) ny = margin; if (ny > WORLD_H - margin) ny = WORLD_H - margin;
                if (!terrain_box_blocked(stage, nx, en->y, 10.0f, 12.0f, 8.0f)) en->x = nx;
                if (!terrain_box_blocked(stage, en->x, ny, 10.0f, 12.0f, 8.0f)) en->y = ny;
                en->dir = get_enemy_dir(en->boss_dash_vx, en->boss_dash_vy);
                en->anim_timer += dt * 16.0f;
                if (en->anim_timer >= 1.0f) { en->anim_timer -= 1.0f; en->anim_frame++; }
                if ((rand() % 2) == 0) {
                    game_spawn_particles(en->x, en->y, RGBA8_C(255, 190, 90, 220), 2, 70.0f);
                }
                continue;
            } else if (en->stun_t <= 0.0f && en->laser_warn_timer <= 0.0f && en->laser_fire_timer <= 0.0f) {
                en->boss_dash_cd -= dt;
                if (en->boss_dash_cd <= 0.0f && dist > 65.0f && dist < 440.0f) {
                    en->boss_dash_cd = 8.0f;
                    en->boss_dash_timer = 0.24f;
                    float d_spd = fmaxf(en->speed * 3.4f, 460.0f);
                    en->boss_dash_vx = (dx / dist) * d_spd;
                    en->boss_dash_vy = (dy / dist) * d_spd;
                    game_spawn_particles(en->x, en->y, RGBA8_C(255, 215, 90, 255), 12, 145.0f);
                    audio_play_sfx(SFX_ATTACK_MELEE);
                    continue;
                }
            }
        }

        if (dist > 1.0f) {
            float dirx = dx / dist;
            float diry = dy / dist;
            float spm = en->stun_t > 0.0f ? 0.0f : (en->slow_t > 0.0f ? 1.0f - en->slow_f : 1.0f);

            if (en->is_boss && en->dex != 493 && !render_is_world_pos_visible(en->x, en->y, 20.0f)) {
                spm *= 2.35f;
            }

            if (en->is_boss && (en->laser_warn_timer > 0.0f || en->laser_fire_timer > 0.0f)) {
                spm *= 0.15f;
            }


            int etx = (int)floorf(en->x / (float)TER_TILE);
            int ety = (int)floorf(en->y / (float)TER_TILE);
            int gx = etx - (ptx - PF_R), gy = ety - (pty - PF_R);
            int t_mid = terrain_at(stage, en->x + dirx * 26.0f, en->y + diry * 26.0f);
            int t_far = terrain_at(stage, en->x + dirx * 52.0f, en->y + diry * 52.0f);
            bool direct_clear = (dist < 420.0f) &&
                (!terrain_is_blocked(t_mid) || enemy_can_jump_ledge_tile(t_mid, dirx, diry)) &&
                (!terrain_is_blocked(t_far) || enemy_can_jump_ledge_tile(t_far, dirx, diry));
            if (!direct_clear && gx >= 1 && gx < PF_N - 1 && gy >= 1 && gy < PF_N - 1) {
                int16_t best_d = pf_dist[gy * PF_N + gx];
                if (best_d < 0) best_d = 30000;
                int best_ox = 0, best_oy = 0;
                float best_tie = 1e9f;
                for (int oy = -1; oy <= 1; oy++) {
                    for (int ox = -1; ox <= 1; ox++) {
                        if (ox == 0 && oy == 0) continue;
                        if (ox != 0 && oy != 0) {
                            if (pf_dist[gy * PF_N + (gx + ox)] < 0 || pf_dist[(gy + oy) * PF_N + gx] < 0) continue;
                        }
                        int16_t nd = pf_dist[(gy + oy) * PF_N + (gx + ox)];
                        if (nd < 0) continue;
                        float wx = ((float)(etx + ox) + 0.5f) * (float)TER_TILE;
                        float wy = ((float)(ety + oy) + 0.5f) * (float)TER_TILE;
                        float d2p = (wx - player->x) * (wx - player->x) + (wy - player->y) * (wy - player->y);
                        if (nd < best_d || (nd == best_d && d2p < best_tie)) {
                            best_d = nd; best_tie = d2p; best_ox = ox; best_oy = oy;
                        }
                    }
                }
                if (best_ox != 0 || best_oy != 0) {
                    float tx_w = ((float)(etx + best_ox) + 0.5f) * (float)TER_TILE;
                    float ty_w = ((float)(ety + best_oy) + 0.5f) * (float)TER_TILE;
                    float sdx = tx_w - en->x, sdy = ty_w - en->y;
                    float sl = sqrtf(sdx * sdx + sdy * sdy);
                    if (sl > 0.1f) { dirx = sdx / sl; diry = sdy / sl; }
                }
            }


            float step = en->speed * spm * dt;
            float look = fmaxf(en->radius + 8.0f, 24.0f);
            int t_ahead = terrain_at(stage, en->x + dirx * look, en->y + diry * look);
            if (terrain_is_blocked(t_ahead) && !enemy_can_jump_ledge_tile(t_ahead, dirx, diry)) {
                float base_a = atan2f(diry, dirx);
                static const float offs[8] = { 0.45f, -0.45f, 0.90f, -0.90f, 1.35f, -1.35f, 1.80f, -1.80f };
                int sign = (i & 1) ? -1 : 1;
                for (int k = 0; k < 8; k++) {
                    float a = base_a + offs[k] * (float)sign;
                    float cx = cosf(a), cy = sinf(a);
                    if (!terrain_is_blocked(terrain_at(stage, en->x + cx * look, en->y + cy * look))) {
                        dirx = cx; diry = cy;
                        break;
                    }
                }
            }

            if (terrain_is_smooth_ice(stage, en->x, en->y)) {

                static const float EDIR_VX[8] = { 0.0f, 0.7071f, 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f };
                static const float EDIR_VY[8] = { 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f, 0.0f, 0.7071f };
                int ed = (en->dir >= 0 && en->dir < 8) ? en->dir : 0;
                dirx = EDIR_VX[ed];
                diry = EDIR_VY[ed];
                spm *= 1.08f;
            }

            float vx = dirx * en->speed * spm;
            float vy = diry * en->speed * spm;
            float nx = en->x + vx * dt;
            float ny = en->y + vy * dt;


            float erx = 7.0f;
            float ern = 8.0f;
            float ers = 5.0f;

            int tx_hit = terrain_at(stage, nx + (dirx > 0.0f ? (erx + 2.0f) : -(erx + 2.0f)), en->y);
            int ty_hit = terrain_at(stage, en->x, ny + (diry > 0.0f ? (ers + 2.0f) : -(ern + 2.0f)));

            if (!terrain_box_blocked(stage, nx, en->y, erx, ern, ers)) {
                en->x = nx;
            } else if ((tx_hit == TER_LEDGE_E && dirx > 0.15f) || (tx_hit == TER_LEDGE_W && dirx < -0.15f) ||
                       (tx_hit == TER_CLIFF && fabsf(dirx) > 0.25f)) {

                float sx_dir = (dirx > 0.0f) ? 1.0f : -1.0f;
                float hop_d = (tx_hit == TER_CLIFF) ? 56.0f : 68.0f;
                float land_x = en->x + sx_dir * hop_d;
                int land = terrain_at(stage, land_x, en->y);
                bool can_hop = (!terrain_is_blocked(land) && !terrain_is_water(land));
                if (can_hop && tx_hit == TER_CLIFF) {
                    can_hop = enemy_cliff_is_downhill(stage, en->x, en->y, land_x, en->y, 0.0f);
                }
                if (can_hop) {
                    en->jump_timer = 0.34f;
                    en->jump_vx = sx_dir * fmaxf(en->speed * 1.35f, 185.0f);
                    en->jump_vy = 0.0f;
                    en->dir = (sx_dir > 0.0f) ? 2 : 6;
                    continue;
                } else {
                    float sy_slide = (dy >= 0.0f ? 1.0f : -1.0f) * step * 0.85f;
                    if (!terrain_box_blocked(stage, en->x, en->y + sy_slide, erx, ern, ers)) en->y += sy_slide;
                }
            } else {
                float sy_slide = (dy >= 0.0f ? 1.0f : -1.0f) * step * 0.85f;
                if (!terrain_box_blocked(stage, en->x, en->y + sy_slide, erx, ern, ers)) en->y += sy_slide;
            }

            if (!terrain_box_blocked(stage, en->x, ny, erx, ern, ers)) {
                en->y = ny;
            } else if ((ty_hit == TER_LEDGE_S && diry > 0.15f) || (ty_hit == TER_LEDGE_N && diry < -0.15f) || (ty_hit == TER_CLIFF && diry > 0.25f)) {
                float sy_dir = (diry > 0.0f) ? 1.0f : -1.0f;
                float hop_d = (ty_hit == TER_CLIFF) ? 56.0f : 68.0f;
                float land_y = en->y + sy_dir * hop_d;
                int land = terrain_at(stage, en->x, land_y);
                bool can_hop = (!terrain_is_blocked(land) && !terrain_is_water(land));
                if (can_hop && ty_hit == TER_CLIFF) {
                    can_hop = enemy_cliff_is_downhill(stage, en->x, en->y, en->x, land_y, sy_dir);
                }
                if (can_hop) {
                    en->jump_timer = 0.34f;
                    en->jump_vx = 0.0f;
                    en->jump_vy = sy_dir * fmaxf(en->speed * 1.35f, 185.0f);
                    en->dir = (sy_dir > 0.0f) ? 0 : 4;
                    continue;
                } else {
                    float sx_slide = (dx >= 0.0f ? 1.0f : -1.0f) * step * 0.85f;
                    if (!terrain_box_blocked(stage, en->x + sx_slide, en->y, erx, ern, ers)) en->x += sx_slide;
                }
            } else {
                float sx_slide = (dx >= 0.0f ? 1.0f : -1.0f) * step * 0.85f;
                if (!terrain_box_blocked(stage, en->x + sx_slide, en->y, erx, ern, ers)) en->x += sx_slide;
            }

            en->dir = get_enemy_dir(vx, vy);


            for (int np = 0; np < n_near_props; np++) {
                const WorldProp *pr = &g_game.props[near_solid_props[np]];
                float pdx = en->x - pr->x, pdy = en->y - pr->y;
                if (pdx > 38.0f || pdx < -38.0f || pdy > 38.0f || pdy < -38.0f) continue;
                float pr_rad = 0.0f;
                if (pr->type <= PROP_TREE_DARK) pr_rad = 10.0f;
                else if (pr->type == PROP_ROCK_A) pr_rad = 12.0f;
                else if (pr->type == PROP_ROCK_B || pr->type == PROP_ROCK_ICE) pr_rad = 9.0f;
                else if (pr->type == PROP_CACTUS_A || pr->type == PROP_CACTUS_B) pr_rad = 8.0f;
                else if (pr->type == PROP_BERRY_TREE) pr_rad = 10.0f;
                else continue;
                float en_col_r = fminf(en->radius * 0.45f, 9.0f);
                float total_r = pr_rad + en_col_r;
                float d2 = pdx * pdx + pdy * pdy;
                if (d2 < total_r * total_r) {
                    if (d2 < 0.001f) { pdx = 0.0f; pdy = 1.0f; d2 = 1.0f; }
                    float d = sqrtf(d2);
                    float nx_p = pdx / d, ny_p = pdy / d;
                    float pnx = en->x + nx_p * (total_r - d);
                    float pny = en->y + ny_p * (total_r - d);
                    if (!terrain_box_blocked(stage, pnx, en->y, erx, ern, ers)) en->x = pnx;
                    if (!terrain_box_blocked(stage, en->x, pny, erx, ern, ers)) en->y = pny;

                    float tx_p = -ny_p, ty_p = nx_p;
                    if (tx_p * dx + ty_p * dy < 0.0f) { tx_p = -tx_p; ty_p = -ty_p; }
                    float slide_x = en->x + tx_p * step * 0.95f;
                    float slide_y = en->y + ty_p * step * 0.95f;
                    if (!terrain_box_blocked(stage, slide_x, slide_y, erx, ern, ers)) {
                        en->x = slide_x;
                        en->y = slide_y;
                    }
                }
            }

            if (en->stun_t <= 0.0f) {
                en->anim_timer += dt * (en->slow_t > 0.0f ? 4.5f : 8.0f);
                if (en->anim_timer >= 1.0f) {
                    en->anim_timer -= 1.0f;
                    en->anim_frame++;
                }
            }
        }
    }


    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *en = &g_game.enemies[i];
        if (!en->active) continue;

        for (int j = i + 1; j < MAX_ENEMIES; j++) {
            Enemy *other = &g_game.enemies[j];
            if (!other->active) continue;

            float min_d = en->radius + other->radius + 6.0f;
            float sep_x = en->x - other->x;
            if (sep_x >= min_d || sep_x <= -min_d) continue;
            float sep_y = en->y - other->y;
            if (sep_y >= min_d || sep_y <= -min_d) continue;

            float sep_d2 = sep_x * sep_x + sep_y * sep_y;
            if (sep_d2 < min_d * min_d) {
                float sep_d = (sep_d2 > 0.0001f) ? sqrtf(sep_d2) : 0.0f;
                float overlap = min_d - sep_d;
                float nx = (sep_d > 0.01f) ? (sep_x / sep_d) : 1.0f;
                float ny = (sep_d > 0.01f) ? (sep_y / sep_d) : 0.0f;
                float push = overlap * 0.5f;

                if (!terrain_is_blocked(terrain_at(stage, en->x + nx * push, en->y))) en->x += nx * push;
                if (!terrain_is_blocked(terrain_at(stage, en->x, en->y + ny * push))) en->y += ny * push;
                if (!terrain_is_blocked(terrain_at(stage, other->x - nx * push, other->y))) other->x -= nx * push;
                if (!terrain_is_blocked(terrain_at(stage, other->x, other->y - ny * push))) other->y -= ny * push;
            }
        }
        float e_m = (stage >= BAKED_FIRST_STAGE) ? 20.0f : 60.0f;
        if (en->x < e_m) en->x = e_m;
        if (en->x > WORLD_W - e_m) en->x = WORLD_W - e_m;
        if (en->y < e_m) en->y = e_m;
        if (en->y > WORLD_H - e_m) en->y = WORLD_H - e_m;
    }


    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *en = &g_game.enemies[i];
        if (!en->active) continue;

        float dx = player->x - en->x;
        float dy = player->y - en->y;
        float dist = sqrtf(dx * dx + dy * dy);


        float hit_dist = en->radius + 12.0f;
        if (dist < hit_dist && player->dash_timer <= 0.0f) {
            bool can_hurt = true;
            if (!en->is_boss) {
                en->contact_timer += dt;
                if (en->contact_timer < 0.60f) {
                    can_hurt = false;
                }
            }
            if (can_hurt && player->iframe_timer <= 0.0f) {
                if (!en->is_boss) {
                    en->contact_timer = 0.25f;
                    if (dist > 0.1f) {
                        en->x -= (dx / dist) * 10.0f;
                        en->y -= (dy / dist) * 10.0f;
                    }
                }
                float taken = fmaxf(1.0f, en->dmg - (float)player->armor);
                player->hp -= taken;
                player->iframe_timer = 0.35f;
                g_game.screen_shake = 0.25f;
                audio_play_sfx(SFX_HIT);
                game_spawn_damage_popup(player->x, player->y - 20.0f, (int)taken, RGBA8_C(255, 60, 60, 255));

                if (player->hp <= 0.0f) {
                    if (g_game.revives_left > 0) {
                        game_trigger_revive(player);
                    } else {
                        player->hp = 0.0f;
                        g_game.state = STATE_GAME_OVER;
                        g_game.grace_timer = 0.7f;
                        audio_stop_bgm();
                        audio_play_sfx(SFX_GAMEOVER);

                        g_save.stats_runs++;
                        g_save.coins += g_game.coins_collected;
                        g_save.tickets += g_game.tickets_collected;
                        g_save.stats_kills += player->kills;
                        if (g_game.run_timer > g_save.stats_best_time) g_save.stats_best_time = g_game.run_timer;
                        if (player->level > g_save.stats_best_level) g_save.stats_best_level = player->level;
                        game_delete_saved_run(g_game.run_mode);
                        save_system_save();
                    }
                }
            }
        } else if (!en->is_boss && en->contact_timer > 0.0f) {
            en->contact_timer -= dt * 2.5f;
            if (en->contact_timer < 0.0f) en->contact_timer = 0.0f;
        }


        if (en->hit_flash > 0.0f) {
            en->hit_flash -= dt * 6.0f;
            if (en->hit_flash < 0.0f) en->hit_flash = 0.0f;
        }


        if (en->hp <= 0.0f) {
            en->active = false;
            player->kills++;

            game_spawn_particles(en->x, en->y, RGBA8_C(255, 230, 100, 255), en->is_boss ? 25 : 8, 140.0f);
            audio_play_sfx(en->is_boss ? SFX_EXPLOSION : SFX_HIT);


            int w_stage = get_wave_stage_idx(g_game.wave_clock);
            int gem_val = 1;
            if (en->is_boss || en->type == 9 || en->type == 10 || w_stage >= 10) gem_val = 3;
            else if (w_stage >= 5) gem_val = 2;
            game_spawn_gem(en->x, en->y, gem_val);


            if (en->is_boss) {
                audio_play_sfx(SFX_BOSS_DOWN);
                audio_play_bgm(BGM_BATTLE_WILD);

                g_game.bosses_defeated_in_run++;
                g_save.stats_bosses++;


                for (int c = 0; c < 5; c++) {
                    float co_ang = ((float)(rand() % 360)) * (3.14159265f / 180.0f);
                    float co_d = 20.0f + (float)(rand() % 30);
                    game_spawn_pickup(en->x + cosf(co_ang) * co_d, en->y + sinf(co_ang) * co_d, 3);
                }
                game_spawn_pickup(en->x, en->y, 1);
                game_spawn_pickup(en->x + 25.0f, en->y, 0);
                game_spawn_pickup(en->x - 26.0f, en->y, 5);


                if ((rand() % 100) < 40) {
                    game_spawn_pickup(en->x + 35.0f, en->y - 15.0f, 4);
                }

                if ((rand() % 100) < 35) {
                    game_spawn_pickup(en->x - 35.0f, en->y + 18.0f, 7);
                }


                bool any_boss_left = false;
                bool any_arceus_left = false;
                for (int bi = 0; bi < MAX_ENEMIES; bi++) {
                    if (g_game.enemies[bi].active && g_game.enemies[bi].is_boss) {
                        any_boss_left = true;
                        if (g_game.enemies[bi].dex == 493) any_arceus_left = true;
                    }
                }


                if (!any_boss_left) {
                    g_game.next_boss_timer = 120.0f;
                }


                int b_per_enc = (g_game.run_mode == MODE_ENDLESS) ? (1 + g_game.endless_cycle) : 1;
                if (b_per_enc > 5) b_per_enc = 5;
                if (en->dex != 493 && !any_boss_left) {
                    if (g_game.run_mode == MODE_ENDLESS) {
                        if (g_game.bosses_defeated_in_run >= 7 * b_per_enc) {
                            g_game.next_boss_dex = 493;
                            snprintf(g_game.next_boss_name, sizeof(g_game.next_boss_name), "ARCEUS");
                            render_preload_pokemon(493);
                        }
                    } else if ((g_game.bosses_defeated_in_run % 8) == 7) {
                        g_game.next_boss_dex = 493;
                        snprintf(g_game.next_boss_name, sizeof(g_game.next_boss_name), "ARCEUS [FINAL]");
                        render_preload_pokemon(493);
                    }
                }


                if (en->dex == 493) {
                    if (g_game.run_mode == MODE_ENDLESS) {
                        game_spawn_pickup(en->x + 18.0f, en->y + 18.0f, 5);
                        game_spawn_pickup(en->x - 18.0f, en->y - 18.0f, 5);

                        if (!any_arceus_left) {
                            g_game.endless_cycle++;
                            if (g_game.wave_clock < 840.0f) g_game.wave_clock = 840.0f;
                            g_game.final_reaper_spawned = false;
                            g_game.used_boss_mask = 0;
                            g_game.bosses_defeated_in_run = 0;
                            g_game.next_boss_dex = game_pick_next_boss(g_game.next_boss_name, sizeof(g_game.next_boss_name));
                            render_preload_pokemon(g_game.next_boss_dex);
                            g_game.next_boss_timer = 120.0f;
                            game_endless_warp_random_map();
                        }
                    } else {
                        g_game.state = STATE_VICTORY;
                        g_game.grace_timer = 1.0f;
                        audio_play_bgm(BGM_VICTORY);
                        audio_play_sfx(SFX_LEVELUP);

                        g_save.stats_runs++;
                        g_game.coins_collected += 1000;
                        g_game.tickets_collected += 3;
                        g_save.coins += g_game.coins_collected;
                        g_save.tickets += g_game.tickets_collected;
                        g_save.stats_kills += player->kills;
                        if (g_game.run_timer > g_save.stats_best_time) g_save.stats_best_time = g_game.run_timer;
                        if (player->level > g_save.stats_best_level) g_save.stats_best_level = player->level;
                        game_delete_saved_run(g_game.run_mode);
                        save_system_save();
                    }
                }


                if (g_game.run_mode == MODE_BOSSRUSH && g_game.state == STATE_PLAYING) {
                    if ((g_game.bosses_defeated_in_run % 8) == 7) {
                        g_game.next_boss_dex = 493;
                        snprintf(g_game.next_boss_name, sizeof(g_game.next_boss_name), "ARCEUS [FINAL]");
                        render_preload_pokemon(493);
                    } else {
                        g_game.next_boss_dex = game_pick_next_boss(g_game.next_boss_name, sizeof(g_game.next_boss_name));
                        render_preload_pokemon(g_game.next_boss_dex);
                    }
                    g_game.next_boss_timer = 3.0f;
                    g_game.boss_active = false;
                }
            } else {

                int drop_threshold = 15;
                int drop_roll = rand() % 100;
                if (drop_roll < 3) {
                    game_spawn_pickup(en->x, en->y, 0);
                } else if (drop_roll == 4) {
                    game_spawn_pickup(en->x, en->y, 2);
                } else if (drop_roll >= 5 && drop_roll < drop_threshold) {
                    game_spawn_pickup(en->x, en->y, 3);
                }
            }
        }
    }


    int active_mob_projs = 0;
    for (int p = 0; p < MAX_PROJECTILES; p++) {
        if (g_game.projectiles[p].active && g_game.projectiles[p].is_enemy && !g_game.projectiles[p].is_boss_proj) {
            active_mob_projs++;
        }
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *en = &g_game.enemies[i];
        if (!en->active) continue;
        if (en->is_boss) {
            en->alive_time += dt;

            if (en->dex == 493 && en->max_hp > 0.0f) {
                float hp_pct = en->hp / en->max_hp;
                float hyp = (g_game.run_mode == MODE_TURBO) ? 1.40f : 1.0f;
                if (hp_pct > 0.75f) {

                    en->speed = 138.0f * hyp;
                    en->attack_cd = 2.30f;
                } else if (hp_pct > 0.50f) {

                    en->speed = 148.0f * hyp;
                    en->attack_cd = 1.95f;
                } else if (hp_pct > 0.25f) {

                    en->speed = 160.0f * hyp;
                    en->attack_cd = 1.65f;
                } else {

                    en->speed = 175.0f * hyp;
                    en->attack_cd = 1.35f;
                }
                en->plate_timer -= dt;
                if (en->plate_timer <= 0.0f) {
                    en->plate_timer = 15.0f;
                    static const int ARCEUS_PLATES[6] = { 1, 2, 4, 5, 14, 15 };
                    en->arceus_plate = ARCEUS_PLATES[rand() % 6];
                    en->debuff_col = MOVE_TYPE_COLOR[en->arceus_plate];
                    game_spawn_particles(en->x, en->y, en->debuff_col, 22, 175.0f);
                }
            }


            if (en->has_laser_skill) {
                if (en->laser_warn_timer > 0.0f) {
                    en->laser_warn_timer -= dt;

                    if (en->laser_warn_timer > 1.45f) {
                        float lead_t = 0.55f;
                        float pred_x = player->x + (player->is_moving ? cosf(player->move_angle) * player->speed * lead_t : 0.0f);
                        float pred_y = player->y + (player->is_moving ? sinf(player->move_angle) * player->speed * lead_t : 0.0f);
                        float want_a = atan2f(pred_y - en->y, pred_x - en->x);
                        float diff = want_a - en->laser_angle;
                        while (diff > 3.14159265f) diff -= 6.2831853f;
                        while (diff < -3.14159265f) diff += 6.2831853f;
                        en->laser_angle += diff * fminf(1.0f, dt * 4.5f);
                        en->dir = get_enemy_dir(cosf(en->laser_angle), sinf(en->laser_angle));
                    }
                    if (en->laser_warn_timer <= 0.0f) {
                        en->laser_warn_timer = 0.0f;
                        en->laser_fire_timer = 0.45f;
                        audio_play_sfx(SFX_ATTACK_BEAM);
                        g_game.screen_shake = fmaxf(g_game.screen_shake, 0.35f);


                        bool l_psy  = (en->dex == 150 || en->dex == 151 || en->dex == 380 || en->dex == 381 || en->dex == 386 || (en->dex == 493 && en->arceus_plate == 10));
                        bool l_wat  = (en->dex == 130 || en->dex == 382 || en->dex == 245 || en->dex == 484 || (en->dex == 493 && en->arceus_plate == 2));
                        bool l_stl  = (en->dex == 377 || en->dex == 378 || en->dex == 379 || en->dex == 376 || en->dex == 483);
                        const char *b_start = "hyper_beam_start", *b_mid = "hyper_beam_mid", *b_end = "hyper_beam_end";
                        uint32_t b_col = RGBA8_C(255, 180, 75, 255);
                        if (l_psy) {
                            b_start = "psybeam_start"; b_mid = "psybeam_mid"; b_end = "psybeam_end";
                            b_col = RGBA8_C(245, 105, 255, 255);
                        } else if (l_wat) {
                            b_start = "hydro_pump_start"; b_mid = "hydro_pump_mid"; b_end = "hydro_pump_end";
                            b_col = RGBA8_C(85, 200, 255, 255);
                        } else if (l_stl) {
                            b_start = "flash_cannon_start"; b_mid = "flash_cannon_mid"; b_end = "flash_cannon_end";
                            b_col = RGBA8_C(210, 235, 255, 255);
                        }
                        float ca = cosf(en->laser_angle), sa = sinf(en->laser_angle);
                        int segs = 12;
                        float step_l = en->laser_len / (float)segs;
                        for (int s = 1; s <= segs; s++) {
                            int pslot = -1;
                            for (int p = 0; p < MAX_PROJECTILES; p++) {
                                if (!g_game.projectiles[p].active) { pslot = p; break; }
                            }
                            if (pslot < 0) break;
                            Projectile *bp = &g_game.projectiles[pslot];
                            memset(bp, 0, sizeof(Projectile));
                            bp->active = true;
                            bp->damage = 0;
                            bp->hits_left = 0;
                            bp->x = en->x + ca * ((float)s * step_l);
                            bp->y = en->y + sa * ((float)s * step_l);
                            bp->src_x = en->x;
                            bp->src_y = en->y;
                            bp->has_src = true;
                            bp->angle = en->laser_angle;
                            bp->color = b_col;
                            bp->life = 0.42f;
                            bp->max_life = 0.42f;
                            bp->draw_scale = (en->laser_half_w / 18.0f);
                            bp->alpha = 248;
                            const char *sh = (s == 1) ? b_start : ((s == segs) ? b_end : b_mid);
                            strncpy(bp->shape_name, sh, sizeof(bp->shape_name) - 1);
                            bp->shape_name[sizeof(bp->shape_name) - 1] = '\0';
                            bp->vfx_shape = bp->shape_name;
                        }
                    }
                } else if (en->laser_fire_timer > 0.0f) {
                    en->laser_fire_timer -= dt;

                    float ca = cosf(en->laser_angle), sa = sinf(en->laser_angle);
                    float pdx = player->x - en->x, pdy = player->y - en->y;
                    float along = pdx * ca + pdy * sa;
                    float perp  = fabsf(-pdx * sa + pdy * ca);
                    if (along >= -14.0f && along <= en->laser_len && perp <= (en->laser_half_w + 10.0f)) {
                        if (player->iframe_timer <= 0.0f && player->dash_timer <= 0.0f) {
                            float raw_l = 18.0f + (float)player->level * 0.70f + (g_game.run_timer / 60.0f) * 1.5f;
                            if (raw_l > 42.0f) raw_l = 42.0f;
                            float taken = fmaxf(1.0f, raw_l - (float)player->armor);
                            player->hp -= taken;
                            player->iframe_timer = 0.35f;
                            g_game.screen_shake = 0.35f;
                            audio_play_sfx(SFX_HIT);
                            game_spawn_damage_popup(player->x, player->y - 24.0f, (int)taken, RGBA8_C(255, 80, 40, 255));
                            if (player->hp <= 0.0f) {
                                if (g_game.revives_left > 0) {
                                    game_trigger_revive(player);
                                } else {
                                    player->hp = 0.0f;
                                    g_game.state = STATE_GAME_OVER;
                                    g_game.grace_timer = 0.7f;
                                    audio_stop_bgm();
                                    audio_play_sfx(SFX_GAMEOVER);
                                    g_save.stats_runs++;
                                    g_save.coins += g_game.coins_collected;
                                    g_save.tickets += g_game.tickets_collected;
                                    g_save.stats_kills += player->kills;
                                    if (g_game.run_timer > g_save.stats_best_time) g_save.stats_best_time = g_game.run_timer;
                                    if (player->level > g_save.stats_best_level) g_save.stats_best_level = player->level;
                                    game_delete_saved_run(g_game.run_mode);
                                    save_system_save();
                                }
                            }
                        }
                    }
                } else if (en->stun_t <= 0.0f) {
                    en->laser_cd -= dt;
                    float ldx = player->x - en->x, ldy = player->y - en->y;
                    float ldist = sqrtf(ldx * ldx + ldy * ldy);
                    if (en->laser_cd <= 0.0f && ldist < en->laser_len * 0.92f) {
                        en->laser_cd = (en->dex == 493) ? 7.5f : 10.0f;

                        float lead_t = 0.65f;
                        float pred_x = player->x + (player->is_moving ? cosf(player->move_angle) * player->speed * lead_t : 0.0f);
                        float pred_y = player->y + (player->is_moving ? sinf(player->move_angle) * player->speed * lead_t : 0.0f);
                        en->laser_angle = atan2f(pred_y - en->y, pred_x - en->x);
                        en->laser_warn_timer = 2.00f;
                        en->dir = get_enemy_dir(pred_x - en->x, pred_y - en->y);
                        audio_play_sfx(SFX_SELECT);
                    }
                }
            }
        }
        if (en->attack_cd <= 0.1f) en->attack_cd = en->is_boss ? 3.0f : 5.0f;
        en->attack_timer -= dt;
        if (en->attack_timer <= 0.0f) {
            en->attack_timer = en->attack_cd;
            float dx = player->x - en->x;
            float dy = player->y - en->y;
            float dist = sqrtf(dx * dx + dy * dy);

            if (en->is_boss) {

                int unlocked = 1 + (int)(en->alive_time / 15.0f);
                if (en->dex == 493 && en->max_hp > 0.0f) {
                    float hp_pct = en->hp / en->max_hp;
                    if (hp_pct <= 0.25f) unlocked = 5;
                    else if (hp_pct <= 0.50f && unlocked < 4) unlocked = 4;
                    else if (hp_pct <= 0.75f && unlocked < 3) unlocked = 3;
                }
                if (unlocked < 1) unlocked = 1;
                if (unlocked > 5) unlocked = 5;
                uint8_t ph = en->attack_phase % (uint8_t)unlocked;
                en->attack_phase++;

                bool is_psy   = (en->dex == 150 || en->dex == 151 || en->dex == 380 || en->dex == 381 || en->dex == 386 || (en->dex == 493 && en->arceus_plate == 10));
                bool is_drg   = (en->dex == 384 || en->dex == 149 || en->dex == 373 || en->dex == 249 || (en->dex == 493 && en->arceus_plate == 14));
                bool is_fire  = (en->dex == 6 || en->dex == 146 || en->dex == 244 || en->dex == 250 || en->dex == 383 || en->type == 16 || (en->dex == 493 && en->arceus_plate == 1));
                bool is_wat   = (en->dex == 130 || en->dex == 382 || en->dex == 245 || en->dex == 484 || en->type == 15 || (en->dex == 493 && en->arceus_plate == 2));
                bool is_rock  = (en->dex == 377 || en->dex == 95 || en->dex == 248 || en->dex == 445);
                bool is_ice   = (en->dex == 378 || en->dex == 144 || (en->dex == 493 && en->arceus_plate == 5));
                bool is_elec  = (en->dex == 145 || en->dex == 243 || (en->dex == 493 && en->arceus_plate == 4));
                bool is_dark  = (en->dex == 94 || en->dex == 487 || en->dex == 491 || (en->dex == 493 && en->arceus_plate == 15));


                const char *sh_a = "boss_steel", *sh_b = "steelshard";
                uint32_t col_a = RGBA8_C(215, 225, 240, 255), col_b = RGBA8_C(170, 235, 255, 255);
                if (is_psy)       { sh_a = "confusion";     sh_b = "psychic";       col_a = RGBA8_C(230, 80, 255, 255);  col_b = RGBA8_C(255, 110, 235, 255); }
                else if (is_drg)  { sh_a = "dragon_breath"; sh_b = "dragon_energy"; col_a = (en->dex == 249) ? RGBA8_C(170, 235, 255, 255) : RGBA8_C(100, 240, 160, 255); col_b = RGBA8_C(255, 200, 70, 255); }
                else if (is_fire) { sh_a = "ember";         sh_b = "flame";         col_a = RGBA8_C(255, 120, 20, 255);  col_b = RGBA8_C(255, 70, 30, 255); }
                else if (is_wat)  { sh_a = "water_gun";     sh_b = "whirlpool";     col_a = (en->dex == 484) ? RGBA8_C(210, 130, 255, 255) : RGBA8_C(60, 170, 255, 255); col_b = RGBA8_C(90, 210, 255, 255); }
                else if (is_rock) { sh_a = "rock_throw";     sh_b = "stone_edge";    col_a = RGBA8_C(215, 190, 160, 255); col_b = RGBA8_C(235, 200, 145, 255); }
                else if (is_ice)  { sh_a = "shard";         sh_b = "snow";          col_a = RGBA8_C(120, 220, 255, 255); col_b = RGBA8_C(175, 240, 255, 255); }
                else if (is_elec) { sh_a = "thunderbolt";   sh_b = "shock";         col_a = RGBA8_C(255, 235, 50, 255);  col_b = RGBA8_C(255, 245, 95, 255); }
                else if (is_dark) { sh_a = "shadow_ball";   sh_b = "sludge";        col_a = RGBA8_C(160, 60, 230, 255);  col_b = RGBA8_C(185, 70, 215, 255); }

                int shots = 4;
                if (ph == 0) {
                    if (is_psy)       shots = 8;
                    else if (is_drg)  shots = 3;
                    else if (is_fire) shots = 3;
                    else if (is_wat)  shots = 4;
                    else if (is_rock) shots = 5;
                    else if (is_ice)  shots = 4;
                    else if (is_elec) shots = 4;
                    else if (is_dark) shots = 4;
                    else              shots = 4;
                } else if (ph == 1) {
                    if (is_psy)       shots = 4;
                    else if (is_drg)  shots = 6;
                    else if (is_fire) shots = 5;
                    else if (is_wat)  shots = 6;
                    else if (is_rock) shots = 3;
                    else if (is_ice)  shots = 6;
                    else if (is_elec) shots = 8;
                    else if (is_dark) shots = 5;
                    else              shots = 3;
                } else if (ph == 2) {
                    shots = 10;
                } else if (ph == 3) {
                    shots = 8;
                } else {
                    shots = 12;
                }

                float base_a = atan2f(dy, dx);
                for (int s = 0; s < shots; s++) {
                    int pslot = -1;
                    for (int p = 0; p < MAX_PROJECTILES; p++) {
                        if (!g_game.projectiles[p].active) { pslot = p; break; }
                    }
                    if (pslot < 0) break;

                    Projectile *pj = &g_game.projectiles[pslot];
                    memset(pj, 0, sizeof(Projectile));
                    pj->active = true;
                    pj->is_enemy = true;
                    pj->is_boss_proj = true;
                    pj->homing = 0;
                    pj->x = en->x;
                    pj->y = en->y;

                    float p_ang = base_a;
                    float spd = 145.0f;
                    float pscale = 0.88f;
                    float prad = 10.0f;
                    int extra_dmg = 0;

                    if (ph <= 1) {
                        if (is_psy) {
                            if (ph == 0) {
                                p_ang = (float)s * (6.2831853f / 8.0f);
                                pj->vfx_shape = sh_a; pj->color = col_a; spd = 165.0f;
                            } else {
                                p_ang = base_a + ((float)s - 1.5f) * 0.11f;
                                pj->vfx_shape = sh_b; pj->color = col_b; spd = 230.0f; pscale = 1.05f; extra_dmg = 3;
                            }
                        } else if (is_drg) {
                            if (ph == 0) {
                                p_ang = base_a + ((float)s - 1.0f) * 0.32f;
                                pj->vfx_shape = sh_a; pj->color = col_a; pj->tint_by_color = true; spd = 165.0f;
                            } else {
                                p_ang = base_a + (float)s * (6.2831853f / 6.0f);
                                pj->vfx_shape = sh_b; pj->color = col_b; spd = 140.0f; pscale = 1.08f; extra_dmg = 2;
                            }
                        } else if (is_fire) {
                            if (ph == 0) {
                                p_ang = base_a + ((float)s - 1.0f) * 0.28f;
                                pj->vfx_shape = sh_a; pj->color = col_a;
                            } else {
                                p_ang = base_a + ((float)s - 2.0f) * 0.36f;
                                pj->vfx_shape = sh_b; pj->color = col_b; spd = 170.0f; pscale = 1.15f;
                            }
                        } else if (is_wat) {
                            if (ph == 0) {
                                p_ang = base_a + ((float)s - 1.5f) * 0.12f;
                                pj->vfx_shape = sh_a; pj->color = col_a; spd = 175.0f;
                            } else {
                                p_ang = base_a + (float)s * (6.2831853f / 6.0f);
                                pj->vfx_shape = sh_b; pj->color = col_b; spd = 125.0f; pscale = 1.15f; prad = 12.0f;
                            }
                        } else if (is_rock) {
                            if (ph == 0) {
                                p_ang = base_a + ((float)s - 2.0f) * 0.22f;
                                pj->vfx_shape = sh_a; pj->color = col_a;
                            } else {
                                p_ang = base_a + ((float)s - 1.0f) * 0.14f;
                                pj->vfx_shape = sh_b; pj->color = col_b; spd = 220.0f; pscale = 1.10f; prad = 12.0f; extra_dmg = 3;
                            }
                        } else if (is_ice) {
                            if (ph == 0) {
                                p_ang = base_a + ((float)s - 1.5f) * 0.25f;
                                pj->vfx_shape = sh_a; pj->color = col_a;
                            } else {
                                p_ang = base_a + (float)s * (6.2831853f / 6.0f);
                                pj->vfx_shape = sh_b; pj->color = col_b; spd = 130.0f; pscale = 1.12f;
                            }
                        } else if (is_elec) {
                            if (ph == 0) {
                                p_ang = base_a + ((float)s - 1.5f) * 0.20f;
                                pj->vfx_shape = sh_a; pj->color = col_a; spd = 185.0f;
                            } else {
                                p_ang = (float)s * (6.2831853f / 8.0f);
                                pj->vfx_shape = sh_b; pj->color = col_b; spd = 155.0f; pscale = 1.05f;
                            }
                        } else if (is_dark) {
                            if (ph == 0) {
                                p_ang = base_a + ((float)s - 1.5f) * 0.24f;
                                pj->vfx_shape = sh_a; pj->color = col_a;
                            } else {
                                p_ang = base_a + ((float)s - 2.0f) * 0.28f;
                                pj->vfx_shape = sh_b; pj->color = col_b; spd = 160.0f; pscale = 1.08f;
                            }
                        } else {
                            if (ph == 0) {
                                p_ang = base_a + ((float)s - 1.5f) * 0.25f;
                                pj->vfx_shape = sh_a; pj->color = col_a;
                            } else {
                                p_ang = base_a + ((float)s - 1.0f) * 0.13f;
                                pj->vfx_shape = sh_b; pj->color = col_b; spd = 225.0f; pscale = 1.12f; extra_dmg = 3;
                            }
                        }
                    } else if (ph == 2) {

                        p_ang = base_a + en->alive_time * 0.75f + (float)s * (6.2831853f / 10.0f);
                        pj->vfx_shape = (s & 1) ? sh_b : sh_a;
                        pj->color = (s & 1) ? col_b : col_a;
                        spd = 152.0f; pscale = 1.04f; prad = 11.0f; extra_dmg = 2;
                    } else if (ph == 3) {

                        if (s < 4) {
                            p_ang = base_a + ((float)s - 1.5f) * 0.18f;
                            pj->vfx_shape = sh_b; pj->color = col_b;
                            spd = 235.0f; pscale = 1.12f; prad = 11.5f; extra_dmg = 3;
                        } else {
                            p_ang = base_a + 0.785398f + (float)(s - 4) * 1.570796f;
                            pj->vfx_shape = sh_a; pj->color = col_a;
                            spd = 165.0f; pscale = 1.08f; prad = 11.0f; extra_dmg = 2;
                        }
                    } else {

                        p_ang = base_a + (float)s * (6.2831853f / 12.0f);
                        pj->vfx_shape = (s & 1) ? sh_b : sh_a;
                        pj->color = (s & 1) ? col_b : col_a;
                        spd = (s & 1) ? 215.0f : 138.0f;
                        pscale = (s & 1) ? 1.16f : 1.06f;
                        prad = 12.0f; extra_dmg = 4;
                    }


                    float boss_sz = en->radius / 38.0f;
                    if (boss_sz < 1.15f) boss_sz = 1.15f;
                    if (boss_sz > 1.85f) boss_sz = 1.85f;
                    pj->x = en->x + cosf(p_ang) * (en->radius * 0.40f);
                    pj->y = en->y + sinf(p_ang) * (en->radius * 0.40f);
                    pj->vx = cosf(p_ang) * spd;
                    pj->vy = sinf(p_ang) * spd;
                    pj->angle = p_ang;
                    int rush_bonus = (g_game.run_mode == MODE_BOSSRUSH) ? (g_game.bosses_defeated_in_run * 2) : 0;
                    int b_dmg = 12 + extra_dmg * 2 + (int)(g_game.run_timer / 60.0f) * 2 + (player->level - 1) / 3 + rush_bonus;
                    if (b_dmg > 34) b_dmg = 34;
                    pj->damage = b_dmg;
                    pj->radius = prad * boss_sz;
                    pj->life = 3.2f;
                    pj->max_life = 3.2f;
                    pj->draw_scale = pscale * boss_sz;
                    pj->alpha = 240;
                    pj->hits_left = 1;
                    if (pj->vfx_shape) {
                        if (pj->spin_speed == 0.0f) pj->spin_speed = moves_circular_spin_speed(pj->vfx_shape);
                        strncpy(pj->shape_name, pj->vfx_shape, sizeof(pj->shape_name) - 1);
                        pj->shape_name[sizeof(pj->shape_name) - 1] = '\0';
                        pj->vfx_shape = pj->shape_name;
                    }
                }
            } else if (dist > 70.0f && dist < 320.0f && active_mob_projs < 18) {

                int pslot = -1;
                for (int p = 0; p < MAX_PROJECTILES; p++) {
                    if (!g_game.projectiles[p].active) { pslot = p; break; }
                }
                if (pslot >= 0) {
                    active_mob_projs++;
                    Projectile *pj = &g_game.projectiles[pslot];
                    memset(pj, 0, sizeof(Projectile));
                    pj->active = true;
                    pj->is_enemy = true;
                    pj->homing = 0;
                    pj->x = en->x;
                    pj->y = en->y;
                    float dirx = dx / dist;
                    float diry = dy / dist;
                    float spd = 135.0f;
                    pj->vx = dirx * spd;
                    pj->vy = diry * spd;
                    pj->angle = atan2f(diry, dirx);
                    int base_d = 3 + (int)(g_game.run_timer / 150.0f) + (player->level - 1) / 5;
                    if (base_d > 12) base_d = 12;
                    pj->damage = base_d;
                    pj->radius = 8.5f;
                    pj->life = 2.6f;
                    pj->max_life = 2.6f;
                    pj->draw_scale = 0.78f;
                    pj->alpha = 230;
                    pj->hits_left = 1;


                    switch (en->elem_type) {
                        case REGION_DESERT:
                            pj->vfx_shape = "ember";
                            pj->color = RGBA8_C(255, 120, 30, 255);
                            break;
                        case REGION_FOREST:
                            pj->vfx_shape = "shadow_ball";
                            pj->color = RGBA8_C(180, 80, 255, 255);
                            break;
                        case REGION_LAKESIDE:
                            pj->vfx_shape = "water_gun";
                            pj->color = RGBA8_C(65, 165, 255, 255);
                            break;
                        case REGION_MEADOW:
                        default:
                            pj->vfx_shape = "leaf";
                            pj->color = RGBA8_C(100, 220, 60, 255);
                            break;
                    }
                    if (pj->vfx_shape) {
                        if (pj->spin_speed == 0.0f) pj->spin_speed = moves_circular_spin_speed(pj->vfx_shape);
                        strncpy(pj->shape_name, pj->vfx_shape, sizeof(pj->shape_name) - 1);
                        pj->shape_name[sizeof(pj->shape_name) - 1] = '\0';
                        pj->vfx_shape = pj->shape_name;
                    }
                }
            }
        }
    }
}

static void preload_biome_tiers(const WildPoke * const tiers[5], int max_tiers) {
    if (max_tiers > 5) max_tiers = 5;
    for (int t = 0; t < max_tiers; t++) {
        for (int i = 0; i < 6; i++) {
            render_preload_pokemon(tiers[t][i].dex);
        }
    }
}

void enemies_preload_stage_start(int stage_id, int mode, int first_boss_dex) {
    static const char * const BOSS_VFX[] = {
        "confusion", "psychic", "dragon_breath", "dragon_energy",
        "ember", "flame", "water_gun", "whirlpool", "rock_throw", "stone_edge",
        "shard", "snow", "thunderbolt", "shock", "shadow_ball", "sludge",
        "boss_steel", "steelshard",
        "hyper_beam_start", "hyper_beam_mid", "hyper_beam_end",
        "psybeam_start", "psybeam_mid", "psybeam_end",
        "hydro_pump_start", "hydro_pump_mid", "hydro_pump_end",
        "flash_cannon_start", "flash_cannon_mid", "flash_cannon_end"
    };
    if (first_boss_dex > 0) render_preload_pokemon(first_boss_dex);
    render_preload_pokemon(493);

    render_preload_pokemon(306);
    render_preload_pokemon(76);
    render_preload_pokemon(289);
    render_preload_pokemon(365);
    for (size_t v = 0; v < sizeof(BOSS_VFX) / sizeof(BOSS_VFX[0]); v++) {
        render_preload_vfx(BOSS_VFX[v]);
    }
    if (mode == MODE_BOSSRUSH) return;

    static const WildPoke * const MEADOW_ALL[5] = { MEADOW_TIER1, MEADOW_TIER2, MEADOW_TIER3, MEADOW_TIER4, MEADOW_TIER5 };
    static const WildPoke * const DESERT_ALL[5] = { DESERT_TIER1, DESERT_TIER2, DESERT_TIER3, DESERT_TIER4, DESERT_TIER5 };
    static const WildPoke * const FOREST_ALL[5] = { FOREST_TIER1, FOREST_TIER2, FOREST_TIER3, FOREST_TIER4, FOREST_TIER5 };
    static const WildPoke * const LAKE_ALL[5]   = { LAKE_TIER1,   LAKE_TIER2,   LAKE_TIER3,   LAKE_TIER4,   LAKE_TIER5   };
    static const WildPoke * const CAVE_ALL[5]   = { CAVE_TIER1,   CAVE_TIER2,   CAVE_TIER3,   CAVE_TIER4,   CAVE_TIER5   };
    static const WildPoke * const MAGMA_ALL[5]  = { MAGMA_TIER1,  MAGMA_TIER2,  MAGMA_TIER3,  MAGMA_TIER4,  MAGMA_TIER5  };
    static const WildPoke * const ICE_ALL[5]    = { ICE_TIER1,    ICE_TIER2,    ICE_TIER3,    ICE_TIER4,    ICE_TIER5    };
    static const WildPoke * const TOWN_ALL[5]   = { TOWN_TIER1,   TOWN_TIER2,   TOWN_TIER3,   TOWN_TIER4,   TOWN_TIER5   };

    if (stage_id == STAGE_CAVE_GRANITE) {
        preload_biome_tiers(CAVE_ALL, 5);
    } else if (stage_id == STAGE_CAVE_CHIMNEY) {
        preload_biome_tiers(MAGMA_ALL, 5);
    } else if (stage_id == STAGE_CAVE_ICE) {
        preload_biome_tiers(ICE_ALL, 5);
    } else if (stage_id == STAGE_TOWN_HOENN) {
        preload_biome_tiers(TOWN_ALL, 5);
        preload_biome_tiers(LAKE_ALL, 2);
    } else if (stage_id == STAGE_NATIONAL_PARK) {
        preload_biome_tiers(DESERT_ALL, 5);
    } else if (stage_id == STAGE_ILEX_FOREST) {
        preload_biome_tiers(FOREST_ALL, 5);
    } else if (stage_id == STAGE_ROUTE_29) {
        preload_biome_tiers(MEADOW_ALL, 5);
        preload_biome_tiers(LAKE_ALL, 2);
        preload_biome_tiers(DESERT_ALL, 2);
        preload_biome_tiers(FOREST_ALL, 2);
    } else {
        preload_biome_tiers(MEADOW_ALL, 5);
        preload_biome_tiers(LAKE_ALL, 2);
    }
}
