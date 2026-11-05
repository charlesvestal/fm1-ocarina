/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* OCA-1: an ocarina for the FM-1, with a songbook of the twelve songs a game listens for. The app's half of the
 * contract with fm1-base (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "oca.h"

#define APP_NAME "OCA-1"
#define APP_SPLASH_RGB RGB(86, 150, 214)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */

/* the store: the settings at 0xFC000, the panel calibration (the platform's) at 0xDC000 */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the voice, then the saved settings (or defaults) */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);                  /* the first screen, after the splash */
void app_frame(void);                    /* the UI: input and a redraw, about 60 times a second */
void app_input_only(void);               /* between frames: input only */
void app_say(const char *a, const char *b);   /* a short message on the screen and to a screen reader */

/* the host simulator's checks: song, step, heard, note, octave, dirty, auto, breath, vibrato, glide, tune */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
#define APP_HOST_SPEAKDUMP
void app_speak_dump(void);
