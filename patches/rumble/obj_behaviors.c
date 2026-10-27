// Shindou's rumble (../rumble.c) in the decomp's
// src/game/behaviors/{controllable_platform,mushroom_1up,treasure_chest}.inc.c, which src/game/obj_behaviors.c
// includes: each function in it that rumbles in the Shindou edition, as the US game has it, with its #if ENABLE_RUMBLE
// lines compiled in (../rumble.h). Above them are the file's own declarations those functions use.

#include "../patches.h"
#include "sm64.h"
#include "game/area.h"
#include "audio/external.h"
#include "game/behavior_actions.h"
#include "behavior_data.h"
#include "game/camera.h"
#include "course_table.h"
#include "dialog_ids.h"
#include "engine/behavior_script.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "game/envfx_bubbles.h"
#include "game/game_init.h"
#include "game/ingame_menu.h"
#include "game/interaction.h"
#include "level_misc_macros.h"
#include "level_table.h"
#include "game/level_update.h"
#include "levels/bob/header.h"
#include "levels/ttm/header.h"
#include "game/mario.h"
#include "game/mario_actions_cutscene.h"
#include "game/mario_misc.h"
#include "game/memory.h"
#include "game/obj_behaviors.h"
#include "game/object_helpers.h"
#include "game/object_list_processor.h"
#include "game/rendering_graph_node.h"
#include "game/save_file.h"
#include "game/spawn_object.h"
#include "game/spawn_sound.h"
#include "game/rumble_init.h"

#define o gCurrentObject
void bhv_1up_interact(void);
extern s8 D_80331694;
void bhv_treasure_chest_ship_loop(void);

#include "../rumble.h"

RECOMP_PATCH void controllable_platform_hit_wall(s32 sp1BWord) {
    // Narrowed here: IDO narrows a char or short argument in the function that takes it (CLAUDE.md, Narrow arguments).
    s8 sp1B = (s8) sp1BWord;
    o->oControllablePlatformUnkF8 = sp1B;
    o->oTimer = 0;
    D_80331694 = 5;

    cur_obj_play_sound_2(SOUND_GENERAL_QUIET_POUND1);
#if ENABLE_RUMBLE
    queue_rumble_data(50, 80);
#endif
}

RECOMP_PATCH void bhv_1up_interact(void) {
    UNUSED u8 filler[4];

    if (obj_check_if_collided_with_object(o, gMarioObject) == TRUE) {
        play_sound(SOUND_GENERAL_COLLECT_1UP, gGlobalSoundSource);
        gMarioState->numLives++;
        o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
#if ENABLE_RUMBLE
        queue_rumble_data(5, 80);
#endif
    }
}

RECOMP_PATCH void bhv_treasure_chest_ship_loop(void) {
    switch (o->oAction) {
        case 0:
            if (o->oTreasureChestUnkF4 == 5) {
                play_puzzle_jingle();
                fade_volume_scale(0, 127, 1000);
                o->oAction = 1;
            }
            break;

        case 1:
            if (gEnvironmentRegions != NULL) {
                gEnvironmentRegions[6] -= 5;
                play_sound(SOUND_ENV_WATER_DRAIN, gGlobalSoundSource);
                set_environmental_camera_shake(SHAKE_ENV_JRB_SHIP_DRAIN);
                if (gEnvironmentRegions[6] < -335) {
                    gEnvironmentRegions[6] = -335;
                    o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
                }
#if ENABLE_RUMBLE
                reset_rumble_timers_2(2);
#endif
            }
            break;
    }
}
