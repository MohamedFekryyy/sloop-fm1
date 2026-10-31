/* SPDX-License-Identifier: GPL-3.0-only */
/* BLE MIDI clock in (FELUCCA_BLE=1): the BLE cable (2) is SYNC source 3 - seq.c maps the cable + 1, params.c's
 * N_SYNC gains "BLE". START / 24 ppqn / STOP and the tempo follow the BLE cable only; USB and TRS packets are
 * not the source; half a second without a pulse and the internal tempo plays (the same fallback as USB / TRS).
 * Packets go in through usb.c's midi_in_push, the function ble_radio_poll uses. Mirrors seq2_test.c's t_mclk
 * for the new source. Exit status: the number of failed checks. */
#define FELUCCA_ARRANGER 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
static uint32_t trk_def_engine(uint32_t i) { return i < NPART ? i : 0; }
#include "../firmware/src/project.c"
static struct { uint8_t force; } ui;
static uint8_t sync_reload;
#include "../firmware/src/arranger_scene.c"

static int fails;
static void check(int ok, const char *what)
{
    printf("ble-sync: %-74s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static void run_block(void) { int32_t out[CTL * 2]; mix_block(out, CTL); }

static void reset(uint32_t bpm)
{
    uint32_t i;
    host_tracks_init();
    for (i = 0; i < NTRK; i++)
        steps_clear(&trk[i]);
    memset(&drums, 0, sizeof drums);
    drums.set = -2;
    song.g[G_BPM] = (int16_t)bpm;
    song.rec = 0;
    song.playing = 0;
    rec_wait = 0;
    fm1_in.notes = fm1_in.buttons = 0;
    kb_prev = 0;
    memset(live_held, 0, sizeof live_held);
    memset(mi_trk_held, 0, sizeof mi_trk_held);
    ly_bit[LY_ROLL] = 1u << 8;
    ly_bit[LY_ERASE] = 1u << 4;
    dyn_bit[0] = 1u << 0;
    dyn_bit[1] = 1u << 1;
    fm1_ms = 0;
    mi_r = mi_w;
}

/* clock ticks on CABLE every per ms until UNTIL (ms of song time), the seq running block by block */
static void mclk_run(double *t, double *next, double per, double until, uint32_t cable)
{
    while (*t < until) {
        while (*next <= *t) {
            midi_in_push(0xF80Fu | cable << 4);
            *next += per;
        }
        fm1_ms = (uint32_t)*t;
        run_block();
        *t += (double)CTL * 1000.0 / FS;
    }
}

static void t_ble_mclk(void)
{
    double t = 0, next = 0, per = 60000.0 / 120 / 24;
    reset(90);                                        /* the internal tempo: 90 */
    song.g[G_SYNC] = 3;                               /* BLE */
    mclk_run(&t, &next, per, 700, 2);                 /* the clock runs before START (the tempo shows) */
    check(!song.playing && song.g[G_BPM] == 120, "SYNC BLE: stopped, BPM follows the BLE master (120)");
    midi_in_push(0xFA2Fu);                            /* START over BLE, then the downbeat pulse */
    next = t;
    mclk_run(&t, &next, per, t + 4000.0 + per / 2, 2);   /* two bars of 120 */
    check(song.playing && clk_beat == 8u, "SYNC BLE: START, 2 bars at 120 -> beat 8 exactly");
    per = 60000.0 / 100 / 24;                          /* the master slows to 100 */
    mclk_run(&t, &next, per, t + 4800.0, 2);
    check(clk_beat == 16u && song.g[G_BPM] == 100, "SYNC BLE: the master at 100 -> 2 more bars, BPM 100");
    midi_in_push(0xFC0Fu);                            /* STOP over USB: not the source */
    mclk_run(&t, &next, per, t + 50.0, 2);
    check(song.playing, "SYNC BLE ignores USB packets");
    midi_in_push(0xFC1Fu);                            /* STOP over TRS: not the source */
    mclk_run(&t, &next, per, t + 50.0, 2);
    check(song.playing, "SYNC BLE ignores TRS packets");
    midi_in_push(0xFC2Fu);                            /* STOP over BLE */
    mclk_run(&t, &next, per, t + 10.0, 2);
    check(!song.playing, "SYNC BLE: STOP");
    t += 1000.0;                                      /* the BLE master is gone: PLAY on the FM-1 plays */
    fm1_ms = (uint32_t)t;
    transport_req = 1;
    run_block();
    {
        uint32_t b0 = clk_beat, k;
        for (k = 0; k < (uint32_t)(FS / CTL); k++)
            run_block();
        check(song.playing && clk_beat >= b0 + 1u, "SYNC BLE: no pulse for 0.5 s -> the internal tempo plays");
    }
    transport_req = 2;
    run_block();
}

static void t_ble_not_a_source_for_others(void)
{
    double t = 0, next = 0, per = 60000.0 / 140 / 24;
    reset(90);
    song.g[G_SYNC] = 2;                               /* TRS */
    mclk_run(&t, &next, per, 700, 2);                 /* fast BLE ticks: not the source */
    check(!song.playing && song.g[G_BPM] == 90, "SYNC TRS ignores the BLE cable (BPM stays 90)");
    midi_in_push(0xFA2Fu);                            /* a BLE START does not start it */
    mclk_run(&t, &next, per, t + 200.0, 2);
    check(!song.playing, "SYNC TRS: no transport from the BLE cable");
    song.g[G_SYNC] = 1;                               /* USB */
    reset(90);
    song.g[G_SYNC] = 1;
    t = next = 0;
    mclk_run(&t, &next, per, 700, 2);
    check(!song.playing && song.g[G_BPM] == 90, "SYNC USB ignores the BLE cable (BPM stays 90)");
}

int main(void)
{
    check(GP[G_SYNC].max == 3 && !strcmp(GP[G_SYNC].names[3], "BLE"), "SYNC is INT / USB / TRS / BLE (max 3)");
    t_ble_mclk();
    t_ble_not_a_source_for_others();
    return fails;
}
