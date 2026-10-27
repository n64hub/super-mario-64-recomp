// Shindou's rumble (../rumble.c) in the decomp's src/menu/title_screen.c: each function in it that rumbles in the
// Shindou edition, as the US game has it, with its #if ENABLE_RUMBLE lines compiled in (../rumble.h). Above them are
// the file's own declarations those functions use.

#include "../patches.h"
#include "audio/external.h"
#include "engine/math_util.h"
#include "game/area.h"
#include "game/game_init.h"
#include "game/level_update.h"
#include "game/main.h"
#include "game/memory.h"
#include "game/print.h"
#include "game/save_file.h"
#include "game/sound_init.h"
#include "game/rumble_init.h"
#include "level_table.h"
#include "seq_ids.h"
#include "sm64.h"
#include "menu/title_screen.h"

extern s16 sPlayMarioGreeting;
extern s16 sPlayMarioGameOver;
s32 run_level_id_or_demo(s32 level);
s32 intro_regular(void);
s32 intro_game_over(void);

#include "../rumble.h"

RECOMP_PATCH s32 intro_regular(void) {
    s32 level = LEVEL_NONE;

#ifndef VERSION_JP
    // When the game stars, gGlobalTimer is less than 129 frames,
    // so Mario greets the player. After that, he will always say
    // "press start to play" when it goes back to the title screen
    // (using SAVE AND QUIT)
    if (sPlayMarioGreeting == TRUE) {
        if (gGlobalTimer < 129) {
            play_sound(SOUND_MARIO_HELLO, gGlobalSoundSource);
        } else {
            play_sound(SOUND_MARIO_PRESS_START_TO_PLAY, gGlobalSoundSource);
        }
        sPlayMarioGreeting = FALSE;
    }
#endif
    print_intro_text();

    if (gPlayer1Controller->buttonPressed & START_BUTTON) {
        play_sound(SOUND_MENU_STAR_SOUND, gGlobalSoundSource);
#if ENABLE_RUMBLE
        queue_rumble_data(60, 70);
        func_sh_8024C89C(1);
#endif
        // calls level ID 100 (or 101 adding level select bool value)
        // defined in level_intro_mario_head_regular JUMP_IF commands
        // 100 is File Select - 101 is Level Select
        level = 100 + gDebugLevelSelect;
#ifndef VERSION_JP
        sPlayMarioGreeting = TRUE;
#endif
    }
    return run_level_id_or_demo(level);
}

RECOMP_PATCH s32 intro_game_over(void) {
    s32 level = LEVEL_NONE;

#ifndef VERSION_JP
    if (sPlayMarioGameOver == TRUE) {
        play_sound(SOUND_MARIO_GAME_OVER, gGlobalSoundSource);
        sPlayMarioGameOver = FALSE;
    }
#endif

    print_intro_text();

    if (gPlayer1Controller->buttonPressed & START_BUTTON) {
        play_sound(SOUND_MENU_STAR_SOUND, gGlobalSoundSource);
#if ENABLE_RUMBLE
        queue_rumble_data(60, 70);
        func_sh_8024C89C(1);
#endif
        // same criteria as intro_regular
        level = 100 + gDebugLevelSelect;
#ifndef VERSION_JP
        sPlayMarioGameOver = TRUE;
#endif
    }
    return run_level_id_or_demo(level);
}
