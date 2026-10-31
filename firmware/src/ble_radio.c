/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* BLE MIDI in, minimal receive-only spike (FELUCCA_BLE=1): fm1-base's own BLE stack (firmware/ble,
 * SPDX GPL-3.0-only OR MPL-2.0, Copyright (C) 2026 Charles Vestal; stack by Henri David) on the
 * WL82 radio (firmware/hal/fm1_ble*.h, MPL-2.0; start-up tables ble_rf_tables_v15.h, Apache-2.0).
 * Advertises, accepts one central, feeds incoming MIDI (clock included) into usb.c's midi_in_q as
 * cable 2. No sending, no pairing, no central role; no settings record: the static-random address is
 * folded from the flash's factory UID, so it is stable across boots without persistence.
 * The stack runs in radio IRQs 45 (event) + 29 (RX) at priority 2; the main loop drains the ring. */
#define BLE_DEVICE_NAME "SLOOP"
#include "fm1_flash.h"     /* irq_save/irq_restore: declare before first use (unity build order) */
#include "ble_stack.c"

static struct { uint32_t pkt; uint16_t ts; } blr_in[64];
static volatile uint32_t blr_w, blr_r;
static volatile uint8_t blr_role;                        /* 1: a computer is connected */

BLE_API int ble_app_midi_peek(uint32_t *pkt, uint32_t *t_ms) { (void)pkt; (void)t_ms; return 0; }
BLE_API void ble_app_midi_pop(void) {}
BLE_API void ble_app_midi_in(uint32_t pkt, uint16_t ts, uint16_t last_ts)
{
    (void)last_ts;
    if (blr_w - blr_r >= 64u)
        return;
    blr_in[blr_w % 64u].pkt = (pkt & ~0xF0u) | 0x20u;    /* cable 2: from BLE (seq.c src map) */
    blr_in[blr_w % 64u].ts = ts;
    RING_PUBLISH();
    blr_w++;
}
BLE_API void ble_app_state(void) { blr_role = ble_connected() ? 1u : 0u; }

#if !FELUCCA_BLE_STUB                          /* the radio + its IRQs: target only (host tests use the stub) */
/* the SDK board's constant trims (fm1-base bt_own.c; fm1_rf_cal.h measures the rest at every start) */
static const struct ble_rf_trims BLR_TRIMS = {{0x0B, 0x0B}, {1, 7, 4, 7, 11, 1, 7}, {0}, {0}};

static void ble_radio_start(void)
{
    /* our static-random address, the same on every boot: the flash's factory unique ID (fm1_flash.h
     * fl_uid_ram, FL_UID) folded to 48 bits, the top two bits set (Core Vol 6 Part B 1.3.2.1). No UID
     * read (a part without 0x4B): the stack's own address - fresh each boot on stock V15 (fm1-base HW
     * §14.5: no provisioned VM 104), still a valid static-random one. */
    uint8_t uid[16], addr[6];
    uint32_t f, i;
    if (FL_FAR(fl_uid_ram)(uid) == 0) {
        for (i = 0; i < 6u; i++)
            addr[i] = (uint8_t)(uid[i] ^ uid[i + 6u] ^ uid[(i + 12u) & 15u]);
        addr[5] = (uint8_t)((addr[5] & 0x3Fu) | 0xC0u);
    } else
        ble_hw_addr(addr);
    ble_init(addr, 1);
    f = irq_save();
    fm1_guard_unlock_top();
    fm1_ble_irq_attach(2);                  /* IRQ 45 + IRQ 29 at priority 2 (fm1_ble.h) */
    fm1_ble_irqs_hold(1);
    fm1_guard_lock_top();
    irq_restore(f);
    fm1_rf_cal_on = 1;                      /* measure the calibration, nothing stored needed */
    ble_hw_wl82_start(&BLR_TRIMS);
    fm1_ble_irqs_hold(1);
    ble_enable(1);                          /* advertising starts */
    fm1_ble_irqs_hold(0);
}
#endif

static void ble_radio_poll(void)            /* the main loop: BLE MIDI into the shared input queue */
{
    while (blr_r != blr_w) {
        midi_in_push(blr_in[blr_r % 64u].pkt);
        RING_PUBLISH();
        blr_r++;
    }
}
