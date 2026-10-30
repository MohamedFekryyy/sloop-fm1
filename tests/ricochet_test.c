/* SPDX-License-Identifier: GPL-3.0-only */
/* RICO (seq.c AM_RICO): the bouncing-balls generative mode.
 *   mapping   SWG -> 3..12 walls; a wall -> its degree of the track's scale (PEN: 0 2 4 7 9, climbing);
 *             the drum track's walls play its lanes
 *   physics   a fast ball never tunnels or escapes (substeps), the sim is exact integer: two tracks with
 *             the same settings hit the same walls in the same order
 *   playing   no keys held, on or off the transport: hits start notes (velocity 100, GATE long), PROB 0
 *             is silent, a mode change away ends the sounding note; what plays while recording is
 *             recorded (as the arp's notes)
 * Exit status: the number of failed checks. */
#define FELUCCA_ARRANGER 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
#define PROJ_HOST 1
static uint32_t trk_def_engine(uint32_t i) { return i < NPART ? i : 0; }
#include "../firmware/src/project.c"
static struct { uint8_t force; } ui;
static uint8_t sync_reload;
#include "../firmware/src/arranger_scene.c"

static uint32_t fails;
#define check(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

static void reset(void)
{
    uint32_t i;
    host_tracks_init();
    for (i = 0; i < NTRK; i++)
        steps_clear(&trk[i]);
    song.playing = 0;
    song.rec = 0;
    song.g[G_BPM] = 120;
}

static void run_blocks(uint32_t n)                  /* n audio blocks (CTL samples each) */
{
    int32_t out[CTL * 2];
    while (n--)
        mix_block(out, CTL);
}

int main(void)
{
    track_t *t = &trk[0];
    uint32_t i, b, w;

    reset();
    /* SWG -> walls */
    t->p[P_ASWING] = 0;
    check(rico_walls_of(t) == 3u, "SWG 0: a triangle");
    t->p[P_ASWING] = 44;
    check(rico_walls_of(t) == 7u, "SWG 44: seven walls");
    t->p[P_ASWING] = 100;
    check(rico_walls_of(t) == 12u, "SWG 100: twelve walls");

    /* a wall -> its degree of the track's scale */
    t->p[P_SCALE] = 5;                              /* PEN: 0 2 4 7 9 */
    t->p[P_ROOT] = 0;
    t->p[P_TRANS] = 0;
    {
        static const uint8_t PEN5[5] = {60, 62, 64, 67, 69};
        for (i = 0; i < 5u; i++)
            check(rico_note(t, i) == PEN5[i], "PEN: wall i is degree i");
        check(rico_note(t, 5) == 72u, "PEN: wall 5 is the root an octave up");
        check(rico_note(t, 9) == 81u, "PEN: wall 9 climbs two octaves");
        t->p[P_ROOT] = 2;
        check(rico_note(t, 0) == 62u, "ROOT moves the walls");
        t->p[P_ROOT] = 0;
        t->p[P_SCALE] = 0;                          /* CHR: every semitone */
        check(rico_note(t, 11) == 71u && rico_note(t, 12) == 72u, "CHR: the walls climb chromatically");
        t->p[P_SCALE] = 5;
    }

    /* physics: a fast ball stays inside, hits keep coming, the sim is deterministic */
    t->p[P_ARATE] = 5;                              /* the fastest */
    t->p[P_AOCT] = 4;
    t->p[P_ASWING] = 44;
    rico_config(t);
    check(t->rico_on && t->rico_walls == 7u && t->rico_balls == 4u, "config: 7 walls, 4 balls");
    {
        uint32_t hits = 0;
        for (i = 0; i < 200000u; i++) {
            for (b = 0; b < t->rico_balls; b++) {
                if (rico_step(t, b, 47) >= 0)
                    hits++;
                for (w = 0; w < t->rico_walls; w++) {
                    int64_t d = (((int64_t)t->rico_px[b] - t->rico_ax[w]) * t->rico_nx[w] +
                                 ((int64_t)t->rico_py[b] - t->rico_ay[w]) * t->rico_ny[w]) >> 15;
                    if (d < (RICO_BALL << 16) - (4 << 16)) {
                        check(0, "a ball escaped its wall");
                        i = 200000u;
                        break;
                    }
                }
            }
        }
        check(hits > 200u, "the balls keep hitting walls");
    }
    {
        /* determinism: two identical sims hit the same walls in the same order */
        track_t a, bb;
        uint32_t same = 1;
        memset(&a, 0, sizeof a);
        memset(&bb, 0, sizeof bb);
        for (i = 0; i < 2u; i++) {
            track_t *x = i ? &bb : &a;
            x->p[P_ARATE] = 3;
            x->p[P_AOCT] = 3;
            x->p[P_ASWING] = 44;
            rico_config(x);
        }
        for (i = 0; i < 20000u && same; i++)
            for (b = 0; b < 3u; b++)
                if (rico_step(&a, b, 47) != rico_step(&bb, b, 47))
                    same = 0;
        check(same, "the sim is deterministic");
    }

    /* playing: hits start notes with no keys held, on and off the transport */
    reset();
    t->p[P_AMODE] = AM_RICO;
    t->p[P_ARATE] = 4;
    t->p[P_AOCT] = 3;
    t->p[P_ASWING] = 44;
    t->p[P_AGATE] = 64;
    t->p[P_APROB] = 127;
    {
        uint32_t ons = 0, sounding = 0, off_seen = 0;
        uint8_t last = 0;
        for (i = 0; i < 6000u; i++) {               /* about 4.4 s, stopped */
            int32_t out[CTL * 2];
            mix_block(out, CTL);
            if (t->arp_note && t->arp_note != last)
                ons++;
            if (t->arp_note)
                sounding++;
            else if (ons)
                off_seen++;
            last = t->arp_note;
        }
        check(ons >= 8u, "RICO plays with no keys held, stopped");
        check(sounding > 0u, "the notes are on for a while");
        check(off_seen > 0u, "the notes end (GATE)");
    }
    /* PROB 0: silence (let the note sounding from above end first) */
    t->p[P_APROB] = 0;
    for (i = 0; i < 2000u && t->arp_note; i++) {
        int32_t out[CTL * 2];
        mix_block(out, CTL);
    }
    {
        uint32_t ons = 0;
        for (i = 0; i < 3000u; i++) {
            int32_t out[CTL * 2];
            mix_block(out, CTL);
            if (t->arp_note)
                ons++;
        }
        check(ons == 0u, "PROB 0 is silent");
    }
    t->p[P_APROB] = 127;
    /* a mode change away ends the sounding note */
    {
        for (i = 0; i < 3000u && !t->arp_note; i++) {
            int32_t out[CTL * 2];
            mix_block(out, CTL);
        }
        check(t->arp_note, "a note sounding before the mode change");
        t->p[P_AMODE] = AM_UP;                      /* no held notes: the arp stops, the note ends */
        for (i = 0; i < 3000u && t->arp_note; i++) {
            int32_t out[CTL * 2];
            mix_block(out, CTL);
        }
        check(!t->arp_note, "leaving RICO ends the sounding note");
        t->p[P_AMODE] = AM_RICO;
    }
    /* recording: what the balls play lands in the pattern */
    reset();
    t = &trk[0];
    t->p[P_AMODE] = AM_RICO;
    t->p[P_ARATE] = 4;
    t->p[P_AOCT] = 3;
    t->p[P_ASWING] = 44;
    t->p[P_AGATE] = 64;
    t->p[P_APROB] = 127;
    t->p[P_SCALE] = 5;
    song.rec = 1u;
    song.playing = 1;
    run_blocks(8000u);                              /* about 5.8 s at 120 BPM */
    {
        uint32_t notes = 0, inscale = 1;
        for (i = 0; i < NSTEP; i++)
            if (t->step[i].time == ST_NOTE)
                for (b = 0; b < 4u && t->step[i].note[b]; b++) {
                    int32_t rel = ((int32_t)t->step[i].note[b] - 60) % 12;
                    static const int8_t PEN_OK[12] = {1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0};
                    notes++;
                    if (rel < 0)
                        rel += 12;
                    if (!PEN_OK[rel])
                        inscale = 0;
                }
        check(notes >= 4u, "RICO hits are recorded while recording");
        check(inscale, "the recorded notes are in the track's scale");
    }
    song.playing = 0;
    song.rec = 0;

    /* the drum track: a wall plays that lane */
    reset();
    {
        track_t *d = &trk[NTRK - 1u];
        check(is_drum(d), "the last track is the drum track");
        for (i = 0; i < 4u; i++)
            check(rico_note(d, i) == kb_map(d, i), "the drum walls play the lanes");
    }

    if (!fails)
        puts("ricochet: all checks passed");
    return (int)fails;
}
