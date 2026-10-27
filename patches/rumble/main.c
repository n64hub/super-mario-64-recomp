// Shindou's rumble (../rumble.c) in the decomp's src/game/main.c: each function in it that rumbles in the Shindou
// edition, as the US game has it, with its #if ENABLE_RUMBLE lines compiled in (../rumble.h). Above them are the file's
// own declarations those functions use.

#include "../patches.h"
#include "sm64.h"
#include "audio/external.h"
#include "game/game_init.h"
#include "game/memory.h"
#include "game/sound_init.h"
#include "game/profiler.h"
#include "buffers/buffers.h"
#include "segments.h"
#include "segment_symbols.h"
#include "game/main.h"
#include "game/rumble_init.h"

extern struct VblankHandler *gVblankHandler1;
extern struct VblankHandler *gVblankHandler2;
extern struct SPTask *gActiveSPTask;
extern struct SPTask *sCurrentAudioSPTask;
extern struct SPTask *sCurrentDisplaySPTask;
extern s8 sAudioEnabled;
extern u32 gNumVblanks;
extern s8 gResetTimer;
void stub_main_3(void);
void receive_new_tasks(void);
void start_sptask(s32 taskType);
void interrupt_gfx_sptask(void);
void pretend_audio_sptask_done(void);
void handle_vblank(void);
void handle_sp_complete(void);

#include "../rumble.h"

RECOMP_PATCH void handle_vblank(void) {
    UNUSED u8 filler[4];

    stub_main_3();
    gNumVblanks++;
#if defined(VERSION_SH) || defined(VERSION_CN)
    if (gResetTimer > 0 && gResetTimer < 100) {
        gResetTimer++;
    }
#else
    if (gResetTimer > 0) {
        gResetTimer++;
    }
#endif

    receive_new_tasks();

    // First try to kick off an audio task. If the gfx task is currently
    // running, we need to asynchronously interrupt it -- handle_sp_complete
    // will pick up on what we're doing and start the audio task for us.
    // If there is already an audio task running, there is nothing to do.
    // If there is no audio task available, try a gfx task instead.
    if (sCurrentAudioSPTask != NULL) {
        if (gActiveSPTask != NULL) {
            interrupt_gfx_sptask();
        } else {
            profiler_log_vblank_time();
            if (sAudioEnabled) {
                start_sptask(M_AUDTASK);
            } else {
                pretend_audio_sptask_done();
            }
        }
    } else {
        if (gActiveSPTask == NULL && sCurrentDisplaySPTask != NULL
            && sCurrentDisplaySPTask->state != SPTASK_STATE_FINISHED) {
            profiler_log_gfx_time(TASKS_QUEUED);
            start_sptask(M_GFXTASK);
        }
    }
#if ENABLE_RUMBLE
    rumble_thread_update_vi();
#endif

    // Notify the game loop about the vblank.
    if (gVblankHandler1 != NULL) {
        osSendMesg(gVblankHandler1->queue, gVblankHandler1->msg, OS_MESG_NOBLOCK);
    }
    if (gVblankHandler2 != NULL) {
        osSendMesg(gVblankHandler2->queue, gVblankHandler2->msg, OS_MESG_NOBLOCK);
    }
}
