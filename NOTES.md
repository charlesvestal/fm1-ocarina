# OCA-1 notes

Decisions and status, newest first.

## 2026-11-05: first build

- The Switch 2 Ocarina of Time (2026-11-05) listens for its songs through the console's microphone (humming, a
  real instrument, or Hori's Electric Ocarina of Time, which is a sound toy, not a controller). So OCA-1 is an
  instrument played at the mic, not a Bluetooth controller.
- The notes: measured from the Hori ocarina's auto-play in an unboxing video (youtube Zb_y_9od6hg, 4:10-5:58) by
  pitch tracking. A D5, Down F5, Right A5, Left B5, Up D6; the songs recognised there (Epona, Storms, Sun, Time,
  Lullaby) confirm the mapping. The video read ~20 cents sharp throughout (the toy, or the video): TUNE covers it.
- The voice: a sine with a little 2nd and 3rd harmonic and band-passed breath, mono, last key wins; a legato note
  dips the level so a listener hears a new note. tests/host/oca_test.c checks the five notes within 3 cents.
- Keys from F4 at the middle octave: the five song notes are white keys 9, 12, 16, 18, 21 (they glow; the next one
  of the songbook's song blinks).
- PLAY: 420 ms a note, 90 ms gaps. A song is recognised from the last notes played, whatever the songbook shows.
- Identity FM-1_14. No Bluetooth (ble = false), mirror on.

Open: does the game take it? (Needs the user, a Switch 2 and the speaker; not tried.) Whether it wants exact
octaves or any key; how long a note it needs; whether PLAY's tempo suits it.
