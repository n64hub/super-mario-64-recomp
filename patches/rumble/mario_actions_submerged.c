// Shindou's rumble (../rumble.c) in the decomp's src/game/mario_actions_submerged.c: each function in it that rumbles
// in the Shindou edition, as the US game has it, with its #if ENABLE_RUMBLE lines compiled in (../rumble.h). Above them
// are the file's own declarations those functions use.

#include "../patches.h"
#include "sm64.h"
#include "game/level_update.h"
#include "game/memory.h"
#include "engine/math_util.h"
#include "game/area.h"
#include "game/save_file.h"
#include "game/sound_init.h"
#include "engine/surface_collision.h"
#include "game/interaction.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "game/camera.h"
#include "audio/external.h"
#include "behavior_data.h"
#include "level_table.h"
#include "game/rumble_init.h"

#define MIN_SWIM_STRENGTH 160
#define MIN_SWIM_SPEED 16.0f
extern s16 sSwimStrength;
extern s16 sBobIncrement;
s32 swimming_near_surface(struct MarioState *m);
u32 perform_water_step(struct MarioState *m);
BAD_RETURN(u32) update_water_pitch(struct MarioState *m);
void stationary_slow_down(struct MarioState *m);
void update_swimming_speed(struct MarioState *m, f32 decelThreshold);
void update_swimming_yaw(struct MarioState *m);
void update_swimming_pitch(struct MarioState *m);
void reset_bob_variables(struct MarioState *m);
void common_swimming_step(struct MarioState *m, s16 swimStrength);
s32 check_water_jump(struct MarioState *m);
s32 act_breaststroke(struct MarioState *m);
s32 act_water_throw(struct MarioState *m);
s32 act_water_plunge(struct MarioState *m);
s32 act_caught_in_whirlpool(struct MarioState *m);

#include "../rumble.h"

RECOMP_PATCH s32 act_breaststroke(struct MarioState *m) {
    if (m->actionArg == 0) {
        sSwimStrength = MIN_SWIM_STRENGTH;
    }

    if (m->flags & MARIO_METAL_CAP) {
        return set_mario_action(m, ACT_METAL_WATER_FALLING, 1);
    }

    if (m->input & INPUT_B_PRESSED) {
        return set_mario_action(m, ACT_WATER_PUNCH, 0);
    }

    if (++m->actionTimer == 14) {
        return set_mario_action(m, ACT_FLUTTER_KICK, 0);
    }

    if (check_water_jump(m)) {
        return TRUE;
    }

    if (m->actionTimer < 6) {
        m->forwardVel += 0.5f;
    }

    if (m->actionTimer >= 9) {
        m->forwardVel += 1.5f;
    }

    if (m->actionTimer >= 2) {
        if (m->actionTimer < 6 && (m->input & INPUT_A_PRESSED)) {
            m->actionState = 1;
        }

        if (m->actionTimer == 9 && m->actionState == 1) {
            set_anim_to_frame(m, 0);
            m->actionState = 0;
            m->actionTimer = 1;
            sSwimStrength = MIN_SWIM_STRENGTH;
        }
    }

    if (m->actionTimer == 1) {
        play_sound(sSwimStrength == MIN_SWIM_STRENGTH ? SOUND_ACTION_SWIM : SOUND_ACTION_SWIM_FAST,
                   m->marioObj->header.gfx.cameraToObject);
        reset_bob_variables(m);
    }

#if ENABLE_RUMBLE
    if (m->actionTimer < 6) {
        func_sh_8024CA04();
    }
#endif

    set_mario_animation(m, MARIO_ANIM_SWIM_PART1);
    common_swimming_step(m, sSwimStrength);

    return FALSE;
}

RECOMP_PATCH s32 act_water_throw(struct MarioState *m) {
    update_swimming_yaw(m);
    update_swimming_pitch(m);
    update_swimming_speed(m, MIN_SWIM_SPEED);
    perform_water_step(m);
    update_water_pitch(m);

    set_mario_animation(m, MARIO_ANIM_WATER_THROW_OBJ);
    play_sound_if_no_flag(m, SOUND_ACTION_SWIM, MARIO_ACTION_SOUND_PLAYED);

    m->marioBodyState->headAngle[0] = approach_s32(m->marioBodyState->headAngle[0], 0, 0x200, 0x200);

    if (m->actionTimer++ == 5) {
        mario_throw_held_object(m);
#if ENABLE_RUMBLE
        queue_rumble_data(3, 50);
#endif
    }

    if (is_anim_at_end(m)) {
        set_mario_action(m, ACT_WATER_IDLE, 0);
    }

    return FALSE;
}

RECOMP_PATCH s32 act_water_plunge(struct MarioState *m) {
    u32 stepResult;
    s32 stateFlags = m->heldObj != NULL;

    f32 endVSpeed;
    if (swimming_near_surface(m)) {
        endVSpeed = 0.0f;
    } else {
        endVSpeed = -5.0f;
    }

    if (m->flags & MARIO_METAL_CAP) {
        stateFlags |= 4;
    } else if ((m->prevAction & ACT_FLAG_DIVING) || (m->input & INPUT_A_DOWN)) {
        stateFlags |= 2;
    }

    m->actionTimer++;

    stationary_slow_down(m);

    stepResult = perform_water_step(m);

    if (m->actionState == 0) {
        play_sound(SOUND_ACTION_UNKNOWN430, m->marioObj->header.gfx.cameraToObject);
        if (m->peakHeight - m->pos[1] > 1150.0f) {
            play_sound(SOUND_MARIO_HAHA_2, m->marioObj->header.gfx.cameraToObject);
        }

        m->particleFlags |= PARTICLE_WATER_SPLASH;
        m->actionState = 1;
#if ENABLE_RUMBLE
        if (m->prevAction & ACT_FLAG_AIR) {
            queue_rumble_data(5, 80);
        }
#endif
    }

    if (stepResult == WATER_STEP_HIT_FLOOR || m->vel[1] >= endVSpeed || m->actionTimer > 20) {
        switch (stateFlags) {
            case 0:
                set_mario_action(m, ACT_WATER_ACTION_END, 0);
                break;
            case 1:
                set_mario_action(m, ACT_HOLD_WATER_ACTION_END, 0);
                break;
            case 2:
                set_mario_action(m, ACT_FLUTTER_KICK, 0);
                break;
            case 3:
                set_mario_action(m, ACT_HOLD_FLUTTER_KICK, 0);
                break;
            case 4:
                set_mario_action(m, ACT_METAL_WATER_FALLING, 0);
                break;
            case 5:
                set_mario_action(m, ACT_HOLD_METAL_WATER_FALLING, 0);
                break;
        }
        sBobIncrement = 0;
    }

    switch (stateFlags) {
        case 0:
            set_mario_animation(m, MARIO_ANIM_WATER_ACTION_END);
            break;
        case 1:
            set_mario_animation(m, MARIO_ANIM_WATER_ACTION_END_WITH_OBJ);
            break;
        case 2:
            set_mario_animation(m, MARIO_ANIM_FLUTTERKICK);
            break;
        case 3:
            set_mario_animation(m, MARIO_ANIM_FLUTTERKICK_WITH_OBJ);
            break;
        case 4:
            set_mario_animation(m, MARIO_ANIM_GENERAL_FALL);
            break;
        case 5:
            set_mario_animation(m, MARIO_ANIM_FALL_WITH_LIGHT_OBJ);
            break;
    }

    m->particleFlags |= PARTICLE_PLUNGE_BUBBLE;
    return FALSE;
}

RECOMP_PATCH s32 act_caught_in_whirlpool(struct MarioState *m) {
    f32 sinAngleChange;
    f32 cosAngleChange;
    f32 newDistance;
    s16 angleChange;

    struct Object *marioObj = m->marioObj;
    struct Object *whirlpool = m->usedObj;

    f32 dx = m->pos[0] - whirlpool->oPosX;
    f32 dz = m->pos[2] - whirlpool->oPosZ;
    f32 distance = sqrtf(dx * dx + dz * dz);

    if ((marioObj->oMarioWhirlpoolPosY += m->vel[1]) < 0.0f) {
        marioObj->oMarioWhirlpoolPosY = 0.0f;
        if (distance < 16.1f && m->actionTimer++ == 16) {
            level_trigger_warp(m, WARP_OP_DEATH);
        }
    }

    if (distance <= 28.0f) {
        newDistance = 16.0f;
        angleChange = 0x1800;
    } else if (distance < 256.0f) {
        newDistance = distance - (12.0f - distance / 32.0f);
        angleChange = (s16)(0x1C00 - distance * 20.0f);
    } else {
        newDistance = distance - 4.0f;
        angleChange = 0x800;
    }

    m->vel[1] = -640.0f / (newDistance + 16.0f);

    sinAngleChange = sins(angleChange);
    cosAngleChange = coss(angleChange);

    if (distance < 1.0f) {
        dx = newDistance * sins(m->faceAngle[1]);
        dz = newDistance * coss(m->faceAngle[1]);
    } else {
        dx *= newDistance / distance;
        dz *= newDistance / distance;
    }

    m->pos[0] = whirlpool->oPosX + dx * cosAngleChange + dz * sinAngleChange;
    m->pos[2] = whirlpool->oPosZ - dx * sinAngleChange + dz * cosAngleChange;
    m->pos[1] = whirlpool->oPosY + marioObj->oMarioWhirlpoolPosY;

    m->faceAngle[1] = atan2s(dz, dx) + 0x8000;

    set_mario_animation(m, MARIO_ANIM_GENERAL_FALL);
    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
    vec3s_set(m->marioObj->header.gfx.angle, 0, m->faceAngle[1], 0);
#if ENABLE_RUMBLE
    reset_rumble_timers();
#endif

    return FALSE;
}
