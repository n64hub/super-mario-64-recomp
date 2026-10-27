// Shindou's rumble (../rumble.c) in the decomp's src/game/interaction.c: each function in it that rumbles in the
// Shindou edition, as the US game has it, with its #if ENABLE_RUMBLE lines compiled in (../rumble.h). Above them are
// the file's own declarations those functions use.

#include "../patches.h"
#include "game/area.h"
#include "actors/common1.h"
#include "audio/external.h"
#include "game/behavior_actions.h"
#include "behavior_data.h"
#include "game/camera.h"
#include "course_table.h"
#include "dialog_ids.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "game/game_init.h"
#include "game/interaction.h"
#include "game/level_update.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "game/memory.h"
#include "game/obj_behaviors.h"
#include "game/object_helpers.h"
#include "game/save_file.h"
#include "seq_ids.h"
#include "sm64.h"
#include "game/sound_init.h"
#include "game/rumble_init.h"

#define INT_GROUND_POUND_OR_TWIRL (1 << 0) 
#define INT_PUNCH                 (1 << 1) 
#define INT_KICK                  (1 << 2) 
#define INT_TRIP                  (1 << 3) 
#define INT_SLIDE_KICK            (1 << 4) 
#define INT_FAST_ATTACK_OR_SHELL  (1 << 5) 
#define INT_HIT_FROM_ABOVE        (1 << 6) 
#define INT_HIT_FROM_BELOW        (1 << 7) 
#define INT_ATTACK_NOT_FROM_BELOW                                                 \
    (INT_GROUND_POUND_OR_TWIRL | INT_PUNCH | INT_KICK | INT_TRIP | INT_SLIDE_KICK \
     | INT_FAST_ATTACK_OR_SHELL | INT_HIT_FROM_ABOVE)
#define INT_ANY_ATTACK                                                            \
    (INT_GROUND_POUND_OR_TWIRL | INT_PUNCH | INT_KICK | INT_TRIP | INT_SLIDE_KICK \
     | INT_FAST_ATTACK_OR_SHELL | INT_HIT_FROM_ABOVE | INT_HIT_FROM_BELOW)
extern u8 sDelayInvincTimer;
extern s16 sInvulnerable;
u32 interact_coin(struct MarioState *, u32, struct Object *);
u32 interact_star_or_key(struct MarioState *, u32, struct Object *);
u32 interact_warp(struct MarioState *, u32, struct Object *);
u32 interact_tornado(struct MarioState *, u32, struct Object *);
u32 interact_whirlpool(struct MarioState *, u32, struct Object *);
u32 interact_flame(struct MarioState *, u32, struct Object *);
u32 interact_bully(struct MarioState *, u32, struct Object *);
u32 interact_shock(struct MarioState *, u32, struct Object *);
u32 interact_hit_from_below(struct MarioState *, u32, struct Object *);
u32 interact_bounce_top(struct MarioState *, u32, struct Object *);
u32 interact_pole(struct MarioState *, u32, struct Object *);
u32 interact_hoot(struct MarioState *, u32, struct Object *);
extern u8 sJustTeleported;
u32 object_facing_mario(struct MarioState *m, struct Object *o, s16 angleRange);
u32 determine_interaction(struct MarioState *m, struct Object *o);
u32 attack_object(struct Object *o, s32 interaction);
void mario_stop_riding_object(struct MarioState *m);
void mario_stop_riding_and_holding(struct MarioState *m);
u32 bully_knock_back_mario(struct MarioState *mario);
void bounce_off_object(struct MarioState *m, struct Object *o, f32 velY);
void hit_object_from_below(struct MarioState *m, UNUSED struct Object *o);
void push_mario_out_of_object(struct MarioState *m, struct Object *o, f32 padding);
void bounce_back_from_attack(struct MarioState *m, u32 interaction);
u32 take_damage_from_interact_object(struct MarioState *m);
u32 take_damage_and_knock_back(struct MarioState *m, struct Object *o);
void reset_mario_pitch(struct MarioState *m);
u32 interact_coin(struct MarioState *m, UNUSED u32 interactType, struct Object *o);
u32 interact_star_or_key(struct MarioState *m, UNUSED u32 interactType, struct Object *o);
u32 interact_warp(struct MarioState *m, UNUSED u32 interactType, struct Object *o);
u32 interact_tornado(struct MarioState *m, UNUSED u32 interactType, struct Object *o);
u32 interact_whirlpool(struct MarioState *m, UNUSED u32 interactType, struct Object *o);
u32 interact_flame(struct MarioState *m, UNUSED u32 interactType, struct Object *o);
u32 interact_bully(struct MarioState *m, UNUSED u32 interactType, struct Object *o);
u32 interact_shock(struct MarioState *m, UNUSED u32 interactType, struct Object *o);
u32 interact_hit_from_below(struct MarioState *m, UNUSED u32 interactType, struct Object *o);
u32 interact_bounce_top(struct MarioState *m, UNUSED u32 interactType, struct Object *o);
u32 check_object_grab_mario(struct MarioState *m, UNUSED u32 interactType, struct Object *o);
u32 interact_pole(struct MarioState *m, UNUSED u32 interactType, struct Object *o);
u32 interact_hoot(struct MarioState *m, UNUSED u32 interactType, struct Object *o);

#include "../rumble.h"

RECOMP_PATCH u32 take_damage_from_interact_object(struct MarioState *m) {
    s32 shake;
    s32 damage = m->interactObj->oDamageOrCoinValue;

    if (damage >= 4) {
        shake = SHAKE_LARGE_DAMAGE;
    } else if (damage >= 2) {
        shake = SHAKE_MED_DAMAGE;
    } else {
        shake = SHAKE_SMALL_DAMAGE;
    }

    if (!(m->flags & MARIO_CAP_ON_HEAD)) {
        damage += (damage + 1) / 2;
    }

    if (m->flags & MARIO_METAL_CAP) {
        damage = 0;
    }

    m->hurtCounter += 4 * damage;

#if ENABLE_RUMBLE
    queue_rumble_data(5, 80);
#endif
    set_camera_shake_from_hit(shake);

    return damage;
}

RECOMP_PATCH u32 interact_coin(struct MarioState *m, UNUSED u32 interactType, struct Object *o) {
    m->numCoins += o->oDamageOrCoinValue;
    m->healCounter += 4 * o->oDamageOrCoinValue;

    o->oInteractStatus = INT_STATUS_INTERACTED;

    if (COURSE_IS_MAIN_COURSE(gCurrCourseNum)
        && m->numCoins - o->oDamageOrCoinValue < 100 && m->numCoins >= 100) {
        bhv_spawn_star_no_level_exit(STAR_INDEX_100_COINS);
    }

#if ENABLE_RUMBLE
    if (o->oDamageOrCoinValue >= 2) {
        queue_rumble_data(5, 80);
    }
#endif

    return FALSE;
}

RECOMP_PATCH u32 interact_star_or_key(struct MarioState *m, UNUSED u32 interactType, struct Object *o) {
    u32 starIndex;
    u32 starGrabAction = ACT_STAR_DANCE_EXIT;
    u32 noExit = (o->oInteractionSubtype & INT_SUBTYPE_NO_EXIT) != 0;
    u32 grandStar = (o->oInteractionSubtype & INT_SUBTYPE_GRAND_STAR) != 0;

    if (m->health >= 0x100) {
        mario_stop_riding_and_holding(m);
#if ENABLE_RUMBLE
        queue_rumble_data(5, 80);
#endif

        if (!noExit) {
            m->hurtCounter = 0;
            m->healCounter = 0;
            if (m->capTimer > 1) {
                m->capTimer = 1;
            }
        }

        if (noExit) {
            starGrabAction = ACT_STAR_DANCE_NO_EXIT;
        }

        if (m->action & ACT_FLAG_SWIMMING) {
            starGrabAction = ACT_STAR_DANCE_WATER;
        }

        if (m->action & ACT_FLAG_METAL_WATER) {
            starGrabAction = ACT_STAR_DANCE_WATER;
        }

        if (m->action & ACT_FLAG_AIR) {
            starGrabAction = ACT_FALL_AFTER_STAR_GRAB;
        }

        spawn_object(o, MODEL_NONE, bhvStarKeyCollectionPuffSpawner);

        o->oInteractStatus = INT_STATUS_INTERACTED;
        m->interactObj = o;
        m->usedObj = o;

        starIndex = (o->oBhvParams >> 24) & 0x1F;
        save_file_collect_star_or_key(m->numCoins, starIndex);

        m->numStars =
            save_file_get_total_star_count(gCurrSaveFileNum - 1, COURSE_MIN - 1, COURSE_MAX - 1);

        if (!noExit) {
            drop_queued_background_music();
            fadeout_level_music(126);
        }

        play_sound(SOUND_MENU_STAR_SOUND, m->marioObj->header.gfx.cameraToObject);
#ifndef VERSION_JP
        update_mario_sound_and_camera(m);
#endif

        if (grandStar) {
            return set_mario_action(m, ACT_JUMBO_STAR_CUTSCENE, 0);
        }

        return set_mario_action(m, starGrabAction, noExit + 2 * grandStar);
    }

    return FALSE;
}

RECOMP_PATCH u32 interact_warp(struct MarioState *m, UNUSED u32 interactType, struct Object *o) {
    u32 action;

    if (o->oInteractionSubtype & INT_SUBTYPE_FADING_WARP) {
        action = m->action;

        if (action == ACT_TELEPORT_FADE_IN) {
            sJustTeleported = TRUE;

        } else if (!sJustTeleported) {
            if (action == ACT_IDLE || action == ACT_PANTING || action == ACT_STANDING_AGAINST_WALL
                || action == ACT_CROUCHING) {
                m->interactObj = o;
                m->usedObj = o;

                sJustTeleported = TRUE;
                return set_mario_action(m, ACT_TELEPORT_FADE_OUT, 0);
            }
        }
    } else {
        if (m->action != ACT_EMERGE_FROM_PIPE) {
            o->oInteractStatus = INT_STATUS_INTERACTED;
            m->interactObj = o;
            m->usedObj = o;

#if ENABLE_RUMBLE
            if (o->collisionData == segmented_to_virtual(warp_pipe_seg3_collision_03009AC8)) {
                play_sound(SOUND_MENU_ENTER_PIPE, m->marioObj->header.gfx.cameraToObject);
                queue_rumble_data(15, 80);
            } else {
                play_sound(SOUND_MENU_ENTER_HOLE, m->marioObj->header.gfx.cameraToObject);
                queue_rumble_data(12, 80);
            }
#else
            play_sound(o->collisionData == segmented_to_virtual(warp_pipe_seg3_collision_03009AC8)
                           ? SOUND_MENU_ENTER_PIPE
                           : SOUND_MENU_ENTER_HOLE,
                       m->marioObj->header.gfx.cameraToObject);
#endif

            mario_stop_riding_object(m);
            return set_mario_action(m, ACT_DISAPPEARED, (WARP_OP_WARP_OBJECT << 16) + 2);
        }
    }

    return FALSE;
}

RECOMP_PATCH u32 interact_tornado(struct MarioState *m, UNUSED u32 interactType, struct Object *o) {
    struct Object *marioObj = m->marioObj;

    if (m->action != ACT_TORNADO_TWIRLING && m->action != ACT_SQUISHED) {
        mario_stop_riding_and_holding(m);
        mario_set_forward_vel(m, 0.0f);
        update_mario_sound_and_camera(m);

        o->oInteractStatus = INT_STATUS_INTERACTED;
        m->interactObj = o;
        m->usedObj = o;

        marioObj->oMarioTornadoYawVel = 0x400;
        marioObj->oMarioTornadoPosY = m->pos[1] - o->oPosY;

        play_sound(SOUND_MARIO_WAAAOOOW, m->marioObj->header.gfx.cameraToObject);
#if ENABLE_RUMBLE
        queue_rumble_data(30, 60);
#endif
        return set_mario_action(m, ACT_TORNADO_TWIRLING, m->action == ACT_TWIRLING);
    }

    return FALSE;
}

RECOMP_PATCH u32 interact_whirlpool(struct MarioState *m, UNUSED u32 interactType, struct Object *o) {
    struct Object *marioObj = m->marioObj;

    if (m->action != ACT_CAUGHT_IN_WHIRLPOOL) {
        mario_stop_riding_and_holding(m);
        o->oInteractStatus = INT_STATUS_INTERACTED;
        m->interactObj = o;
        m->usedObj = o;

        m->forwardVel = 0.0f;

        marioObj->oMarioWhirlpoolPosY = m->pos[1] - o->oPosY;

        play_sound(SOUND_MARIO_WAAAOOOW, m->marioObj->header.gfx.cameraToObject);
#if ENABLE_RUMBLE
        queue_rumble_data(30, 60);
#endif
        return set_mario_action(m, ACT_CAUGHT_IN_WHIRLPOOL, 0);
    }

    return FALSE;
}

RECOMP_PATCH u32 interact_flame(struct MarioState *m, UNUSED u32 interactType, struct Object *o) {
    u32 burningAction = ACT_BURNING_JUMP;

    if (!sInvulnerable && !(m->flags & MARIO_METAL_CAP) && !(m->flags & MARIO_VANISH_CAP)
        && !(o->oInteractionSubtype & INT_SUBTYPE_DELAY_INVINCIBILITY)) {
#if ENABLE_RUMBLE
        queue_rumble_data(5, 80);
#endif
        o->oInteractStatus = INT_STATUS_INTERACTED;
        m->interactObj = o;

        if ((m->action & (ACT_FLAG_SWIMMING | ACT_FLAG_METAL_WATER))
            || m->waterLevel - m->pos[1] > 50.0f) {
            play_sound(SOUND_GENERAL_FLAME_OUT, m->marioObj->header.gfx.cameraToObject);
        } else {
            m->marioObj->oMarioBurnTimer = 0;
            update_mario_sound_and_camera(m);
            play_sound(SOUND_MARIO_ON_FIRE, m->marioObj->header.gfx.cameraToObject);

            if ((m->action & ACT_FLAG_AIR) && m->vel[1] <= 0.0f) {
                burningAction = ACT_BURNING_FALL;
            }

            return drop_and_set_mario_action(m, burningAction, 1);
        }
    }

    return FALSE;
}

RECOMP_PATCH u32 interact_bully(struct MarioState *m, UNUSED u32 interactType, struct Object *o) {
    UNUSED u8 filler[4];

    u32 interaction;
    if (m->flags & MARIO_METAL_CAP) {
        interaction = INT_FAST_ATTACK_OR_SHELL;
    } else {
        interaction = determine_interaction(m, o);
    }

    m->interactObj = o;

    if (interaction & INT_ATTACK_NOT_FROM_BELOW) {
#if ENABLE_RUMBLE
        queue_rumble_data(5, 80);
#endif
        push_mario_out_of_object(m, o, 5.0f);

        m->forwardVel = -16.0f;
        o->oMoveAngleYaw = m->faceAngle[1];
        o->oForwardVel = 3392.0f / o->hitboxRadius;

        attack_object(o, interaction);
        bounce_back_from_attack(m, interaction);
        return TRUE;
    }

    else if (!sInvulnerable && !(m->flags & MARIO_VANISH_CAP)
             && !(o->oInteractionSubtype & INT_SUBTYPE_DELAY_INVINCIBILITY)) {
        o->oInteractStatus = INT_STATUS_INTERACTED;
        m->invincTimer = 2;

        update_mario_sound_and_camera(m);
        play_sound(SOUND_MARIO_EEUH, m->marioObj->header.gfx.cameraToObject);
        play_sound(SOUND_OBJ_BULLY_METAL, m->marioObj->header.gfx.cameraToObject);

        push_mario_out_of_object(m, o, 5.0f);
        drop_and_set_mario_action(m, bully_knock_back_mario(m), 0);
#if ENABLE_RUMBLE
        queue_rumble_data(5, 80);
#endif
        return TRUE;
    }

    return FALSE;
}

RECOMP_PATCH u32 interact_shock(struct MarioState *m, UNUSED u32 interactType, struct Object *o) {
    if (!sInvulnerable && !(m->flags & MARIO_VANISH_CAP)
        && !(o->oInteractionSubtype & INT_SUBTYPE_DELAY_INVINCIBILITY)) {
        u32 actionArg = (m->action & (ACT_FLAG_AIR | ACT_FLAG_ON_POLE | ACT_FLAG_HANGING)) == 0;

        o->oInteractStatus = INT_STATUS_INTERACTED | INT_STATUS_ATTACKED_MARIO;
        m->interactObj = o;

        take_damage_from_interact_object(m);
        play_sound(SOUND_MARIO_ATTACKED, m->marioObj->header.gfx.cameraToObject);
#if ENABLE_RUMBLE
        queue_rumble_data(70, 60);
#endif

        if (m->action & (ACT_FLAG_SWIMMING | ACT_FLAG_METAL_WATER)) {
            return drop_and_set_mario_action(m, ACT_WATER_SHOCKED, 0);
        } else {
            update_mario_sound_and_camera(m);
            return drop_and_set_mario_action(m, ACT_SHOCKED, actionArg);
        }
    }

    if (!(o->oInteractionSubtype & INT_SUBTYPE_DELAY_INVINCIBILITY)) {
        sDelayInvincTimer = TRUE;
    }

    return FALSE;
}

RECOMP_PATCH u32 interact_hit_from_below(struct MarioState *m, UNUSED u32 interactType, struct Object *o) {
    UNUSED u8 filler[4];

    u32 interaction;
    if (m->flags & MARIO_METAL_CAP) {
        interaction = INT_FAST_ATTACK_OR_SHELL;
    } else {
        interaction = determine_interaction(m, o);
    }

    if (interaction & INT_ANY_ATTACK) {
#if ENABLE_RUMBLE
        queue_rumble_data(5, 80);
#endif
        attack_object(o, interaction);
        bounce_back_from_attack(m, interaction);

        if (interaction & INT_HIT_FROM_BELOW) {
            hit_object_from_below(m, o);
        }

        if (interaction & INT_HIT_FROM_ABOVE) {
            if (o->oInteractionSubtype & INT_SUBTYPE_TWIRL_BOUNCE) {
                bounce_off_object(m, o, 80.0f);
                reset_mario_pitch(m);
#ifndef VERSION_JP
                play_sound(SOUND_MARIO_TWIRL_BOUNCE, m->marioObj->header.gfx.cameraToObject);
#endif
                return drop_and_set_mario_action(m, ACT_TWIRLING, 0);
            } else {
                bounce_off_object(m, o, 30.0f);
            }
        }
    } else if (take_damage_and_knock_back(m, o)) {
        return TRUE;
    }

    if (!(o->oInteractionSubtype & INT_SUBTYPE_DELAY_INVINCIBILITY)) {
        sDelayInvincTimer = TRUE;
    }

    return FALSE;
}

RECOMP_PATCH u32 interact_bounce_top(struct MarioState *m, UNUSED u32 interactType, struct Object *o) {
    u32 interaction;
    if (m->flags & MARIO_METAL_CAP) {
        interaction = INT_FAST_ATTACK_OR_SHELL;
    } else {
        interaction = determine_interaction(m, o);
    }

    if (interaction & INT_ATTACK_NOT_FROM_BELOW) {
#if ENABLE_RUMBLE
        queue_rumble_data(5, 80);
#endif
        attack_object(o, interaction);
        bounce_back_from_attack(m, interaction);

        if (interaction & INT_HIT_FROM_ABOVE) {
            if (o->oInteractionSubtype & INT_SUBTYPE_TWIRL_BOUNCE) {
                bounce_off_object(m, o, 80.0f);
                reset_mario_pitch(m);
#ifndef VERSION_JP
                play_sound(SOUND_MARIO_TWIRL_BOUNCE, m->marioObj->header.gfx.cameraToObject);
#endif
                return drop_and_set_mario_action(m, ACT_TWIRLING, 0);
            } else {
                bounce_off_object(m, o, 30.0f);
            }
        }
    } else if (take_damage_and_knock_back(m, o)) {
        return TRUE;
    }

    if (!(o->oInteractionSubtype & INT_SUBTYPE_DELAY_INVINCIBILITY)) {
        sDelayInvincTimer = TRUE;
    }

    return FALSE;
}

RECOMP_PATCH u32 check_object_grab_mario(struct MarioState *m, UNUSED u32 interactType, struct Object *o) {
    if ((!(m->action & (ACT_FLAG_AIR | ACT_FLAG_INVULNERABLE | ACT_FLAG_ATTACKING)) || !sInvulnerable)
        && (o->oInteractionSubtype & INT_SUBTYPE_GRABS_MARIO)) {
        if (object_facing_mario(m, o, 0x2AAA)) {
            mario_stop_riding_and_holding(m);
            o->oInteractStatus = INT_STATUS_INTERACTED | INT_STATUS_GRABBED_MARIO;

            m->faceAngle[1] = o->oMoveAngleYaw;
            m->interactObj = o;
            m->usedObj = o;

            update_mario_sound_and_camera(m);
            play_sound(SOUND_MARIO_OOOF, m->marioObj->header.gfx.cameraToObject);
#if ENABLE_RUMBLE
            queue_rumble_data(5, 80);
#endif
            return set_mario_action(m, ACT_GRABBED, 0);
        }
    }

    push_mario_out_of_object(m, o, -5.0f);
    return FALSE;
}

RECOMP_PATCH u32 interact_pole(struct MarioState *m, UNUSED u32 interactType, struct Object *o) {
    s32 actionId = m->action & ACT_ID_MASK;
    if (actionId >= 0x080 && actionId < 0x0A0) {
        if (!(m->prevAction & ACT_FLAG_ON_POLE) || m->usedObj != o) {
#if defined(VERSION_SH) || defined(VERSION_CN)
            f32 velConv = m->forwardVel; // conserve the velocity.
            struct Object *marioObj = m->marioObj;
            u32 lowSpeed;
#else
            u32 lowSpeed = (m->forwardVel <= 10.0f);
            struct Object *marioObj = m->marioObj;
#endif

            mario_stop_riding_and_holding(m);

#if defined(VERSION_SH) || defined(VERSION_CN)
            lowSpeed = (velConv <= 10.0f);
#endif

            m->interactObj = o;
            m->usedObj = o;
            m->vel[1] = 0.0f;
            m->forwardVel = 0.0f;

            marioObj->oMarioPoleUnk108 = 0;
            marioObj->oMarioPoleYawVel = 0;
            marioObj->oMarioPolePos = m->pos[1] - o->oPosY;

            if (lowSpeed) {
                return set_mario_action(m, ACT_GRAB_POLE_SLOW, 0);
            }

            //! @bug Using m->forwardVel here is assumed to be 0.0f due to the set from earlier.
            //       This is fixed in the Shindou version.
#if defined(VERSION_SH) || defined(VERSION_CN)
            marioObj->oMarioPoleYawVel = (s32)(velConv * 0x100 + 0x1000);
#else
            marioObj->oMarioPoleYawVel = (s32)(m->forwardVel * 0x100 + 0x1000);
#endif
            reset_mario_pitch(m);
#if ENABLE_RUMBLE
            queue_rumble_data(5, 80);
#endif
            return set_mario_action(m, ACT_GRAB_POLE_FAST, 0);
        }
    }

    return FALSE;
}

RECOMP_PATCH u32 interact_hoot(struct MarioState *m, UNUSED u32 interactType, struct Object *o) {
    s32 actionId = m->action & ACT_ID_MASK;

    //! Can pause to advance the global timer without falling too far, allowing
    // you to regrab after letting go.
    if (actionId >= 0x080 && actionId < 0x098
        && (gGlobalTimer - m->usedObj->oHootMarioReleaseTime > 30)) {
        mario_stop_riding_and_holding(m);

        o->oInteractStatus = TRUE; //! Note: Not a flag, treated as a TRUE/FALSE statement
        m->interactObj = o;
        m->usedObj = o;

#if ENABLE_RUMBLE
        queue_rumble_data(5, 80);
#endif
        update_mario_sound_and_camera(m);
        return set_mario_action(m, ACT_RIDING_HOOT, 0);
    }

    return FALSE;
}
