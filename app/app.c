/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* OCA-1: an ocarina on the keys, one note at a time, loud and clean enough for a game listening through its
 * microphone. The songbook shows a song as the game's five buttons; the keys to play light up in turn, PLAY plays
 * it, and any song played is named. Part of the unity build (device/unity.c, host/plat_host.c), after the platform:
 * gfx.c and announce.c are in scope. */
#include "fastmath.h"
#include "songs.h"

#define SET_MAGIC 0x3141434Fu            /* "OCA1": the settings record; another magic loads defaults */
typedef struct {
    uint32_t magic;
    int8_t octave;                       /* -1 .. +1: the keys from F4 at 0 */
    uint8_t song;                        /* the songbook's page */
    uint8_t breath, vibrato, glide;      /* 0..10 */
    int8_t tune;                         /* cents, -50..+50 */
    uint8_t rsv[2];
} settings_t;
static settings_t set;
static uint8_t set_dirty;                /* changed since the last save: SAVE is lit */

#define KEY0 65                          /* key 0 at octave 0: F4; D5 .. D6, the songs' notes, are white keys */

/* the colours: paper, ink, and the game's two button colours */
#define C_PAPER RGB(250, 245, 230)
#define C_CARD RGB(240, 232, 212)
#define C_INK RGB(46, 52, 64)
#define C_SOFT RGB(140, 138, 128)
#define C_FAINT RGB(214, 206, 186)
#define C_ABLUE RGB(64, 112, 214)
#define C_CYELLOW RGB(238, 186, 36)
#define C_GOOD RGB(52, 150, 96)

/* ------------------------------------------------------------------ sound --- */
static float mono[APP_BLOCK];
static float gain_now;                   /* glides to MASTER: no step at a block's edge */

static void midi_in(void)                /* audio interrupt: notes from USB or TRS play the ocarina too */
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xF0u, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24;
        static int32_t held = -1;
        if (st == 0x90u && d2) {
            held = (int32_t)d1;
            oca_note_on(held, (float)d2 / 127.0f);
        } else if ((st == 0x80u || st == 0x90u) && (int32_t)d1 == held) {
            held = -1;
            oca_note_off();
        }
    }
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, done = 0;
    float gain = (float)master_q12 / 4096.0f * 7000000.0f;
    midi_in();
    while (done < n) {
        uint32_t m = n - done > APP_BLOCK ? APP_BLOCK : n - done;
        oca_render(mono, m);
        for (i = 0; i < m; i++) {
            int32_t s;
            gain_now += (gain - gain_now) * 0.004f;
            s = (int32_t)(mono[i] * gain_now);
            lr[2 * (done + i)] = s;
            lr[2 * (done + i) + 1] = s;
        }
        done += m;
    }
}

void app_panic(void) { oca_note_off(); }

static void params_send(void)
{
    oca_params_t p;
    p.breath = (float)set.breath * 0.1f;
    p.vibrato = (float)set.vibrato * 0.1f;
    p.glide = (float)set.glide * 0.1f;
    p.tune = (float)set.tune;
    oca_set(&p);
}

/* --------------------------------------------------------------- settings --- */
static void settings_defaults(void)
{
    set.magic = SET_MAGIC;
    set.octave = 0;
    set.song = 0;
    set.breath = 3;
    set.vibrato = 3;
    set.glide = 0;
    set.tune = 0;
}

void app_init(void)
{
    oca_init();
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC
        || set.octave < -1 || set.octave > 1 || set.song >= NSONGS || set.breath > 10u || set.vibrato > 10u
        || set.glide > 10u || set.tune < -50 || set.tune > 50)
        settings_defaults();
    params_send();
}

/* --------------------------------------------------------------------- UI --- */
static struct {
    char msg[2][24];                     /* a short message over the header */
    uint32_t msg_until;
    uint16_t msg_rgb;
    uint32_t keys, btn;                  /* held at the last input */
    uint8_t order[NKEYS];                /* the keys held, oldest first: the newest one sounds */
    uint32_t norder;
    int32_t sounding;                    /* the note sounding (key or PLAY), -1 none */
    uint8_t hist[SONG_MAX];              /* the last buttons played, newest last (NBUTTONS: not a song note) */
    uint8_t step;                        /* how far into the songbook's song the playing has come */
    int8_t heard;                        /* the song heard last, -1 none */
    uint32_t heard_at;
    int8_t auto_i;                       /* PLAY: the note of the song playing, -1 not playing */
    uint32_t auto_t;
    uint8_t redraw;
} ui = {.sounding = -1, .heard = -1, .auto_i = -1};

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b)     /* signed, no padding */
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void note_name(int32_t note, char *b)
{
    static const char *const N[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    char o[12];
    b[0] = 0;
    text_cat(b, N[note % 12]);
    int_text(note / 12 - 1, o);
    text_cat(b, o);
}

/* a message: A and B on the screen for a moment, SPOKEN (or A and B) to a screen reader */
static void tell(const char *a, const char *b, const char *spoken, uint16_t rgb, uint32_t flags)
{
    uint32_t i;
    char t[64];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1500u;
    ui.msg_rgb = rgb;
    ui.redraw = 1;
    if (!spoken) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        spoken = t;
    }
    ann_say(spoken, 0, flags);
}

void app_say(const char *a, const char *b) { tell(a, b, 0, C_ABLUE, ANN_NOW); }

/* the spoken song: "ZELDA'S LULLABY, 1 OF 12. LEFT, UP, RIGHT, LEFT, UP, RIGHT" */
static void song_words(int32_t s, char *t)
{
    char n[8];
    uint32_t i;
    t[0] = 0;
    text_cat(t, SONGS[s].name);
    text_cat(t, ", ");
    int_text(s + 1, n);
    text_cat(t, n);
    text_cat(t, " OF ");
    int_text(NSONGS, n);
    text_cat(t, n);
    text_cat(t, ".");
    for (i = 0; i < SONGS[s].n; i++) {
        text_cat(t, i ? ", " : " ");
        text_cat(t, BUTTON_WORD[SONGS[s].b[i]]);
    }
}

static const char *const KNOB_NAME[4] = {"BREATH", "VIBRATO", "GLIDE", "TUNE"};

static void knob_value(int k, char *t)
{
    int32_t v = k == 0 ? set.breath : k == 1 ? set.vibrato : k == 2 ? set.glide : set.tune;
    char n[8];
    t[0] = 0;
    if (k == 3 && v > 0)
        text_cat(t, "+");
    int_text(v, n);
    text_cat(t, n);
    if (k == 3)
        text_cat(t, "C");
}

static void knob_words(int k, char *t)       /* "TUNE, 20 CENTS SHARP", "BREATH 3" */
{
    char n[8];
    t[0] = 0;
    text_cat(t, KNOB_NAME[k]);
    if (k == 3) {
        if (!set.tune) {
            text_cat(t, ", A 440");
            return;
        }
        int_text(set.tune < 0 ? -set.tune : set.tune, n);
        text_cat(t, ", ");
        text_cat(t, n);
        text_cat(t, set.tune < 0 ? " CENTS FLAT" : " CENTS SHARP");
        return;
    }
    text_cat(t, " ");
    knob_value(k, n);
    text_cat(t, n);
}

static const char *octave_word(void) { return set.octave < 0 ? "LOW" : set.octave > 0 ? "HIGH" : "MIDDLE"; }

/* the whole screen, for a screen reader that asks */
static void where_words(char *t)
{
    char s[160];
    uint32_t k;
    t[0] = 0;
    text_cat(t, APP_NAME ". ");
    song_words(set.song, s);
    text_cat(t, s);
    text_cat(t, ". OCTAVE ");
    text_cat(t, octave_word());
    for (k = 0; k < 4u; k++) {
        knob_words((int)k, s);
        text_cat(t, ". ");
        text_cat(t, s);
    }
}

static void speak(void)
{
    if (ann_on() && ann_where()) {
        char t[ANN_MAX + 64];
        where_words(t);
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

/* --------------------------------------------------------------- playing --- */
static int32_t button_of(int32_t note)       /* the song button a note is (at the octave), NBUTTONS none */
{
    int32_t b;
    for (b = 0; b < NBUTTONS; b++)
        if (BUTTON_NOTE[b] + 12 * set.octave == note)
            return b;
    return NBUTTONS;
}

/* a note begun (a key or PLAY): the songbook's song moves on, and a song the last notes make is named */
static void played(int32_t note)
{
    const song_t *g = &SONGS[set.song];
    uint8_t b = (uint8_t)button_of(note);
    int32_t s;
    uint32_t i;
    for (i = 0; i + 1u < SONG_MAX; i++)
        ui.hist[i] = ui.hist[i + 1];
    ui.hist[SONG_MAX - 1] = b;
    if (ui.step < g->n && g->b[ui.step] == b)
        ui.step++;
    else
        ui.step = g->b[0] == b ? 1u : 0u;
    for (s = 0; s < NSONGS; s++) {           /* the history ends with song s */
        const song_t *h = &SONGS[s];
        for (i = 0; i < h->n && ui.hist[SONG_MAX - h->n + i] == h->b[i]; i++)
            ;
        if (i == h->n) {
            char t[40];
            ui.heard = (int8_t)s;
            ui.heard_at = plat_ms();
            for (i = 0; i < SONG_MAX; i++)   /* heard once: the next song starts afresh */
                ui.hist[i] = NBUTTONS;
            t[0] = 0;
            text_cat(t, h->name);
            text_cat(t, ", PLAYED");
            tell("PLAYED", "", t, C_GOOD, ANN_NOW);
            break;
        }
    }
    ui.redraw = 1;
}

static void sound(int32_t note, uint32_t vel)
{
    ui.sounding = note;
    oca_note_on(note, (float)vel / 127.0f);
    plat_midi_send(0x09u | 0x90u << 8 | (uint32_t)note << 16 | vel << 24, TO_USB);
    ui.redraw = 1;
}

static void silence(void)
{
    if (ui.sounding >= 0)
        plat_midi_send(0x08u | 0x80u << 8 | (uint32_t)ui.sounding << 16, TO_USB);
    ui.sounding = -1;
    oca_note_off();
    ui.redraw = 1;
}

static void auto_stop(void)
{
    if (ui.auto_i < 0)
        return;
    ui.auto_i = -1;
    silence();
}

#define AUTO_ON_MS 420u                  /* PLAY: each note this long, then a breath */
#define AUTO_GAP_MS 90u

static void auto_tick(uint32_t now)
{
    const song_t *g = &SONGS[set.song];
    uint32_t t;
    if (ui.auto_i < 0)
        return;
    t = now - ui.auto_t;
    if (ui.sounding >= 0 && t >= AUTO_ON_MS)
        silence();
    if (t >= AUTO_ON_MS + AUTO_GAP_MS) {
        ui.auto_t = now;
        if (++ui.auto_i >= (int8_t)g->n) {
            ui.auto_i = -1;
            return;
        }
        t = 0;
    }
    if (ui.sounding < 0 && t < AUTO_ON_MS) {
        int32_t note = BUTTON_NOTE[g->b[ui.auto_i]] + 12 * set.octave;
        sound(note, 110u);
        played(note);
    }
}

static void auto_start(void)
{
    char t[48];
    silence();
    ui.auto_i = 0;
    ui.auto_t = plat_ms();
    ui.step = 0;
    t[0] = 0;
    text_cat(t, "PLAYING ");
    text_cat(t, SONGS[set.song].name);
    tell("PLAYING", "", t, C_ABLUE, ANN_NOW);
}

/* ------------------------------------------------------------------ input --- */
static void song_step(int32_t d)
{
    char t[ANN_MAX];
    int32_t s = set.song + d;
    auto_stop();
    while (s < 0)
        s += NSONGS;
    set.song = (uint8_t)(s % NSONGS);
    set_dirty = 1;
    ui.step = 0;
    ui.redraw = 1;
    song_words(set.song, t);
    ann_later(t, ANN_NOW, 200u, plat_ms());
}

static void knob_turn(int k, int32_t e)
{
    char t[48];
    if (k == 0)
        set.breath = (uint8_t)clamp(set.breath + e, 0, 10);
    else if (k == 1)
        set.vibrato = (uint8_t)clamp(set.vibrato + e, 0, 10);
    else if (k == 2)
        set.glide = (uint8_t)clamp(set.glide + e, 0, 10);
    else
        set.tune = (int8_t)clamp(set.tune + e, -50, 50);
    set_dirty = 1;
    params_send();
    knob_words(k, t);
    ann_later(t, ANN_NOW, 120u, plat_ms());
    ui.redraw = 1;
}

static void octave_step(int32_t d)
{
    int32_t o = clamp(set.octave + d, -1, 1);
    char t[24];
    if (o == set.octave)
        return;
    auto_stop();
    silence();
    ui.norder = 0;                           /* the held keys are let go: a new press starts afresh */
    set.octave = (int8_t)o;
    set_dirty = 1;
    t[0] = 0;
    text_cat(t, "OCTAVE ");
    text_cat(t, octave_word());
    tell("OCTAVE", octave_word(), t, C_ABLUE, ANN_NOW);
}

static void input(void)
{
    uint32_t keys = plat_keys(), btn = plat_buttons(), k, i;
    int32_t e;
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        int32_t note = KEY0 + (int32_t)k + 12 * set.octave;
        if ((keys & bit) && !(ui.keys & bit)) {      /* pressed: it sounds, newest wins */
            auto_stop();
            if (ui.norder < NKEYS)
                ui.order[ui.norder++] = (uint8_t)k;
            sound(note, 100u);
            played(note);
        } else if (!(keys & bit) && (ui.keys & bit)) {   /* let go: the key held before it sounds again */
            uint32_t j = 0, was_top = ui.norder && ui.order[ui.norder - 1] == k;
            for (i = 0; i < ui.norder; i++)
                if (ui.order[i] != k)
                    ui.order[j++] = ui.order[i];
            ui.norder = j;
            if (was_top) {
                if (ui.norder) {
                    silence();
                    sound(KEY0 + ui.order[ui.norder - 1] + 12 * set.octave, 100u);
                } else {
                    silence();
                }
            }
        }
    }
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        if (k == B_OCTDN)
            octave_step(-1);
        else if (k == B_OCTUP)
            octave_step(1);
        else if (k == B_PLAY) {
            if (ui.auto_i >= 0) {
                auto_stop();
                tell("STOPPED", "", 0, C_ABLUE, ANN_NOW);
            } else {
                auto_start();
            }
        } else if (k == B_SAVE) {
            if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0) {
                set_dirty = 0;
                tell("SAVED", "", 0, C_GOOD, ANN_NOW);
            } else {
                tell("SAVE FAILED", "", "SAVE FAILED, NOT SAVED", C_INK, ANN_NOW | ANN_KEEP);
            }
        }
    }
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0)
            continue;
        if (k == EN_SELECT || k == EN_PRESET)
            song_step(e);
        else if (k >= EN_K1)
            knob_turn((int)(k - EN_K1), e);
    }
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
}

static void lights(void)
{
    uint32_t lit = ui.keys, dim = 0, b;
    const song_t *g = &SONGS[set.song];
    for (b = 0; b < NBUTTONS; b++) {         /* the five song notes glow */
        int32_t k = BUTTON_NOTE[b] - KEY0;
        dim |= 1u << k;
    }
    if (ui.auto_i >= 0 && ui.sounding >= 0)
        lit |= 1u << (ui.sounding - KEY0 - 12 * set.octave);
    else if (ui.step < g->n && (plat_ms() / 300u) % 3u)   /* the next one blinks */
        lit |= 1u << (BUTTON_NOTE[g->b[ui.step]] - KEY0);
    plat_lights(ui.btn | (set_dirty ? 1u << B_SAVE : 0u) | (ui.auto_i >= 0 ? 1u << B_PLAY : 0u), lit,
                (1u << NB) - 1u, dim, 1);
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------------- draw --- */
static void disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy, w;
    for (dy = -r; dy <= r; dy++) {
        for (w = r; w > 0 && w * w + dy * dy > r * r; w--)
            ;
        cv_rect(cx - w, cy + dy, 2 * w + 1, 1, c);
    }
}

static void arrow(int32_t cx, int32_t cy, int32_t s, int dir, uint16_t c)   /* a solid triangle pointing DIR */
{
    int32_t i;
    for (i = 0; i <= s; i++) {
        int32_t w = i;                       /* the row's half width, from the tip */
        if (dir == O_UP)
            cv_rect(cx - w, cy - s / 2 + i, 2 * w + 1, 1, c);
        else if (dir == O_DOWN)
            cv_rect(cx - w, cy + s / 2 - i, 2 * w + 1, 1, c);
        else if (dir == O_LEFT)
            cv_rect(cx - s / 2 + i, cy - w, 1, 2 * w + 1, c);
        else
            cv_rect(cx + s / 2 - i, cy - w, 1, 2 * w + 1, c);
    }
}

/* one of the game's buttons: A, blue with its letter; the others, yellow with an arrow. FADED: not played yet */
static void button_glyph(int32_t cx, int32_t cy, int32_t r, int b, int faded)
{
    uint16_t bg = faded ? C_FAINT : b == O_A ? C_ABLUE : C_CYELLOW, fg = faded ? C_PAPER : b == O_A ? C_WHITE : C_INK;
    disc(cx, cy, r, bg);
    if (b == O_A) {
        const felucca_font_t *f = r >= 14 ? &FONT_B : &FONT_XS;
        cv_text(cx - text_w(f, "A") / 2, cy - f->h / 2, f, "A", fg);
    } else {
        arrow(cx, cy, r * 2 / 3, b, fg);
    }
}

static void draw_header(void)
{
    char t[16];
    uint32_t now = plat_ms();
    cv_begin(240, 36, C_PAPER);
    if (now < ui.msg_until) {
        int32_t x = cv_text(12, 9, &FONT_B, ui.msg[0], ui.msg_rgb);
        cv_text(x + 8, 9, &FONT_B, ui.msg[1], C_INK);
    } else {
        cv_text(12, 9, &FONT_B, APP_NAME, C_INK);
        if (ui.sounding >= 0) {
            note_name(ui.sounding, t);
            cv_text(228 - text_w(&FONT_B, t), 9, &FONT_B, t, C_ABLUE);
        } else {
            t[0] = 0;
            text_cat(t, "OCT ");
            text_cat(t, set.octave < 0 ? "LOW" : set.octave > 0 ? "HIGH" : "MID");
            cv_text(228 - text_w(&FONT_XS, t), 12, &FONT_XS, t, C_SOFT);
        }
    }
    cv_rect(12, 34, 216, 1, C_FAINT);
    cv_blit(0, 0);
}

static void draw_song(void)
{
    const song_t *g = &SONGS[set.song];
    const felucca_font_t *f = &FONT_M;
    char t[16], n[8];
    int32_t i, w = 26, x0 = 120 - (int32_t)g->n * w / 2 + w / 2;
    int done = ui.heard == (int8_t)set.song && plat_ms() - ui.heard_at < 2500u;
    cv_begin(240, 116, C_PAPER);
    cv_rect(8, 4, 224, 108, C_CARD);
    int_text(set.song + 1, t);
    text_cat(t, " OF ");
    int_text(NSONGS, n);
    text_cat(t, n);
    cv_text(120 - text_w(&FONT_XS, t) / 2, 9, &FONT_XS, t, C_SOFT);
    if (text_w(f, g->name) > 216)
        f = &FONT_B;
    cv_text(120 - text_w(f, g->name) / 2, 24, f, g->name, done ? C_GOOD : C_INK);
    for (i = 0; i < (int32_t)g->n; i++) {
        int32_t cx = x0 + i * w, cy = 72;
        int played_yet = done || i < (int32_t)ui.step;
        button_glyph(cx, cy, 11, g->b[i], !played_yet && i != (int32_t)ui.step);
        if (i == (int32_t)ui.step && !done)
            cv_rect(cx - 8, cy + 15, 17, 2, C_INK);
        note_name(BUTTON_NOTE[g->b[i]] + 12 * set.octave, t);
        cv_text(cx - text_w(&FONT_XS, t) / 2, cy + 20, &FONT_XS, t, played_yet ? C_INK : C_SOFT);
    }
    cv_blit(0, 36);
}

static void draw_bottom(void)
{
    int32_t b, k;
    char t[16];
    cv_begin(240, 88, C_PAPER);
    for (b = 0; b < NBUTTONS; b++) {        /* the five notes, lit while sounding */
        int32_t x = 6 + b * 46, note = BUTTON_NOTE[b] + 12 * set.octave;
        int on = ui.sounding == note;
        cv_rect(x, 2, 44, 46, on ? C_INK : C_CARD);
        button_glyph(x + 22, 18, 12, (int)b, 0);
        note_name(note, t);
        cv_text(x + 22 - text_w(&FONT_XS, t) / 2, 33, &FONT_XS, t, on ? C_PAPER : C_INK);
    }
    for (k = 0; k < 4; k++) {               /* the knobs */
        int32_t x = 6 + k * 58;
        cv_text(x, 54, &FONT_XS, KNOB_NAME[k], C_SOFT);
        knob_value((int)k, t);
        cv_text(x, 67, &FONT_B, t, C_INK);
    }
    cv_blit(0, 152);
}

static void draw(void)
{
    draw_header();
    draw_song();
    draw_bottom();
}

void app_ui_init(void)
{
    uint32_t i;
    for (i = 0; i < SONG_MAX; i++)
        ui.hist[i] = NBUTTONS;
    lcd_fill(0, 0, 240, 240, C_PAPER);
    ui.redraw = 1;
}

void app_frame(void)
{
    static uint32_t blink;
    uint32_t now = plat_ms();
    input();
    auto_tick(now);
    speak();
    lights();
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (ui.heard >= 0 && now - ui.heard_at >= 2500u) {   /* the song heard: its card goes back to the guide */
        ui.heard = -1;
        ui.step = 0;
        ui.redraw = 1;
    }
    if (ui.redraw || now / 300u != blink) {
        blink = now / 300u;
        ui.redraw = 0;
        draw();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "song"))
        *got = set.song;
    else if (!strcmp(what, "step"))
        *got = ui.step;
    else if (!strcmp(what, "heard"))
        *got = ui.heard;
    else if (!strcmp(what, "note"))
        *got = ui.sounding;
    else if (!strcmp(what, "octave"))
        *got = set.octave;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "auto"))
        *got = ui.auto_i;
    else if (!strcmp(what, "breath"))
        *got = set.breath;
    else if (!strcmp(what, "vibrato"))
        *got = set.vibrato;
    else if (!strcmp(what, "glide"))
        *got = set.glide;
    else if (!strcmp(what, "tune"))
        *got = set.tune;
    else
        return 0;
    return 1;
}

void app_speak_dump(void)
{
    settings_t keep = set;
    char t[ANN_MAX + 64];
    int32_t s, k, v;
    for (s = 0; s < NSONGS; s++) {
        set.song = (uint8_t)s;
        where_words(t);
        printf("SCREEN\t%s\n", t);
        song_words(s, t);
        printf("READOUT\t%s\n", t);
        t[0] = 0;
        text_cat(t, SONGS[s].name);
        text_cat(t, ", PLAYED");
        printf("MSG\t%s\n", t);
        t[0] = 0;
        text_cat(t, "PLAYING ");
        text_cat(t, SONGS[s].name);
        printf("MSG\t%s\n", t);
    }
    for (k = 0; k < 4; k++)
        for (v = -50; v <= 50; v += 25) {
            set.breath = set.vibrato = set.glide = (uint8_t)clamp(v / 5, 0, 10);
            set.tune = (int8_t)v;
            knob_words((int)k, t);
            printf("READOUT\t%s\n", t);
        }
    for (s = -1; s <= 1; s++) {
        set.octave = (int8_t)s;
        printf("MSG\tOCTAVE %s\n", octave_word());
    }
    printf("MSG\tSTOPPED\nMSG\tSAVED\nWARN\tSAVE FAILED, NOT SAVED\n");
    set = keep;
}
#endif
