/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* OCA-1's voice (oca.h). The main loop asks for notes, the audio interrupt plays them: a request is a note and a
 * count, so the render sees each press once, whenever it comes. */
#include "oca.h"
#include "fastmath.h"

static oca_params_t par = {0.35f, 0.3f, 0.0f, 0.0f};
static volatile int32_t req_note = -1;   /* the note asked for, -1 = released */
static volatile float req_vel = 1.0f;
static volatile uint32_t req_count;      /* presses asked for */

static struct {
    uint32_t seen;                       /* presses played */
    int32_t note;                        /* the note sounding (or fading), -1 none yet */
    float pitch, target;                 /* in semitones from A4, gliding to target */
    float phase, env, vel, chiff;
    float vib_phase, vib_amt;            /* the vibrato's depth comes in after the note starts */
    uint32_t held;                       /* samples since the note began */
    float hp, lp;                        /* the breath's band */
    uint32_t seed;
} v;

float oca_note_hz(int32_t note, float tune)
{
    return 440.0f * fm_exp2f(((float)(note - 69) + tune * 0.01f) * (1.0f / 12.0f));
}

void oca_init(void)
{
    v.note = -1;
    v.env = 0.0f;
    v.seed = 0x1234567u;
}

void oca_set(const oca_params_t *p) { par = *p; }

void oca_note_on(int32_t note, float vel)
{
    req_vel = vel;
    req_note = note;
    req_count++;
}

void oca_note_off(void) { req_note = -1; }

int oca_sounding(void) { return v.env > 0.0005f || req_note >= 0; }

static inline float noise(void)
{
    v.seed = v.seed * 1664525u + 1013904223u;
    return (float)(int32_t)v.seed * (1.0f / 2147483648.0f);
}

#define SUB 16u                          /* pitch and vibrato move every 16 samples */

void oca_render(float *out, uint32_t n)
{
    int32_t note = req_note;
    uint32_t count = req_count, i, j;
    float gate, glide_k, att = 1.0f / (0.012f * OCA_SR), rel = 1.0f / (0.035f * OCA_SR);
    if (count != v.seen) {               /* a press: a new note, or the next one played legato */
        int legato = v.env > 0.05f && v.note >= 0;
        v.seen = count;
        if (note >= 0) {
            v.target = (float)(note - 69);
            if (!legato || par.glide <= 0.0f)
                v.pitch = v.target;
            if (legato)
                v.env *= 0.55f;          /* tongued: a dip the ear (and a listening game) hears as a new note */
            v.note = note;
            v.vel = 0.55f + 0.45f * req_vel;
            v.chiff = legato ? 0.5f : 1.0f;
            v.held = 0;
            v.vib_amt = 0.0f;
        }
    }
    gate = note >= 0 ? 1.0f : 0.0f;
    glide_k = par.glide > 0.0f ? (float)SUB / (par.glide * 0.15f * OCA_SR) : 1.0f;
    if (glide_k > 1.0f)
        glide_k = 1.0f;
    for (i = 0; i < n; i += SUB) {
        float inc, vib, m = (n - i < SUB) ? (float)(n - i) : (float)SUB;
        v.pitch += (v.target - v.pitch) * glide_k;
        if (v.held > (uint32_t)(0.25f * OCA_SR))   /* vibrato: after a quarter second, in over half a second */
            v.vib_amt = fm_minf(1.0f, v.vib_amt + m / (0.5f * OCA_SR));
        v.vib_phase += 5.5f * m / OCA_SR;
        if (v.vib_phase >= 1.0f)
            v.vib_phase -= 1.0f;
        vib = fm_sin_turns(v.vib_phase) * v.vib_amt * par.vibrato * 0.30f;
        inc = 440.0f * fm_exp2f((v.pitch + vib + par.tune * 0.01f) * (1.0f / 12.0f)) / OCA_SR;
        for (j = i; j < n && j < i + SUB; j++) {
            float s, b;
            v.env += (gate - v.env) * (gate > v.env ? att : rel);
            v.chiff *= 0.9985f;          /* the attack's puff of air: ~15 ms */
            v.phase += inc;
            if (v.phase >= 1.0f)
                v.phase -= 1.0f;
            s = fm_sin_turns(v.phase) + 0.10f * fm_sin_turns(2.0f * v.phase) + 0.03f * fm_sin_turns(3.0f * v.phase);
            b = noise();                 /* the breath: noise in a band around 1-4 kHz */
            v.hp += (b - v.hp) * 0.14f;
            v.lp += ((b - v.hp) - v.lp) * 0.45f;
            s = 0.78f * s + v.lp * (par.breath * 0.22f + v.chiff * 0.35f);
            out[j] = s * v.env * v.vel;
        }
        v.held += (uint32_t)m;
    }
    v.env = fm_flush(v.env);
}
