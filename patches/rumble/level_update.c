// Shindou's rumble (../rumble.c) in the decomp's src/game/level_update.c: each function in it that rumbles in the
// Shindou edition, as the US game has it, with its #if ENABLE_RUMBLE lines compiled in (../rumble.h). Above them are
// the file's own declarations those functions use.

#include "../patches.h"
#include "sm64.h"
#include "seq_ids.h"
#include "dialog_ids.h"
#include "audio/external.h"
#include "game/level_update.h"
#include "game/game_init.h"
#include "game/level_update.h"
#include "game/main.h"
#include "engine/math_util.h"
#include "engine/graph_node.h"
#include "game/area.h"
#include "game/save_file.h"
#include "game/sound_init.h"
#include "game/mario.h"
#include "game/camera.h"
#include "game/object_list_processor.h"
#include "game/ingame_menu.h"
#include "game/obj_behaviors.h"
#include "game/save_file.h"
#include "game/debug_course.h"
#include "game/memory.h"
#include "eu_translation.h"
#include "segment_symbols.h"
#include "level_table.h"
#include "course_table.h"
#include "game/rumble_init.h"

#define PLAY_MODE_NORMAL 0
#define PLAY_MODE_PAUSED 2
#define PLAY_MODE_CHANGE_AREA 3
#define PLAY_MODE_CHANGE_LEVEL 4
#define WARP_TYPE_NOT_WARPING 0
#define WARP_TYPE_CHANGE_LEVEL 1
extern struct HudDisplay gHudDisplay;
extern FORCE_BSS s16 sCurrPlayMode;
extern FORCE_BSS s16 sTransitionTimer;
extern FORCE_BSS struct WarpDest sWarpDest;
extern FORCE_BSS s16 D_80339EE0;
extern FORCE_BSS s16 sDelayedWarpOp;
extern FORCE_BSS s8 sTimerRunning;
extern struct MarioState *gMarioState;
extern s8 sWarpCheckpointActive;
u32 pressed_pause(void);
void set_play_mode(s16 playMode);
void warp_area(void);
void warp_level(void);
void warp_credits(void);
void check_instant_warp(void);
void initiate_warp(s16 destLevel, s16 destArea, s16 destWarpNode, s32 arg3);
struct WarpNode *get_painting_warp_node(void);
void initiate_painting_warp(void);
s16 level_trigger_warp(struct MarioState *m, s32 warpOp);
void initiate_delayed_warp(void);
void update_hud_values(void);
void basic_update(UNUSED s16 *arg);
s32 play_mode_normal(void);
void level_set_transition(s16 length, void (*updateFunction)(s16 *));
s32 init_level(void);

#include "../rumble.h"

RECOMP_PATCH void initiate_painting_warp(void) {
    if (gCurrentArea->paintingWarpNodes != NULL && gMarioState->floor != NULL) {
        struct WarpNode warpNode;
        struct WarpNode *pWarpNode = get_painting_warp_node();

        if (pWarpNode != NULL) {
            if (gMarioState->action & ACT_FLAG_INTANGIBLE) {
                play_painting_eject_sound();
            } else if (pWarpNode->id != 0) {
                warpNode = *pWarpNode;

                if (!(warpNode.destLevel & 0x80)) {
                    sWarpCheckpointActive = check_warp_checkpoint(&warpNode);
                }

                initiate_warp(warpNode.destLevel & 0x7F, warpNode.destArea, warpNode.destNode, 0);
                check_if_should_set_warp_checkpoint(&warpNode);

                play_transition_after_delay(WARP_TRANSITION_FADE_INTO_COLOR, 30, 255, 255, 255, 45);
                level_set_transition(74, basic_update);

                set_mario_action(gMarioState, ACT_DISAPPEARED, 0);

                gMarioState->marioObj->header.gfx.node.flags &= ~GRAPH_RENDER_ACTIVE;

                play_sound(SOUND_MENU_STAR_SOUND, gGlobalSoundSource);
                fadeout_music(398);
#if ENABLE_RUMBLE
                queue_rumble_data(80, 70);
                func_sh_8024C89C(1);
#endif
            }
        }
    }
}

RECOMP_PATCH s32 play_mode_normal(void) {
    if (gCurrDemoInput != NULL) {
        print_intro_text();
        if (gPlayer1Controller->buttonPressed & END_DEMO) {
            level_trigger_warp(gMarioState,
                               gCurrLevelNum == LEVEL_PSS ? WARP_OP_DEMO_END : WARP_OP_DEMO_NEXT);
        } else if (!gWarpTransition.isActive && sDelayedWarpOp == WARP_OP_NONE
                   && (gPlayer1Controller->buttonPressed & START_BUTTON)) {
            level_trigger_warp(gMarioState, WARP_OP_DEMO_NEXT);
        }
    }

    warp_area();
    check_instant_warp();

    if (sTimerRunning && gHudDisplay.timer < 17999) {
        gHudDisplay.timer++;
    }

    area_update_objects();
    update_hud_values();

    if (gCurrentArea != NULL) {
        update_camera(gCurrentArea->camera);
    }

    initiate_painting_warp();
    initiate_delayed_warp();

    // If either initiate_painting_warp or initiate_delayed_warp initiated a
    // warp, change play mode accordingly.
    if (sCurrPlayMode == PLAY_MODE_NORMAL) {
        if (sWarpDest.type == WARP_TYPE_CHANGE_LEVEL) {
            set_play_mode(PLAY_MODE_CHANGE_LEVEL);
        } else if (sTransitionTimer != 0) {
            set_play_mode(PLAY_MODE_CHANGE_AREA);
        } else if (pressed_pause()) {
            lower_background_noise(1);
#if ENABLE_RUMBLE
            cancel_rumble();
#endif
            gCameraMovementFlags |= CAM_MOVE_PAUSE_SCREEN;
            set_play_mode(PLAY_MODE_PAUSED);
        }
    }

    return 0;
}

RECOMP_PATCH s32 init_level(void) {
    s32 val4 = FALSE;

    set_play_mode(PLAY_MODE_NORMAL);

    sDelayedWarpOp = WARP_OP_NONE;
    sTransitionTimer = 0;
    D_80339EE0 = 0;
    gHudDisplay.flags = gCurrCreditsEntry == NULL ? HUD_DISPLAY_DEFAULT : HUD_DISPLAY_NONE;
    sTimerRunning = FALSE;

    if (sWarpDest.type != WARP_TYPE_NOT_WARPING) {
        if (sWarpDest.nodeId >= WARP_NODE_CREDITS_MIN) {
            warp_credits();
        } else {
            warp_level();
        }
    } else {
        if (gPlayerSpawnInfos[0].areaIndex >= 0) {
            load_mario_area();
            init_mario();
        }

        if (gCurrentArea != NULL) {
            reset_camera(gCurrentArea->camera);

            if (gCurrDemoInput != NULL) {
                set_mario_action(gMarioState, ACT_IDLE, 0);
            } else if (!gDebugLevelSelect) {
                if (gMarioState->action != ACT_UNINITIALIZED) {
                    if (save_file_exists(gCurrSaveFileNum - 1)) {
                        set_mario_action(gMarioState, ACT_IDLE, 0);
                    } else {
                        set_mario_action(gMarioState, ACT_INTRO_CUTSCENE, 0);
                        val4 = TRUE;
                    }
                }
            }
        }

        if (val4) {
            play_transition(WARP_TRANSITION_FADE_FROM_COLOR, 0x5A, 0xFF, 0xFF, 0xFF);
        } else {
            play_transition(WARP_TRANSITION_FADE_FROM_STAR, 0x10, 0xFF, 0xFF, 0xFF);
        }

        if (gCurrDemoInput == NULL) {
            set_background_music(gCurrentArea->musicParam, gCurrentArea->musicParam2, 0);
        }
    }
#if ENABLE_RUMBLE
    if (gCurrDemoInput == NULL) {
        cancel_rumble();
    }
#endif

    if (gMarioState->action == ACT_INTRO_CUTSCENE) {
        sound_banks_disable(SEQ_PLAYER_SFX, SOUND_BANKS_DISABLED_DURING_INTRO_CUTSCENE);
    }

    return 1;
}
