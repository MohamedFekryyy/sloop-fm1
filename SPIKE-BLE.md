# BLE spike findings (2026-10-31) - NOT for merge, proposal only

Question: can SLOOP ship native BLE MIDI (cable-free Mac/djay Pro sync via Ableton Link)?

## Verdict: does not fit without feature cuts or flash-layout changes (both out of scope)

Measured facts (pi32, real builds, October 2026, fm1-base README):
- FM-1 app slot: 581 KB flash (app.ld: 581,564 B), 96 KB RAM, 336 KB pool.
- SLOOP image: 575,280 B -> 6,284 B free.
- Own (open) BLE stack, Bluetooth-MIDI-only config: +41 to +46 KB flash, +10 KB RAM, +1 KB pool (template: 77 KB no-radio -> 118-123 KB with BLE MIDI).
- Gap: ~35-40 KB flash. Cannot be closed by optimization: SLOOP already builds -Os -ffunction-sections with ld --gc-sections (tools/build.py:40,156).
- No large movable data blobs found in SLOOP firmware (no fonts/bitmaps/wavetables in image; biggest sources are code: seq.c 170 KB, fx.c 70 KB).

## What IS proven viable elsewhere
- The same own stack ran on a real FM-1 (2026-10): Mac Audio MIDI Setup connect, MIDI both ways (fm1-base docs/bt/BLE-STACK.md). Open issues: radio analog performance unmeasured, one unreproduced boot hang, Windows untested.
- Licensing compatible: bt/ble SPDX GPL-3.0-only OR MPL-2.0; RF tables Apache-2.0; obligations = preserve copyright headers + license texts (+ NOTICE if present); MPL election requires MPL files stay MPL.
- JieLi binary BT/WiFi libs remain GPL-incompatible -> WiFi in SLOOP is license-blocked; BLE via own stack is not.

## Integration design (PROPOSAL, unverified against a real build)
- Drop bt/ble/ (zero platform deps) + bsp/chip BLE headers into unity build; port ~1,200 lines of glue (rings, IRQ routing, main-loop pump) from fm1-base device/radio.c + device/bt_own.c.
- Radio IRQs 45 (event) + 29 (RX) at priority 2; single-producer rings to main loop; SLOOP has fm1_irq_attach infra.
- BLE MIDI packets tagged new source id -> midi_in_q (coexists with USB/TRS arbitration); clock source = 4th G_SYNC value "BLE" (params.c:62 has 3).
- MODE_OFF default; rescue breadcrumb + watchdog recovery from fm1-base.
- RF start-up tables live at fixed flash area 0x0E8000 OUTSIDE the app slot (committed as firmware/hal/ble_rf_tables_v15.h, format 3, 39.9 KB) - do not count them against the slot.

## Toolchain recovery (exhausted 2026-10-31)
pkgman.jieliapp.com, gitee, aliyun OSS all unreachable from build network (geo-block pattern per FM-1 wiki Getting-Started). Colmi_P80 gitea vendored-toolchain clone broken server-side. No docker for mvave-fm1 image. GitHub Actions CAN reach pkgman (zvenson/fm1-build proof) if re-measurement is ever wanted - but measured template numbers above already settle the fit question.

## Cable-free path without firmware cuts (for main/user decision)
- Mac bridge (verified from vendor sources): Synclock (free, GPLv2, macOS 13+, Apple Silicon - user has M3/M2 Air/Pro) joins Link session, emits MIDI clock to any CoreMIDI destination. VolcaClock (free, GPLv2, macOS 12+, Intel too) alternative; one user-reported 5-min hang in Oct 2026 thread.
- djay Pro and Live both speak Ableton Link natively; neither needs to be the clock source.
- FM-1 side needs a BLE-MIDI hardware dongle (WIDI Jack class; price/compatibility unverified here, would be a purchase) OR a USB cable (free, works with delivered RICO clock-sync firmware via G_SYNC=USB today).
