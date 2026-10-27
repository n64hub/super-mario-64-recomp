// Shindou's rumble (../rumble.c) in the decomp's src/game/spawn_sound.c: each function in it that rumbles in the
// Shindou edition, as the US game has it, with its #if ENABLE_RUMBLE lines compiled in (../rumble.h). Above them are
// the file's own declarations those functions use.

#include "../patches.h"
#include "audio/external.h"
#include "behavior_data.h"
#include "engine/behavior_script.h"
#include "engine/graph_node.h"
#include "game/object_helpers.h"
#include "game/object_list_processor.h"
#include "sm64.h"
#include "game/spawn_sound.h"
#include "game/rumble_init.h"

void cur_obj_play_sound_2(s32 soundMagic);

#include "../rumble.h"

RECOMP_PATCH void cur_obj_play_sound_2(s32 soundMagic) {
    if (gCurrentObject->header.gfx.node.flags & GRAPH_RENDER_ACTIVE) {
        play_sound(soundMagic, gCurrentObject->header.gfx.cameraToObject);
#if ENABLE_RUMBLE
        if (soundMagic == SOUND_OBJ_BOWSER_WALK) {
            queue_rumble_data(3, 60);
        }
        if (soundMagic == SOUND_OBJ_POUNDING_LOUD) {
            queue_rumble_data(3, 60);
        }
        if (soundMagic == SOUND_OBJ_WHOMP) {
            queue_rumble_data(5, 80);
        }
#endif
    }
}
