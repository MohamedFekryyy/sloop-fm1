# firmware/ble - provenance and licenses (ble-spike branch)

The BLE stack in this directory is copied UNMODIFIED from fm1-base
(https://github.com/charlesvestal/fm1-base, commit 4039a8b), bt/ble/:
Copyright (C) 2026 Charles Vestal; the stack itself is by Henri David
(written from the Bluetooth Core and BLE-MIDI specifications).
Per-file SPDX: GPL-3.0-only OR MPL-2.0. SLOOP elects GPL-3.0-only where a
choice is needed; MPL-2.0 files keep their notices either way.
License texts: /LICENSE (GPL-3.0-only) and /LICENSES/ (MPL-2.0, Apache-2.0).

firmware/hal/fm1_ble.h, fm1_ble_rf.h, fm1_rf_cal.h: from fm1-base's felucca-bsp
(chip/), MPL-2.0 / GPL-3.0-only OR MPL-2.0 per file.
firmware/hal/ble_rf_tables_v15.h: Apache-2.0; the radio start-up writes recorded
from M-VAVE's stock V15 firmware (m-vave/fm1_open, Apache-2.0) and JieLi's AC79
AIoT SDK (Apache-2.0); see the header's own comment.
