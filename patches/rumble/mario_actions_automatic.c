// Shindou's rumble (../rumble.c) in the decomp's src/game/mario_actions_automatic.c: each function in it that rumbles
// in the Shindou edition, as the US game has it, with its #if ENABLE_RUMBLE lines compiled in (../rumble.h). Above them
// are the file's own declarations those functions use.

#include "../patches.h"
#include "sm64.h"
#include "behavior_data.h"
#include "game/mario_actions_automatic.h"
#include "audio/external.h"
#include "game/area.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "engine/math_util.h"
#include "game/memory.h"
#include "engine/graph_node.h"
#include "game/save_file.h"
#include "engine/surface_collision.h"
#include "game/interaction.h"
#include "game/camera.h"
#include "level_table.h"
#include "game/rumble_init.h"

#define POLE_NONE          0
#define HANG_LEFT_CEIL       2
void add_tree_leaf_particles(struct MarioState *m);
void play_climbing_sounds(struct MarioState *m, s32 b);
s32 set_pole_position(struct MarioState *m, f32 offsetY);
s32 act_holding_pole(struct MarioState *m);
s32 update_hang_moving(struct MarioState *m);
void update_hang_stationary(struct MarioState *m);
s32 act_start_hanging(struct MarioState *m);
s32 act_hang_moving(struct MarioState *m);
s32 act_grabbed(struct MarioState *m);
s32 act_in_cannon(struct MarioState *m);
s32 act_tornado_twirling(struct MarioState *m);

#include "../rumble.h"

RECOMP_PATCH s32 act_holding_pole(struct MarioState *m) {
    struct Object *marioObj = m->marioObj;

#ifdef VERSION_JP
    if (m->input & INPUT_A_PRESSED) {
        add_tree_leaf_particles(m);
        m->faceAngle[1] += 0x8000;
        return set_mario_action(m, ACT_WALL_KICK_AIR, 0);
    }

    if (m->input & INPUT_Z_PRESSED) {
        add_tree_leaf_particles(m);
        m->forwardVel = -2.0f;
        return set_mario_action(m, ACT_SOFT_BONK, 0);
    }
#else
    if ((m->input & INPUT_Z_PRESSED) || m->health < 0x100) {
        add_tree_leaf_particles(m);
        m->forwardVel = -2.0f;
        return set_mario_action(m, ACT_SOFT_BONK, 0);
    }

    if (m->input & INPUT_A_PRESSED) {
        add_tree_leaf_particles(m);
        m->faceAngle[1] += 0x8000;
        return set_mario_action(m, ACT_WALL_KICK_AIR, 0);
    }
#endif

    if (m->controller->stickY > 16.0f) {
        f32 poleTop = m->usedObj->hitboxHeight - 100.0f;
        const BehaviorScript *poleBehavior = virtual_to_segmented(0x13, m->usedObj->behavior);

        if (marioObj->oMarioPolePos < poleTop - 0.4f) {
            return set_mario_action(m, ACT_CLIMBING_POLE, 0);
        }

        if (poleBehavior != bhvGiantPole && m->controller->stickY > 50.0f) {
            return set_mario_action(m, ACT_TOP_OF_POLE_TRANSITION, 0);
        }
    }

    if (m->controller->stickY < -16.0f) {
        marioObj->oMarioPoleYawVel -= m->controller->stickY * 2;
        if (marioObj->oMarioPoleYawVel > 0x1000) {
            marioObj->oMarioPoleYawVel = 0x1000;
        }

        m->faceAngle[1] += marioObj->oMarioPoleYawVel;
        marioObj->oMarioPolePos -= marioObj->oMarioPoleYawVel / 0x100;

        if (m->usedObj->behavior == segmented_to_virtual(bhvTree)) {
            //! The Shifting Sand Land palm tree check is done climbing up in
            // add_tree_leaf_particles, but not here, when climbing down.
            if (m->pos[1] - m->floorHeight > 100.0f) {
                m->particleFlags |= PARTICLE_LEAF;
            }
        }
        play_climbing_sounds(m, 2);
#if ENABLE_RUMBLE
        reset_rumble_timers();
#endif
        set_sound_moving_speed(SOUND_BANK_MOVING, marioObj->oMarioPoleYawVel / 0x100 * 2);
    } else {
        marioObj->oMarioPoleYawVel = 0;
        m->faceAngle[1] -= m->controller->stickX * 16.0f;
    }

    if (set_pole_position(m, 0.0f) == POLE_NONE) {
        set_mario_animation(m, MARIO_ANIM_IDLE_ON_POLE);
    }

    return FALSE;
}

RECOMP_PATCH s32 act_start_hanging(struct MarioState *m) {
#if ENABLE_RUMBLE
    if (m->actionTimer++ == 0) {
        queue_rumble_data(5, 80);
    }
#else
    m->actionTimer++;
#endif

    if ((m->input & INPUT_NONZERO_ANALOG) && m->actionTimer >= 31) {
        return set_mario_action(m, ACT_HANGING, 0);
    }

    if (!(m->input & INPUT_A_DOWN)) {
        return set_mario_action(m, ACT_FREEFALL, 0);
    }

    if (m->input & INPUT_Z_PRESSED) {
        return set_mario_action(m, ACT_GROUND_POUND, 0);
    }

    //! Crash if Mario's referenced ceiling is NULL (same for other hanging actions)
    if (m->ceil->type != SURFACE_HANGABLE) {
        return set_mario_action(m, ACT_FREEFALL, 0);
    }

    set_mario_animation(m, MARIO_ANIM_HANG_ON_CEILING);
    play_sound_if_no_flag(m, SOUND_ACTION_HANGING_STEP, MARIO_ACTION_SOUND_PLAYED);
    update_hang_stationary(m);

    if (is_anim_at_end(m)) {
        set_mario_action(m, ACT_HANGING, 0);
    }

    return FALSE;
}

RECOMP_PATCH s32 act_hang_moving(struct MarioState *m) {
    if (!(m->input & INPUT_A_DOWN)) {
        return set_mario_action(m, ACT_FREEFALL, 0);
    }

    if (m->input & INPUT_Z_PRESSED) {
        return set_mario_action(m, ACT_GROUND_POUND, 0);
    }

    if (m->ceil->type != SURFACE_HANGABLE) {
        return set_mario_action(m, ACT_FREEFALL, 0);
    }

    if (m->actionArg & 1) {
        set_mario_animation(m, MARIO_ANIM_MOVE_ON_WIRE_NET_RIGHT);
    } else {
        set_mario_animation(m, MARIO_ANIM_MOVE_ON_WIRE_NET_LEFT);
    }

    if (m->marioObj->header.gfx.animInfo.animFrame == 12) {
        play_sound(SOUND_ACTION_HANGING_STEP, m->marioObj->header.gfx.cameraToObject);
#if ENABLE_RUMBLE
        queue_rumble_data(1, 30);
#endif
    }

    if (is_anim_past_end(m)) {
        m->actionArg ^= 1;
        if (m->input & INPUT_UNKNOWN_5) {
            return set_mario_action(m, ACT_HANGING, m->actionArg);
        }
    }

    if (update_hang_moving(m) == HANG_LEFT_CEIL) {
        set_mario_action(m, ACT_FREEFALL, 0);
    }

    return FALSE;
}

RECOMP_PATCH s32 act_grabbed(struct MarioState *m) {
    if (m->marioObj->oInteractStatus & INT_STATUS_MARIO_UNK2) {
        s32 thrown = (m->marioObj->oInteractStatus & INT_STATUS_MARIO_UNK6) == 0;

        m->faceAngle[1] = m->usedObj->oMoveAngleYaw;
        vec3f_copy(m->pos, m->marioObj->header.gfx.pos);
#if ENABLE_RUMBLE
        queue_rumble_data(5, 60);
#endif

        return set_mario_action(m, (m->forwardVel >= 0.0f) ? ACT_THROWN_FORWARD : ACT_THROWN_BACKWARD,
                                thrown);
    }

    set_mario_animation(m, MARIO_ANIM_BEING_GRABBED);
    return FALSE;
}

RECOMP_PATCH s32 act_in_cannon(struct MarioState *m) {
    struct Object *marioObj = m->marioObj;
    s16 startFacePitch = m->faceAngle[0];
    s16 startFaceYaw = m->faceAngle[1];

    switch (m->actionState) {
        case 0:
            m->marioObj->header.gfx.node.flags &= ~GRAPH_RENDER_ACTIVE;
            m->usedObj->oInteractStatus = INT_STATUS_INTERACTED;

            m->statusForCamera->cameraEvent = CAM_EVENT_CANNON;
            m->statusForCamera->usedObj = m->usedObj;

            vec3f_set(m->vel, 0.0f, 0.0f, 0.0f);

            m->pos[0] = m->usedObj->oPosX;
            m->pos[1] = m->usedObj->oPosY + 350.0f;
            m->pos[2] = m->usedObj->oPosZ;

            m->forwardVel = 0.0f;

            m->actionState = 1;
            break;

        case 1:
            if (m->usedObj->oAction == 1) {
                m->faceAngle[0] = m->usedObj->oMoveAnglePitch;
                m->faceAngle[1] = m->usedObj->oMoveAngleYaw;

                marioObj->oMarioCannonObjectYaw = m->usedObj->oMoveAngleYaw;
                marioObj->oMarioCannonInputYaw = 0;

                m->actionState = 2;
            }
            break;

        case 2:
            m->faceAngle[0] -= (s16)(m->controller->stickY * 10.0f);
            marioObj->oMarioCannonInputYaw -= (s16)(m->controller->stickX * 10.0f);

            if (m->faceAngle[0] > 0x38E3) {
                m->faceAngle[0] = 0x38E3;
            }
            if (m->faceAngle[0] < 0) {
                m->faceAngle[0] = 0;
            }

            if (marioObj->oMarioCannonInputYaw > 0x4000) {
                marioObj->oMarioCannonInputYaw = 0x4000;
            }
            if (marioObj->oMarioCannonInputYaw < -0x4000) {
                marioObj->oMarioCannonInputYaw = -0x4000;
            }

            m->faceAngle[1] = marioObj->oMarioCannonObjectYaw + marioObj->oMarioCannonInputYaw;
            if (m->input & INPUT_A_PRESSED) {
                m->forwardVel = 100.0f * coss(m->faceAngle[0]);

                m->vel[1] = 100.0f * sins(m->faceAngle[0]);

                m->pos[0] += 120.0f * coss(m->faceAngle[0]) * sins(m->faceAngle[1]);
                m->pos[1] += 120.0f * sins(m->faceAngle[0]);
                m->pos[2] += 120.0f * coss(m->faceAngle[0]) * coss(m->faceAngle[1]);

                play_sound(SOUND_ACTION_FLYING_FAST, m->marioObj->header.gfx.cameraToObject);
                play_sound(SOUND_OBJ_POUNDING_CANNON, m->marioObj->header.gfx.cameraToObject);

                m->marioObj->header.gfx.node.flags |= GRAPH_RENDER_ACTIVE;

                set_mario_action(m, ACT_SHOT_FROM_CANNON, 0);
#if ENABLE_RUMBLE
                queue_rumble_data(60, 70);
#endif
                m->usedObj->oAction = 2;
                return FALSE;
            } else if (m->faceAngle[0] != startFacePitch || m->faceAngle[1] != startFaceYaw) {
                play_sound(SOUND_MOVING_AIM_CANNON, m->marioObj->header.gfx.cameraToObject);
#if ENABLE_RUMBLE
                reset_rumble_timers_2(0);
#endif
            }
    }

    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
    vec3s_set(m->marioObj->header.gfx.angle, 0, m->faceAngle[1], 0);
    set_mario_animation(m, MARIO_ANIM_DIVE);

    return FALSE;
}

RECOMP_PATCH s32 act_tornado_twirling(struct MarioState *m) {
    struct Surface *floor;
    Vec3f nextPos;
    f32 sinAngleVel;
    f32 cosAngleVel;
    f32 floorHeight;
    struct Object *marioObj = m->marioObj;
    struct Object *usedObj = m->usedObj;
    s16 prevTwirlYaw = m->twirlYaw;

    f32 dx = (m->pos[0] - usedObj->oPosX) * 0.95f;
    f32 dz = (m->pos[2] - usedObj->oPosZ) * 0.95f;

    if (m->vel[1] < 60.0f) {
        m->vel[1] += 1.0f;
    }

    if ((marioObj->oMarioTornadoPosY += m->vel[1]) < 0.0f) {
        marioObj->oMarioTornadoPosY = 0.0f;
    }
    if (marioObj->oMarioTornadoPosY > usedObj->hitboxHeight) {
        if (m->vel[1] < 20.0f) {
            m->vel[1] = 20.0f;
        }
        return set_mario_action(m, ACT_TWIRLING, 1);
    }

    if (m->angleVel[1] < 0x3000) {
        m->angleVel[1] += 0x100;
    }

    if (marioObj->oMarioTornadoYawVel < 0x1000) {
        marioObj->oMarioTornadoYawVel += 0x100;
    }

    m->twirlYaw += m->angleVel[1];

    sinAngleVel = sins(marioObj->oMarioTornadoYawVel);
    cosAngleVel = coss(marioObj->oMarioTornadoYawVel);

    nextPos[0] = usedObj->oPosX + dx * cosAngleVel + dz * sinAngleVel;
    nextPos[2] = usedObj->oPosZ - dx * sinAngleVel + dz * cosAngleVel;
    nextPos[1] = usedObj->oPosY + marioObj->oMarioTornadoPosY;

    f32_find_wall_collision(&nextPos[0], &nextPos[1], &nextPos[2], 60.0f, 50.0f);

    floorHeight = find_floor(nextPos[0], nextPos[1], nextPos[2], &floor);
    if (floor != NULL) {
        m->floor = floor;
        m->floorHeight = floorHeight;
        vec3f_copy(m->pos, nextPos);
    } else {
        if (nextPos[1] >= m->floorHeight) {
            m->pos[1] = nextPos[1];
        } else {
            m->pos[1] = m->floorHeight;
        }
    }

    m->actionTimer++;

    set_mario_animation(m, (m->actionArg == 0) ? MARIO_ANIM_START_TWIRL : MARIO_ANIM_TWIRL);

    if (is_anim_past_end(m)) {
        m->actionArg = 1;
    }

    // Play sound on angle overflow
    if (prevTwirlYaw > m->twirlYaw) {
        play_sound(SOUND_ACTION_TWIRL, m->marioObj->header.gfx.cameraToObject);
    }

    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
    vec3s_set(m->marioObj->header.gfx.angle, 0, m->faceAngle[1] + m->twirlYaw, 0);
#if ENABLE_RUMBLE
    reset_rumble_timers();
#endif

    return FALSE;
}
