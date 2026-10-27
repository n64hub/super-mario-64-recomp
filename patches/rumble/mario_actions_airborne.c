// Shindou's rumble (../rumble.c) in the decomp's src/game/mario_actions_airborne.c: each function in it that rumbles in
// the Shindou edition, as the US game has it, with its #if ENABLE_RUMBLE lines compiled in (../rumble.h). Above them
// are the file's own declarations those functions use.

#include "../patches.h"
#include "sm64.h"
#include "game/area.h"
#include "audio/external.h"
#include "game/camera.h"
#include "engine/graph_node.h"
#include "engine/math_util.h"
#include "game/game_init.h"
#include "game/interaction.h"
#include "game/level_update.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "game/save_file.h"
#include "game/rumble_init.h"

void play_flip_sounds(struct MarioState *m, s16 frame1, s16 frame2, s16 frame3);
s32 lava_boost_on_wall(struct MarioState *m);
s32 check_fall_damage(struct MarioState *m, u32 hardFallAction);
s32 should_get_stuck_in_ground(struct MarioState *m);
s32 check_fall_damage_or_get_stuck(struct MarioState *m, u32 hardFallAction);
void update_air_without_turn(struct MarioState *m);
void update_lava_boost_or_twirling(struct MarioState *m);
void update_flying(struct MarioState *m);
u32 common_air_action_step(struct MarioState *m, u32 landAction, s32 animation, u32 stepArg);
s32 act_triple_jump(struct MarioState *m);
s32 act_backflip(struct MarioState *m);
s32 act_long_jump(struct MarioState *m);
s32 act_twirling(struct MarioState *m);
s32 act_dive(struct MarioState *m);
s32 act_ground_pound(struct MarioState *m);
s32 act_burning_jump(struct MarioState *m);
s32 act_burning_fall(struct MarioState *m);
s32 act_crazy_box_bounce(struct MarioState *m);
u32 common_air_knockback_step(struct MarioState *m, u32 landAction, u32 hardFallAction, s32 animation, f32 speed);
s32 act_lava_boost(struct MarioState *m);
s32 act_shot_from_cannon(struct MarioState *m);
s32 act_flying(struct MarioState *m);
s32 act_riding_hoot(struct MarioState *m);
s32 act_flying_triple_jump(struct MarioState *m);
s32 act_vertical_wind(struct MarioState *m);

#include "../rumble.h"

RECOMP_PATCH s32 check_fall_damage(struct MarioState *m, u32 hardFallAction) {
    f32 fallHeight;
    f32 damageHeight;

    fallHeight = m->peakHeight - m->pos[1];

#pragma GCC diagnostic push
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif

    //! Never true
    if (m->actionState == ACT_GROUND_POUND) {
        damageHeight = 600.0f;
    } else {
        damageHeight = 1150.0f;
    }

#pragma GCC diagnostic pop

    if (m->action != ACT_TWIRLING && m->floor->type != SURFACE_BURNING) {
        if (m->vel[1] < -55.0f) {
            if (fallHeight > 3000.0f) {
                m->hurtCounter += (m->flags & MARIO_CAP_ON_HEAD) ? 16 : 24;
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
                set_camera_shake_from_hit(SHAKE_FALL_DAMAGE);
                play_sound(SOUND_MARIO_ATTACKED, m->marioObj->header.gfx.cameraToObject);
                return drop_and_set_mario_action(m, hardFallAction, 4);
            } else if (fallHeight > damageHeight && !mario_floor_is_slippery(m)) {
                m->hurtCounter += (m->flags & MARIO_CAP_ON_HEAD) ? 8 : 12;
                m->squishTimer = 30;
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
                set_camera_shake_from_hit(SHAKE_FALL_DAMAGE);
                play_sound(SOUND_MARIO_ATTACKED, m->marioObj->header.gfx.cameraToObject);
            }
        }
    }

    return FALSE;
}

RECOMP_PATCH s32 check_fall_damage_or_get_stuck(struct MarioState *m, u32 hardFallAction) {
    if (should_get_stuck_in_ground(m)) {
#ifdef VERSION_JP
        play_sound(SOUND_MARIO_OOOF, m->marioObj->header.gfx.cameraToObject);
#else
        play_sound(SOUND_MARIO_OOOF2, m->marioObj->header.gfx.cameraToObject);
#endif
        m->particleFlags |= PARTICLE_MIST_CIRCLE;
        drop_and_set_mario_action(m, ACT_FEET_STUCK_IN_GROUND, 0);
#if ENABLE_RUMBLE
        queue_rumble_data(5, 80);
#endif
        return TRUE;
    }

    return check_fall_damage(m, hardFallAction);
}

RECOMP_PATCH u32 common_air_action_step(struct MarioState *m, u32 landAction, s32 animation, u32 stepArg) {
    u32 stepResult;

    update_air_without_turn(m);

    stepResult = perform_air_step(m, stepArg);
    switch (stepResult) {
        case AIR_STEP_NONE:
            set_mario_animation(m, animation);
            break;

        case AIR_STEP_LANDED:
            if (!check_fall_damage_or_get_stuck(m, ACT_HARD_BACKWARD_GROUND_KB)) {
                set_mario_action(m, landAction, 0);
            }
            break;

        case AIR_STEP_HIT_WALL:
            set_mario_animation(m, animation);

            if (m->forwardVel > 16.0f) {
#if ENABLE_RUMBLE
                queue_rumble_data(5, 40);
#endif
                mario_bonk_reflection(m, FALSE);
                m->faceAngle[1] += 0x8000;

                if (m->wall != NULL) {
                    set_mario_action(m, ACT_AIR_HIT_WALL, 0);
                } else {
                    if (m->vel[1] > 0.0f) {
                        m->vel[1] = 0.0f;
                    }

                    //! Hands-free holding. Bonking while no wall is referenced
                    // sets Mario's action to a non-holding action without
                    // dropping the object, causing the hands-free holding
                    // glitch. This can be achieved using an exposed ceiling,
                    // out of bounds, grazing the bottom of a wall while
                    // falling such that the final quarter step does not find a
                    // wall collision, or by rising into the top of a wall such
                    // that the final quarter step detects a ledge, but you are
                    // not able to ledge grab it.
                    if (m->forwardVel >= 38.0f) {
                        m->particleFlags |= PARTICLE_VERTICAL_STAR;
                        set_mario_action(m, ACT_BACKWARD_AIR_KB, 0);
                    } else {
                        if (m->forwardVel > 8.0f) {
                            mario_set_forward_vel(m, -8.0f);
                        }
                        return set_mario_action(m, ACT_SOFT_BONK, 0);
                    }
                }
            } else {
                mario_set_forward_vel(m, 0.0f);
            }
            break;

        case AIR_STEP_GRABBED_LEDGE:
            set_mario_animation(m, MARIO_ANIM_IDLE_ON_LEDGE);
            drop_and_set_mario_action(m, ACT_LEDGE_GRAB, 0);
            break;

        case AIR_STEP_GRABBED_CEILING:
            set_mario_action(m, ACT_START_HANGING, 0);
            break;

        case AIR_STEP_HIT_LAVA_WALL:
            lava_boost_on_wall(m);
            break;
    }

    return stepResult;
}

RECOMP_PATCH s32 act_triple_jump(struct MarioState *m) {
    if (gSpecialTripleJump) {
        return set_mario_action(m, ACT_SPECIAL_TRIPLE_JUMP, 0);
    }

    if (m->input & INPUT_B_PRESSED) {
        return set_mario_action(m, ACT_DIVE, 0);
    }

    if (m->input & INPUT_Z_PRESSED) {
        return set_mario_action(m, ACT_GROUND_POUND, 0);
    }

#ifndef VERSION_JP
    play_mario_sound(m, SOUND_ACTION_TERRAIN_JUMP, 0);
#else
    play_mario_sound(m, SOUND_ACTION_TERRAIN_JUMP, SOUND_MARIO_YAHOO);
#endif

    common_air_action_step(m, ACT_TRIPLE_JUMP_LAND, MARIO_ANIM_TRIPLE_JUMP, 0);
#if ENABLE_RUMBLE
    if (m->action == ACT_TRIPLE_JUMP_LAND) {
        queue_rumble_data(5, 40);
    }
#endif
    play_flip_sounds(m, 2, 8, 20);
    return FALSE;
}

RECOMP_PATCH s32 act_backflip(struct MarioState *m) {
    if (m->input & INPUT_Z_PRESSED) {
        return set_mario_action(m, ACT_GROUND_POUND, 0);
    }

    play_mario_sound(m, SOUND_ACTION_TERRAIN_JUMP, SOUND_MARIO_YAH_WAH_HOO);
    common_air_action_step(m, ACT_BACKFLIP_LAND, MARIO_ANIM_BACKFLIP, 0);
#if ENABLE_RUMBLE
    if (m->action == ACT_BACKFLIP_LAND) {
        queue_rumble_data(5, 40);
    }
#endif
    play_flip_sounds(m, 2, 3, 17);
    return FALSE;
}

RECOMP_PATCH s32 act_long_jump(struct MarioState *m) {
    s32 animation;
    if (!m->marioObj->oMarioLongJumpIsSlow) {
        animation = MARIO_ANIM_FAST_LONGJUMP;
    } else {
        animation = MARIO_ANIM_SLOW_LONGJUMP;
    }

    play_mario_sound(m, SOUND_ACTION_TERRAIN_JUMP, SOUND_MARIO_YAHOO);

    if (m->floor->type == SURFACE_VERTICAL_WIND && m->actionState == 0) {
        play_sound(SOUND_MARIO_HERE_WE_GO, m->marioObj->header.gfx.cameraToObject);
        m->actionState = 1;
    }

    common_air_action_step(m, ACT_LONG_JUMP_LAND, animation, AIR_STEP_CHECK_LEDGE_GRAB);
#if ENABLE_RUMBLE
    if (m->action == ACT_LONG_JUMP_LAND) {
        queue_rumble_data(5, 40);
    }
#endif
    return FALSE;
}

RECOMP_PATCH s32 act_twirling(struct MarioState *m) {
    s16 startTwirlYaw = m->twirlYaw;
    s16 yawVelTarget;

    if (m->input & INPUT_A_DOWN) {
        yawVelTarget = 0x2000;
    } else {
        yawVelTarget = 0x1800;
    }

    m->angleVel[1] = approach_s32(m->angleVel[1], yawVelTarget, 0x200, 0x200);
    m->twirlYaw += m->angleVel[1];

    set_mario_animation(m, m->actionArg == 0 ? MARIO_ANIM_START_TWIRL : MARIO_ANIM_TWIRL);
    if (is_anim_past_end(m)) {
        m->actionArg = 1;
    }

    if (startTwirlYaw > m->twirlYaw) {
        play_sound(SOUND_ACTION_TWIRL, m->marioObj->header.gfx.cameraToObject);
    }

    update_lava_boost_or_twirling(m);

    switch (perform_air_step(m, 0)) {
        case AIR_STEP_LANDED:
            set_mario_action(m, ACT_TWIRL_LAND, 0);
            break;

        case AIR_STEP_HIT_WALL:
            mario_bonk_reflection(m, FALSE);
            break;

        case AIR_STEP_HIT_LAVA_WALL:
            lava_boost_on_wall(m);
            break;
    }

    m->marioObj->header.gfx.angle[1] += m->twirlYaw;
#if ENABLE_RUMBLE
    reset_rumble_timers();
#endif
    return FALSE;
}

RECOMP_PATCH s32 act_dive(struct MarioState *m) {
    if (m->actionArg == 0) {
        play_mario_sound(m, SOUND_ACTION_THROW, SOUND_MARIO_HOOHOO);
    } else {
        play_mario_sound(m, SOUND_ACTION_TERRAIN_JUMP, 0);
    }

    set_mario_animation(m, MARIO_ANIM_DIVE);
    if (mario_check_object_grab(m)) {
        mario_grab_used_object(m);
        m->marioBodyState->grabPos = GRAB_POS_LIGHT_OBJ;
        if (m->action != ACT_DIVE) {
            return TRUE;
        }
    }

    update_air_without_turn(m);

    switch (perform_air_step(m, 0)) {
        case AIR_STEP_NONE:
            if (m->vel[1] < 0.0f && m->faceAngle[0] > -0x2AAA) {
                m->faceAngle[0] -= 0x200;
                if (m->faceAngle[0] < -0x2AAA) {
                    m->faceAngle[0] = -0x2AAA;
                }
            }
            m->marioObj->header.gfx.angle[0] = -m->faceAngle[0];
            break;

        case AIR_STEP_LANDED:
            if (should_get_stuck_in_ground(m) && m->faceAngle[0] == -0x2AAA) {
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
#ifdef VERSION_JP
                play_sound(SOUND_MARIO_OOOF, m->marioObj->header.gfx.cameraToObject);
#else
                play_sound(SOUND_MARIO_OOOF2, m->marioObj->header.gfx.cameraToObject);
#endif
                m->particleFlags |= PARTICLE_MIST_CIRCLE;
                drop_and_set_mario_action(m, ACT_HEAD_STUCK_IN_GROUND, 0);
            } else if (!check_fall_damage(m, ACT_HARD_FORWARD_GROUND_KB)) {
                if (m->heldObj == NULL) {
                    set_mario_action(m, ACT_DIVE_SLIDE, 0);
                } else {
                    set_mario_action(m, ACT_DIVE_PICKING_UP, 0);
                }
            }
            m->faceAngle[0] = 0;
            break;

        case AIR_STEP_HIT_WALL:
            mario_bonk_reflection(m, TRUE);
            m->faceAngle[0] = 0;

            if (m->vel[1] > 0.0f) {
                m->vel[1] = 0.0f;
            }

            m->particleFlags |= PARTICLE_VERTICAL_STAR;
            drop_and_set_mario_action(m, ACT_BACKWARD_AIR_KB, 0);
            break;

        case AIR_STEP_HIT_LAVA_WALL:
            lava_boost_on_wall(m);
            break;
    }

    return FALSE;
}

RECOMP_PATCH s32 act_ground_pound(struct MarioState *m) {
    u32 stepResult;
    f32 yOffset;

    play_sound_if_no_flag(m, SOUND_ACTION_THROW, MARIO_ACTION_SOUND_PLAYED);

    if (m->actionState == 0) {
        if (m->actionTimer < 10) {
            yOffset = 20 - 2 * m->actionTimer;
            if (m->pos[1] + yOffset + 160.0f < m->ceilHeight) {
                m->pos[1] += yOffset;
                m->peakHeight = m->pos[1];
                vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
            }
        }

        m->vel[1] = -50.0f;
        mario_set_forward_vel(m, 0.0f);

        set_mario_animation(m, m->actionArg == 0 ? MARIO_ANIM_START_GROUND_POUND
                                                 : MARIO_ANIM_TRIPLE_JUMP_GROUND_POUND);
        if (m->actionTimer == 0) {
            play_sound(SOUND_ACTION_SPIN, m->marioObj->header.gfx.cameraToObject);
        }

        m->actionTimer++;
        if (m->actionTimer >= m->marioObj->header.gfx.animInfo.curAnim->loopEnd + 4) {
            play_sound(SOUND_MARIO_GROUND_POUND_WAH, m->marioObj->header.gfx.cameraToObject);
            m->actionState = 1;
        }
    } else {
        set_mario_animation(m, MARIO_ANIM_GROUND_POUND);

        stepResult = perform_air_step(m, 0);
        if (stepResult == AIR_STEP_LANDED) {
            if (should_get_stuck_in_ground(m)) {
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
#ifdef VERSION_JP
                play_sound(SOUND_MARIO_OOOF, m->marioObj->header.gfx.cameraToObject);
#else
                play_sound(SOUND_MARIO_OOOF2, m->marioObj->header.gfx.cameraToObject);
#endif
                m->particleFlags |= PARTICLE_MIST_CIRCLE;
                set_mario_action(m, ACT_BUTT_STUCK_IN_GROUND, 0);
            } else {
                play_mario_heavy_landing_sound(m, SOUND_ACTION_TERRAIN_HEAVY_LANDING);
                if (!check_fall_damage(m, ACT_HARD_BACKWARD_GROUND_KB)) {
                    m->particleFlags |= PARTICLE_MIST_CIRCLE | PARTICLE_HORIZONTAL_STAR;
                    set_mario_action(m, ACT_GROUND_POUND_LAND, 0);
                }
            }
            set_camera_shake_from_hit(SHAKE_GROUND_POUND);
        } else if (stepResult == AIR_STEP_HIT_WALL) {
            mario_set_forward_vel(m, -16.0f);
            if (m->vel[1] > 0.0f) {
                m->vel[1] = 0.0f;
            }

            m->particleFlags |= PARTICLE_VERTICAL_STAR;
            set_mario_action(m, ACT_BACKWARD_AIR_KB, 0);
        }
    }

    return FALSE;
}

RECOMP_PATCH s32 act_burning_jump(struct MarioState *m) {
    play_mario_sound(m, SOUND_ACTION_TERRAIN_JUMP, m->actionArg == 0 ? 0 : -1);
    mario_set_forward_vel(m, m->forwardVel);

    if (perform_air_step(m, 0) == AIR_STEP_LANDED) {
        play_mario_landing_sound(m, SOUND_ACTION_TERRAIN_LANDING);
        set_mario_action(m, ACT_BURNING_GROUND, 0);
    }

    set_mario_animation(m, m->actionArg == 0 ? MARIO_ANIM_SINGLE_JUMP : MARIO_ANIM_FIRE_LAVA_BURN);
    m->particleFlags |= PARTICLE_FIRE;
    play_sound(SOUND_MOVING_LAVA_BURN, m->marioObj->header.gfx.cameraToObject);

    m->marioObj->oMarioBurnTimer += 3;

    m->health -= 10;
    if (m->health < 0x100) {
        m->health = 0xFF;
    }
#if ENABLE_RUMBLE
    reset_rumble_timers();
#endif
    return FALSE;
}

RECOMP_PATCH s32 act_burning_fall(struct MarioState *m) {
    mario_set_forward_vel(m, m->forwardVel);

    if (perform_air_step(m, 0) == AIR_STEP_LANDED) {
        play_mario_landing_sound(m, SOUND_ACTION_TERRAIN_LANDING);
        set_mario_action(m, ACT_BURNING_GROUND, 0);
    }

    set_mario_animation(m, MARIO_ANIM_GENERAL_FALL);
    m->particleFlags |= PARTICLE_FIRE;
    m->marioObj->oMarioBurnTimer += 3;

    m->health -= 10;
    if (m->health < 0x100) {
        m->health = 0xFF;
    }
#if ENABLE_RUMBLE
    reset_rumble_timers();
#endif
    return FALSE;
}

RECOMP_PATCH s32 act_crazy_box_bounce(struct MarioState *m) {
    f32 minSpeed;

    if (m->actionTimer == 0) {
        switch (m->actionArg) {
            case 0:
                m->vel[1] = 45.0f;
                minSpeed = 32.0f;
                break;

            case 1:
                m->vel[1] = 60.0f;
                minSpeed = 36.0f;
                break;

            case 2:
                m->vel[1] = 100.0f;
                minSpeed = 48.0f;
                break;
        }

        play_sound(minSpeed < 40.0f ? SOUND_GENERAL_BOING1 : SOUND_GENERAL_BOING2,
                   m->marioObj->header.gfx.cameraToObject);

        if (m->forwardVel < minSpeed) {
            mario_set_forward_vel(m, minSpeed);
        }

        m->actionTimer = 1;
    }

    play_mario_sound(m, SOUND_ACTION_TERRAIN_JUMP, 0);
    set_mario_animation(m, MARIO_ANIM_DIVE);

    update_air_without_turn(m);

    switch (perform_air_step(m, 0)) {
        case AIR_STEP_LANDED:
            if (m->actionArg < 2) {
                set_mario_action(m, ACT_CRAZY_BOX_BOUNCE, m->actionArg + 1);
            } else {
                m->heldObj->oInteractStatus = INT_STATUS_STOP_RIDING;
                m->heldObj = NULL;
                set_mario_action(m, ACT_STOMACH_SLIDE, 0);
            }
#if ENABLE_RUMBLE
            queue_rumble_data(5, 80);
#endif
            m->particleFlags |= PARTICLE_MIST_CIRCLE;
            break;

        case AIR_STEP_HIT_WALL:
            mario_bonk_reflection(m, FALSE);
            break;

        case AIR_STEP_HIT_LAVA_WALL:
            lava_boost_on_wall(m);
            break;
    }

    m->marioObj->header.gfx.angle[0] = atan2s(m->forwardVel, -m->vel[1]);
    return FALSE;
}

RECOMP_PATCH u32 common_air_knockback_step(struct MarioState *m, u32 landAction, u32 hardFallAction, s32 animation,
                              f32 speed) {
    u32 stepResult;

    mario_set_forward_vel(m, speed);

    stepResult = perform_air_step(m, 0);
    switch (stepResult) {
        case AIR_STEP_NONE:
            set_mario_animation(m, animation);
            break;

        case AIR_STEP_LANDED:
#if ENABLE_RUMBLE
            if (m->action != ACT_SOFT_BONK) {
                queue_rumble_data(5, 40);
            }
#endif
            if (!check_fall_damage_or_get_stuck(m, hardFallAction)) {
#ifndef VERSION_JP
                if (m->action == ACT_THROWN_FORWARD || m->action == ACT_THROWN_BACKWARD) {
                    set_mario_action(m, landAction, m->hurtCounter);
                } else {
                    set_mario_action(m, landAction, m->actionArg);
                }
#else
                set_mario_action(m, landAction, m->actionArg);
#endif
            }
            break;

        case AIR_STEP_HIT_WALL:
            set_mario_animation(m, MARIO_ANIM_BACKWARD_AIR_KB);
            mario_bonk_reflection(m, FALSE);

            if (m->vel[1] > 0.0f) {
                m->vel[1] = 0.0f;
            }

            mario_set_forward_vel(m, -speed);
            break;

        case AIR_STEP_HIT_LAVA_WALL:
            lava_boost_on_wall(m);
            break;
    }

    return stepResult;
}

RECOMP_PATCH s32 act_lava_boost(struct MarioState *m) {
#if ENABLE_RUMBLE
    if (!(m->flags & MARIO_MARIO_SOUND_PLAYED)) {
#endif
        play_sound_if_no_flag(m, SOUND_MARIO_ON_FIRE, MARIO_MARIO_SOUND_PLAYED);
#if ENABLE_RUMBLE
        queue_rumble_data(5, 80);
    }
#endif

    if (!(m->input & INPUT_NONZERO_ANALOG)) {
        m->forwardVel = approach_f32(m->forwardVel, 0.0f, 0.35f, 0.35f);
    }

    update_lava_boost_or_twirling(m);

    switch (perform_air_step(m, 0)) {
        case AIR_STEP_LANDED:
            if (m->floor->type == SURFACE_BURNING) {
                m->actionState = 0;
                if (!(m->flags & MARIO_METAL_CAP)) {
                    m->hurtCounter += (m->flags & MARIO_CAP_ON_HEAD) ? 12 : 18;
                }
                m->vel[1] = 84.0f;
                play_sound(SOUND_MARIO_ON_FIRE, m->marioObj->header.gfx.cameraToObject);
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
            } else {
                play_mario_heavy_landing_sound(m, SOUND_ACTION_TERRAIN_BODY_HIT_GROUND);
                if (m->actionState < 2 && m->vel[1] < 0.0f) {
                    m->vel[1] = -m->vel[1] * 0.4f;
                    mario_set_forward_vel(m, m->forwardVel * 0.5f);
                    m->actionState++;
                } else {
                    set_mario_action(m, ACT_LAVA_BOOST_LAND, 0);
                }
            }
            break;

        case AIR_STEP_HIT_WALL:
            mario_bonk_reflection(m, FALSE);
            break;

        case AIR_STEP_HIT_LAVA_WALL:
            lava_boost_on_wall(m);
            break;
    }

    set_mario_animation(m, MARIO_ANIM_FIRE_LAVA_BURN);
    if ((m->area->terrainType & TERRAIN_MASK) != TERRAIN_SNOW && !(m->flags & MARIO_METAL_CAP)
        && m->vel[1] > 0.0f) {
        m->particleFlags |= PARTICLE_FIRE;
        if (m->actionState == 0) {
            play_sound(SOUND_MOVING_LAVA_BURN, m->marioObj->header.gfx.cameraToObject);
        }
    }

    if (m->health < 0x100) {
        level_trigger_warp(m, WARP_OP_DEATH);
    }

    m->marioBodyState->eyeState = MARIO_EYES_DEAD;
#if ENABLE_RUMBLE
    reset_rumble_timers();
#endif
    return FALSE;
}

RECOMP_PATCH s32 act_shot_from_cannon(struct MarioState *m) {
    if (m->area->camera->mode != CAMERA_MODE_BEHIND_MARIO) {
        m->statusForCamera->cameraEvent = CAM_EVENT_SHOT_FROM_CANNON;
    }

    mario_set_forward_vel(m, m->forwardVel);

    play_sound_if_no_flag(m, SOUND_MARIO_YAHOO, MARIO_MARIO_SOUND_PLAYED);

    switch (perform_air_step(m, 0)) {
        case AIR_STEP_NONE:
            set_mario_animation(m, MARIO_ANIM_AIRBORNE_ON_STOMACH);
            m->faceAngle[0] = atan2s(m->forwardVel, m->vel[1]);
            m->marioObj->header.gfx.angle[0] = -m->faceAngle[0];
            break;

        case AIR_STEP_LANDED:
            set_mario_action(m, ACT_DIVE_SLIDE, 0);
            m->faceAngle[0] = 0;
            set_camera_mode(m->area->camera, m->area->camera->defMode, 1);
#if ENABLE_RUMBLE
            queue_rumble_data(5, 80);
#endif
            break;

        case AIR_STEP_HIT_WALL:
            mario_set_forward_vel(m, -16.0f);

            m->faceAngle[0] = 0;
            if (m->vel[1] > 0.0f) {
                m->vel[1] = 0.0f;
            }

            m->particleFlags |= PARTICLE_VERTICAL_STAR;
            set_mario_action(m, ACT_BACKWARD_AIR_KB, 0);
            set_camera_mode(m->area->camera, m->area->camera->defMode, 1);
            break;

        case AIR_STEP_HIT_LAVA_WALL:
            lava_boost_on_wall(m);
            break;
    }

    if ((m->flags & MARIO_WING_CAP) && m->vel[1] < 0.0f) {
        set_mario_action(m, ACT_FLYING, 0);
    }

    if ((m->forwardVel -= 0.05) < 10.0f) {
        mario_set_forward_vel(m, 10.0f);
    }

    if (m->vel[1] > 0.0f) {
        m->particleFlags |= PARTICLE_DUST;
    }
#if ENABLE_RUMBLE
    reset_rumble_timers();
#endif
    return FALSE;
}

RECOMP_PATCH s32 act_flying(struct MarioState *m) {
    s16 startPitch = m->faceAngle[0];

    if (m->input & INPUT_Z_PRESSED) {
        if (m->area->camera->mode == CAMERA_MODE_BEHIND_MARIO) {
            set_camera_mode(m->area->camera, m->area->camera->defMode, 1);
        }
        return set_mario_action(m, ACT_GROUND_POUND, 1);
    }

    if (!(m->flags & MARIO_WING_CAP)) {
        if (m->area->camera->mode == CAMERA_MODE_BEHIND_MARIO) {
            set_camera_mode(m->area->camera, m->area->camera->defMode, 1);
        }
        return set_mario_action(m, ACT_FREEFALL, 0);
    }

    if (m->area->camera->mode != CAMERA_MODE_BEHIND_MARIO) {
        set_camera_mode(m->area->camera, CAMERA_MODE_BEHIND_MARIO, 1);
    }

    if (m->actionState == 0) {
        if (m->actionArg == 0) {
            set_mario_animation(m, MARIO_ANIM_FLY_FROM_CANNON);
        } else {
            set_mario_animation(m, MARIO_ANIM_FORWARD_SPINNING_FLIP);
            if (m->marioObj->header.gfx.animInfo.animFrame == 1) {
                play_sound(SOUND_ACTION_SPIN, m->marioObj->header.gfx.cameraToObject);
            }
        }

        if (is_anim_at_end(m)) {
            if (m->actionArg == 2) {
                load_level_init_text(0);
                m->actionArg = 1;
            }

            set_mario_animation(m, MARIO_ANIM_WING_CAP_FLY);
            m->actionState = 1;
        }
    }

    update_flying(m);

    switch (perform_air_step(m, 0)) {
        case AIR_STEP_NONE:
            m->marioObj->header.gfx.angle[0] = -m->faceAngle[0];
            m->marioObj->header.gfx.angle[2] = m->faceAngle[2];
            m->actionTimer = 0;
            break;

        case AIR_STEP_LANDED:
            set_mario_action(m, ACT_DIVE_SLIDE, 0);

            set_mario_animation(m, MARIO_ANIM_DIVE);
            set_anim_to_frame(m, 7);

            m->faceAngle[0] = 0;
            set_camera_mode(m->area->camera, m->area->camera->defMode, 1);
#if ENABLE_RUMBLE
            queue_rumble_data(5, 60);
#endif
            break;

        case AIR_STEP_HIT_WALL:
            if (m->wall != NULL) {
                mario_set_forward_vel(m, -16.0f);
                m->faceAngle[0] = 0;

                if (m->vel[1] > 0.0f) {
                    m->vel[1] = 0.0f;
                }

                play_sound((m->flags & MARIO_METAL_CAP) ? SOUND_ACTION_METAL_BONK
                                                        : SOUND_ACTION_BONK,
                           m->marioObj->header.gfx.cameraToObject);

                m->particleFlags |= PARTICLE_VERTICAL_STAR;
                set_mario_action(m, ACT_BACKWARD_AIR_KB, 0);
                set_camera_mode(m->area->camera, m->area->camera->defMode, 1);
            } else {
                if (m->actionTimer++ == 0) {
                    play_sound(SOUND_ACTION_HIT, m->marioObj->header.gfx.cameraToObject);
                }

                if (m->actionTimer == 30) {
                    m->actionTimer = 0;
                }

                m->faceAngle[0] -= 0x200;
                if (m->faceAngle[0] < -0x2AAA) {
                    m->faceAngle[0] = -0x2AAA;
                }

                m->marioObj->header.gfx.angle[0] = -m->faceAngle[0];
                m->marioObj->header.gfx.angle[2] = m->faceAngle[2];
            }
            break;

        case AIR_STEP_HIT_LAVA_WALL:
            lava_boost_on_wall(m);
            break;
    }

    if (m->faceAngle[0] > 0x800 && m->forwardVel >= 48.0f) {
        m->particleFlags |= PARTICLE_DUST;
    }

    if (startPitch <= 0 && m->faceAngle[0] > 0 && m->forwardVel >= 48.0f) {
        play_sound(SOUND_ACTION_FLYING_FAST, m->marioObj->header.gfx.cameraToObject);
#ifndef VERSION_JP
        play_sound(SOUND_MARIO_YAHOO_WAHA_YIPPEE + ((gAudioRandom % 5) << 16),
                   m->marioObj->header.gfx.cameraToObject);
#endif
#if ENABLE_RUMBLE
        queue_rumble_data(50, 40);
#endif
    }

    play_sound(SOUND_MOVING_FLYING, m->marioObj->header.gfx.cameraToObject);
    adjust_sound_for_speed(m);
    return FALSE;
}

RECOMP_PATCH s32 act_riding_hoot(struct MarioState *m) {
    if (!(m->input & INPUT_A_DOWN) || (m->marioObj->oInteractStatus & INT_STATUS_MARIO_UNK7)) {
        m->usedObj->oInteractStatus = 0;
        m->usedObj->oHootMarioReleaseTime = gGlobalTimer;

        play_sound_if_no_flag(m, SOUND_MARIO_UH, MARIO_MARIO_SOUND_PLAYED);
#if ENABLE_RUMBLE
        queue_rumble_data(4, 40);
#endif
        return set_mario_action(m, ACT_FREEFALL, 0);
    }

    m->pos[0] = m->usedObj->oPosX;
    m->pos[1] = m->usedObj->oPosY - 92.5f;
    m->pos[2] = m->usedObj->oPosZ;

    m->faceAngle[1] = 0x4000 - m->usedObj->oMoveAngleYaw;

    if (m->actionState == 0) {
        set_mario_animation(m, MARIO_ANIM_HANG_ON_CEILING);
        if (is_anim_at_end(m)) {
            set_mario_animation(m, MARIO_ANIM_HANG_ON_OWL);
            m->actionState = 1;
        }
    }

    vec3f_set(m->vel, 0.0f, 0.0f, 0.0f);
    vec3f_set(m->marioObj->header.gfx.pos, m->pos[0], m->pos[1], m->pos[2]);
    vec3s_set(m->marioObj->header.gfx.angle, 0, 0x4000 - m->faceAngle[1], 0);
    return FALSE;
}

RECOMP_PATCH s32 act_flying_triple_jump(struct MarioState *m) {
#ifndef VERSION_JP
    if (m->input & (INPUT_B_PRESSED | INPUT_Z_PRESSED)) {
        if (m->area->camera->mode == CAMERA_MODE_BEHIND_MARIO) {
            set_camera_mode(m->area->camera, m->area->camera->defMode, 1);
        }
        if (m->input & INPUT_B_PRESSED) {
            return set_mario_action(m, ACT_DIVE, 0);
        } else {
            return set_mario_action(m, ACT_GROUND_POUND, 0);
        }
    }
#else
    if (m->input & INPUT_B_PRESSED) {
        return set_mario_action(m, ACT_DIVE, 0);
    }

    if (m->input & INPUT_Z_PRESSED) {
        return set_mario_action(m, ACT_GROUND_POUND, 0);
    }
#endif

    play_mario_sound(m, SOUND_ACTION_TERRAIN_JUMP, SOUND_MARIO_YAHOO);
    if (m->actionState == 0) {
        set_mario_animation(m, MARIO_ANIM_TRIPLE_JUMP_FLY);

        if (m->marioObj->header.gfx.animInfo.animFrame == 7) {
            play_sound(SOUND_ACTION_SPIN, m->marioObj->header.gfx.cameraToObject);
        }

        if (is_anim_past_end(m)) {
            set_mario_animation(m, MARIO_ANIM_FORWARD_SPINNING);
#if ENABLE_RUMBLE
            queue_rumble_data(8, 80);
#endif
            m->actionState = 1;
        }
    }

    if (m->actionState == 1 && m->marioObj->header.gfx.animInfo.animFrame == 1) {
        play_sound(SOUND_ACTION_SPIN, m->marioObj->header.gfx.cameraToObject);
    }

    if (m->vel[1] < 4.0f) {
        if (m->area->camera->mode != CAMERA_MODE_BEHIND_MARIO) {
            set_camera_mode(m->area->camera, CAMERA_MODE_BEHIND_MARIO, 1);
        }

        if (m->forwardVel < 32.0f) {
            mario_set_forward_vel(m, 32.0f);
        }

        set_mario_action(m, ACT_FLYING, 1);
    }

    if (m->actionTimer++ == 10 && m->area->camera->mode != CAMERA_MODE_BEHIND_MARIO) {
        set_camera_mode(m->area->camera, CAMERA_MODE_BEHIND_MARIO, 1);
    }

    update_air_without_turn(m);

    switch (perform_air_step(m, 0)) {
        case AIR_STEP_LANDED:
            if (!check_fall_damage_or_get_stuck(m, ACT_HARD_BACKWARD_GROUND_KB)) {
                set_mario_action(m, ACT_DOUBLE_JUMP_LAND, 0);
            }
            break;

        case AIR_STEP_HIT_WALL:
            mario_bonk_reflection(m, FALSE);
            break;

        case AIR_STEP_HIT_LAVA_WALL:
            lava_boost_on_wall(m);
            break;
    }

    return FALSE;
}

RECOMP_PATCH s32 act_vertical_wind(struct MarioState *m) {
    s16 intendedDYaw = m->intendedYaw - m->faceAngle[1];
    f32 intendedMag = m->intendedMag / 32.0f;

    play_sound_if_no_flag(m, SOUND_MARIO_HERE_WE_GO, MARIO_MARIO_SOUND_PLAYED);
    if (m->actionState == 0) {
        set_mario_animation(m, MARIO_ANIM_FORWARD_SPINNING_FLIP);
        if (m->marioObj->header.gfx.animInfo.animFrame == 1) {
            play_sound(SOUND_ACTION_SPIN, m->marioObj->header.gfx.cameraToObject);
#if ENABLE_RUMBLE
            queue_rumble_data(8, 80);
#endif
        }

        if (is_anim_past_end(m)) {
            m->actionState = 1;
        }
    } else {
        set_mario_animation(m, MARIO_ANIM_AIRBORNE_ON_STOMACH);
    }

    update_air_without_turn(m);

    switch (perform_air_step(m, 0)) {
        case AIR_STEP_LANDED:
            set_mario_action(m, ACT_DIVE_SLIDE, 0);
            break;

        case AIR_STEP_HIT_WALL:
            mario_set_forward_vel(m, -16.0f);
            break;
    }

    m->marioObj->header.gfx.angle[0] = (s16)(6144.0f * intendedMag * coss(intendedDYaw));
    m->marioObj->header.gfx.angle[2] = (s16)(-4096.0f * intendedMag * sins(intendedDYaw));
    return FALSE;
}
