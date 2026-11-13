# Notices and sources

WTLWireless is one bundle with three parts. Until 0.14.0 they were three kexts in three repositories (MtkWlan,
MtkWlanNative and MtkBt); the notices below are theirs, with the paths of this repository.

## Wi-Fi core (`src/core`)

### Linux mt76 (ISC)
Most of what the core knows about the hardware is ported from `drivers/net/wireless/mediatek/mt76`
in Linux v6.18 (https://github.com/torvalds/linux, tag v6.18, commit 7d0a66e4bb90), plus two TLV
length fixes from Linux master after v6.18 (commit 9ddb7487 and its CLC equivalent) in `src/core/MtkPower.cpp`.
Copyright (C) 2018 Felix Fietkau, Copyright (C) 2020, 2023 MediaTek Inc. mt76 moved to BSD-3-Clause-Clear
after v6.18; what comes from the later versions is those two fixes and the MT7927 support listed in the next section.

| File | Ported from |
| --- | --- |
| `src/core/WTLWireless.cpp` | `mt7925/pci.c` probe, `mt792x_core.c` fw/drv ownership handshakes |
| `src/core/MtkRegs.cpp` | `mt7925/pci.c` bus address remapping (L1/L2 remap), `util.c` poll helper |
| `src/core/MtkFw.cpp` | `mt7925/pci.c`, `mt792x_dma.c`, `dma.c` (WFDMA rings), `mt7925/mcu.c`, `mt76_connac_mcu.c` (firmware download) |
| `src/core/MtkScan.cpp` | `mt7925/mcu.c` (UNI commands, NIC capability, EEPROM, channel domain, hw_scan), `mt7925/init.c`, `mt792x_mac.c` |
| `src/core/MtkJoin.cpp` | tag values and TLV layouts from `mt76_connac_mcu.h`, `mt7925/mcu.c`; STA_REC TLV order from `mt7925_mcu_sta_cmd` |
| `src/core/MtkNet.cpp` | `mt7925/mac.c` (`write_txwi`, `fill_rx`, `tx_free`, `queue_rx_skb`), `mt76_connac_mac.c` (`write_hw_txp`), `mac80211.c` (`mt76_check_ccmp_pn`) |
| `src/core/MtkBa.cpp` | `agg-rx.c` (`mt76_rx_aggr_reorder`, `_release_head`, `_check_release`, BlockAckReq), `mt7925/mcu.c` (`mt7925_mcu_sta_ba`), `mt7925/main.c` (`mt7925_ampdu_action`) |
| `src/core/MtkPower.cpp` | `mt7925/mcu.c` (`mt7925_load_clc`, `__mt7925_mcu_set_clc`, `mt7925_mcu_set_clc`, `mt7925_mcu_read_eeprom`, `mt7925_mcu_rate_txpower_band`, `mt7925_mcu_build_sku`) |
| `src/core/MtkSec.cpp` | the key install order and `mt7925_mcu_bss_sec_tlv` values from `mt7925/mcu.c`; `sta_rec_key_v3` layout from `mt76_connac_mcu.h` |

`src/core/MtkDma.cpp` (IODMACommand memory), `src/core/WTLWirelessEth.cpp` (the IOEthernetController), `src/core/MtkUserClient.cpp`
(the IOUserClient), `src/core/MtkCoreApi.h` and `src/core/MtkUserApi.h` are written against Apple's IOKit headers and
contain no mt76 code.

### Linux mt76 after v6.18, MT7927 (BSD-3-Clause-Clear)
The MT7927 row of the chip table and the code paths only that chip takes are written from Linux master, commit
79eb6679ef6a (2026-11-11), files `mt7925/pci.c`, `mt7925/regs.h`, `mt7925/mt7925.h`, `mt792x_regs.h`, `mt792x_dma.c`
and `mt792x.h`. Those files carry `SPDX-License-Identifier: BSD-3-Clause-Clear`, Copyright (C) 2023 MediaTek Inc.;
the licence text is in `LICENSES/BSD-3-Clause-Clear-mt76.txt`.

| File | From |
| --- | --- |
| `src/core/MtkChip.cpp` | the MT7927 row: PCI ids, RX ring numbers and interrupt bits, the prefetch table (`mt7927_dma_prefetch_setup`), the CBTOP reset addresses, the quirk list |
| `src/core/MtkFw.cpp` | `mt7927_wfsys_reset` (reset through CBTOP), `mt7927_wfdma_setup` (two GLO_CFG bits), the DTX and DRX pointer reset before the rings, the third RX ring |
| `src/core/WTLWireless.cpp` | ASPM off in Link Control, the CHIPID words an MT7927 board may report (`mt7925_pci_probe`) |
| `src/core/MtkScan.cpp`, `src/core/MtkJoin.cpp` | the band index in DEV_INFO and BSS_INFO (`mt7927_band_idx`), the DBDC command (`mt7925_mcu_set_dbdc`) |
| `tests/hwtrace/golden-mt7927.txt` | the expected register sequence, worked out by hand from the same files |

### itlmt7925 (BSD-3-Clause-Clear)
itlmt7925, from https://github.com/thanhnndev/ga403wm-hackintosh (release kext-v1.1), by thanhnndev
and contributors. It was the first MT7925 driver for macOS that I know of. The core follows it in these places:

- `src/core/MtkScan.cpp`: the short post-firmware command sequence (NIC capability, DEV_INFO, BSS_INFO,
  domain, scan) and the beacon locator in `mtkParseFrame` (`itlmt7925_m2.cpp`, `_m3.cpp`).
- `src/core/MtkJoin.cpp`: the join sequence and the BSS_INFO, STA_REC, ROC and EDCA layouts, plus the
  legacy-rate WTBL preparation (`itlmt7925_m3.cpp`: `m3Run1`, `m3Bss`, `m3Edca`, `m3Sta`, `m3Roc`,
  `m3Tx`, `m3Auth`, `m3Assoc`, `m3ParseAp`, `m3HwPrep`), changed for open networks and restructured
  as a state machine in 0.9.0.
- `src/core/MtkCrypto.c`, `src/core/MtkCrypto.h`: adapted from `itl_wpa.cpp` / `itl_wpa.h` (SHA-1, HMAC, PBKDF2,
  the 802.11 PRF, AES-128, RFC 3394 key wrap). Whole files under the Clear BSD licence; the notice is
  in `src/core/MtkCrypto.h`.
- `src/core/MtkSupp.c`, `src/core/MtkSupp.h`: the EAPOL-Key frame handling started from `m3Handshake` in
  `itlmt7925_m3.cpp` and was restructured as an event-driven machine. Dual-licensed ISC AND
  BSD-3-Clause-Clear. The rules it enforces are from IEEE 802.11-2020 12.7.6 and from reading hostap's
  `wpa_supplicant` (`src/rsn_supp/wpa.c`); no hostap code was copied.

Those parts stay under the Clear BSD licence, whose full text and copyright notice are in
`LICENSES/BSD-3-Clause-Clear-itlmt7925.txt`. Every value taken from itlmt7925 was checked against
Linux; where the two differ, the core follows Linux. itlmt7925 itself credits mt76 for its hardware
layer. Anyone shipping a built WTLWireless.kext must include `LICENSE`, `NOTICE.md` and `LICENSES/`
with it (the Clear BSD licence requires this for binary redistribution).

`tools/mtkwifi/mtkwifi.c` links `src/core/MtkCrypto.c` for the PMK derivation and is otherwise IOKit user
client code under ISC.

## Wi-Fi menu part (`src/native`)

### ga403wm-hackintosh, kexts/itlmt7925Native (BSD-3-Clause-Clear)

Copyright (c) 2026 thanhnndev and contributors. Licence text in `LICENSES/BSD-3-Clause-Clear-itlmt7925.txt`.

Adapted, not copied: the shape of `src/native/WTLWirelessNativeIf.cpp` (interface start with a RegistrationInfo block, packet pool and
queues, the core-gate / work-queue split), the join completion sequence and the event payload layouts in
`src/native/WTLWirelessNativeReq.cpp` (link status 0xd8, assoc 0x4e, join 0xd3, first beacon 0xd4, connect complete 0xd5), the
CoreCapture logger and fault reporter setup in `src/native/WTLWirelessNative.cpp` and `src/native/corecapture_min.h`, and the idea of
generating the request-slot stubs from a dump. Every offset and slot number was re-read from the macOS 26.7.1 (25G241)
binaries; that project targets macOS 15.

### Apple

`abi/24H32/` is generated from thanhnndev's 24H32 (macOS 15.8.1) dump in mt7925-macos-wifi, branch `sequoia`
(BSD-3-Clause-Clear, Copyright (c) 2026 thanhnndev and contributors), turned into the kcabi layout by
`tools/kcabi/import_24h32.py`; the Sequoia payload layouts in `src/native/MtkAbiCompat.h` follow that branch's front end.

The headers in `abi/<build>/` are generated by `tools/kcabi` from the kernel collection of that macOS build: class names,
instance sizes, virtual slot order and method names of IO80211Family, IOSkywalkFamily and corecapture. They contain no
Apple code. `src/native/skywalk_min.h` and `src/native/corecapture_min.h` declare the handful of Apple factory functions this part
calls, with their mangled names.

## Bluetooth part (`src/bt`)

### Linux btmtk (ISC)
The procedure in `src/bt/WTLWirelessBt.cpp` is ported from `drivers/bluetooth/btmtk.c` and `btmtk.h` in Linux v6.18 (https://github.com/torvalds/linux, tag v6.18), SPDX ISC, Copyright (C) 2021 MediaTek Inc.: `btmtk_usb_setup` (the 0x7925 case), `btmtk_setup_firmware_79xx`, `btmtk_usb_hci_wmt_sync`, `btmtk_usb_reg_read`, `btmtk_usb_uhw_reg_read` / `_write`, `btmtk_usb_subsys_reset`, and the register and opcode constants.

### itlmt7925 / MtkBtFw (BSD-3-Clause-Clear)
Cross-checked against and partly adapted from `kexts/MtkBtFw/MtkBtFw.cpp` of https://github.com/thanhnndev/ga403wm-hackintosh, Copyright (c) 2026 thanhnndev and contributors, The Clear BSD License. Adapted parts: the `IOUSBHostInterface` match on interface 0, the open / work / close pattern, the descriptor walk for the interrupt endpoint and the interrupt drain. Their notice is kept in full at the top of `src/bt/WTLWirelessBt.cpp` and in `LICENSES/BSD-3-Clause-Clear-itlmt7925.txt`; any binary distribution of MtkBt must carry it.

## Firmware
`firmware/mt7925/WIFI_MT7925_PATCH_MCU_1_1_hdr.bin` and `firmware/mt7925/WIFI_RAM_CODE_MT7925_1_1.bin` are
MediaTek's, from linux-firmware (https://gitlab.com/kernel-firmware/linux-firmware, commit
d5455717ead0), unmodified and redistributable under `LICENSES/LICENCE.mediatek`. They are not
under ISC. The build embeds them in the core.

`firmware/mt7927/WIFI_MT6639_PATCH_MCU_2_1_hdr.bin` and `firmware/mt7927/WIFI_RAM_CODE_MT6639_2_1.bin` are the MT7927
pair (the chip's die is called MT6639), also MediaTek's and unmodified, from linux-firmware commit 69e51bb26f3f,
`mediatek/mt7927/`, under the same `LICENSES/LICENCE.mediatek`. The build embeds them in the core too; they are only
used on an MT7927 that was opted in.

`firmware/mt7925/BT_RAM_CODE_MT7925_1_1_hdr.bin` is MediaTek's, from linux-firmware (https://gitlab.com/kernel-firmware/linux-firmware, commit d5455717ead0, `mediatek/mt7925/`), unmodified, sha256 `be7c18e37221e277baaef853be3a0ba930138ee879f336ff7b185e79ab09a5c9`, redistributable under `LICENSES/LICENCE.mediatek` (for use with MediaTek devices). It is not under ISC or Clear BSD. The build embeds it in the Bluetooth part.

## Design references (no code copied)
- **itlwm** (https://github.com/OpenIntelWireless/itlwm, GPL-2.0): the idea of a Wi-Fi kext that
  shows up as a plain Ethernet device and does its own 802.11 handling instead of using IO80211Family.
- **RealtekRTL8111** by Laura Müller (Mieze) (https://github.com/Mieze/RealtekRTL8111-OSX): the usual
  shape of an open-source IOEthernetController kext (own work loop, gated output queue, medium
  dictionary, link status), which `src/core/WTLWirelessEth.cpp` follows.
- **hostap wpa_supplicant** (https://w1.fi/wpa_supplicant/, BSD): the handshake rules in `MtkSupp.h`.
- **IEEE 802.11-2020**: management and data frame layouts, LLC/SNAP encapsulation, RSN, the 4-way
  handshake (12.7.6), block ack (9.6.5).
- `src/kmod_info.c` reproduces the `kmod_info` block Xcode generates, using Apple's
  `KMOD_EXPLICIT_DECL` macro from `<mach/kmod.h>`.
- BlueToolFixup (acidanthera/BrcmPatchRAM, created by Dhinak G, GPL-2.0) is what lets `bluetoothd` use the controller after the Bluetooth part has loaded its firmware. Nothing from it is in this repository.

## Build tools (used, not redistributed)
- acidanthera **MacKernelSDK** (https://github.com/acidanthera/MacKernelSDK): Apple kernel and IOKit
  headers, `libkmod`, for the Linux cross-build.
- tpoechtrager **cctools-port** (https://github.com/tpoechtrager/cctools-port): Apple's ld64 on Linux.
- swiftlang **swift-corelibs-libdispatch** (https://github.com/swiftlang/swift-corelibs-libdispatch):
  needed by ld64.
- LLVM/clang from Ubuntu, or Apple clang from the Xcode Command Line Tools on macOS.

## History
The three parts were developed inside a private notebook repository for a Lenovo Legion 5 15IAX10 hackintosh, with
one test boot per version and the OpenCore logs and registry dumps of each boot kept there. The
repository audit of itlmt7925 (cross-checks against Linux) was done before any of it was used.

## Everything else
Copyright (C) 2026 watchthelight, ISC (`LICENSE`).
