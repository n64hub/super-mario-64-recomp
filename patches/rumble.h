#ifndef __RUMBLE_H__
#define __RUMBLE_H__

// Shindou's rumble over the US game (rumble.c, rumble/*.c). The decomp compiles its rumble code in for VERSION_SH and
// VERSION_CN only (ENABLE_RUMBLE, include/config.h); each file in rumble/ includes this after the decomp's headers it
// needs, so they keep the US game's layouts (types.h gives struct Controller a port only with ENABLE_RUMBLE), and the
// functions copied below it compile their #if ENABLE_RUMBLE lines in.

#include "types.h"

#undef ENABLE_RUMBLE
#define ENABLE_RUMBLE 1

// rumble.c, Shindou's src/game/rumble_init.c.
extern s32 gRumblePakTimer;

void queue_rumble_data(s16 a0, s16 a1);
void func_sh_8024C89C(s16 a0);
u8 is_rumble_finished_and_queue_empty(void);
void reset_rumble_timers(void);
void reset_rumble_timers_2(s32 a0);
void func_sh_8024CA04(void);
void cancel_rumble(void);
void rumble_thread_update_vi(void);

#endif
