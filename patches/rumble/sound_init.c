// Shindou's rumble (../rumble.c) in the decomp's src/game/sound_init.c: each function in it that rumbles in the Shindou
// edition, as the US game has it, with its #if ENABLE_RUMBLE lines compiled in (../rumble.h). Above them are the file's
// own declarations those functions use.

#include "../patches.h"
#include "game/area.h"
#include "audio/external.h"
#include "engine/graph_node.h"
#include "engine/math_util.h"
#include "level_table.h"
#include "game/level_update.h"
#include "game/main.h"
#include "game/paintings.h"
#include "game/print.h"
#include "game/profiler.h"
#include "game/save_file.h"
#include "seq_ids.h"
#include "sm64.h"
// The header declares it with its s16, which the patch below takes as an s32.
#define play_menu_sounds play_menu_sounds_s16
#include "game/sound_init.h"
#undef play_menu_sounds
#include "game/rumble_init.h"

void play_menu_sounds_extra(s32 a, void *b);

#include "../rumble.h"

RECOMP_PATCH void play_menu_sounds(s32 soundMenuFlagsWord) {
    // Narrowed here: IDO narrows a char or short argument in the function that takes it (CLAUDE.md, Narrow arguments).
    s16 soundMenuFlags = (s16) soundMenuFlagsWord;
    if (soundMenuFlags & SOUND_MENU_FLAG_HANDAPPEAR) {
        play_sound(SOUND_MENU_HAND_APPEAR, gGlobalSoundSource);
    } else if (soundMenuFlags & SOUND_MENU_FLAG_HANDISAPPEAR) {
        play_sound(SOUND_MENU_HAND_DISAPPEAR, gGlobalSoundSource);
    } else if (soundMenuFlags & SOUND_MENU_FLAG_UNKNOWN1) {
        play_sound(SOUND_MENU_UNK0C, gGlobalSoundSource);
    } else if (soundMenuFlags & SOUND_MENU_FLAG_PINCHMARIOFACE) {
        play_sound(SOUND_MENU_PINCH_MARIO_FACE, gGlobalSoundSource);
    } else if (soundMenuFlags & SOUND_MENU_FLAG_PINCHMARIOFACE2) {
        play_sound(SOUND_MENU_PINCH_MARIO_FACE, gGlobalSoundSource);
    } else if (soundMenuFlags & SOUND_MENU_FLAG_LETGOMARIOFACE) {
        play_sound(SOUND_MENU_LET_GO_MARIO_FACE, gGlobalSoundSource);
    } else if (soundMenuFlags & SOUND_MENU_FLAG_CAMERAZOOMIN) {
        play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gGlobalSoundSource);
    } else if (soundMenuFlags & SOUND_MENU_FLAG_CAMERAZOOMOUT) {
        play_sound(SOUND_MENU_CAMERA_ZOOM_OUT, gGlobalSoundSource);
    }

    if (soundMenuFlags & 0x100) {
        play_menu_sounds_extra(20, NULL);
    }
#if ENABLE_RUMBLE
    if (soundMenuFlags & SOUND_MENU_FLAG_LETGOMARIOFACE) {
        queue_rumble_data(10, 60);
    }
#endif
}
