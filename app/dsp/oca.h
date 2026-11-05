/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* OCA-1's voice: one ocarina, nearly a pure sine with a little breath, played one note at a time (a microphone
 * that listens for notes hears one clean pitch). */
#pragma once
#include <stdint.h>

#define OCA_SR 44100.0f

typedef struct {
    float breath;                        /* 0..1: the noise of the air beside the tone */
    float vibrato;                       /* 0..1: up to 30 cents, after the note has sounded a moment */
    float glide;                         /* 0..1: up to 150 ms from one note to the next, played legato */
    float tune;                          /* cents, -50..+50 */
} oca_params_t;

void oca_init(void);
void oca_set(const oca_params_t *p);
void oca_note_on(int32_t note, float vel);   /* MIDI note; legato from a sounding note */
void oca_note_off(void);
int oca_sounding(void);                  /* still heard (the release included) */
void oca_render(float *out, uint32_t n); /* mono, about -1..1 */
float oca_note_hz(int32_t note, float tune);   /* the note's frequency, tuned */
