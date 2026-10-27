// Shindou's rumble (../rumble.c) in the decomp's src/menu/file_select.c: each function in it that rumbles in the
// Shindou edition, as the US game has it, with its #if ENABLE_RUMBLE lines compiled in (../rumble.h). Above them are
// the file's own declarations those functions use.

#include "../patches.h"
#include "audio/external.h"
#include "behavior_data.h"
#include "dialog_ids.h"
#include "engine/behavior_script.h"
#include "engine/graph_node.h"
#include "engine/math_util.h"
#include "menu/file_select.h"
#include "game/area.h"
#include "game/game_init.h"
#include "game/ingame_menu.h"
#include "game/object_helpers.h"
#include "game/object_list_processor.h"
#include "game/print.h"
#include "game/save_file.h"
#include "game/segment2.h"
#include "game/segment7.h"
#include "game/spawn_object.h"
#include "game/rumble_init.h"
#include "sm64.h"
#include "eu_translation.h"
#include "game/segment7.h"

#define NUM_BUTTONS MENU_BUTTON_OPTION_MAX
extern struct Object *sMainMenuButtons[NUM_BUTTONS];
extern u8 sYesNoColor[2];
extern s8 sSelectedButtonID;
extern s8 sCurrentMenuLevel;
extern u8 sTextBaseAlpha;
extern f32 sCursorPos[];
extern s16 sCursorClickingTimer;
extern s8 sSelectedFileIndex;
extern s8 sFadeOutText;
extern s16 sMainMenuTimer;
extern s8 sSoundMode;
extern s8 sEraseYesNoHoverState;
extern s8 sAllFilesExist;
extern u8 textYes[];
extern u8 textNo[];
s32 check_clicked_button(s16 x, s16 y, f32 depth);
void render_score_menu_buttons(struct Object *scoreButton);
    #define SCORE_TIMER 31
void check_score_menu_clicked_buttons(struct Object *scoreButton);
void render_copy_menu_buttons(struct Object *copyButton);
    #define BUZZ_TIMER 21
void copy_action_file_button(struct Object *copyButton, s32 copyFileButtonID);
    #define ACTION_TIMER      31
    #define MAIN_RETURN_TIMER 31
void check_copy_menu_clicked_buttons(struct Object *copyButton);
void render_erase_menu_buttons(struct Object *eraseButton);
void erase_action_file_button(struct Object *eraseButton, s32 eraseFileButtonID);
void check_erase_menu_clicked_buttons(struct Object *eraseButton);
void render_sound_mode_menu_buttons(struct Object *soundModeButton);
void check_sound_mode_menu_clicked_buttons(struct Object *soundModeButton);
    #define SAVE_FILE_SOUND SOUND_MENU_STAR_SOUND_OKEY_DOKEY
void check_main_menu_clicked_buttons(void);
void bhv_menu_button_manager_loop(void);
    #define CURSOR_X (x + 70)
    #define MENU_ERASE_YES_MIN_X 140
    #define MENU_ERASE_YES_MAX_X 169
    #define MENU_ERASE_YES_NO_MIN_Y 191
    #define MENU_ERASE_YES_NO_MAX_Y 210
    #define MENU_ERASE_NO_MIN_X 189
    #define MENU_ERASE_NO_MAX_X 218
    #define MENU_ERASE_YES_X_OFFSET 56

#include "../rumble.h"

RECOMP_PATCH void exit_score_file_to_score_menu(struct Object *scoreFileButton, s32 scoreButtonIDWord) {
    // Narrowed here: IDO narrows a char or short argument in the function that takes it (CLAUDE.md, Narrow arguments).
    s8 scoreButtonID = (s8) scoreButtonIDWord;
    // Begin exit
    if (scoreFileButton->oMenuButtonState == MENU_BUTTON_STATE_FULLSCREEN
        && sCursorClickingTimer == 2) {
        play_sound(SOUND_MENU_CAMERA_ZOOM_OUT, gGlobalSoundSource);
#if ENABLE_RUMBLE
        queue_rumble_data(5, 80);
#endif
        scoreFileButton->oMenuButtonState = MENU_BUTTON_STATE_SHRINKING;
    }
    // End exit
    if (scoreFileButton->oMenuButtonState == MENU_BUTTON_STATE_DEFAULT) {
        sSelectedButtonID = scoreButtonID;
        if (sCurrentMenuLevel == MENU_LAYER_SUBMENU) {
            sCurrentMenuLevel = MENU_LAYER_MAIN;
        }
    }
}

RECOMP_PATCH void check_score_menu_clicked_buttons(struct Object *scoreButton) {
    if (scoreButton->oMenuButtonState == MENU_BUTTON_STATE_FULLSCREEN) {
        s32 buttonID;
        // Configure score menu button group
        for (buttonID = MENU_BUTTON_SCORE_MIN; buttonID < MENU_BUTTON_SCORE_MAX; buttonID++) {
            s16 buttonX = sMainMenuButtons[buttonID]->oPosX;
            s16 buttonY = sMainMenuButtons[buttonID]->oPosY;

            if (check_clicked_button(buttonX, buttonY, 22.0f) == TRUE && sMainMenuTimer >= SCORE_TIMER) {
                // If menu button clicked, select it
                if (buttonID == MENU_BUTTON_SCORE_RETURN || buttonID == MENU_BUTTON_SCORE_COPY_FILE
                    || buttonID == MENU_BUTTON_SCORE_ERASE_FILE) {
                    play_sound(SOUND_MENU_CLICK_FILE_SELECT, gGlobalSoundSource);
#if ENABLE_RUMBLE
                    queue_rumble_data(5, 80);
#endif
                    sMainMenuButtons[buttonID]->oMenuButtonState = MENU_BUTTON_STATE_ZOOM_IN_OUT;
                    sSelectedButtonID = buttonID;
                }
                else { // Check if a save file is clicked
                    if (sMainMenuTimer >= SCORE_TIMER) {
                        // If clicked in a existing save file, select it too see it's score
                        if (save_file_exists(buttonID - MENU_BUTTON_SCORE_MIN) == TRUE) {
                            play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gGlobalSoundSource);
#if ENABLE_RUMBLE
                            queue_rumble_data(5, 80);
#endif
                            sMainMenuButtons[buttonID]->oMenuButtonState = MENU_BUTTON_STATE_GROWING;
                            sSelectedButtonID = buttonID;
                        }
                        else {
                            // If clicked in a non-existing save file, play buzz sound
                            play_sound(SOUND_MENU_CAMERA_BUZZ, gGlobalSoundSource);
#if ENABLE_RUMBLE
                            queue_rumble_data(5, 80);
#endif
                            sMainMenuButtons[buttonID]->oMenuButtonState =
                                MENU_BUTTON_STATE_ZOOM_IN_OUT;
                            if (sMainMenuTimer >= SCORE_TIMER) {
                                sFadeOutText = TRUE;
                                sMainMenuTimer = 0;
                            }
                        }
                    }
                }
                sCurrentMenuLevel = MENU_LAYER_SUBMENU;
                break;
            }
        }
    }
}

RECOMP_PATCH void copy_action_file_button(struct Object *copyButton, s32 copyFileButtonID) {
    switch (copyButton->oMenuButtonActionPhase) {
        case COPY_PHASE_MAIN: // Copy Menu Main Phase
            if (sAllFilesExist == TRUE) { // Don't enable copy if all save files exists
                return;
            }
            if (save_file_exists(copyFileButtonID - MENU_BUTTON_COPY_MIN) == TRUE) {
                // If clicked in a existing save file, ask where it wants to copy
                play_sound(SOUND_MENU_CLICK_FILE_SELECT, gGlobalSoundSource);
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
                sMainMenuButtons[copyFileButtonID]->oMenuButtonState = MENU_BUTTON_STATE_ZOOM_IN;
                sSelectedFileIndex = copyFileButtonID - MENU_BUTTON_COPY_MIN;
                copyButton->oMenuButtonActionPhase = COPY_PHASE_COPY_WHERE;
                sFadeOutText = TRUE;
                sMainMenuTimer = 0;
            } else {
                // If clicked in a non-existing save file, play buzz sound
                play_sound(SOUND_MENU_CAMERA_BUZZ, gGlobalSoundSource);
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
                sMainMenuButtons[copyFileButtonID]->oMenuButtonState = MENU_BUTTON_STATE_ZOOM_IN_OUT;
                if (sMainMenuTimer >= BUZZ_TIMER) {
                    sFadeOutText = TRUE;
                    sMainMenuTimer = 0;
                }
            }
            break;
        case COPY_PHASE_COPY_WHERE: // Copy Menu "COPY IT TO WHERE?" Phase (after a file is selected)
            sMainMenuButtons[copyFileButtonID]->oMenuButtonState = MENU_BUTTON_STATE_ZOOM_IN_OUT;
            if (save_file_exists(copyFileButtonID - MENU_BUTTON_COPY_MIN) == FALSE) {
                // If clicked in a non-existing save file, copy the file
                play_sound(SOUND_MENU_STAR_SOUND, gGlobalSoundSource);
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
                copyButton->oMenuButtonActionPhase = COPY_PHASE_COPY_COMPLETE;
                sFadeOutText = TRUE;
                sMainMenuTimer = 0;
                save_file_copy(sSelectedFileIndex, copyFileButtonID - MENU_BUTTON_COPY_MIN);
                sMainMenuButtons[copyFileButtonID]->header.gfx.sharedChild =
                    gLoadedGraphNodes[MODEL_MAIN_MENU_MARIO_SAVE_BUTTON_FADE];
                sMainMenuButtons[copyFileButtonID - MENU_BUTTON_COPY_MIN]->header.gfx.sharedChild =
                    gLoadedGraphNodes[MODEL_MAIN_MENU_MARIO_SAVE_BUTTON_FADE];
            } else {
                // If clicked in a existing save file, play buzz sound
                if (copyFileButtonID == MENU_BUTTON_COPY_FILE_A + sSelectedFileIndex) {
                    play_sound(SOUND_MENU_CAMERA_BUZZ, gGlobalSoundSource);
#if ENABLE_RUMBLE
                    queue_rumble_data(5, 80);
#endif
                    sMainMenuButtons[MENU_BUTTON_COPY_FILE_A + sSelectedFileIndex]->oMenuButtonState =
                        MENU_BUTTON_STATE_ZOOM_OUT;
                    copyButton->oMenuButtonActionPhase = COPY_PHASE_MAIN;
                    sFadeOutText = TRUE;
                    return;
                }
                if (sMainMenuTimer >= BUZZ_TIMER) {
                    sFadeOutText = TRUE;
                    sMainMenuTimer = 0;
                }
            }
            break;
    }
}

RECOMP_PATCH void check_copy_menu_clicked_buttons(struct Object *copyButton) {
    if (copyButton->oMenuButtonState == MENU_BUTTON_STATE_FULLSCREEN) {
        s32 buttonID;
        // Configure copy menu button group
        for (buttonID = MENU_BUTTON_COPY_MIN; buttonID < MENU_BUTTON_COPY_MAX; buttonID++) {
            s16 buttonX = sMainMenuButtons[buttonID]->oPosX;
            s16 buttonY = sMainMenuButtons[buttonID]->oPosY;

            if (check_clicked_button(buttonX, buttonY, 22.0f) == TRUE) {
                // If menu button clicked, select it
                if (buttonID == MENU_BUTTON_COPY_RETURN || buttonID == MENU_BUTTON_COPY_CHECK_SCORE
                    || buttonID == MENU_BUTTON_COPY_ERASE_FILE) {
                    if (copyButton->oMenuButtonActionPhase == COPY_PHASE_MAIN) {
                        play_sound(SOUND_MENU_CLICK_FILE_SELECT, gGlobalSoundSource);
#if ENABLE_RUMBLE
                        queue_rumble_data(5, 80);
#endif
                        sMainMenuButtons[buttonID]->oMenuButtonState = MENU_BUTTON_STATE_ZOOM_IN_OUT;
                        sSelectedButtonID = buttonID;
                    }
                }
                else {
                    // Check if a file button is clicked to play a copy action
                    if (sMainMenuButtons[buttonID]->oMenuButtonState == MENU_BUTTON_STATE_DEFAULT
                        && sMainMenuTimer >= ACTION_TIMER) {
                        copy_action_file_button(copyButton, buttonID);
                    }
                }
                sCurrentMenuLevel = MENU_LAYER_SUBMENU;
                break;
            }
        }

        // After copy is complete, return to main copy phase
        if (copyButton->oMenuButtonActionPhase == COPY_PHASE_COPY_COMPLETE
            && sMainMenuTimer >= MAIN_RETURN_TIMER) {
            copyButton->oMenuButtonActionPhase = COPY_PHASE_MAIN;
            sMainMenuButtons[MENU_BUTTON_COPY_MIN + sSelectedFileIndex]->oMenuButtonState =
                MENU_BUTTON_STATE_ZOOM_OUT;
        }
    }
}

RECOMP_PATCH void erase_action_file_button(struct Object *eraseButton, s32 eraseFileButtonID) {
    switch (eraseButton->oMenuButtonActionPhase) {
        case ERASE_PHASE_MAIN: // Erase Menu Main Phase
            if (save_file_exists(eraseFileButtonID - MENU_BUTTON_ERASE_MIN) == TRUE) {
                // If clicked in a existing save file, ask if it wants to delete it
                play_sound(SOUND_MENU_CLICK_FILE_SELECT, gGlobalSoundSource);
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
                sMainMenuButtons[eraseFileButtonID]->oMenuButtonState = MENU_BUTTON_STATE_ZOOM_IN;
                sSelectedFileIndex = eraseFileButtonID - MENU_BUTTON_ERASE_MIN;
                eraseButton->oMenuButtonActionPhase = ERASE_PHASE_PROMPT;
                sFadeOutText = TRUE;
                sMainMenuTimer = 0;
            } else {
                // If clicked in a non-existing save file, play buzz sound
                play_sound(SOUND_MENU_CAMERA_BUZZ, gGlobalSoundSource);
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
                sMainMenuButtons[eraseFileButtonID]->oMenuButtonState = MENU_BUTTON_STATE_ZOOM_IN_OUT;

                if (sMainMenuTimer >= BUZZ_TIMER) {
                    sFadeOutText = TRUE;
                    sMainMenuTimer = 0;
                }
            }
            break;
        case ERASE_PHASE_PROMPT: // Erase Menu "SURE? YES NO" Phase (after a file is selected)
            if (eraseFileButtonID == MENU_BUTTON_ERASE_MIN + sSelectedFileIndex) {
                // If clicked in a existing save file, play click sound and zoom out button
                // Note: The prompt functions are actually called when the ERASE_MSG_PROMPT
                // message is displayed with print_erase_menu_prompt
                play_sound(SOUND_MENU_CLICK_FILE_SELECT, gGlobalSoundSource);
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
                sMainMenuButtons[MENU_BUTTON_ERASE_MIN + sSelectedFileIndex]->oMenuButtonState =
                    MENU_BUTTON_STATE_ZOOM_OUT;
                eraseButton->oMenuButtonActionPhase = ERASE_PHASE_MAIN;
                sFadeOutText = TRUE;
            }
            break;
    }
}

RECOMP_PATCH void check_erase_menu_clicked_buttons(struct Object *eraseButton) {
    if (eraseButton->oMenuButtonState == MENU_BUTTON_STATE_FULLSCREEN) {
        s32 buttonID;
        // Configure erase menu button group
        for (buttonID = MENU_BUTTON_ERASE_MIN; buttonID < MENU_BUTTON_ERASE_MAX; buttonID++) {
            s16 buttonX = sMainMenuButtons[buttonID]->oPosX;
            s16 buttonY = sMainMenuButtons[buttonID]->oPosY;

            if (check_clicked_button(buttonX, buttonY, 22.0f) == TRUE) {
                // If menu button clicked, select it
                if (buttonID == MENU_BUTTON_ERASE_RETURN || buttonID == MENU_BUTTON_ERASE_CHECK_SCORE
                    || buttonID == MENU_BUTTON_ERASE_COPY_FILE) {
                    if (eraseButton->oMenuButtonActionPhase == ERASE_PHASE_MAIN) {
                        play_sound(SOUND_MENU_CLICK_FILE_SELECT, gGlobalSoundSource);
#if ENABLE_RUMBLE
                        queue_rumble_data(5, 80);
#endif
                        sMainMenuButtons[buttonID]->oMenuButtonState = MENU_BUTTON_STATE_ZOOM_IN_OUT;
                        sSelectedButtonID = buttonID;
                    }
                }
                else {
                    // Check if a file button is clicked to play an erase action
                    if (sMainMenuTimer >= ACTION_TIMER) {
                        erase_action_file_button(eraseButton, buttonID);
                    }
                }
                sCurrentMenuLevel = MENU_LAYER_SUBMENU;
                break;
            }
        }
        // After erase is complete, return to main erase phase
        if (eraseButton->oMenuButtonActionPhase == ERASE_PHASE_MARIO_ERASED
            && sMainMenuTimer >= MAIN_RETURN_TIMER) {
            eraseButton->oMenuButtonActionPhase = ERASE_PHASE_MAIN;
            sMainMenuButtons[MENU_BUTTON_ERASE_MIN + sSelectedFileIndex]->oMenuButtonState =
                MENU_BUTTON_STATE_ZOOM_OUT;
        }
    }
}

RECOMP_PATCH void check_sound_mode_menu_clicked_buttons(struct Object *soundModeButton) {
    if (soundModeButton->oMenuButtonState == MENU_BUTTON_STATE_FULLSCREEN) {
        s32 buttonID;
        // Configure sound mode menu button group
        for (buttonID = MENU_BUTTON_OPTION_MIN; buttonID < MENU_BUTTON_OPTION_MAX; buttonID++) {
            s16 buttonX = sMainMenuButtons[buttonID]->oPosX;
            s16 buttonY = sMainMenuButtons[buttonID]->oPosY;

            if (check_clicked_button(buttonX, buttonY, 22.0f) == TRUE) {
                // If sound mode button clicked, select it and define sound mode
                // The check will always be true because of the group configured above (In JP & US)
                if (buttonID == MENU_BUTTON_STEREO || buttonID == MENU_BUTTON_MONO
                    || buttonID == MENU_BUTTON_HEADSET) {
                    if (soundModeButton->oMenuButtonActionPhase == SOUND_MODE_PHASE_MAIN) {
                        play_sound(SOUND_MENU_CLICK_FILE_SELECT, gGlobalSoundSource);
#if ENABLE_RUMBLE
                        queue_rumble_data(5, 80);
#endif
                        sMainMenuButtons[buttonID]->oMenuButtonState = MENU_BUTTON_STATE_ZOOM_IN_OUT;
#ifndef VERSION_EU
                        // Sound menu buttons don't return to Main Menu in EU
                        // because they don't have a case in bhv_menu_button_manager_loop
                        sSelectedButtonID = buttonID;
#endif
                        sSoundMode = buttonID - MENU_BUTTON_OPTION_MIN;
                        save_file_set_sound_mode(sSoundMode);
                    }
                }
#ifdef VERSION_EU
                // If language mode button clicked, select it and change language
                if (buttonID == MENU_BUTTON_LANGUAGE_ENGLISH || buttonID == MENU_BUTTON_LANGUAGE_FRENCH
                         || buttonID == MENU_BUTTON_LANGUAGE_GERMAN) {
                    if (soundModeButton->oMenuButtonActionPhase == SOUND_MODE_PHASE_MAIN) {
                        play_sound(SOUND_MENU_CLICK_FILE_SELECT, gGlobalSoundSource);
                        sMainMenuButtons[buttonID]->oMenuButtonState = MENU_BUTTON_STATE_ZOOM_IN_OUT;
                        sLanguageMode = buttonID - MENU_BUTTON_LANGUAGE_MIN;
                        eu_set_language(sLanguageMode);
                    }
                }
                // If neither of the buttons above are pressed, return to main menu
                if (buttonID == MENU_BUTTON_LANGUAGE_RETURN) {
                    play_sound(SOUND_MENU_CLICK_FILE_SELECT, gGlobalSoundSource);
                    sMainMenuButtons[buttonID]->oMenuButtonState = MENU_BUTTON_STATE_ZOOM_IN_OUT;
                    sSelectedButtonID = buttonID;
                }
#endif
                sCurrentMenuLevel = MENU_LAYER_SUBMENU;

                break;
            }
        }
    }
}

RECOMP_PATCH void check_main_menu_clicked_buttons(void) {
#ifdef VERSION_EU
    if (sMainMenuTimer >= 5) {
#endif
        // Sound mode menu is handled separately because the button ID for it
        // is not grouped with the IDs of the other submenus.
        if (check_clicked_button(sMainMenuButtons[MENU_BUTTON_SOUND_MODE]->oPosX,
                                sMainMenuButtons[MENU_BUTTON_SOUND_MODE]->oPosY, 200.0f) == TRUE) {
            sMainMenuButtons[MENU_BUTTON_SOUND_MODE]->oMenuButtonState = MENU_BUTTON_STATE_GROWING;
            sSelectedButtonID = MENU_BUTTON_SOUND_MODE;
        } else {
            // Main Menu buttons
            s8 buttonID;
            // Configure Main Menu button group
            for (buttonID = MENU_BUTTON_MAIN_MIN; buttonID < MENU_BUTTON_MAIN_MAX; buttonID++) {
                s16 buttonX = sMainMenuButtons[buttonID]->oPosX;
                s16 buttonY = sMainMenuButtons[buttonID]->oPosY;

                if (check_clicked_button(buttonX, buttonY, 200.0f) == TRUE) {
                    // If menu button clicked, select it
                    sMainMenuButtons[buttonID]->oMenuButtonState = MENU_BUTTON_STATE_GROWING;
                    sSelectedButtonID = buttonID;
                    break;
                }
            }
        }
#ifdef VERSION_EU
        // Open Options Menu if sOpenLangSettings is TRUE (It's TRUE when there's no saves)
        if (sOpenLangSettings == TRUE) {
            sMainMenuButtons[MENU_BUTTON_SOUND_MODE]->oMenuButtonState = MENU_BUTTON_STATE_GROWING;
            sSelectedButtonID = MENU_BUTTON_SOUND_MODE;
            sOpenLangSettings = FALSE;
        }
#endif

        // Play sound of the save file clicked
        switch (sSelectedButtonID) {
            case MENU_BUTTON_PLAY_FILE_A:
                play_sound(SAVE_FILE_SOUND, gGlobalSoundSource);
#if ENABLE_RUMBLE
                queue_rumble_data(60, 70);
                func_sh_8024C89C(1);
#endif
                break;
            case MENU_BUTTON_PLAY_FILE_B:
                play_sound(SAVE_FILE_SOUND, gGlobalSoundSource);
#if ENABLE_RUMBLE
                queue_rumble_data(60, 70);
                func_sh_8024C89C(1);
#endif
                break;
            case MENU_BUTTON_PLAY_FILE_C:
                play_sound(SAVE_FILE_SOUND, gGlobalSoundSource);
#if ENABLE_RUMBLE
                queue_rumble_data(60, 70);
                func_sh_8024C89C(1);
#endif
                break;
            case MENU_BUTTON_PLAY_FILE_D:
                play_sound(SAVE_FILE_SOUND, gGlobalSoundSource);
#if ENABLE_RUMBLE
                queue_rumble_data(60, 70);
                func_sh_8024C89C(1);
#endif
                break;
            // Play sound of the button clicked and render buttons of that menu.
            case MENU_BUTTON_SCORE:
                play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gGlobalSoundSource);
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
                render_score_menu_buttons(sMainMenuButtons[MENU_BUTTON_SCORE]);
                break;
            case MENU_BUTTON_COPY:
                play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gGlobalSoundSource);
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
                render_copy_menu_buttons(sMainMenuButtons[MENU_BUTTON_COPY]);
                break;
            case MENU_BUTTON_ERASE:
                play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gGlobalSoundSource);
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
                render_erase_menu_buttons(sMainMenuButtons[MENU_BUTTON_ERASE]);
                break;
            case MENU_BUTTON_SOUND_MODE:
                play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gGlobalSoundSource);
#if ENABLE_RUMBLE
                queue_rumble_data(5, 80);
#endif
                render_sound_mode_menu_buttons(sMainMenuButtons[MENU_BUTTON_SOUND_MODE]);
                break;
        }
#ifdef VERSION_EU
    }
#endif
}

RECOMP_PATCH void print_erase_menu_prompt(s32 xWord, s32 yWord) {
    // Narrowed here: IDO narrows a char or short argument in the function that takes it (CLAUDE.md, Narrow arguments).
    s16 x = (s16) xWord;
    s16 y = (s16) yWord;
    s16 colorTransTimer = gGlobalTimer * (1 << 12);

    s16 cursorX = sCursorPos[0] + CURSOR_X;
    s16 cursorY = sCursorPos[1] + 120.0f;

    // TODO: Merge IDO/GCC
    if (cursorX < MENU_ERASE_YES_MAX_X && cursorX >= MENU_ERASE_YES_MIN_X &&
#ifdef VERSION_CN
        (u16) (cursorY - MENU_ERASE_YES_NO_MIN_Y) < MENU_ERASE_YES_NO_MAX_Y - MENU_ERASE_YES_NO_MIN_Y
#else
        cursorY < MENU_ERASE_YES_NO_MAX_Y && cursorY >= MENU_ERASE_YES_NO_MIN_Y
#endif
    ) {
        // Fade "YES" string color but keep "NO" gray
        sYesNoColor[0] = sins(colorTransTimer) * 50.0f + 205.0f;
        sYesNoColor[1] = 150;
        sEraseYesNoHoverState = MENU_ERASE_HOVER_YES;
    } else if (cursorX < MENU_ERASE_NO_MAX_X && cursorX >= MENU_ERASE_NO_MIN_X &&
#ifdef VERSION_CN
        (u16) (cursorY - MENU_ERASE_YES_NO_MIN_Y) < MENU_ERASE_YES_NO_MAX_Y - MENU_ERASE_YES_NO_MIN_Y
#else
        cursorY < MENU_ERASE_YES_NO_MAX_Y && cursorY >= MENU_ERASE_YES_NO_MIN_Y
#endif
    ) {
        // Fade "NO" string color but keep "YES" gray
        sYesNoColor[0] = 150;
        sYesNoColor[1] = sins(colorTransTimer) * 50.0f + 205.0f;
        sEraseYesNoHoverState = MENU_ERASE_HOVER_NO;
    } else {
        // Don't fade both strings and keep them gray
        sYesNoColor[0] = 150;
        sYesNoColor[1] = 150;
        sEraseYesNoHoverState = MENU_ERASE_HOVER_NONE;
    }
    // If the cursor is clicked...
    if (sCursorClickingTimer == 2) {
        // ..and is hovering "YES", delete file
        if (sEraseYesNoHoverState == MENU_ERASE_HOVER_YES) {
            play_sound(SOUND_MARIO_WAAAOOOW, gGlobalSoundSource);
#if ENABLE_RUMBLE
            queue_rumble_data(5, 80);
#endif
            sMainMenuButtons[MENU_BUTTON_ERASE]->oMenuButtonActionPhase = ERASE_PHASE_MARIO_ERASED;
            sFadeOutText = TRUE;
            sMainMenuTimer = 0;
            save_file_erase(sSelectedFileIndex);
            sMainMenuButtons[MENU_BUTTON_ERASE_MIN + sSelectedFileIndex]->header.gfx.sharedChild =
                gLoadedGraphNodes[MODEL_MAIN_MENU_MARIO_NEW_BUTTON_FADE];
            sMainMenuButtons[sSelectedFileIndex]->header.gfx.sharedChild =
                gLoadedGraphNodes[MODEL_MAIN_MENU_MARIO_NEW_BUTTON_FADE];
            sEraseYesNoHoverState = MENU_ERASE_HOVER_NONE;
            // ..and is hovering "NO", return back to main phase
        } else if (sEraseYesNoHoverState == MENU_ERASE_HOVER_NO) {
            play_sound(SOUND_MENU_CLICK_FILE_SELECT, gGlobalSoundSource);
#if ENABLE_RUMBLE
            queue_rumble_data(5, 80);
#endif
            sMainMenuButtons[MENU_BUTTON_ERASE_MIN + sSelectedFileIndex]->oMenuButtonState =
                MENU_BUTTON_STATE_ZOOM_OUT;
            sMainMenuButtons[MENU_BUTTON_ERASE]->oMenuButtonActionPhase = ERASE_PHASE_MAIN;
            sFadeOutText = TRUE;
            sMainMenuTimer = 0;
            sEraseYesNoHoverState = MENU_ERASE_HOVER_NONE;
        }
    }

    // Print "YES NO" strings
    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    gDPSetEnvColor(gDisplayListHead++, sYesNoColor[0], sYesNoColor[0], sYesNoColor[0], sTextBaseAlpha);
    print_generic_string(x + MENU_ERASE_YES_X_OFFSET, y, LANGUAGE_ARRAY(textYes));
    gDPSetEnvColor(gDisplayListHead++, sYesNoColor[1], sYesNoColor[1], sYesNoColor[1], sTextBaseAlpha);
    print_generic_string(x + 98, y, LANGUAGE_ARRAY(textNo));
    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
}
