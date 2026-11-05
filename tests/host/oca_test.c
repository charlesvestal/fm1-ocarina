/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* OCA-1's voice on the computer: the five song notes come out at their pitch (a game listening for them needs the
 * notes, not near them), the tune knob moves them by its cents, a note stops when let go, and nothing clips. */
#include <stdio.h>
#include <stdlib.h>
#include "oca.h"

#define N 22050
static float buf[N];
static int fails;

static void check(int ok, const char *what)
{
    printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    fails += !ok;
}

/* the pitch over the steady part: the lag with the least difference (YIN's), refined between samples */
static float pitch(const float *x, int n)
{
    int lag, best = 0, lo = 20, hi = 300;
    float d[301], cum = 0.0f, bestv = 1e30f;
    for (lag = 1; lag <= hi; lag++) {
        float s = 0.0f;
        int i;
        for (i = 0; i < n - hi; i++) {
            float e = x[i] - x[i + lag];
            s += e * e;
        }
        cum += s;
        d[lag] = s * (float)lag / cum;
    }
    for (lag = lo; lag < hi; lag++)
        if (d[lag] < 0.1f && d[lag] < d[lag + 1]) {
            best = lag;
            break;
        }
    if (!best)
        for (lag = lo; lag < hi; lag++)
            if (d[lag] < bestv)
                bestv = d[best = lag];
    {
        float a = d[best - 1], b = d[best], c = d[best + 1];
        return OCA_SR / ((float)best + 0.5f * (a - c) / (a - 2.0f * b + c));
    }
}

static float cents(float f, float ref)
{
    float r = f / ref, c = 0.0f;
    while (r > 1.0005f) { r /= 1.0005778f; c += 1.0f; }   /* 1 cent steps: no libm needed */
    while (r < 0.9995f) { r *= 1.0005778f; c -= 1.0f; }
    return c + (r - 1.0f) / 0.0005778f;
}

static void note_test(int note, float tune, float breath)
{
    oca_params_t p = {breath, 0.0f, 0.0f, tune};
    char what[96];
    float f, want = oca_note_hz(note, tune), peak = 0.0f, c;
    int i;
    oca_init();
    oca_set(&p);
    oca_note_on(note, 1.0f);
    oca_render(buf, N);
    for (i = 0; i < N; i++)
        peak = buf[i] > peak ? buf[i] : -buf[i] > peak ? -buf[i] : peak;
    f = pitch(buf + 4410, 4096);             /* after the attack */
    c = cents(f, want);
    snprintf(what, sizeof what, "note %d tune %+.0f breath %.1f: %.1f Hz, want %.1f (%+.1f cents), peak %.2f", note,
             (double)tune, (double)breath, (double)f, (double)want, (double)c, (double)peak);
    check(c > -3.0f && c < 3.0f && peak > 0.4f && peak < 1.0f, what);
}

int main(void)
{
    static const int NOTES[5] = {74, 77, 81, 83, 86};
    int i;
    float tail = 0.0f;
    check(oca_note_hz(69, 0.0f) > 439.9f && oca_note_hz(69, 0.0f) < 440.1f, "A4 is 440 Hz");
    for (i = 0; i < 5; i++)
        note_test(NOTES[i], 0.0f, 0.0f);
    for (i = 0; i < 5; i++)
        note_test(NOTES[i], 0.0f, 0.3f);
    note_test(74, 20.0f, 0.3f);
    note_test(86, -35.0f, 0.3f);
    oca_note_off();                          /* let go: silent within 0.3 s */
    oca_render(buf, 13230);
    for (i = 13230 - 441; i < 13230; i++)
        tail = buf[i] > tail ? buf[i] : -buf[i] > tail ? -buf[i] : tail;
    check(tail < 0.001f && !oca_sounding(), "silent after the release");
    printf(fails ? "oca: %d failed\n" : "oca: all passed\n", fails);
    return fails != 0;
}
