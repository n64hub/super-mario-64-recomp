// Shindou's rumble (../rumble.c) in the decomp's
// src/game/behaviors/{capswitch,exclamation_box,purple_switch,star_door,water_pillar,wdw_water_level}.inc.c, which
// src/game/behavior_actions.c includes: each function in it that rumbles in the Shindou edition, as the US game has it,
// with its #if ENABLE_RUMBLE lines compiled in (../rumble.h). Above them are the file's own declarations those
// functions use.

#include "../patches.h"
#include "types.h"
#include "actors/common1.h"
#include "actors/group12.h"
#include "actors/group13.h"
#include "game/area.h"
#include "audio/external.h"
#include "game/behavior_actions.h"
#include "behavior_data.h"
#include "game/camera.h"
#include "game/debug.h"
#include "dialog_ids.h"
#include "engine/behavior_script.h"
#include "engine/graph_node.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "engine/surface_load.h"
#include "game/game_init.h"
#include "game/ingame_menu.h"
#include "game/interaction.h"
#include "level_misc_macros.h"
#include "level_table.h"
#include "game/level_update.h"
#include "levels/bob/header.h"
#include "levels/bowser_3/header.h"
#include "levels/castle_inside/header.h"
#include "levels/hmc/header.h"
#include "game/main.h"
#include "game/mario.h"
#include "game/mario_actions_cutscene.h"
#include "game/mario_step.h"
#include "game/obj_behaviors.h"
#include "game/obj_behaviors_2.h"
#include "object_constants.h"
#include "game/object_helpers.h"
#include "game/object_list_processor.h"
#include "game/paintings.h"
#include "game/platform_displacement.h"
#include "game/rendering_graph_node.h"
#include "game/save_file.h"
#include "seq_ids.h"
#include "sm64.h"
#include "game/spawn_object.h"
#include "game/spawn_sound.h"
#include "game/rumble_init.h"

#define o gCurrentObject
void star_door_update_pos(void);
void bhv_star_door_loop(void);
void cap_switch_act_2(void);
void spawn_triangle_break_particles(s16 numTris, s16 triModel, f32 triSize, s16 triAnimState);
void bhv_purple_switch_loop(void);
void water_level_pillar_undrained(void);
extern struct ObjectHitbox sExclamationBoxHitbox;
void exclamation_box_act_2(void);
void bhv_water_level_diamond_loop(void);

#include "../rumble.h"

RECOMP_PATCH void cap_switch_act_2(void) {
    if (o->oTimer < 5) {
        cur_obj_scale_over_time(2, 4, 0.5f, 0.1f);
        if (o->oTimer == 4) {
            cur_obj_shake_screen(SHAKE_POS_SMALL);
            spawn_mist_particles();
            spawn_triangle_break_particles(60, MODEL_CARTOON_STAR, 0.3f, o->oBhvParams2ndByte);
#if ENABLE_RUMBLE
            queue_rumble_data(5, 80);
#endif
        }
    } else {
        //! Neither of these flags are defined in this function so they do nothing.
        //  On an extra note, there's a specific check for this cutscene and
        //  there's no dialog defined since the cutscene itself calls the dialog.
        s32 dialogResponse = cur_obj_update_dialog_with_cutscene(MARIO_DIALOG_LOOK_FRONT,
            (DIALOG_FLAG_TEXT_RESPONSE | DIALOG_FLAG_UNK_CAPSWITCH), CUTSCENE_CAP_SWITCH_PRESS, 0);
        if (dialogResponse) {
            o->oAction = 3;
        }
    }
}

RECOMP_PATCH void exclamation_box_act_2(void) {
    obj_set_hitbox(o, &sExclamationBoxHitbox);

    if (o->oTimer == 0) {
        cur_obj_unhide();
        cur_obj_become_tangible();
        o->oInteractStatus = 0;
        o->oPosY = o->oHomeY;
        o->oGraphYOffset = 0.0f;
    }

    if (cur_obj_was_attacked_or_ground_pounded()) {
        cur_obj_become_intangible();
        o->oExclamationBoxUnkFC = 0x4000;
        o->oVelY = 30.0f;
        o->oGravity = -8.0f;
        o->oFloorHeight = o->oPosY;
        o->oAction = 3;
#if ENABLE_RUMBLE
        queue_rumble_data(5, 80);
#endif
    }

    load_object_collision_model();
}

RECOMP_PATCH void bhv_purple_switch_loop(void) {
    UNUSED u8 filler[4];

    switch (o->oAction) {
        /**
         * Set the switch's model and scale. If Mario is standing near the
         * switch's middle section, transition to the pressed state.
         */
        case PURPLE_SWITCH_ACT_IDLE:
            cur_obj_set_model(MODEL_PURPLE_SWITCH);
            cur_obj_scale(1.5f);
            if (gMarioObject->platform == o && !(gMarioStates[0].action & MARIO_UNKNOWN_13)) {
                if (lateral_dist_between_objects(o, gMarioObject) < 127.5) {
                    o->oAction = PURPLE_SWITCH_ACT_PRESSED;
                }
            }
            break;

        /**
         * Collapse the switch downward, play a sound, and shake the screen.
         * Immediately transition to the ticking state.
         */
        case PURPLE_SWITCH_ACT_PRESSED:
            cur_obj_scale_over_time(2, 3, 1.5f, 0.2f);
            if (o->oTimer == 3) {
                cur_obj_play_sound_2(SOUND_GENERAL2_PURPLE_SWITCH);
                o->oAction = PURPLE_SWITCH_ACT_TICKING;
                cur_obj_shake_screen(SHAKE_POS_SMALL);
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
            }
            break;

        /**
         * Play a continuous ticking sound that gets faster when time is almost
         * up. When time is up, move to a waiting-while-pressed state.
         */
        case PURPLE_SWITCH_ACT_TICKING:
            if (o->oBhvParams2ndByte != 0) {
                if (o->oBhvParams2ndByte == 1 && gMarioObject->platform != o) {
                    o->oAction++;
                } else {
                    if (o->oTimer < 360) {
                        play_sound(SOUND_GENERAL2_SWITCH_TICK_FAST, gGlobalSoundSource);
                    } else {
                        play_sound(SOUND_GENERAL2_SWITCH_TICK_SLOW, gGlobalSoundSource);
                    }
                    if (o->oTimer > 400) {
                        o->oAction = PURPLE_SWITCH_ACT_WAIT_FOR_MARIO_TO_GET_OFF;
                    }
                }
            }
            break;

        /**
         * Make the switch look unpressed again, and transition back to the
         * idle state.
         */
        case PURPLE_SWITCH_ACT_UNPRESSED:
            cur_obj_scale_over_time(2, 3, 0.2f, 1.5f);
            if (o->oTimer == 3) {
                o->oAction = PURPLE_SWITCH_ACT_IDLE;
            }
            break;

        /**
         * Mario is standing on the switch, but time has expired. Wait for
         * him to get off the switch, and when he does so, transition to the
         * unpressed state.
         */
        case PURPLE_SWITCH_ACT_WAIT_FOR_MARIO_TO_GET_OFF:
            if (!cur_obj_is_mario_on_platform()) {
                o->oAction = PURPLE_SWITCH_ACT_UNPRESSED;
            }
            break;
    }
}

RECOMP_PATCH void bhv_star_door_loop(void) {
    UNUSED u8 filler[4];
    struct Object *sp18 = cur_obj_nearest_object_with_behavior(bhvStarDoor);

    switch (o->oAction) {
        case 0:
            cur_obj_become_tangible();
            if (o->oInteractStatus & (INT_STATUS_UNK16 | INT_STATUS_UNK17)) {
                o->oAction = 1;
            }
            if (sp18 != NULL && sp18->oAction != 0) {
                o->oAction = 1;
            }
            break;

        case 1:
            if (o->oTimer == 0 && (s16) o->oMoveAngleYaw >= 0) {
                cur_obj_play_sound_2(SOUND_GENERAL_STAR_DOOR_OPEN);
#if ENABLE_RUMBLE
                queue_rumble_data(35, 30);
#endif
            }
            cur_obj_become_intangible();
            o->oLeftVel = -8.0f;
            star_door_update_pos();
            if (o->oTimer > 15) {
                o->oAction++;
            }
            break;

        case 2:
            if (o->oTimer > 30) {
                o->oAction++;
            }
            break;

        case 3:
            if (o->oTimer == 0 && (s16) o->oMoveAngleYaw >= 0) {
                cur_obj_play_sound_2(SOUND_GENERAL_STAR_DOOR_CLOSE);
#if ENABLE_RUMBLE
                queue_rumble_data(35, 30);
#endif
            }
            o->oLeftVel = 8.0f;
            star_door_update_pos();
            if (o->oTimer > 15) {
                o->oAction++;
            }
            break;

        case 4:
            o->oInteractStatus = 0;
            o->oAction = 0;
            break;
    }
}

RECOMP_PATCH void water_level_pillar_undrained(void) {
    struct Object *otherWaterPillar;

    switch (o->oAction) {
        case 0:
            if (cur_obj_is_mario_ground_pounding_platform()) {
                o->oAction++;
                spawn_mist_particles();
            }
            break;

        case 1:
            if (o->oTimer < 4) {
                o->oPosY -= 20.0f;
            } else {
                o->oAction++;
            }
            break;

        case 2:
            otherWaterPillar = cur_obj_nearest_object_with_behavior(bhvWaterLevelPillar);
            if (otherWaterPillar != NULL) {
                if (otherWaterPillar->oAction < 2) {
                    o->oAction++;
                }
            }
            break;

        case 3:
            otherWaterPillar = cur_obj_nearest_object_with_behavior(bhvWaterLevelPillar);
            if (otherWaterPillar != NULL) {
                if (otherWaterPillar->oAction > 1) {
                    o->oAction++;
                    save_file_set_flags(SAVE_FLAG_MOAT_DRAINED);
                    play_puzzle_jingle();
                }
            }
            break;

        case 4:
            cur_obj_play_sound_1(SOUND_ENV_WATER_DRAIN);
            if (o->oTimer < 300) {
                gEnvironmentLevels[2] =
                    (s32) approach_f32_symmetric(gEnvironmentLevels[2], -2450.0f, 5.0f);
                gEnvironmentLevels[0] =
                    (s32) approach_f32_symmetric(gEnvironmentLevels[0], -2450.0f, 5.0f);
#if ENABLE_RUMBLE
                reset_rumble_timers_2(2);
#endif
            } else {
                o->oAction++;
            }
            break;

        case 5:
            break;
    }
}

RECOMP_PATCH void bhv_water_level_diamond_loop(void) {
    if (gEnvironmentRegions != NULL) {
        switch (o->oAction) {
            case WATER_LEVEL_DIAMOND_ACT_INIT:
                o->oFaceAngleYaw = 0;
                o->oWaterLevelTriggerTargetWaterLevel = (s32) o->oPosY;
                if (o->oTimer > 10) {
                    o->oAction++; // Sets to WATER_LEVEL_DIAMOND_ACT_IDLE
                }
                break;

            case WATER_LEVEL_DIAMOND_ACT_IDLE:
                if (obj_check_if_collided_with_object(o, gMarioObject) && !gWDWWaterLevelChanging) {
                    o->oAction++; // Sets to WATER_LEVEL_DIAMOND_ACT_CHANGE_WATER_LEVEL
                    gWDWWaterLevelChanging = TRUE;
                }
                break;

            case WATER_LEVEL_DIAMOND_ACT_CHANGE_WATER_LEVEL:
                o->oAngleVelYaw = 0;
                gEnvironmentLevels[0] = (s32) approach_f32_symmetric(
                    (f32) gEnvironmentLevels[0], (f32) o->oWaterLevelTriggerTargetWaterLevel, 10.0f);
                if (gEnvironmentLevels[0] == o->oWaterLevelTriggerTargetWaterLevel) {
                    if ((s16) o->oFaceAngleYaw == 0) {
                        o->oAction++; // Sets to WATER_LEVEL_DIAMOND_ACT_IDLE_SPINNING
                    } else {
                        o->oAngleVelYaw = 0x800;
                    }
                } else {
                    if (o->oTimer == 0) {
                        cur_obj_play_sound_2(SOUND_GENERAL_WATER_LEVEL_TRIG);
                    } else {
                        if (gEnvironmentLevels[0] > o->oWaterLevelTriggerTargetWaterLevel) {
                            cur_obj_play_sound_1(SOUND_ENV_WATER_DRAIN);
                        } else {
                            cur_obj_play_sound_1(SOUND_ENV_WATER_DRAIN); // same as above
                        }
                    }
                    o->oAngleVelYaw = 0x800;
#if ENABLE_RUMBLE
                    reset_rumble_timers_2(2);
#endif
                }
                break;

            case WATER_LEVEL_DIAMOND_ACT_IDLE_SPINNING:
                if (!obj_check_if_collided_with_object(o, gMarioObject)) {
                    gWDWWaterLevelChanging = FALSE;
                    o->oAction = WATER_LEVEL_DIAMOND_ACT_IDLE;
                    o->oAngleVelYaw = 0;
                }
                break;
        }

        o->oFaceAngleYaw += o->oAngleVelYaw;
    }
}
