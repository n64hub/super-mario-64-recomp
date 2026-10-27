// Shindou's rumble (../rumble.c) in the decomp's src/game/mario_actions_stationary.c: each function in it that rumbles
// in the Shindou edition, as the US game has it, with its #if ENABLE_RUMBLE lines compiled in (../rumble.h). Above them
// are the file's own declarations those functions use.

#include "../patches.h"
#include "sm64.h"
#include "game/area.h"
#include "audio/external.h"
#include "behavior_data.h"
#include "game/camera.h"
#include "engine/math_util.h"
#include "game/interaction.h"
#include "game/level_update.h"
#include "game/mario.h"
#include "game/mario_actions_stationary.h"
#include "game/mario_step.h"
#include "game/memory.h"
#include "game/save_file.h"
#include "game/sound_init.h"
#include "surface_terrains.h"
#include "game/rumble_init.h"

s32 act_shockwave_bounce(struct MarioState *m);

#include "../rumble.h"

RECOMP_PATCH s32 act_shockwave_bounce(struct MarioState *m) {
    s16 sp1E;
    f32 sp18;

    if (m->marioObj->oInteractStatus & INT_STATUS_MARIO_SHOCKWAVE) {
#if ENABLE_RUMBLE
        queue_rumble_data(70, 40);
#endif
        return hurt_and_set_mario_action(m, ACT_SHOCKED, 0, 4);
    }

    if (m->actionTimer == 0) {
#if ENABLE_RUMBLE
        queue_rumble_data(70, 40);
#endif
        if (m->marioObj->oInteractStatus & INT_STATUS_MARIO_KNOCKBACK_DMG) {
            return hurt_and_set_mario_action(m, ACT_BACKWARD_GROUND_KB, 0, 0xc);
        }
    }

    if (++m->actionTimer == 48) {
        return set_mario_action(m, ACT_IDLE, 0);
    }

    sp1E = (m->actionTimer % 16) << 12;
    sp18 = (f32)(((f32)(6 - m->actionTimer / 8) * 8.0f) + 4.0f);
    mario_set_forward_vel(m, 0);
    vec3f_set(m->vel, 0.0f, 0.0f, 0.0f);
    if (sins(sp1E) >= 0.0f) {
        m->pos[1] = sins(sp1E) * sp18 + m->floorHeight;
    } else {
        m->pos[1] = m->floorHeight - sins(sp1E) * sp18;
    }

    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
    vec3s_set(m->marioObj->header.gfx.angle, 0, m->faceAngle[1], 0);
    set_mario_animation(m, MARIO_ANIM_A_POSE);
    return FALSE;
}
