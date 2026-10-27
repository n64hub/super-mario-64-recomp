// Shindou's rumble (../rumble.c) in the decomp's src/game/mario_actions_object.c: each function in it that rumbles in
// the Shindou edition, as the US game has it, with its #if ENABLE_RUMBLE lines compiled in (../rumble.h). Above them
// are the file's own declarations those functions use.

#include "../patches.h"
#include "sm64.h"
#include "game/mario_actions_object.h"
#include "types.h"
#include "game/mario_step.h"
#include "game/mario.h"
#include "audio/external.h"
#include "game/interaction.h"
#include "engine/math_util.h"
#include "game/rumble_init.h"

void animated_stationary_ground_step(struct MarioState *m, s32 animation, u32 endAction);
s32 act_throwing(struct MarioState *m);
s32 act_heavy_throw(struct MarioState *m);
s32 act_picking_up_bowser(struct MarioState *m);
s32 act_holding_bowser(struct MarioState *m);
s32 act_releasing_bowser(struct MarioState *m);

#include "../rumble.h"

RECOMP_PATCH s32 act_throwing(struct MarioState *m) {
    if (m->heldObj && (m->heldObj->oInteractionSubtype & INT_SUBTYPE_HOLDABLE_NPC)) {
        return set_mario_action(m, ACT_PLACING_DOWN, 0);
    }

    if (m->input & INPUT_STOMPED) {
        return drop_and_set_mario_action(m, ACT_SHOCKWAVE_BOUNCE, 0);
    }

    if (m->input & INPUT_OFF_FLOOR) {
        return drop_and_set_mario_action(m, ACT_FREEFALL, 0);
    }

    if (++m->actionTimer == 7) {
        mario_throw_held_object(m);
        play_sound_if_no_flag(m, SOUND_MARIO_WAH2, MARIO_MARIO_SOUND_PLAYED);
        play_sound_if_no_flag(m, SOUND_ACTION_THROW, MARIO_ACTION_SOUND_PLAYED);
#if ENABLE_RUMBLE
        queue_rumble_data(3, 50);
#endif
    }

    animated_stationary_ground_step(m, MARIO_ANIM_GROUND_THROW, ACT_IDLE);
    return FALSE;
}

RECOMP_PATCH s32 act_heavy_throw(struct MarioState *m) {
    if (m->input & INPUT_STOMPED) {
        return drop_and_set_mario_action(m, ACT_SHOCKWAVE_BOUNCE, 0);
    }

    if (m->input & INPUT_OFF_FLOOR) {
        return drop_and_set_mario_action(m, ACT_FREEFALL, 0);
    }

    if (++m->actionTimer == 13) {
        mario_drop_held_object(m);
        play_sound_if_no_flag(m, SOUND_MARIO_WAH2, MARIO_MARIO_SOUND_PLAYED);
        play_sound_if_no_flag(m, SOUND_ACTION_THROW, MARIO_ACTION_SOUND_PLAYED);
#if ENABLE_RUMBLE
        queue_rumble_data(3, 50);
#endif
    }

    animated_stationary_ground_step(m, MARIO_ANIM_HEAVY_THROW, ACT_IDLE);
    return FALSE;
}

RECOMP_PATCH s32 act_picking_up_bowser(struct MarioState *m) {
    if (m->actionState == 0) {
        m->actionState = 1;
        m->angleVel[1] = 0;
        m->marioBodyState->grabPos = GRAB_POS_BOWSER;
        mario_grab_used_object(m);
#if ENABLE_RUMBLE
        queue_rumble_data(5, 80);
#endif
        play_sound(SOUND_MARIO_HRMM, m->marioObj->header.gfx.cameraToObject);
    }

    set_mario_animation(m, MARIO_ANIM_GRAB_BOWSER);
    if (is_anim_at_end(m)) {
        set_mario_action(m, ACT_HOLDING_BOWSER, 0);
    }

    stationary_ground_step(m);
    return FALSE;
}

RECOMP_PATCH s32 act_holding_bowser(struct MarioState *m) {
    s16 spin;

    if (m->input & INPUT_B_PRESSED) {
#ifndef VERSION_JP
        if (m->angleVel[1] <= -0xE00 || m->angleVel[1] >= 0xE00) {
            play_sound(SOUND_MARIO_SO_LONGA_BOWSER, m->marioObj->header.gfx.cameraToObject);
        } else {
            play_sound(SOUND_MARIO_HERE_WE_GO, m->marioObj->header.gfx.cameraToObject);
        }
#else
        play_sound(SOUND_MARIO_HERE_WE_GO, m->marioObj->header.gfx.cameraToObject);
#endif
        return set_mario_action(m, ACT_RELEASING_BOWSER, 0);
    }

    if (m->angleVel[1] == 0) {
        if (m->actionTimer++ > 120) {
            return set_mario_action(m, ACT_RELEASING_BOWSER, 1);
        }

        set_mario_animation(m, MARIO_ANIM_HOLDING_BOWSER);
    } else {
        m->actionTimer = 0;
        set_mario_animation(m, MARIO_ANIM_SWINGING_BOWSER);
    }

    if (m->intendedMag > 20.0f) {
        if (m->actionArg == 0) {
            m->actionArg = 1;
            m->twirlYaw = m->intendedYaw;
        } else {
            // spin = acceleration
            spin = (s16)(m->intendedYaw - m->twirlYaw) / 0x80;

            if (spin < -0x80) {
                spin = -0x80;
            }
            if (spin > 0x80) {
                spin = 0x80;
            }

            m->twirlYaw = m->intendedYaw;
            m->angleVel[1] += spin;

            if (m->angleVel[1] > 0x1000) {
                m->angleVel[1] = 0x1000;
            }
            if (m->angleVel[1] < -0x1000) {
                m->angleVel[1] = -0x1000;
            }
        }
    } else {
        m->actionArg = 0;
        m->angleVel[1] = approach_s32(m->angleVel[1], 0, 64, 64);
    }

    // spin = starting yaw
    spin = m->faceAngle[1];
    m->faceAngle[1] += m->angleVel[1];

    // play sound on overflow
    if (m->angleVel[1] <= -0x100 && spin < m->faceAngle[1]) {
#if ENABLE_RUMBLE
        queue_rumble_data(4, 20);
#endif
        play_sound(SOUND_OBJ_BOWSER_SPINNING, m->marioObj->header.gfx.cameraToObject);
    }
    if (m->angleVel[1] >= 0x100 && spin > m->faceAngle[1]) {
#if ENABLE_RUMBLE
        queue_rumble_data(4, 20);
#endif
        play_sound(SOUND_OBJ_BOWSER_SPINNING, m->marioObj->header.gfx.cameraToObject);
    }

    stationary_ground_step(m);
    if (m->angleVel[1] >= 0) {
        m->marioObj->header.gfx.angle[0] = -m->angleVel[1];
    } else {
        m->marioObj->header.gfx.angle[0] = m->angleVel[1];
    }

    return FALSE;
}

RECOMP_PATCH s32 act_releasing_bowser(struct MarioState *m) {
    if (++m->actionTimer == 1) {
        if (m->actionArg == 0) {
#if ENABLE_RUMBLE
            queue_rumble_data(5, 50);
#endif
            mario_throw_held_object(m);
        } else {
#if ENABLE_RUMBLE
            queue_rumble_data(4, 50);
#endif
            mario_drop_held_object(m);
        }
    }

    m->angleVel[1] = 0;
    animated_stationary_ground_step(m, MARIO_ANIM_RELEASE_BOWSER, ACT_IDLE);
    return FALSE;
}
