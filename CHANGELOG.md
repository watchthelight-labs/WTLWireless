# Changelog

## 0.14.0 (2026-11-10)

One kext in place of three. `WTLWireless.kext` holds the Wi-Fi driver, and inside it, under `Contents/PlugIns`, the Wi-Fi menu part (`WTLWirelessNative.kext`) and the Bluetooth firmware loader (`WTLWirelessBt.kext`). It replaces `MtkWlan.kext` 0.13.0, `MtkWlanNative.kext` 0.4.17 and `MtkBt.kext` 0.2.4. The code of the three is the same as in those versions. What changed is the names, the packaging and the version, which the three parts now share.

Moving from the old kexts (`tools/bundle/INSTALL.md` has the entries in full):

1. Copy `WTLWireless.kext` to `EFI/OC/Kexts`.
2. In `config.plist`, `Kernel > Add`: take out the entries for `MtkWlan.kext`, `MtkWlanNative.kext` and `MtkBt.kext` and add three for the bundle, the core first: `WTLWireless.kext`, `WTLWireless.kext/Contents/PlugIns/WTLWirelessNative.kext`, `WTLWireless.kext/Contents/PlugIns/WTLWirelessBt.kext`. The Wi-Fi menu entry keeps `MinKernel 25.0.0`.
3. Delete the three old kext folders. `boot-args` (`mtkfe=native mtknativeif=1`), the two Bluetooth NVRAM keys, BlueToolFixup and LegionBtFix stay as they are.

What is different:

- Classes and bundle IDs are renamed: `MtkWlan` to `WTLWireless` (org.watchthelight.WTLWireless), `MtkWlanEth` to `WTLWirelessEth`, `MtkWlanUserClient` to `WTLWirelessUserClient`, `MtkWlanNative` to `WTLWirelessNative` (org.watchthelight.WTLWireless.Native), `MtkWlanNativeIf` to `WTLWirelessNativeIf`, `MtkBt` to `WTLWirelessBt` (org.watchthelight.WTLWireless.Bt). In `ioreg -rc` commands the class name changes; the properties (`Mtk*`, `MtkNative*`, `MtkBt*`) and the boot-args keep their names.
- If the old `MtkWlan.kext` is still in the config, the core leaves the card to it and publishes `MtkConflict` on the PCI device with the reason. Two drivers never write to one card.
- `mtkwifi`, `tools/wifi.sh` and `tools/mtk-report.sh` find the driver under either class name.
- The kernel symbols the Wi-Fi menu and Bluetooth parts import are the same as in 0.4.17 and 0.2.4. The core imports two more (`OSMetaClass::getMetaClassWithName` and `OSSymbol::withCString`, for the check above).
- One repository: this one, renamed from WTLMtkWireless to WTLWireless. WTLWifiMenu and WTLMtkBluetooth are archived and point here. Sources are in `src/core`, `src/native` and `src/bt`, and `./build.sh` builds the whole bundle. The histories of the three kexts are further down this file.
- The release is one zip, `WTLWireless-0.14.0-RELEASE.zip`: the kext, `INSTALL.md`, a config snippet and the Recovery folder.

How it was checked: the host harness (`tests/hwtrace`) records 3589 events, and against 0.13.0 the 3573 hardware lines are identical; one status line differs, the one that names the Wi-Fi menu part. With the old class present the harness shows the core touching nothing. Then two boots on the Legion 5 (macOS 26.7.3 developer beta, 25G416): the first with Wi-Fi from the bundle and MtkBt 0.2.4 still loaded, the second with all three parts from the bundle. Wi-Fi joined the saved network at boot both times with the same channel list as 0.13.0, 5 GHz, 80 MHz, MCS 9, 2 streams, 866 Mbit/s. Bluetooth: `MtkBtState` `ready` after 1251 ms (1265 ms with 0.2.4), controller on, the known devices still listed.

Not checked on hardware: pairing a new Bluetooth device, a connection and audio with the loader inside the bundle (nothing in its code changed); the `MtkConflict` path and the Ethernet fallback under the new names (host test only).

Also in this release, changed since 0.13.0:

- Bundle `mt7925-wifi-bundle-0.12.1+0.4.16.zip` (2026-11-10): MtkWlanNative 0.4.16, which loads on the macOS 26.7.3 developer beta (25G416). The driver kext is unchanged.
- `mtkwifi add` no longer fails with `mkdir ... No such file or directory` on a machine that has no `/Library/Application Support/legion` folder yet. The tool now creates that folder before its own ([#6](https://github.com/watchthelight-labs/WTLWireless/issues/6)). The bundle `mt7925-wifi-bundle-0.12.1+0.4.16.zip` has the fixed tool in its Recovery folder; with an older one, run `sudo mkdir -p "/Library/Application Support/legion"` once.
- `tests/hwtrace/`: the host harness behind the "same register accesses" checks of 0.12.2 and 0.13.0 is in the repo. `run.sh`, `compare.sh <tag>` and `selftest.sh`; no kernel, no sudo, no network.
- README: the Legion joins a WPA2 Personal network itself now, so the note that only a tester had done so is gone.
- `tools/bundle/make-bundle.sh` packs the release zip from `out/WTLWireless.kext`; it no longer downloads two kexts.
- `tests/native/`: the channel encoding test of the Wi-Fi menu part.
- The `Makefile` (Linux cross-build) builds the driver alone, into `out/linux/`. It follows the new paths but has not been run on Linux since the move; the release binaries come from `build.sh` on macOS.

## Before the bundle

Until 0.14.0 the three parts were separate kexts with their own versions and repositories. Their changelogs follow as they were; file paths in them are the ones of that time (`src/` is `src/core/` now, and the Wi-Fi menu and Bluetooth sources are in `src/native/` and `src/bt/`).

## MtkWlan, the Wi-Fi core (0.5.0 to 0.13.0)

### 0.13.0 (2026-11-10)

Core API version 6: the driver tells a front end what the chip is. Nothing changes in bring-up, scan, join or the data path, and versions 1 to 5 of the API are untouched, so MtkWlanNative 0.4.16 keeps working.

- `get_chip_info` (`src/MtkCoreApi.h`): chip name, chip ID, PCI device ID, firmware build string, and two sets of capabilities. `hw_*` is what the silicon can do. The plain fields are what this driver version uses: no 6 GHz (it needs WPA3-SAE), two streams, 80 MHz at most, no HE or EHT yet.
- `get_channels2`: the country's channels as `mtk_chan` entries with a band and passive / DFS flags. A channel number alone stops being enough once 6 GHz is on, where 1 to 233 exist again.
- `get_link_chan`: the channel of the association with its band, width and centre.
- Scan results keep `get_bss` and a plain channel number. The driver scans 2.4 and 5 GHz only; a band there comes with the 6 GHz scan.
- How it was checked: the host harness records the same 3589 register accesses and firmware commands as for 0.12.2, and it now runs the new calls next to the old ones while joined (36 channels, all agree). Then one boot on the Legion (macOS 26.7.3 developer beta, 25G416) with MtkWlanNative 0.4.17, which uses version 6: joined at boot, same channel list, 5 GHz, 80 MHz, MCS 9, 2 streams, 866 Mbit/s.

### 0.12.2 (2026-11-10)

No change in what the driver does to an MT7925. This release prepares the code for other MT792x chips.

- Chip table (`src/MtkChip.h`, `src/MtkChip.cpp`). What differs between MT792x chips is now data: PCI IDs, the fixed register map, the firmware blobs, the WFSYS reset register, the RX ring numbers with their interrupt bits, the prefetch table and the capabilities. The table has one row, MT7925. A second chip is a second row plus the code paths only that chip needs.
- The driver picks its row from the PCI device ID before the first register access. A card that has no row is not mapped and not touched; the node shows `MtkChipUnsupported`. A chip ID register that does not match the row gets firmware ownership back and the driver stops there.
- New registry properties for reports: `MtkPciDevice`, `MtkPciSubsystem`, `MtkChipName`, `MtkChipBands` (bit 0 2.4 GHz, bit 1 5 GHz, bit 2 6 GHz), `MtkChipNss`, `MtkChipMaxBw`.
- `MtkFrontEnd` now shows the front end that was asked for after the `mtkfe=` boot-arg. Before, it kept the Info.plist default (`ethernet`) even when the native front end ran, which read as if the Wi-Fi menu were off ([#6](https://github.com/watchthelight-labs/WTLWireless/issues/6)). `MtkFrontEndActive` is still the one that says what runs.
- `tools/build-mac.sh`: `NO_KMUTIL=1` skips the read of the installed kernel collections.
- How it was checked: a host harness ran the bring-up, a scan, a join, a country change, a reload and the shutdown path against a simulated MT7925 and recorded every register access and firmware command, 3589 events. The record is the same before and after the change. Then one boot on the Legion (macOS 26.7.3 developer beta, 25G416, with MtkWlanNative 0.4.16): joined at boot, 5 GHz, 80 MHz, MCS 9, 2 streams, 866 Mbit/s, as with 0.12.1.

### 0.12.1 (2026-10-23)

- Core API version 5 for the native front end ([WTLWifiMenu](https://github.com/watchthelight-labs/WTLWifiMenu) 0.4.13):
  - `get_rate_info` reports the link's PHY (legacy, HT, VHT), MCS, streams, width, guard interval and rate. It uses the firmware's TX status records when there are any, and the best rate the association allows otherwise.
  - `set_country` takes the country macOS asks for. The CLC power table for that country is used, and the channel domain is sent again between scans.
  - Outside the US and its territories, channels 12 and 13 are added as passive, and 149 to 165 become passive too. Passive means listen only until an access point is heard, so a country can only make the radio quieter than the US set, never louder.
- `MtkCountry` property: the current country, its channel count and the firmware's answer.
- Tested on the author's Legion 5 (N-261): joins at boot. macOS shows 11ac, 80 MHz, MCS 9, 2 streams, short GI, 866 Mbit/s for a VHT80 access point. That is the negotiated maximum, because this link produced no TX status records.

### 0.12.0 (built 2026-10-22; first boot 2026-10-23 together with 0.12.1)

- Restart and power off quiesce the chip (interrupts off, driver own, DMA off, WFSYS reset, bus master off). Before this a warm restart could leave the firmware running and the next boot failed with `W3 fail: patch download`, sometimes with the card gone from the bus until a full power off ([#1](https://github.com/watchthelight-labs/WTLWireless/issues/1)).
- Attach recovers a wedged chip: one more WFSYS reset and a second try, then a PCIe function level reset and a third. The FLR idea comes from flxxxxddd's [SimpleMtkWlan PR #7](https://github.com/laobamac/SimpleMtkWlan/pull/7); the code here is our own.
- Link supervisor: a link that stays associated but stops working is noticed within about 8 s. An AP that no longer acknowledges us gets a rejoin; a chip whose firmware or DMA stopped gets a reset, a firmware reload and a rejoin without detaching the interface. Inspired by thanhnndev's link-loss recovery ([ga403wm-hackintosh 5f00557](https://github.com/thanhnndev/ga403wm-hackintosh/commit/5f00557b5ed4377b6cb0f45a4e1c9cab5503163f)). `MtkStallSec` (default 8, 0 = off), boot-arg `mtkstall=`, properties `MtkStall` and `MtkStallPostMortem`.
- `MtkAutoNetworks`: WPA2 networks the driver joins by itself when the native menu is not there (a USB installer, Recovery on another macOS build). `mtkwifi pmk <ssid>` prints an entry. Asked for in [#3](https://github.com/watchthelight-labs/WTLWireless/issues/3).
- `tools/recovery/`: a Terminal helper for Recovery and the installer that scans, joins with a typed password (not stored) and saves a capture. On 26.7.1 Recovery the native Wi-Fi menu should work by itself: its boot kernel collection has the same IO80211Family as the installed system.
- `mtkwifi` builds for macOS 10.15 and later, so it runs from older installers too.

- The BSS cache holds 128 networks instead of 48, and when it is full it drops the oldest network the running scan has not heard. In a crowded place one scan can hear more than 48 networks; the 5 GHz channels are scanned after 2.4 GHz, so the 2.4 GHz finds were the oldest and got evicted before the scan ended. Found and fixed first by thanhnndev in [ga403wm-hackintosh 4257353](https://github.com/thanhnndev/ga403wm-hackintosh/commit/4257353065b34181b42993c946fb0956c47c6e32) (Clear BSD). `MTK_SCAN_MAX` stays 48: it is now only the most results one `get_scan_results` call returns.
- WPA2 joins keep the join ROC until the keys are installed instead of releasing it right after association. Releasing it just before message 2 made the chip miss message 3 and its retries for several ms, so the handshake could end with reason 15. Ported from mt76 [fddc43d](https://github.com/openwrt/mt76/commit/fddc43d51481ca8c0e2d16a88caaca528791f359) (Mark Anthony Agarro, Devin Wittmayer). We had not seen this failure ourselves.

### 0.11.3 (2026-10-18)

- Tx data ring 256 -> 1024 slots. The in-flight cap stays at 128 frames with a Tx block-ack session (32 without).
- New user-client selector `kMtkUcTune` (5, administrator only) sets the in-flight cap live, so a value can be tested without a reboot. Keys in `MtkUserApi.h`; the current caps show in `MtkW6TxInflightCaps`.
- A sweep on the author's laptop (VHT 80 MHz, 2 streams, -53 dBm) gave about 410 Mbit/s of UDP for every cap from 128 to 512 once the system had settled. The first few minutes after login are slower at any cap: macOS scans about 20 times then. 0.11.2, which raised the cap to 512, was never released.

### 0.11.1 (2026-10-18)

- Front-end race fixed. With `mtkfe=native` the core used to start its Ethernet front end after a fixed 15 s and then refuse the native one for the whole boot, so on some boots the Wi-Fi menu was missing and the card showed up as an Ethernet port. The native front end spends 0 to 26 s inside Apple's `IO80211SkywalkInterface` start on 25G241 (five boots measured), so 15 s was a coin flip. A front end can now claim the core at probe (`MTK_CORE_FE_CLAIM` in `MtkCoreApi.h`); the core then waits up to 90 s, and a claimed front end that gives up declines at once. New properties `MtkFrontEndClaimMs` and `MtkFrontEndBoundMs` show the timing. Needs MtkWlanNative 0.4.10 for the claim; with an older front end nothing changes.
- Matches PCI `14C3:0717` as well, the AMD RZ717 flavour of the MT7925 on AM5 desktop boards (same firmware in Linux). Untested on that card so far.
- Tested on the author's laptop with MtkWlanNative 0.4.10 and a forced 25 s front-end delay: the native front end bound after 24.2 s, no fallback.

### 0.11.0 (2026-10-18)

- Core ops table v4 (`MtkCoreApi.h`): `set_mac` changes the station address after bring-up. The association ends first, the firmware station interface is removed and added again with the new address, and the manager rejoins the wanted network. macOS gives a Wi-Fi interface a private address per network; without this the station associated with the old address in the firmware and DHCP never saw a reply.
- `MtkMacAddr` and `MtkMacChanges` properties; `MtkMacChangeRefused` when the firmware rejects an address.
- `tools/mtk-report.sh`: one archive with everything a bug report needs, MAC addresses and SSIDs redacted.
- Ethernet front end unchanged. Tested with the native front end MtkWlanNative 0.4.1 on macOS 26.7.1 (25G241): macOS-driven join with a private address, DHCP lease on it.

### 0.10.0 (2026-10-17)

- Core ops table v3 (`MtkCoreApi.h`): `set_mode`, `get_bss`, `get_channels` and `tx_bytes`, so a native Wi-Fi front end can drive the core without a C++ link.
- Front-end selection: `MtkFrontEnd` in `Info.plist` picks the Ethernet front end (default) or a native front end built against the same ops table; if the chosen front end does not come up the core falls back to Ethernet (`MtkFrontEndFallback`). `OSBundleCompatibleVersion` is set so a separate front-end kext can link against the core.
- `mtkwifi status` reports the active front end and stands down (no join management) when the native front end owns the interface.
- Same behaviour as 0.9.0 on the Ethernet front end. Measured on the same open 5 GHz network: 192.7 Mb/s down, 134.6 Mb/s up (20 MB POST), 16 ms idle ping, 0 % loss, 15 dropped RX frames in the first minutes (0 in the 0.9.0 run; being watched).
- The release binary predates the last source edit of this version (`MtkNet.cpp`, `MtkFrontEndActive` property); see the README for both hashes.

### 0.9.0 (2026-10-17)

- The network name is no longer fixed in `Info.plist`. The driver scans from its 1 ms tick, keeps a cache of the networks it hears (with each one's security), and joins what it is asked to join.
- `tools/mtkwifi`: a command line tool that talks to the driver through an IOUserClient. Scan, list, join, disconnect, status, saved networks (stored as the derived PMK, never the passphrase), and an autojoin LaunchDaemon that joins the first saved network in range at boot.
- WPA2-PSK (CCMP): RSN element in the association request, the 4-way and group key handshakes in the driver, keys installed in the order Linux uses, host-side port gating, CCMP replay check after the block-ack reorder. Passes the host tests in `tests/` against a Python oracle. Not yet joined a real WPA2 network on air (see the README).
- Open-network fallback: when nothing is saved or asked for, the driver joins a network named in `MtkAutoOpenSsids` (exact match) 20 s after its first scan.
- The Ethernet front end starts before any association, so `en1` shows "no carrier" instead of an unknown link.
- Deauth during a join backs off and names a wrong key; legacy association retry on silence; a firmware watchdog stops the machines after three command timeouts in a row.
- Measured on the same open 5 GHz network as before: 190.8 Mb/s down, 174.8 Mb/s up, 16 ms idle ping.

### 0.8.0 (2026-10-16)

- The driver is split into a core (`MtkWlan`, IOService on the PCI device) and an Ethernet front end (`MtkWlanEth`, IOEthernetController) joined by a plain C ops table (`MtkCoreApi.h`), so a later native Wi-Fi front end can attach without a C++ link to the core.
- Native build on macOS with the Command Line Tools (`tools/build-mac.sh`).
- Same behaviour as 0.7.0: 195 Mb/s down, 97 Mb/s up in that boot (upload was a 20 MB POST, too short to settle).

### 0.7.0 (2026-10-15)

- Block ack in both directions: RX reorder (ported from mt76 `agg-rx.c`), TX ADDBA with the firmware building A-MPDUs.
- TX power: the firmware's country limit tables (CLC) and per-rate limits (SKU), sent as Linux mt76 does, with the TLV length fixes from Linux master.
- Station record TLVs in Linux order.
- 193 Mb/s down, 146 Mb/s up on an installed macOS 15.8.1.

### 0.6.1 (2026-10-15)

- Sampled TX status (rate, power, tries) and TX-free attempt counts in the registry.

### 0.6.0 (2026-10-15)

- HT/VHT association (80 MHz, 2 streams) with firmware rate control: 47.4 Mb/s down, 9.2 Mb/s up.

### 0.5.1 (2026-10-15)

- TX frames in flight capped at 32 and the output queue at 64: upload latency 1.2 s to 69 ms.

### 0.5.0 (2026-10-15)

- First working version: PCI attach, firmware download, scan, open-system join, Ethernet interface with DHCP. Safari and an Ookla speedtest from macOS Recovery: 19.4 Mb/s down, 9.6 Mb/s up.

## MtkWlanNative, the Wi-Fi menu part (0.1.0 to 0.4.17)

### 0.4.17 (2026-11-10)

The kext stops assuming an MT7925. With MtkWlan 0.13.0 it asks the driver (core API version 6) and reports what it is told. On an MT7925 macOS sees the same as before.

- The hardware name (`IO80211HardwareVersion`, System Information) is "MediaTek" plus the chip name from the driver.
- The HT and VHT maps in the association details use the driver's stream count.
- The band of each supported channel and of the current channel comes from the driver, not from the channel number.
- With an older MtkWlan (0.12.x and before) it assumes what it did before: MT7925, two streams, 2.4 and 5 GHz, 80 MHz. That path was booted once after the release, with `mtknativeops=5` against MtkWlan 0.13.0 (not with a real 0.12.x driver): same channel list, name and link.
- New properties: `MtkNativeCoreOps` (the API version in use) and `MtkNativeChip`. Boot-arg `mtknativeops=N` stops at an older API version, for tests.
- Not yet: a 6 GHz channel is left out of the list and a 160 MHz link shows as 80 MHz. No driver offers either today, and Apple's channel flags for both are not confirmed yet.
- Booted on 25G416 on the Legion 5 with MtkWlan 0.13.0: `MtkNativeCoreOps` 6, the supported channel list and the hardware name identical to 0.4.16, `wdutil info` shows 5g44/80, 11ac, 866 Mbps.

### 0.4.16 (2026-11-10)

- The macOS 26.7.3 developer beta (25G416) is allow-listed. Its kernel is xnu-12377.161.15.702.13~1, newer than the one 25G332 and the 26.7.3 release (25G342) share, so 0.4.15 stayed out there and the driver fell back to its Ethernet front end with nothing joined. The kcabi dump of 25G416 has the same instance sizes, 50 vtables and request tables as 25G342. Of 2324 compared functions 94 differ: 58 in addresses only, 36 in a log line number, and one, `IO80211InfraInterface::updateCountryCodeProperty`, gained a call to `IO80211_io80211isSensitiveInfoAllowed`, which this kext does not depend on.
- No code change. The headers in `abi/25G416/` differ from 25G332's only in the build name.
- Booted on 25G416 on the Legion 5 with MtkWlan 0.12.1: the native interface came up, macOS joined the saved network at boot, `wdutil info` shows 5g44/80, 11ac, 866 Mbps.
- README: on a build the kext does not know, macOS joins nothing by itself. `sudo mtkwifi join` brings the saved network back on the Ethernet front end.

### 0.4.15 (2026-10-31)

- Link quality goes to macOS. The interface now calls `setLQM` (slot 378, which ends in `IOSkywalkNetworkInterface::reportLinkQuality`) once a second while associated: 100 at -75 dBm or better, 50 down to -85 dBm, 20 below that, and -1 (unknown) when the link drops. Before, `State:/Network/Interface/en1/LinkQuality` was never set, apsd logged `isBadLQ? YES` and `Push is connected? NO`, so Apple push, and with it iMessage, FaceTime and Handoff, did not connect over Wi-Fi. Lead from the 2026-10-31 research run (an itlwm fork calls it the same way); checked live on the Legion before the change.
- Scan results carry each network's real width ([WTLWifiMenu #1](https://github.com/watchthelight-labs/WTLWifiMenu/issues/1)). The chanspec in each scan-result event is built from the HT and VHT Operation elements, in the Broadcom d11ac layout that `ChanSpecGetPrimaryChannel` and `ChanSpecConvToApple80211Channel` decode on 25G332 (center channel, control sideband, 0x1800 for 40 MHz, 0x2000 for 80 MHz). 160 MHz is reported as 80, the width the core joins at. Host test in `tests/chanspec_test.c` checks every case against Apple's decoder math.
- The country code is the associated AP's Country element (802.11d) until macOS sets one, instead of XZ. Report only: the core's regulatory domain does not change.
- The extended BSS info has the AP's HT RX MCS set (two streams) and the VHT MCS map both sides support, instead of "none".
- `build.sh` skips the kernel-collection link check with `NO_LINKCHECK=1`.
- Booted on 26.7.2 (25G332) on the Legion 5 with MtkWlan 0.12.1. `LinkQuality : 100` appears for en1, apsd logs `isBadLQ? NO` and received a push over Wi-Fi, `wdutil info` shows Country Code US and 5g108/80, and a CoreWLAN scan reports 8 of 8 networks at 5 GHz/80 MHz.

### 0.4.14 (2026-10-28)

- macOS 26.7.2 (25G332) is allow-listed ([#4](https://github.com/watchthelight-labs/WTLWireless/issues/4)). The kcabi dump of 25G332's boot kernel collection, taken from Apple's OTA, has the same instance sizes, vtables and 200 request slots as 25G241, and `genhdr.py` makes headers that differ only in the build name (`abi/25G332/`). Every undefined symbol of the kext is exported by the 25G332 collection. Of the functions Apple changed, only `kern_netif_queue_rx_enqueue` (a field moved from +0x4d8 to +0x4e8) touches the receive path, and the kext never calls it directly. Not yet booted on 25G332.
- System Information showed "Firmware Version: MtkWlanNative 0.4.11" on 0.4.12 and 0.4.13: `MTK_NATIVE_VERSION` was not bumped. It reads 0.4.13 now, and `build.sh` stops when it and Info.plist differ (seen in the report on [WTLWifiMenu #3](https://github.com/watchthelight-labs/WTLWifiMenu/issues/3)).
- `tools/mtk-report.sh` also replaces MACs printed without leading zeros (netstat's route table showed the router's), IPv6 addresses and the `.local` host name.
- README: WPA2-PSK from the Wi-Fi menu confirmed on air by oliverchu ([WTLWifiMenu #3](https://github.com/watchthelight-labs/WTLWifiMenu/issues/3)); notes on OpenCore keeping an old `boot-args` and on the leftover "Ethernet 2" service.

### 0.4.13 (2026-10-23)

- The Wi-Fi details in macOS are real now ([WTLWifiMenu #1](https://github.com/watchthelight-labs/WTLWifiMenu/issues/1)). With MtkWlan 0.12.1, `wdutil info` on the author's Legion 5 shows PHY Mode 11ac, Channel 5g100/80, MCS Index 9, NSS 2, Guard Interval 400 and Tx Rate 866 Mbps. 0.4.11 showed 20 MHz and a fixed 54 Mbps.
  - `getMCS` (slot 479), `getMCS_VHT` (482) and `getGUARD_INTERVAL` (478) are answered, and requests 0x37, 0x39 and 0xc3 are no longer refused. The layouts were read from AppleBCMWLANCore on 25G241: getMCS_VHT has the MCS at +4, streams at +8, width in MHz at +0xc and the guard interval in ns at +0x10.
  - The current channel carries its real width and DFS flag, the same way Apple's `ChanSpecConvToApple80211Channel` writes them: 0x4 for 40 MHz (+0x200 when the extension is above), 0x400 for 80 MHz, 0x100 for channels 52 to 144.
  - `getRATE` reports the core's rate instead of the fixed link speed.
  - f0xeri's [qwz-macos](https://github.com/f0xeri/qwz-macos) 0.0.26 showed which requests Tahoe makes before it shows these fields.
- `setCOUNTRY_CODE` goes to the core (MtkWlan 0.12.1 `set_country`) instead of only being recorded. On N-261 airportd never set a country, so this has not run yet.
- A private Wi-Fi address the core refuses is tried again every 2 s, up to 6 times. Before, one refusal (for example during a join) left the firmware on the old address, and a network could look dead until Private Wi-Fi Address was turned off. Reported on an HP OMEN in [Game-5040/hackintosh-hp-8f06-macos26](https://github.com/Game-5040/hackintosh-hp-8f06-macos26).
- Still needs MtkWlan 0.11.x or later. With a core older than 0.12.1 the rate fields fall back to the old answers.

### 0.4.12 (built 2026-10-22; first boot 2026-10-23 together with 0.4.13)

- Pairs with MtkWlan 0.12.0. When the core's link supervisor resets the chip and reloads the firmware, the link goes down and comes back through the existing "link restored by core" path, so the Wi-Fi menu shows a short drop and rejoins.
- Works in macOS Recovery on 26.7.1 (25G241): Recovery's boot kernel collection carries the same IO80211Family 1200.13.1, IOSkywalkFamily and corecapture, so the ABI guard engages there too (checked from the Recovery volume, a boot test is next).

- Up to 128 networks go to the Wi-Fi menu instead of 48, to match the core's larger BSS cache (MtkWlan branch scan-cache-roc). With 48, weaker 2.4 GHz networks could still be left out in a crowded place. Idea from thanhnndev's [ga403wm-hackintosh 4257353](https://github.com/thanhnndev/ga403wm-hackintosh/commit/4257353065b34181b42993c946fb0956c47c6e32).

### 0.4.11 (2026-10-18)

- Tx queues restart on the next 2 ms tick after the core frees a slot. They used to wait for the 10 ms control poll, which capped upload near 190 Mbit/s. Frames refused while the link was down are asked for again when it comes up.
- Tx completion moves the pending list once per tick instead of once per packet.
- Measured on the author's laptop with MtkWlan 0.11.3: about 410 Mbit/s of local UDP on VHT 80 MHz 2x2, gateway ping 3-4 ms (5 ms before).

### 0.4.10 (2026-10-18)

- Claims the core at the end of `probe()` (`MTK_CORE_FE_CLAIM`, MtkWlan 0.11.1), before Apple's interface start can outlast the core's 15 s fallback. Before this, a slow `IO80211SkywalkInterface` start (17.2 s on one boot) let the Ethernet front end win by 230 ms and the Wi-Fi menu stayed empty until the next reboot. Every place `start()` gives up now declines, so the fallback still comes at once when the front end really cannot run.
- Debug boot-arg `mtknativedelay=<ms>` (at most 60000) holds the interface start back, to test the late path.

### 0.4.9 (2026-10-18)

- Link quality update events (0x27, 0x1dc bytes) once a second while the link is up. The Family's assoc timer declares "missed beacons" when its beacon and traffic timestamps are both older than 60 s, and only this event refreshes them; the beacon counter of 0.4.7 feeds a different counter. Proven on the author's laptop: ten quiet minutes without a leave.

### 0.4.8 (2026-10-18)

- Beacon-counter events are posted while the core holds the link, not only while its scan cache entry for the access point is fresh (the firmware keeps the associated AP's beacons to itself on some channels, and the Family raised BeaconLoss again).

### 0.4.7 (2026-10-18)

- Beacon-counter events (0xaf, u32 count at +0x54) once a second while the link is up. Without them the Family raises the fault "missed beacons timeout" about 60 s after every join and leaves the network. Under test.

### 0.4.6 (2026-10-18)

- The candidate BSSID is read at +0x21c (0.4.5 read the wrong field). Confirmed on the author's laptop: the join lands on the access point macOS chose.

### 0.4.5 (2026-10-18)

- `setWCL_ASSOCIATE` passes the first candidate's BSSID to the core (`apple80211AssocCandidates`: count at +0x214, entries of 18 bytes from +0x21c, BSSID at +6). 0.4.4 joined the core's own favourite access point and the Family re-joined every 15 to 60 s. Under test.

### 0.4.4 (2026-10-18)

- CoreCapture pipes and streams are started after the interface has bound the core. Starting them earlier took the native front end past the core's 15 s fallback and the Ethernet front end won the boot.

### 0.4.3 (2026-10-18)

- `IOService::start` on the CoreCapture data pipe, data stream and log pipe. The factories only alloc and init; a fault report completing on an unstarted data pipe panics in `CCDataPipe::enqueueBlob`.

### 0.4.2 (2026-10-18)

- One version string (`MTK_NATIVE_VERSION`), shown by the Family as "Firmware Version".

### 0.4.1 (2026-10-18)

- ioctls 0x170 `SET_MAC_ADDRESS` and 0x22d `DBRG_ENTROPY` admitted on the join path; a refused address answers NotPermitted.

### 0.4.0 (2026-10-18)

- The Family drives the association: `WCL_ASSOCIATE`, `WCL_LEAVE_NETWORK`, `WCL_JOIN_ABORT`, the events 0xd8 / 0x4e / 0xd3 / 0xd4 / 0xd5, `WCL_BSS_INFO`, `WCL_EXTENDED_BSS_INFO` (0x214 bytes on 25G241), `WCL_LINK_STATE_UPDATE`, `WCL_LINK_UP_DONE` and the link-path requests. The core's open-network fallback is off in native mode.

### 0.3.1 (2026-10-18)

- The ioctl allow-list admits `WCL_SCAN_REQ` and `WCL_SCAN_ABORT`; every WCL request is gated by `isCommandProhibited` first.

### 0.3.0 (2026-10-18)

- Scan requests run the core's scan; one 0xc9 event per BSS in the 25G241 layout (IEs at +0x44), then 0xed.

### 0.2.1 (2026-10-18)

- The interface follows the BSD link-layer address into the core (`set_mac`, core ops v4): macOS gives the interface a private address per network and the firmware must answer to it.

### 0.1.0 (2026-10-17)

- Controller and interface, Skywalk packet pool and queues, data path bridged to the core, ABI guard per macOS build.

## MtkBt, the Bluetooth part (0.1.0 to 0.2.4)

### 0.2.4 (2026-10-22)

- Subsystem resets are now capped at 2 per boot across every MtkBt instance, not just per instance ([WTLMtkBluetooth #1](https://github.com/watchthelight-labs/WTLMtkBluetooth/issues/1)). If a reset makes the chip drop off the USB bus, it comes back as a new device, a new MtkBt starts and 0.2.3 would reset it again, round after round. On the board in [WTLMtkBluetooth #1](https://github.com/watchthelight-labs/WTLMtkBluetooth/issues/1) that kept the USB controller busy enough to freeze the keyboard and mouse every few seconds, wired ones included. Once the budget is used up MtkBt gives up and says so in `MtkBtWarmPath`; a full power off clears the chip.

### 0.2.3 (2026-10-21)

- Warm restart fix for a chip that still holds part of the firmware from the previous boot ([WTLMtkBluetooth #1](https://github.com/watchthelight-labs/WTLMtkBluetooth/issues/1)). When sections 0 and 1 came back "already loaded", 0.2.2 sent only section 2, called the download good, and then gave up when FUNC_CTRL got no event (`MtkBtLastError` = `wmt op 6 flag 0: no event`, state `error`, Bluetooth off). A silent FUNC_CTRL after a pass that reused loaded sections now counts as half-loaded: the MT7925 subsystem reset runs, the whole firmware is downloaded again and FUNC_CTRL is retried, within the same limit of 2 resets. Cold boots and passes that downloaded everything themselves behave as before.

### 0.2.2 (2026-10-18)

- Matches MediaTek `0E8D:0717`, the AMD RZ717 card on AM5 desktop boards (first seen on an ASRock X870 Steel Legend WiFi running Tahoe).
- Sleep protection bypass: bit 1 of register `0x18011100` (`MT_HW_EMI_CTL`) is set through the uhw path before every download pass and again after firmware activation, before FUNC_CTRL. This is the linux-bluetooth patch "Bluetooth: btmtk: Fix MT7925 WMT command timeout due to sleep protection" (2026-05-31) for the `0717` id: on some boards the BT core wakes with the protection active, ignores WMT commands and the first download query gets no event. It is a precaution, since the first `0717` report (ASRock X870) loaded its firmware fine without it. A failed register access is logged (`MtkBtSlpProt`) and the start carries on as 0.2.1 did.
- `MtkBtVersion` now reports the real version (it said 0.2.0 in 0.2.1).

### 0.2.1 (2026-10-18)

- Matches every MT7925 Bluetooth USB id the Linux `btusb` driver knows (Foxconn 0489 family, IMC Networks 13D3 family, MediaTek 0E8D:8C38), as three personalities with `idProductArray`. 0.2.0 matched only `0489:E111`. No code change.

### 0.2.0 (2026-10-17)

- Warm-reboot handling: sections the chip reports as loaded are skipped; a half-loaded chip gets the MT7925 subsystem reset and a fresh download (at most 3 passes); a firmware that already answers HCI is left alone.
- WMT FUNC_CTRL 0 at power off, restart and `stop()`, so the next boot starts with the function off.
- More registry properties (`MtkBtStep`, `MtkBtLog`, `MtkBtTries`, `MtkBtWarmPath`, `MtkBtFuncOff`).
- Running daily on macOS 26.7.1 since 2026-10-16.

### 0.1.0 (2026-10-16)

- First version: firmware download, endpoint reset option, FUNC_CTRL 1, close. Brought the controller up on cold boots of macOS 15.8.1.
