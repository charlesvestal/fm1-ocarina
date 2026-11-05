/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The twelve songs a game listens for, as its five ocarina buttons. The buttons sound D5, F5, A5, B5 and D6
 * (measured from an electronic ocarina made for the game, 2026-11-05). */
#pragma once
#include <stdint.h>

enum { O_A, O_DOWN, O_RIGHT, O_LEFT, O_UP, NBUTTONS };
static const uint8_t BUTTON_NOTE[NBUTTONS] = {74, 77, 81, 83, 86};
static const char *const BUTTON_WORD[NBUTTONS] = {"A", "DOWN", "RIGHT", "LEFT", "UP"};

#define SONG_MAX 8
typedef struct {
    const char *name;
    uint8_t n;
    uint8_t b[SONG_MAX];
} song_t;

static const song_t SONGS[] = {
    {"ZELDA'S LULLABY", 6, {O_LEFT, O_UP, O_RIGHT, O_LEFT, O_UP, O_RIGHT}},
    {"EPONA'S SONG", 6, {O_UP, O_LEFT, O_RIGHT, O_UP, O_LEFT, O_RIGHT}},
    {"SARIA'S SONG", 6, {O_DOWN, O_RIGHT, O_LEFT, O_DOWN, O_RIGHT, O_LEFT}},
    {"SUN'S SONG", 6, {O_RIGHT, O_DOWN, O_UP, O_RIGHT, O_DOWN, O_UP}},
    {"SONG OF TIME", 6, {O_RIGHT, O_A, O_DOWN, O_RIGHT, O_A, O_DOWN}},
    {"SONG OF STORMS", 6, {O_A, O_DOWN, O_UP, O_A, O_DOWN, O_UP}},
    {"MINUET OF FOREST", 6, {O_A, O_UP, O_LEFT, O_RIGHT, O_LEFT, O_RIGHT}},
    {"BOLERO OF FIRE", 8, {O_DOWN, O_A, O_DOWN, O_A, O_RIGHT, O_DOWN, O_RIGHT, O_DOWN}},
    {"SERENADE OF WATER", 5, {O_A, O_DOWN, O_RIGHT, O_RIGHT, O_LEFT}},
    {"NOCTURNE OF SHADOW", 7, {O_LEFT, O_RIGHT, O_RIGHT, O_A, O_LEFT, O_RIGHT, O_DOWN}},
    {"REQUIEM OF SPIRIT", 6, {O_A, O_DOWN, O_A, O_RIGHT, O_DOWN, O_A}},
    {"PRELUDE OF LIGHT", 6, {O_UP, O_RIGHT, O_UP, O_LEFT, O_RIGHT, O_UP}},
};
#define NSONGS ((int32_t)(sizeof SONGS / sizeof SONGS[0]))
