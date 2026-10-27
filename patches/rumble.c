// Shindou's rumble, the README's Rumble: the Rumble Pak code of Super Mario 64's Shindou Edition (the decomp's
// VERSION_SH) over the US game, which has none.
//
// This is the decomp's src/game/rumble_init.c without its thread. Shindou's game queues a rumble (queue_rumble_data:
// how many frames, how strong) or keeps one going (reset_rumble_timers) at about 130 places, which rumble/*.c are: each
// file there holds the US functions of one of the decomp's files that Shindou rumbles in, copied as they are with their
// #if ENABLE_RUMBLE lines compiled in (rumble.h), and nothing else changed. Shindou's retrace handler wakes a thread of
// its own that turns the queue into the motor's pulses once a retrace; here handle_vblank (rumble/main.c) runs that
// step itself (rumble_thread_update_vi). The motor is the runtime's osMotorInit, osMotorStart and osMotorStop, which
// main.cpp's Rumble Pak in controller 1 sends to recompinput: it drives the gamepad at the General tab's Rumble
// Strength, smoothing the pulses as a Rumble Pak's motor does, and Rumble Strength 0 keeps the gamepad still.
//
// Not ported: the lock Shindou takes around each motor command and each EEPROM access (block_until_rumble_pak_free in
// save_file.c and game_init.c), which keeps them apart on the console's serial bus and is nothing to the runtime; and
// the controller's port (game_init.c's init_controllers), which the US game doesn't keep: controller 1 is channel 0.

#include "patches.h"
#include "game/game_init.h"
#include "game/main.h"
#include "rumble.h"

struct RumbleData {
    u8 unk00;
    u8 unk01;
    s16 unk02;
    s16 unk04;
};

struct StructSH8031D9B0 {
    s16 unk00;
    s16 unk02;
    s16 unk04;
    s16 unk06;
    s16 unk08;
    s16 unk0A;
    s16 unk0C;
    s16 unk0E;
};

static OSPfs gRumblePakPfs;

static struct RumbleData gRumbleDataQueue[3];
static struct StructSH8031D9B0 gCurrRumbleSettings;

static s32 sRumblePakThreadActive = FALSE;
static s32 sRumblePakActive = FALSE;
static s32 sRumblePakErrorCount = 0;
s32 gRumblePakTimer = 0;

static void start_rumble(void) {
    if (!sRumblePakActive) {
        return;
    }

    if (!osMotorStart(&gRumblePakPfs)) {
        sRumblePakErrorCount = 0;
    } else {
        sRumblePakErrorCount++;
    }
}

static void stop_rumble(void) {
    if (!sRumblePakActive) {
        return;
    }

    if (!osMotorStop(&gRumblePakPfs)) {
        sRumblePakErrorCount = 0;
    } else {
        sRumblePakErrorCount++;
    }
}

static void update_rumble_pak(void) {
    if (gResetTimer > 0) {
        stop_rumble();
        return;
    }

    if (gCurrRumbleSettings.unk08 > 0) {
        gCurrRumbleSettings.unk08--;
        start_rumble();
    } else if (gCurrRumbleSettings.unk04 > 0) {
        gCurrRumbleSettings.unk04--;

        gCurrRumbleSettings.unk02 -= gCurrRumbleSettings.unk0E;
        if (gCurrRumbleSettings.unk02 < 0) {
            gCurrRumbleSettings.unk02 = 0;
        }

        if (gCurrRumbleSettings.unk00 == 1) {
            start_rumble();
        } else if (gCurrRumbleSettings.unk06 >= 0x100) {
            gCurrRumbleSettings.unk06 -= 0x100;
            start_rumble();
        } else {
            gCurrRumbleSettings.unk06 +=
                ((gCurrRumbleSettings.unk02 * gCurrRumbleSettings.unk02 * gCurrRumbleSettings.unk02) / (1 << 9)) + 4;

            stop_rumble();
        }
    } else {
        gCurrRumbleSettings.unk04 = 0;

        if (gCurrRumbleSettings.unk0A >= 5) {
            start_rumble();
        } else if ((gCurrRumbleSettings.unk0A >= 2) && (gNumVblanks % gCurrRumbleSettings.unk0C == 0)) {
            start_rumble();
        } else {
            stop_rumble();
        }
    }

    if (gCurrRumbleSettings.unk0A > 0) {
        gCurrRumbleSettings.unk0A--;
    }
}

static void update_rumble_data_queue(void) {
    if (gRumbleDataQueue[0].unk00) {
        gCurrRumbleSettings.unk06 = 0;
        gCurrRumbleSettings.unk08 = 4;
        gCurrRumbleSettings.unk00 = gRumbleDataQueue[0].unk00;
        gCurrRumbleSettings.unk04 = gRumbleDataQueue[0].unk02;
        gCurrRumbleSettings.unk02 = gRumbleDataQueue[0].unk01;
        gCurrRumbleSettings.unk0E = gRumbleDataQueue[0].unk04;
    }

    gRumbleDataQueue[0] = gRumbleDataQueue[1];
    gRumbleDataQueue[1] = gRumbleDataQueue[2];

    gRumbleDataQueue[2].unk00 = 0;
}

void queue_rumble_data(s16 a0, s16 a1) {
    if (gCurrDemoInput != NULL) {
        return;
    }

    if (a1 > 70) {
        gRumbleDataQueue[2].unk00 = 1;
    } else {
        gRumbleDataQueue[2].unk00 = 2;
    }

    gRumbleDataQueue[2].unk01 = a1;
    gRumbleDataQueue[2].unk02 = a0;
    gRumbleDataQueue[2].unk04 = 0;
}

void func_sh_8024C89C(s16 a0) {
    gRumbleDataQueue[2].unk04 = a0;
}

u8 is_rumble_finished_and_queue_empty(void) {
    if (gCurrRumbleSettings.unk08 + gCurrRumbleSettings.unk04 >= 4) {
        return FALSE;
    }

    if (gRumbleDataQueue[0].unk00 != 0) {
        return FALSE;
    }

    if (gRumbleDataQueue[1].unk00 != 0) {
        return FALSE;
    }

    if (gRumbleDataQueue[2].unk00 != 0) {
        return FALSE;
    }

    return TRUE;
}

void reset_rumble_timers(void) {
    if (gCurrDemoInput != NULL) {
        return;
    }

    if (gCurrRumbleSettings.unk0A == 0) {
        gCurrRumbleSettings.unk0A = 7;
    }

    if (gCurrRumbleSettings.unk0A < 4) {
        gCurrRumbleSettings.unk0A = 4;
    }

    gCurrRumbleSettings.unk0C = 7;
}

void reset_rumble_timers_2(s32 a0) {
    if (gCurrDemoInput != NULL) {
        return;
    }

    if (gCurrRumbleSettings.unk0A == 0) {
        gCurrRumbleSettings.unk0A = 7;
    }

    if (gCurrRumbleSettings.unk0A < 4) {
        gCurrRumbleSettings.unk0A = 4;
    }

    if (a0 == 4) {
        gCurrRumbleSettings.unk0C = 1;
    }

    if (a0 == 3) {
        gCurrRumbleSettings.unk0C = 2;
    }

    if (a0 == 2) {
        gCurrRumbleSettings.unk0C = 3;
    }

    if (a0 == 1) {
        gCurrRumbleSettings.unk0C = 4;
    }

    if (a0 == 0) {
        gCurrRumbleSettings.unk0C = 5;
    }
}

void func_sh_8024CA04(void) {
    if (gCurrDemoInput != NULL) {
        return;
    }

    gCurrRumbleSettings.unk0A = 4;
    gCurrRumbleSettings.unk0C = 4;
}

void cancel_rumble(void) {
    sRumblePakActive = osMotorInit(&gSIEventMesgQueue, &gRumblePakPfs, 0) == 0;

    if (sRumblePakActive) {
        osMotorStop(&gRumblePakPfs);
    }

    gRumbleDataQueue[0].unk00 = 0;
    gRumbleDataQueue[1].unk00 = 0;
    gRumbleDataQueue[2].unk00 = 0;

    gCurrRumbleSettings.unk04 = 0;
    gCurrRumbleSettings.unk0A = 0;

    gRumblePakTimer = 0;
}

// Shindou's thread6_rumble_loop, one pass a retrace: what its thread does each time the retrace wakes it.
void rumble_thread_update_vi(void) {
    if (!sRumblePakThreadActive) {
        cancel_rumble();
        sRumblePakThreadActive = TRUE;
    }

    update_rumble_data_queue();
    update_rumble_pak();

    if (sRumblePakActive) {
        if (sRumblePakErrorCount >= 30) {
            sRumblePakActive = FALSE;
        }
    } else if (gNumVblanks % 60 == 0) {
        sRumblePakActive = osMotorInit(&gSIEventMesgQueue, &gRumblePakPfs, 0) == 0;
        sRumblePakErrorCount = 0;
    }

    if (gRumblePakTimer > 0) {
        gRumblePakTimer--;
    }
}
