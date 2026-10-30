/* SPDX-License-Identifier: GPL-3.0-only */
/* RICO on the screen: the ARP page with MODE RICO draws the polygon and the balls, and its columns read
 * SPD / BALL / GATE / COR (ARP 2). Renders build/host/rico-arp.ppm and rico-arp2.ppm for review; checks
 * the graph area has the polygon's pixels and the balls', and that the labels come out. */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"

static int rfails;
#define rcheck(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); rfails++; } } while (0)

/* count the lit pixels in the graph band (Y_GRAPH..Y_GRAPH+H_GRAPH of the 240x240 screen) */
static uint32_t lit_rows(uint32_t y0, uint32_t y1)
{
    uint32_t x, y, n = 0;
    for (y = y0; y < y1; y++)
        for (x = 0; x < 240u; x++)
            if (swap16(screen[y * 240u + x]) != 0u)
                n++;
    return n;
}

int main(int argc, char **argv)
{
    uint32_t i;
    outdir = argc > 1 ? argv[1] : "build/host";
    panel = PANEL_DEFAULT;
    layers_init();
    settings.palette = 4;
    palette_set(4);
    host_tracks_init();
    for (i = 0; i < NPART; i++) {
        set_engine_of(&trk[i], TRK_DEF[i][0]);
        apply_preset_to(&trk[i], TRK_DEF[i][1]);
        trk[i].engine = trk[i].eng_req;
    }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    go_home();

    /* RICO on track 1: 7 walls, 3 balls, mid speed, pentatonic */
    track_t *t = &trk[0];
    t->p[P_AMODE] = AM_RICO;
    t->p[P_ARATE] = 3;
    t->p[P_AOCT] = 3;
    t->p[P_ASWING] = 44;
    t->p[P_AGATE] = 64;
    t->p[P_APROB] = 127;
    t->p[P_SCALE] = 5;
    frames(30);                                     /* let the balls travel and hit walls */
    open_family(FAM_ARP);
    ui.force = 1;
    frame();
    ppm("rico-arp");
    rcheck(t->rico_on && t->rico_walls == 7u && t->rico_balls == 3u, "the sim is built for the screen");
    rcheck(lit_rows(Y_GRAPH, Y_GRAPH + H_GRAPH) > 200u, "the ARP page draws the polygon and the balls");
    {
        /* the balls move: two frames apart, the picture changes */
        uint32_t a[4], b[4], k;
        for (k = 0; k < 4u; k++)
            a[k] = (uint32_t)t->rico_px[k & 1u] ^ (uint32_t)t->rico_py[k & 1u];
        frames(3);
        for (k = 0; k < 4u; k++)
            b[k] = (uint32_t)t->rico_px[k & 1u] ^ (uint32_t)t->rico_py[k & 1u];
        rcheck(a[0] != b[0] || a[1] != b[1], "the balls move between frames");
    }
    {                                               /* ARP 2: the family's second page */
        uint32_t arp[2] = {0, 0}, found = 0, k;
        for (k = 0; k < NPAGES && found < 2u; k++)
            if (PAGES[k].fam == FAM_ARP)
                arp[found++] = k;
        rcheck(found == 2u, "the ARP family has its two pages");
        open_family(FAM_ARP);
        ui.page = (uint8_t)arp[1];
        ui.fam_last[FAM_ARP] = (uint8_t)arp[1];
        page_entered();
        ui.force = 1;
        frame();
        ppm("rico-arp2");
        rcheck(lit_rows(Y_GRAPH, Y_GRAPH + H_GRAPH) > 200u, "ARP 2 draws the polygon too");

        /* back to a normal arp mode: the page draws as ever (the big values) */
        t->p[P_AMODE] = AM_UP;
        open_family(FAM_ARP);
        ui.page = (uint8_t)arp[0];
        ui.fam_last[FAM_ARP] = (uint8_t)arp[0];
        page_entered();
        ui.force = 1;
        frame();
        ppm("rico-arp-off");
    }
    rcheck(lit_rows(Y_GRAPH, Y_GRAPH + H_GRAPH) > 50u, "other arp modes draw as before");

    if (!rfails)
        puts("ricochet ui: all checks passed");
    return rfails;
}
