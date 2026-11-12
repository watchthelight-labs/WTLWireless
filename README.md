# WTLWireless: MediaTek MT7925 Wi-Fi and Bluetooth for macOS Hackintosh

Renamed from `WTLMtkWireless` on 2026-11-10 (`mt7925-hackintosh-wifi` before that); old links redirect here. From 0.14.0 this is one kext, `WTLWireless.kext`, in place of `MtkWlan.kext`, `MtkWlanNative.kext` and `MtkBt.kext`. Moving over means swapping three entries in `config.plist`: see [Coming from the three old kexts](#coming-from-the-three-old-kexts).

Wi-Fi and Bluetooth for the MediaTek MT7925 (Filogic 360, Wi-Fi 7, PCI `14C3:7925`, also the AMD RZ717 `14C3:0717`) on a
Hackintosh, for OpenCore on macOS Sequoia 15 and Tahoe 26. The driver brings the card up, loads its firmware, scans,
joins open and WPA2-PSK networks and moves traffic at 190 Mb/s down, 175 Mb/s up on a 5 GHz network. macOS has no driver
for this card; the other community projects that target it are in the credits.

On Tahoe macOS treats the card as a real Wi-Fi card: the Wi-Fi icon in the menu bar, the Wi-Fi pane in System
Settings, saved networks, auto-join at boot and a private address per network, without a menu-bar client app or the
command line. SimpleMtkWlan and its forks show the card as Ethernet and join networks through a client app or a tool
(comparison below). The same kext loads the firmware of the card's Bluetooth half, after which `bluetoothd` drives
it like any third-party USB controller: pairing, AirPods, LE devices.

## What is in the kext

One folder goes into `EFI/OC/Kexts`. It holds three kexts, and each gets its own `Kernel > Add` entry:

| Part | Entry | What it does | Needs |
|---|---|---|---|
| Wi-Fi driver | `WTLWireless.kext` | firmware, DMA, scan, join, WPA2, data path; on its own macOS sees an Ethernet port | nothing (no Lilu) |
| Wi-Fi menu | `WTLWireless.kext/Contents/PlugIns/WTLWirelessNative.kext` | macOS's own Wi-Fi menu, settings pane, saved networks, auto-join | the driver; macOS 26.7.1 (25G241), 26.7.2 (25G332), 26.7.3 (25G342) or the 26.7.3 developer beta 25G416 |
| Bluetooth | `WTLWireless.kext/Contents/PlugIns/WTLWirelessBt.kext` | loads the Bluetooth firmware of the same card (USB `0489:E111` and the other MT7925 ids) | BlueToolFixup; on Tahoe also [WTLLegionBtFix](https://github.com/watchthelight-labs/WTLLegionBtFix) |

Each part fails alone. On a macOS build the Wi-Fi menu part was not built for, it stays out and the driver brings up
its Ethernet port by itself. The Bluetooth part links nothing from the driver. The zip on the
[releases page](../../releases) has the kext, the licences and an `INSTALL.md` with the exact `config.plist` lines. A
full EFI that uses it is [WTLLegionOpenCore](https://github.com/watchthelight-labs/WTLLegionOpenCore).

The two earlier repositories, WTLWifiMenu (`MtkWlanNative.kext`) and WTLMtkBluetooth (`MtkBt.kext`), are archived;
their code, history and open issues are here.

## Which cards

| Card | PCI id | State |
|---|---|---|
| MediaTek MT7925 (Filogic 360), Lenovo Legion 5 15IAX10 and other 2025 laptops | `14C3:7925`, subsystem `17AA:E0FF` | tested daily, every number in this README comes from it |
| AMD RZ717 (MT7925 on AM5 desktop boards: ASRock X870 Steel Legend WiFi and others) | `14C3:0717` | matched since 0.11.1; Bluetooth confirmed on an X870, Wi-Fi from the Wi-Fi menu on another AM5 board ([WTLWifiMenu #3](https://github.com/watchthelight-labs/WTLWifiMenu/issues/3)) |
| Other MT7925 boards (different subsystem ids) | `14C3:7925` | should work; an ASUS ROG Strix B860-G desktop reports Wi-Fi and Bluetooth working on 26.7.2 ([#4](https://github.com/watchthelight-labs/WTLWireless/issues/4)), and an HP HyperX OMEN PRO 16 laptop (subsystem `103C:8C38`, Ryzen 9 9955HX) reports the native menu joining at boot on 26.7.2 ([#6](https://github.com/watchthelight-labs/WTLWireless/issues/6)); report either way |
| MediaTek MT7927 (Filogic 380), AMD RZ738 | `14C3:7927`, `14C3:6639`, `14C3:0738` | **in the chip table since 0.15.0, switched off, never run on hardware by me.** Without boot-arg `mtk7927=1` the driver only reports the card and touches no register. Testers wanted: see "Other MediaTek cards" below. No Bluetooth yet (no firmware in linux-firmware) |
| MT7921, MT7921K (RZ608), MT7922 (RZ616), MT7920, MT7902 | `14C3:7961`, `0608`, `7922`, `0616`, ... | **not supported**: different firmware and a different init path. See "Other MediaTek cards" below for what it would take and how to help; until then SimpleMtkWlan and its forks cover those cards |

### Bluetooth ids

The Bluetooth half of the card is a USB device, and MT7925 cards from different makers carry different USB ids
for it. The Bluetooth part matches every id the Linux
`btusb` driver lists for the MT7925 (2026-10): Foxconn `0489:E111 E113 E118 E11E E124 E139 E13A E0FA E10F E110 E116 E14E
E14F E150 E151`, IMC Networks `13D3:3588 3602 3603 3604`, MediaTek `0E8D:8C38`, plus MediaTek `0E8D:0717`, the AMD RZ717
card on AM5 desktop boards (ASRock X870 Steel Legend WiFi and others). Tested: `0E8D:8C38` (HP OMEN 16-ap1xxx on MtkBt 0.2.4, reported in [Game-5040/hackintosh-hp-8f06-macos26](https://github.com/Game-5040/hackintosh-hp-8f06-macos26)), `0489:E111` (the Legion 5 15IAX10) and
`0E8D:0717` (ASRock X870 Steel Legend WiFi, macOS 26.7.1, controller On as THIRD_PARTY_DONGLE), `13D3:3602` (ASUS ROG
Strix B860-G, macOS 26.7.2, [#4](https://github.com/watchthelight-labs/WTLWireless/issues/4)).
A Dell Latitude 5421 on macOS 15.8.1 (24H32) with MtkBt 0.2.4 also reports `ready` ([#10](https://github.com/watchthelight-labs/WTLWireless/issues/10)). The firmware and the
loading protocol are the same for all of them. To see what your card reports:

```
ioreg -p IOUSB -l -w0 | grep -E '"USB Product Name"|"idVendor"|"idProduct"'
```

(ids print in decimal: 1161 = 0x0489, 5075 = 0x13D3, 3725 = 0x0E8D; 0x0717 is 1815). If no MediaTek device shows up,
the USB port it sits on is missing from your USB map, so fix the map before suspecting the loader. On AMD desktop boards
check that USBToolBox actually attached to the chipset controller (a `UTB Version` property on it in ioreg); a map made
on Windows can name the controller by an ACPI path macOS never exposes, and then the chipset ports stay at defaults.
If your MediaTek id isn't in the list above, open an issue with the output and I will add it.

The loader also sets the MT7925 sleep protection bypass that Linux added for the `0E8D:0717` flavour (see the changelog):
with the protection active the chip takes the WMT download command and never answers. It is written for every card as a
precaution; the Legion's `0489:E111` and the first `0717` report both loaded their firmware without it, and a card that
does not answer the register read carries on.

If `MtkBtState` is `ready` and `system_profiler SPBluetoothDataType` still shows the placeholder controller (Chipset
`BCM_4350C2`, Address NULL), the loader did its job and `bluetoothd` is not adopting the dongle: check the two NVRAM
keys from the installation steps (`nvram -p | grep -i bluetooth` must list both), then the BlueToolFixup load.

## Compared with SimpleMtkWlan and AirportMtkWlan

Both projects are good work and they cover cards this one does not. The table is from their READMEs as of
2026-10-29:

| | WTLWireless | SimpleMtkWlan (pengubow and forks) | AirportMtkWlan (pengubow, mnnobre, Arxusion) |
|---|---|---|---|
| Cards | MT7925, RZ717 | MT7921, MT7921K/RZ608, MT7922, RZ616 | MT7922/RZ616 (pengubow), MT7921/RZ608 (mnnobre), MT7921, RZ608 and MT7922/RZ616 ([Arxusion](https://github.com/Arxusion/AirportMtkWlan)) |
| How you join a network | macOS's own Wi-Fi menu and System Settings; networks saved by macOS, auto-join at boot, private address per network | the card is an Ethernet port; `mtkwifi join` on the command line, HeliPort "not tested yet", or the SimpleMTK menu-bar client some forks ship | macOS's own Wi-Fi menu (experimental) |
| macOS | Sequoia 15 and Tahoe 26 for the driver; the Wi-Fi menu on Tahoe 26.7.1 (25G241), 26.7.2 (25G332), 26.7.3 (25G342) and the 26.7.3 developer beta 25G416 | "tested in the macOS 15 installer" | macOS 15.2 to 15.x; the Arxusion line (v1.5.1) says Tahoe 26 |
| Measured | 190.8 / 174.8 Mb/s, 16 ms idle ping (0.9.0, Ethernet front end); the Wi-Fi menu part holds the link, auto-joins at boot | not stated on the page | 83 / 44 Mb/s on a G713PV |
| 802.11 | HT/VHT at 80 MHz; no ax yet | n/ac/ax, 40/80 MHz | as SimpleMtkWlan |
| WPA2-PSK | code passes the host tests; joined on air on the Legion (WPA2 Personal, from the Wi-Fi menu, at boot) and by a tester on an RZ717 board | yes | yes |
| Licence | ISC (plus Clear BSD parts) | GPLv2 | GPLv2 |

On an MT7925 use this driver; with its Wi-Fi menu part it behaves like Apple's own Wi-Fi. On an MT7921 or MT7922 use
SimpleMtkWlan for now and watch the issue linked below.

## Status

Everything below was measured on one laptop (Legion 5 15IAX10, OpenCore 1.0.8) on the same open 5 GHz network, with `tools/wifi.sh`. The Windows driver does about 208/211 Mb/s on that network.

| Version | What it adds | Tested on | Result |
|---|---|---|---|
| 0.15.0 | an MT7927 row in the chip table, off unless boot-arg `mtk7927=1` is set; without it the card is only reported. Nothing changes on an MT7925 | macOS 26.7.3 developer beta (25G416), MT7925 | joins the saved network at boot as before (5 GHz, 80 MHz, MCS 9, 2 streams, 866 Mbit/s); `MtkChipName` `MT7925`; the MT7927 path ran only in the host harness |
| 0.14.0 | one kext: the driver, the Wi-Fi menu part and the Bluetooth loader in `WTLWireless.kext`, under new names and one version. Nothing changes on the air | macOS 26.7.3 developer beta (25G416) | Wi-Fi joins at boot through the Wi-Fi menu part inside the bundle, same channel list and link as 0.13.0 (5 GHz, 80 MHz, MCS 9, 2 streams, 866 Mbit/s); Bluetooth `ready` in 1.3 s, controller on |
| 0.13.0 | core ops v6: a front end reads the chip name, streams, bands and width from the driver (`get_chip_info`), and channels come with a band (`get_channels2`, `get_link_chan`). Nothing changes on the air | macOS 26.7.3 developer beta (25G416) | joins at boot with MtkWlanNative 0.4.17 on v6; same channel list and link as 0.12.2 |
| 0.12.2 | chip table: what differs between MT792x chips is data now, one MT7925 row; a card without a row is left untouched; `MtkFrontEnd` shows the request after the boot-arg. Nothing changes on the air | macOS 26.7.3 developer beta (25G416) | joins at boot with MtkWlanNative 0.4.16; 11ac, 80 MHz, MCS 9, 2 streams, 866 Mbit/s as before |
| 0.12.1 | core ops v5 for the native front end: real rate, MCS, streams, width and guard interval; `set_country`; plus everything in 0.12.0 (restart and power-off quiesce, attach recovery, link supervisor, 128-network scan cache, join ROC held through WPA2) | macOS 26.7.1 (25G241), 26.7.2 (25G332) | joins at boot with MtkWlanNative 0.4.13 (0.4.14 and 0.4.15 on 25G332, 0.4.16 on the beta 25G416); macOS shows 11ac, 80 MHz, MCS 9, 2 streams, 866 Mbit/s (the negotiated maximum, no TX status records on this link) |
| 0.11.0 | core ops v4: `set_mac` changes the station address after bring-up (the firmware station interface is re-added with it), so a native front end can apply the per-network private address macOS picks; `MtkMacAddr`, `MtkMacChanges` properties | macOS 26.7.1 (25G241) | with the native front end MtkWlanNative 0.4.9: Wi-Fi menu, macOS-driven joins with a private address, auto-join at boot, link held for ten quiet minutes; Ethernet front end unchanged |
| 0.10.0 | core ops v3 (`set_mode`, `get_bss`, `get_channels`, `tx_bytes`), front-end selection (`MtkFrontEnd`: Ethernet, native front end optional) with Ethernet fallback, `mtkwifi` shows the front end | macOS 26.7.1 (25G241) | 192.7 Mb/s down, 134.6 Mb/s up, 16 ms idle ping, 0 % loss; same auto join at boot |
| 0.9.0 | scan and join machines, `mtkwifi` tool, saved networks, auto join at boot, WPA2-PSK code | macOS 26.7.1 (25G241) | 190.8 Mb/s down, 174.8 Mb/s up, 16 ms idle ping, 0 % loss; joins the network by itself at boot |
| 0.8.0 | core / front end split, native macOS build | macOS 26.7.1 | 195 / 97 Mb/s |
| 0.7.0 | block ack both ways, TX power tables | macOS 15.8.1 (24H32) | 193 / 146 Mb/s |
| 0.6.0 | HT/VHT association, firmware rate control | macOS 15.4.1 Recovery | 47.4 / 9.2 Mb/s |
| 0.5.0 | first join, Ethernet interface, DHCP | macOS 15.4.1 Recovery | Safari and an Ookla speedtest: 19.4 / 9.6 Mb/s |

Rows up to 0.13.0 are versions of `MtkWlan.kext`, the driver before the bundle.

**WPA2 on air:** the WPA2-PSK code passes the host tests in `tests/` (standard test vectors plus simulated handshakes, every derived value checked against an independent Python oracle). The Legion joins a WPA2 Personal network from the Wi-Fi menu at boot since 2026-11-10 (0.12.1 to 0.14.0): the handshake counters show messages 1 to 4 once each and no bad MIC, and the link is 5 GHz, 80 MHz, 866 Mbit/s. Before that I only had an open network, and the first join on air was a tester's, who joined a WPA/WPA2 Personal network from the Wi-Fi menu on an AM5 desktop with an AMD RZ717 card (MtkWlan 0.12.1 and MtkWlanNative 0.4.13), with auto-join at boot and a DHCP lease ([WTLWifiMenu #3](https://github.com/watchthelight-labs/WTLWifiMenu/issues/3)). More reports are welcome.

### Wi-Fi menu part

Versions before 0.14.0 in this table are those of `MtkWlanNative.kext`. Tested by me on one laptop (Legion 5 15IAX10,
OpenCore 1.0.8) on macOS 26.7.1 (25G241), 26.7.2 (25G332), 26.7.3 (25G342) and the 26.7.3 developer beta (25G416).

| Version | What it does | Result |
|---|---|---|
| 0.15.0 | version only; same code as 0.14.0 | joins the saved network at boot on 25G416, 5g157/80 at 866 Mbps |
| 0.14.0 | moved into the bundle as `WTLWirelessNative.kext`; same code as 0.4.17 | joins the saved network at boot on 25G416, 5g44/80 at 866 Mbps; `MtkNativeCoreOps` 6 |
| 0.4.17 | chip name, stream count and channel bands come from the driver (MtkWlan 0.13.0, core API v6); nothing hard-codes an MT7925 any more | same channel list, name and link as 0.4.16 on 25G416; `MtkNativeCoreOps` 6 |
| 0.4.16 | allow-list and headers for the 26.7.3 developer beta (25G416); same code as 0.4.15 | joins the saved network at boot on 25G416, 5g44/80 at 866 Mbps |
| 0.4.15 | link quality to macOS (`setLQM`), real width for every scan result, the AP's country code, HT and VHT maps in the extended BSS info | `LinkQuality : 100` for en1, apsd `isBadLQ? NO` and a push received over Wi-Fi (before, push never connected, so iMessage, FaceTime and Handoff did not work on this card); Country Code US instead of XZ; scan results at 5 GHz/80 MHz instead of 20 |
| 0.4.13 | real width, MCS, streams, guard interval and rate in the Wi-Fi details (needs MtkWlan 0.12.1); country code goes to the core; a refused private address is retried | `wdutil info` shows PHY 11ac, 5g100/80, MCS 9, NSS 2, guard interval 400, 866 Mbps; joins at boot |
| 0.4.12 | pairs with MtkWlan 0.12.0 (link supervisor reset comes back through the restored-link path); 128 networks to the menu | booted together with 0.4.13, joins at boot |
| 0.4.9 | link quality update events (0x27) once a second, which refresh the timestamps the Family's assoc timer checks | **stable**: macOS auto-joins at boot, holds the link (ten quiet minutes, zero leaves), DHCP, real default route, internet for apps. 0.4.7 and 0.4.8 posted beacon counters only and the Family still raised "missed beacons timeout" 60 s after each join |
| 0.4.8 | beacon-counter events while the core holds the link | still lost the link every 60 s: the beacon counter feeds a different check |
| 0.4.6 | the join lands on the access point macOS chose (the candidate BSSID at +0x21c) | confirmed; 0.4.4 and 0.4.5 re-joined every 15 to 60 s because the core joined its own favourite BSS |
| 0.4.4 | CoreCapture pipes started after the core binding (an earlier order lost the core's 15 s front-end race) | macOS joined the network by itself at boot, lease, real default route, internet for apps |
| 0.4.3 | CoreCapture data pipe and stream started (a completed fault report panicked in `CCDataPipe::enqueueBlob` on an unstarted pipe) | no panic |
| 0.4.1 | the join path: `WCL_LOW_LATENCY_INFO`, `SET_MAC_ADDRESS` (private address into the firmware), `WCL_ASSOCIATE`, the link status / assoc / join / first beacon / connect complete events, BSS info and extended BSS info answers, link state update, link up done | macOS-driven join of an open network, DHCP on the private address, network saved and auto-joined on later boots |
| 0.3.1 | scan requests answered with one scan-result event per BSS in the 25G241 layout | airportd parsed 48 networks with security, channel, phy and RSSI |
| 0.2.1 | the station address follows the interface (macOS sets a private address per network; the firmware must answer to it) | DHCP works on the native data path |
| 0.1.0 | controller and interface come up, Skywalk packet pool and queues, data path bridged to the core | packets flow |

WPA2-PSK through the Wi-Fi menu works: oliverchu joined a WPA/WPA2 Personal network from the menu on an AM5 desktop
(AMD RZ717 card, MtkWlan 0.12.1 and MtkWlanNative 0.4.13, macOS 25G241), with auto-join at boot and a DHCP lease
([WTLWifiMenu #3](https://github.com/watchthelight-labs/WTLWifiMenu/issues/3)). On 26.7.2 (25G332) a user with an ASUS ROG
Strix B860-G desktop (on-board MT7925) and a user with an HP Omen 16 (board 8F06) report Wi-Fi and Bluetooth working
with 0.4.14 and LegionBtFix 0.3.0 ([#4](https://github.com/watchthelight-labs/WTLWireless/issues/4)).

### Bluetooth part

Versions before 0.14.0 in this table are those of `MtkBt.kext`.

| Version | Tested on | Result |
|---|---|---|
| 0.15.0 | Legion 5 15IAX10, macOS 26.7.3 developer beta (25G416), OpenCore 1.0.8 | version only; `MtkBtState` = `ready`, controller on. An MT7927's Bluetooth is not loaded |
| 0.14.0 | Legion 5 15IAX10, macOS 26.7.3 developer beta (25G416), OpenCore 1.0.8 | moved into the bundle as `WTLWirelessBt.kext`; same code as 0.2.4. `MtkBtState` = `ready` in 1251 ms, controller on, known devices still listed. Pairing and audio were not repeated for this release |
| 0.2.4 | Legion 5 15IAX10, macOS 26.7.1 (25G241) and 26.7.2 (25G332), OpenCore 1.0.8 | loaded on the Legion since 2026-10-23; changes since 0.2.0 are for other boards (more USB ids, the AMD RZ717 sleep-protection bypass, a warm-restart fix and a per-boot reset cap, reported in [#9](https://github.com/watchthelight-labs/WTLWireless/issues/9)) and are not exercised on the Legion's own chip |
| 0.2.0 | Legion 5 15IAX10, macOS 26.7.1 (25G241), OpenCore 1.0.8, BlueToolFixup 2.7.2 | controller on at every boot since 2026-10-16; AirPods Pro pair, connect and play; `MtkBtState` = `ready`, 3 sections and 447,872 bytes sent, about 20 s from open to close on the current boot |
| 0.1.0 | same laptop, macOS 15.8.1 (24H32) | first version that brought the controller up on cold boots |

What the firmware answers for the FUNC_CTRL status word is not what Linux expects: this chip and firmware (build 20260813113236) return `0xffff` and Bluetooth still comes on, so the value is only recorded.

The MT7925 has two things `bluetoothd` on macOS Tahoe does not expect (an LE connection quirk that crash-loops `bluetoothd`, and a Read Clock reply that leaves AirPods silent). Those are not firmware loading problems and live in a separate Lilu plugin, [WTLLegionBtFix](https://github.com/watchthelight-labs/WTLLegionBtFix). You want both.

## What it can't do

- The Wi-Fi menu part runs on the macOS builds it was built against (26.7.1 to 26.7.3 and the 26.7.3 developer beta 25G416). On any other build macOS sees an Ethernet card and you pick networks with the `mtkwifi` command line tool: no Wi-Fi icon and no Wi-Fi pane in System Settings until a release for that build is out.
- On a macOS build the Wi-Fi menu part does not cover (26.0 to 26.6, Sequoia), xiaoyizpast's [WiFi Menu](https://github.com/xiaoyizpast/Hackintosh-OMEN-8f06/tree/main/wifi-menu) puts `mtkwifi` behind a menu bar icon: scan, join, disconnect ([#7](https://github.com/watchthelight-labs/WTLWireless/issues/7)). It is a third-party app (MIT), tested by its author on macOS 26.0; I have not reviewed it.
- With the Wi-Fi menu: no AirDrop, Sidecar or Continuity Camera (there is no AWDL interface, [#8](https://github.com/watchthelight-labs/WTLWireless/issues/8)). The captive-portal flow is macOS's own: on a portal network the Captive Network Assistant opens and the interface is ranked "Never" until you log in, as on a real Mac.
- It can't join WPA3-SAE only, WPA2 with PSK-SHA256 only, networks that require management frame protection, WPA (TKIP pairwise) or enterprise (802.1X) networks. `mtkwifi list` marks these.
- No roaming between access points of one network. No HE (802.11ax) or EHT; the association is VHT at 80 MHz.
- No sleep and wake handling, for Wi-Fi or Bluetooth. The laptop this runs on has sleep disabled. At power off and restart the Bluetooth loader turns the function off so the next boot starts clean.
- Three firmware command timeouts in a row stop the driver until a reboot.
- RX is polled every millisecond from a timer, not driven by the interrupt.
- The Bluetooth part only loads firmware. Everything after that (pairing, audio, LE) is Apple's `bluetoothd` with BlueToolFixup's patches, and whatever the MT7925 firmware does with it.
- Bluetooth: only chip id 0x7925 is touched; anything else on a matched USB id is left alone.
- The Bluetooth recovery path for a half-loaded chip (subsystem reset after a warm reboot that left the firmware running) is implemented from the Linux driver. It has not triggered on the Legion; on the ASUS B860 board in [#4](https://github.com/watchthelight-labs/WTLWireless/issues/4) it ran once on MtkBt 0.2.4 (`MtkBtWarmPath` = `no WMT event, subsys reset`, 2 tries) and the controller came up `ready`.
- One Bluetooth firmware build and one Wi-Fi firmware pair, the ones embedded here.
- Tested by me on one laptop and one card revision; the other machines named in this README are user reports.

## Installation

The zip from the [releases](../../releases) holds `WTLWireless.kext`, the licences (`LICENSE`, `NOTICE.md` and
`LICENSES/` have to stay next to the binary), a `config-snippet.plist` and `INSTALL.md` with these steps.

1. Copy `WTLWireless.kext` to `EFI/OC/Kexts/`, whole. The two inner kexts stay inside it.
2. Add three entries to `config.plist`, `Kernel > Add`, in this order. OpenCore loads a plugin only when it has an
   entry of its own, and the driver must come before the Wi-Fi menu entry:

   ```
   BundlePath      WTLWireless.kext
   ExecutablePath  Contents/MacOS/WTLWireless
   PlistPath       Contents/Info.plist
   MinKernel       (empty)
   MaxKernel       (empty)
   Enabled         true

   BundlePath      WTLWireless.kext/Contents/PlugIns/WTLWirelessNative.kext
   ExecutablePath  Contents/MacOS/WTLWirelessNative
   PlistPath       Contents/Info.plist
   MinKernel       25.0.0
   MaxKernel       25.99.99
   Enabled         true

   BundlePath      WTLWireless.kext/Contents/PlugIns/WTLWirelessBt.kext
   ExecutablePath  Contents/MacOS/WTLWirelessBt
   PlistPath       Contents/Info.plist
   MinKernel       21.0.0
   MaxKernel       (empty)
   Enabled         true
   ```

   None of the three needs Lilu: the driver depends on Apple's IONetworkingFamily and IOPCIFamily, the Wi-Fi menu
   part on IO80211Family and IOSkywalkFamily, the Bluetooth part on IOUSBHostFamily. The driver is tested on Darwin
   24 (macOS 15) and 25 (macOS 26). Leave an entry out if you do not want that part.
3. For the Wi-Fi menu, add `mtkfe=native mtknativeif=1` to `boot-args` (`NVRAM > Add >
   7C436110-AB2A-4BBB-A880-FE41995C9F82`). Without them the driver keeps its Ethernet front end and the Wi-Fi menu
   part stays idle. OpenCore's `NVRAM > Add` does not replace a `boot-args` that is already in NVRAM (the OpenCore log
   says `ignored, exists`), so the new flags can be silently skipped: list `boot-args` under `NVRAM > Delete` for the
   same GUID, or pick Reset NVRAM in the OpenCore picker once. Make sure the card's PCI slot is not in a
   DeviceProperties entry that disables it.
4. For Bluetooth, install BlueToolFixup (from acidanthera's BrcmPatchRAM release), which needs Lilu, before the
   Bluetooth entry. Its changelog for 2.7.0 asks for two NVRAM keys so `bluetoothd` accepts a third-party controller;
   add them to `NVRAM > Add` and `NVRAM > Delete` under `7C436110-AB2A-4BBB-A880-FE41995C9F82`:

   ```
   bluetoothExternalDongleFailed      Data  00
   bluetoothInternalControllerInfo    Data  0000000000000000000000000000 (exactly 14 zero bytes, 28 hex digits; 16 bytes makes bluetoothd log "CFDataGetLength(internalControllerInfoRef) != 14" and fall back to the UART placeholder)
   ```

   On Tahoe also add [WTLLegionBtFix](https://github.com/watchthelight-labs/WTLLegionBtFix) after BlueToolFixup.
   Without it `bluetoothd` crashes every few seconds once an LE device is around (the MT7925 rejects an LE connection
   that uses a random address `bluetoothd` never set), and Bluetooth turns itself off a moment after it comes up. Seen
   on the Legion and on an ASRock X870. The USB port the Bluetooth device sits on has to be enabled in your USB map.
   On the Legion 5 15IAX10 it is `HS14`, which Lenovo's firmware marks as not connectable, so the port needs a `_UPC`
   patch (in the OpenCore config of [WTLLegionOpenCore](https://github.com/watchthelight-labs/WTLLegionOpenCore)).
5. Reboot and check:

   ```
   kmutil showloaded | grep WTLWireless
   ioreg -r -c WTLWireless -l -w0 | grep MtkState
   ioreg -rc WTLWirelessBt -l | grep MtkBtState
   system_profiler SPBluetoothDataType | head -20
   ```

   `MtkState` names the bring-up step (`W1` PCI to `W6` interface) and says `pass` or `fail` with the reason; after
   bring-up it reads `W6 interface attached (link down until a network is joined)`, then `W5 pass: associated to ...`
   and `W6 pass: en1 passing traffic` once joined. The Wi-Fi icon appears in the menu bar and you join from there,
   like on a Mac. For Bluetooth, expect `MtkBtState` = `ready` and, in `system_profiler`, a real address, State On and
   Chipset `THIRD_PARTY_DONGLE`. If `MtkBtState` is `error`, `MtkBtLog` and `MtkBtStep` say where it stopped.
6. Rollback: set `Enabled` to false on the entries, or boot from a backup EFI. Nothing is written to NVRAM, to flash
   or to the chip's non-volatile storage.

### Coming from the three old kexts

WTLWireless 0.14.0 is `MtkWlan.kext` 0.13.0, `MtkWlanNative.kext` 0.4.17 and `MtkBt.kext` 0.2.4 in one bundle under
new names.

1. Copy `WTLWireless.kext` to `EFI/OC/Kexts/`.
2. In `Kernel > Add`, remove the entries for `MtkWlan.kext`, `MtkWlanNative.kext` and `MtkBt.kext` and add the three
   above, the driver's where `MtkWlan.kext` was.
3. Delete the three old kext folders.
4. Leave the rest alone: `boot-args`, the Bluetooth NVRAM keys, BlueToolFixup and LegionBtFix do not change, and
   macOS keeps its saved networks and paired devices.

Do not run old and new together. If `MtkWlan.kext` is still loaded, the new driver leaves the card to it and says so
in the `MtkConflict` property of the card's PCI device. In commands, the class names are now `WTLWireless`,
`WTLWirelessNative` and `WTLWirelessBt`; the property names (`MtkState`, `MtkNative...`, `MtkBt...`) are unchanged.

### Without the Wi-Fi menu

On Sequoia, or on a Tahoe build the Wi-Fi menu part does not cover, the driver shows an Ethernet port. Build the tool
and save your network (it asks for the passphrase at the prompt and stores only the derived PMK, root-only):

```
cd tools/mtkwifi && ./build.sh && sudo bash install.sh
sudo mtkwifi add "<your SSID>"          # WPA2-PSK; add --open for an open network
sudo mtkwifi join "<your SSID>"         # or just reboot: the autojoin daemon joins the first saved network in range
mtkwifi status
```

`tools/mtkwifi/README.md` has the full command list, and the tool's undo lines are in the header of
`tools/mtkwifi/install.sh`. The release zip carries a built `mtkwifi` in its `Recovery` folder.

If the card ran as an Ethernet port before, macOS keeps that port's network service (often named "Ethernet 2") and
moves it to the Wi-Fi port. The link works, but the service keeps its Ethernet name and System Information can still
say "Network Service Inactive" while you are connected. `networksetup -removenetworkservice` refuses it because it is
the only service on the Wi-Fi port. Renaming it fixes the name without dropping the link
(`networksetup -renamenetworkservice "Ethernet 2" "Wi-Fi"`).

## Properties

Set in the core's `Info.plist` (`WTLWireless.kext/Contents/Info.plist`) under the `WTLWireless` personality (edit the copy inside the kext or rebuild):

| Property | Default | Effect |
|---|---|---|
| `MtkAutoOpenSsids` | `Boingo 5GHz`, `Boingo` | Open networks the driver joins by itself when nothing is saved or asked for (exact names, 20 s after the first scan). Put your café's name here, or empty the list. |
| `MtkPower` | true | Send the firmware's country limit tables and per-rate TX power limits (US, indoor). Off means the firmware's conservative default power. |
| `MtkRxBa`, `MtkTxBa` | true | Block ack in each direction. Off declines ADDBA requests (slower, as in 0.6.x). |
| `MtkHt` | true | HT/VHT association. Off joins with legacy rates only. |
| `MtkMaxBw` | 2 | Widest channel: 0 = 20 MHz, 1 = 40 MHz, 2 = 80 MHz. |
| `MtkDataRate` | 0 | 0 lets the firmware pick the rate; 18 to 25 forces a fixed legacy rate (debugging). |

## Diagnostics

`ioreg -r -c WTLWireless -l -w0` holds every counter the driver keeps: `MtkState`, `MtkScan` (the cached networks with channel, RSSI and security), `MtkW1Join` (join results), `MtkW2Sec` (handshake messages, MIC failures, replays, drops), `MtkCm` (what the connection manager decided), `MtkW2Cmd` (firmware command timeouts), and the data path and block-ack sets. It never contains a key or passphrase. `mtkwifi status --json` gives the same picture from user space. `tools/wifi.sh` runs the measurements the table above came from. `tools/mtk-report.sh` packs all of it for a bug report (see the section below). The `docs/HOW-IT-WORKS.md` property list explains each one.

`ioreg -r -c WTLWirelessNative -l -w0` holds the state of the Wi-Fi menu part: `MtkNativeTrace` (what happened, in order),
`MtkNativeCalls` (every request the Family made, by slot or ioctl code, and every event we posted), `MtkNativeScan`,
`MtkNativeJoin`, `MtkNativeMacFollow`, the data path counters `MtkNativeTx` / `MtkNativeRx`, and `MtkNativeAbi`. For
the Family's own view: `log show --last 5m --info --predicate 'process == "airportd" OR process == "kernel"' | grep -E
'IOC DEBUG|AUTO-JOIN'`. `docs/NOTES-NATIVE.md` is the lab notebook: what each version found in Apple's binaries.

Bluetooth: `ioreg -rc WTLWirelessBt -l | grep MtkBt`

| Property | Meaning |
|---|---|
| `MtkBtState` | `starting`, `loading`, `ready`, `error`, `stopped`, `off` (FUNC_CTRL 0 sent) |
| `MtkBtTries` | download passes in this start (1 normally, up to 3 with the half-loaded recovery) |
| `MtkBtLastError` | last error line; kept after a successful recovery, so it shows what triggered it |
| `MtkBtStep` | last step entered: `open`, `chip id`, `slpprot bypass`, `download`, `hci probe`, `subsys reset`, `ep reset option`, `func ctrl`, `close`, `done` |
| `MtkBtLog` | every step and error, `;` separated (first 1.5 KB kept) |
| `MtkBtVersion` | version of the bundle |
| `MtkBtFirmware`, `MtkBtFirmwareSha256`, `MtkBtFirmwareBytes` | the embedded blob |
| `MtkBtFirmwareBuild`, `MtkBtFirmwareHwSw` | build time and hw/sw version from the blob header |
| `MtkBtChipId`, `MtkBtFwVersion`, `MtkBtFwFlavor` | read from the chip |
| `MtkBtEndpoints` | interrupt IN, bulk IN, bulk OUT addresses of interface 0 |
| `MtkBtOpenTries` | attempts until the interface opened |
| `MtkBtSections`, `MtkBtSectionsSent`, `MtkBtSectionsSkipped`, `MtkBtBytesSent`, `MtkBtBlocksSent` | download counters |
| `MtkBtFuncCtrlStatus` | status word of the FUNC_CTRL event |
| `MtkBtWarmPath`, `MtkBtSubsysReset` | only set when the warm-reboot handling ran |
| `MtkBtSlpProt` | sleep protection bypass: `bypass set`, `was already bypassed`, `read failed`, `write failed` |
| `MtkBtFuncOff` | where FUNC_CTRL 0 was sent (`stop`, `restart`, `power off`) or `failed` |
| `MtkBtIntrPackets` | HCI event packets read and dropped during setup |
| `MtkBtElapsedMs` | time from open to close |

After a restart, skipped Bluetooth sections or a `MtkBtWarmPath` value are normal. On a cold boot expect
`MtkBtChipId` ending in 7925 (31013 decimal), `MtkBtSectionsSent` = 3 and `MtkBtBytesSent` = 447872.

## Other MediaTek cards: what it would take, and how to help

I test this driver on one card (MT7925, PCI `14c3:7925`, subsystem `17aa:e0ff`, in one laptop); users have it running on
an ASUS B860 board and, from 0.11.1, on `14c3:0717`, the AMD RZ717 card on AM5 desktop boards (see Which cards). The chip table in
`src/core/MtkChip.cpp` has two rows, the MT7925 and (since 0.15.0, switched off) the MT7927, and a card without a row is left untouched, on purpose: the MT7921 and MT7922 (RZ608, RZ616) need
their own firmware files (`WIFI_RAM_CODE_MT7961_1.bin` and the MT7961 patch instead of the MT7925 pair), a few MCU
commands in the older non-UNI form, and their own TX power tables, and a kext that loads on a card it cannot bring up
would cost you the Wi-Fi SimpleMtkWlan gives you today. The port is tracked in
[issue #2](https://github.com/watchthelight-labs/WTLWireless/issues/2) with the file-by-file plan; it needs
a card to test on, so if you have an MT7921 or MT7922 and an OpenCore machine, say so there. If you try the kext on a
different MT7925 board, I want the result either way.

**MT7927 and RZ738 (PCI `14C3:7927`, `6639`, `0738`).** The row is written from Linux's mt76 driver and checked in a host
harness against a hand-written list of what Linux does, and that is all: I have no such card. So it is off. Two steps,
and the first one leaves the card alone:

- Install the kext as it is. The driver sees the card, publishes `MtkChipMatched` and `MtkChipUnsupported`, and does
  not touch it. Send the report from step 2 below to [issue #2](https://github.com/watchthelight-labs/WTLWireless/issues/2):
  it tells me the PCI ids and the subsystem of your card.
- To run it, add `mtk7927=1` to `boot-args`. Keep a second way to boot first (a USB stick with your old config): an
  untested bring-up can hang the boot. Then send the report again, pass or fail; `MtkState` holds the step it reached.

The report, for any card:

1. Install the kext as above and reboot. Wait a minute after the desktop appears.
2. Run `bash tools/mtk-report.sh` (no sudo). It writes `~/Desktop/mtk-report-<date>.tar.gz` with: the PCI ids of the
   MediaTek device, loaded kexts, every `ioreg` property of the driver and the Wi-Fi menu part, interfaces and
   routes, `mtkwifi status --json` and `mtkwifi list`, the Wi-Fi system profile, the kernel log lines of the driver and
   the IO80211 family, the Wi-Fi daemons' log, and the last three kernel panic reports if any. MAC addresses and SSIDs
   are replaced by placeholders (`--no-redact` keeps them; `--minutes N` widens the log window, default 30). Nothing in
   it is a key or passphrase.
3. Open an issue at github.com/watchthelight-labs/WTLWireless/issues with the archive attached and three lines: the card
   (PCI ids), the macOS build (`sw_vers`), and what happened (did the kext load, did `mtkwifi list` show networks, did a
   join get a lease). If the machine panicked, say what was on screen; the report includes the panic file.

What the properties mean is in `docs/HOW-IT-WORKS.md`. The usual first failures on a new card are `MtkFwState` (firmware
not found or rejected: the file names in `tools/gen_firmware.py` are for the MT7925) and `MtkW2Cmd` timeouts
(the MCU did not answer: register map or DMA layout).

## Building

On macOS, with the Xcode Command Line Tools:

```
./build.sh                    # out/WTLWireless.kext with both plugins inside; prints the three sha256
bash tests/run.sh             # host tests for the WPA2 code
bash tests/native/run.sh      # channel encoding test of the Wi-Fi menu part
tests/hwtrace/run.sh          # runs the bring-up against a fake MT7925 and records what the driver does to it
tools/bundle/make-bundle.sh   # the release zip from out/
```

`build.sh` stops when the versions of the three parts disagree. The Wi-Fi menu part compiles against the ABI headers
in `abi/<build>/` (25G241, 25G332 and 25G416 ship here; 25G342 has the kernel of 25G332). Those headers are generated
from a dump of the macOS build's kernel collection (class sizes, vtables, the 200 request slots the Family routes
ioctls to); `tools/kcabi/dump.py` makes that dump on the target machine and `tools/kcabi/genhdr.py gen` turns it into
headers for another build. The build then checks that every undefined symbol is exported by the running kernel
collection (`tools/kcabi/linkcheck.py`). `ABI=25G416 ./build.sh` picks a header set by hand; `NO_KMUTIL=1` and
`NO_LINKCHECK=1` skip the two steps that read the installed kernel collections.

The firmware in `firmware/mt7925/` is checked against `firmware/mt7925/SHA256SUMS` and embedded in the binaries. The
build is reproducible: the 0.14.0 release binaries are what `./build.sh` gives from the 0.14.0 tag with Apple clang
21.0.0 (clang-2100.1.1.101), and their sha256 values are in the release notes and in `SHA256SUMS.txt` in the zip.

On Linux (Ubuntu 24.04, also fine in WSL) the `Makefile` cross-builds the driver alone, without the two plugins:

```
sudo bash tools/build-toolchain.sh   # clang, Apple ld64 (cctools-port), libdispatch, MacKernelSDK
make                                 # out/linux/WTLWireless.kext
```

## How it works

The driver claims the card from its firmware, sets up the WFDMA rings, downloads the patch and RAM firmware, and from then on talks to the MCU with the same UNI commands Linux mt76 sends. A 1 ms timer on one work loop drives everything: ring drains, the scan and join state machines, the WPA2 supplicant, the connection manager and block ack. Its own front end translates between Ethernet and 802.11 data frames and opens its port only after the keys are installed.

The Wi-Fi menu part is an `IO80211Controller` plus an `IO80211InfraProtocol` interface built against Apple's IO80211 and Skywalk families. It owns no hardware: it turns the Family's requests into calls on the driver's C ops table (`src/core/MtkCoreApi.h`) and the driver's results into the events macOS expects.

The Bluetooth part runs once per boot in its own kernel thread: open USB interface 0, read the chip id, send each firmware section through WMT PATCH_DWNLD (skipping sections the chip reports as already loaded), write the endpoint reset option, send WMT FUNC_CTRL 1, close the interface. From there `bluetoothd` opens the same interface from user space.

The long version, with the threading rules, why RX is polled and the warm-reboot handling: [docs/HOW-IT-WORKS.md](docs/HOW-IT-WORKS.md).

## Layout

| Path | What |
|---|---|
| `src/core/WTLWireless.cpp`, `MtkChip.cpp`, `MtkRegs.cpp` | PCI attach, chip table, register remap, ownership handshake |
| `src/core/MtkFw.cpp`, `MtkDma.cpp` | rings, MSI, firmware download |
| `src/core/MtkScan.cpp` | MCU commands, hardware init, scan machine, BSS cache, RX dispatch |
| `src/core/MtkJoin.cpp` | join machine, connection manager, BSS/STA records |
| `src/core/MtkSec.cpp`, `MtkSupp.c`, `MtkCrypto.c` | WPA2-PSK glue, supplicant, crypto primitives |
| `src/core/MtkNet.cpp`, `MtkBa.cpp`, `MtkPower.cpp` | data path, block ack, TX power tables |
| `src/core/WTLWirelessEth.cpp`, `MtkCoreApi.h` | Ethernet front end and the C ops table between a front end and the core |
| `src/core/MtkUserClient.cpp`, `MtkUserApi.h` | the IOUserClient the tool talks to |
| `src/native/` | the Wi-Fi menu part: controller, interface, request handlers |
| `src/bt/WTLWirelessBt.cpp` | the Bluetooth firmware loader |
| `abi/<build>/` | generated IO80211 and Skywalk headers per macOS build, for the Wi-Fi menu part |
| `firmware/mt7925/` | MediaTek's Wi-Fi and Bluetooth firmware with checksums |
| `tools/mtkwifi/` | the command line tool and its autojoin daemon |
| `tools/kcabi/` | dumps a build's kernel ABI and generates the headers |
| `tools/bundle/`, `tools/recovery/` | the release zip and its Recovery folder |
| `tests/` | host tests and the Python oracle for the WPA2 code; `tests/native/` for the channel encoding |
| `tests/hwtrace/` | host harness: the real driver sources against a fake MT7925; `compare.sh <tag>` shows whether a change alters any register access or firmware command |

## Credits

Wi-Fi driver:

- Linux **mt76** (MediaTek, Felix Fietkau and contributors, ISC): nearly everything about how the hardware works.
- **itlmt7925** by thanhnndev and contributors (Clear BSD): the join sequence, the post-firmware setup and the WPA2 crypto primitives started there, and the scan-cache eviction fix (2.4 GHz networks lost in crowded places) came from its 4257353.
- **itlmt7922-wifi** by hbyonly1 ([hbyonly1/itlmt7922-wifi](https://github.com/hbyonly1/itlmt7922-wifi), Clear BSD): an MT7922/RZ616 port of itlmt7925 for the older Connac2 firmware family; its porting notes are where MT7922 testers should start.
- **SimpleMtkWlan** fork by eiyuumiru ([eiyuumiru/SimpleMtkWlan](https://github.com/eiyuumiru/SimpleMtkWlan), GPL-2.0, read only): on an RZ616, beacon-loss events (0x13) dropped on the floor left the link up after the AP was gone, and deep sleep switched on before the association finished made joins time out. Both are on our list to check against the 38-minute stall.
- **mtwlm** by HackSucks ([HackSucks/mtwlm](https://github.com/HackSucks/mtwlm), GPL-2.0, read only): a port of OpenBSD's `mwx` driver for MT7921, MT7922 and MT7925 with an IO80211 front end on Big Sur to Sonoma; a second reference for how the chip behaves.
- **itlwm** (OpenIntelWireless): the idea of a Wi-Fi kext that presents itself as Ethernet.
- **RealtekRTL8111** (Mieze): the usual shape of an IOEthernetController kext.
- hostap's **wpa_supplicant**: the handshake rules, read, not copied.
- MediaTek firmware from linux-firmware, unmodified.
- Toolchain: acidanthera **MacKernelSDK**, tpoechtrager **cctools-port**, swiftlang **libdispatch**.

Wi-Fi menu part:

- [oliverchu](https://github.com/oliverchu): the first WPA2-PSK join on air, on a COLORFUL B850M desktop with an AMD
  RZ717 card, and the boot-args and network-service notes under Installation
  ([WTLWifiMenu #3](https://github.com/watchthelight-labs/WTLWifiMenu/issues/3)).
- [ga403wm-hackintosh](https://github.com/thanhnndev/ga403wm-hackintosh) (BSD-3-Clause-Clear, Copyright (c) 2026
  thanhnndev and contributors): the shape of a native front end for a MediaTek core on macOS 15 (the Wi-Fi menu part here), the interface start
  sequence, the join event sequence and several payload layouts, re-read against the 25G241 binaries here. See
  `NOTICE.md`.
- [AirportMtkWlan](https://github.com/mnnobre/AirportMtkWlan) and laobamac's itlwm / SimpleMtkWlan (GPL-2.0): read as
  references for what macOS expects of a Wi-Fi driver; no code from them is in this repository.
- [qwz-macos](https://github.com/f0xeri/qwz-macos) (GPL-2.0, f0xeri): an independent native front end on 25G241 for a
  Qualcomm WCN7850 core; its current-channel width flags and its power-state replay after wake were read as references;
  no code from it is in this repository.
- [mt7925-macos-wifi](https://github.com/thanhnndev/mt7925-macos-wifi) (BSD-3-Clause-Clear, thanhnndev): the itlmt7925
  driver split out of ga403wm-hackintosh, with per-build ABI folders like ours and AWDL work in progress that we follow
  for [#8](https://github.com/watchthelight-labs/WTLWireless/issues/8); no code from it is in this
  repository.
- [AirportRTW89-MT7922](https://github.com/DGBooth/AirportRTW89-MT7922) (GPL-2.0, DGBooth): MT7922 as a native AirPort
  device on Tahoe through OCLP's legacy IO80211 stack, the other route to the Wi-Fi menu; no code from it is in this
  repository.
- [RTL8814Native](https://github.com/DrogaBox/RTL8814Native) (GPL-2.0, DrogaBox): a native IO80211 and Skywalk front
  end for the USB RTL8814AU on 26.7.1 that dumps IO80211's vtables per build and gates its build on them, the same
  per-build pin we use; an independent dump to check ours against; its AWDL notes (the role 6 creation path, the
  wifip2pd registration contract and what the kernel peer manager needs from a driver; since 2026-10-30 its layer
  creates `awdl0` and gets the peer manager through configure and enableAWDL on 25G241, with no peer in the air yet)
  are read as references for
  [#8](https://github.com/watchthelight-labs/WTLWireless/issues/8); no code from it is in this repository.
- [owly](https://github.com/t4t5/owly) (MIT, t4t5): AirDrop and AWDL for non-Apple Wi-Fi cards on Linux. It lists the
  MT7925 as working in shared mode and ships iOS 26 captures taken on an `mt7925e`, plus a protocol spec
  (`docs/SPEC.md`); read as a reference for what the radio has to send for [#8](https://github.com/watchthelight-labs/WTLWireless/issues/8);
  no code from it is in this repository.
- [AirPort-RTW](https://github.com/JoMei9019-real/AirPort-RTW) (GPL-2.0, JoMei9019-real): Realtek PCIe cards as native
  AirPort devices on Tahoe through the same OCLP legacy stack. Its 1.1.0 adds 802.11k/v roaming and an AWDL transmit
  gate, and its install guide lists which legacy components Sonoma to Tahoe need (the system's IOSkywalkFamily blocked,
  OCLP's IOSkywalkFamily and IO80211FamilyLegacy added); no code from it is in this repository.
- Apple's IO80211Family, IOSkywalkFamily and corecapture, read in disassembly to learn the ABI. No Apple code is in this
  repository; the generated headers hold class and method names and slot numbers only.

Bluetooth part:

- Linux **btmtk** (`drivers/bluetooth/btmtk.c`, MediaTek, ISC): the whole procedure and every register and opcode.
- **itlmt7925 / MtkBtFw** by thanhnndev and contributors (Clear BSD): the interface match, the open / work / close shape and the interrupt drain were adapted from it; their notice stays in `src/bt/WTLWirelessBt.cpp`.
- **SimpleMtkBtFw** fork by flxxxxddd ([flxxxxddd/SimpleMtkBtFw](https://github.com/flxxxxddd/SimpleMtkBtFw)): rewriting the ACL packet-boundary flag to first non-flushable and asking Read Clock for clock 1 fixed AirPods audio on an MT7921K. The first is our lead for the A2DP drop; untested on the MT7925.
- **BlueToolFixup** (acidanthera BrcmPatchRAM, Dhinak G): what makes `bluetoothd` accept the controller afterwards.

`NOTICE.md` says which file comes from where.

## Licence

ISC, except `src/core/MtkCrypto.c` / `.h` (Clear BSD, from itlmt7925), `src/core/MtkSupp.c` / `.h` (ISC and Clear BSD), the parts of `src/core/MtkScan.cpp` and `MtkJoin.cpp` that follow itlmt7925 and the parts of `src/bt/WTLWirelessBt.cpp` adapted from MtkBtFw (Clear BSD, notices in the files and in `LICENSES/`). The firmware is MediaTek's under `LICENSES/LICENCE.mediatek`. `LICENSE`, `NOTICE.md` and `LICENSES/` must travel with any built copy of the kext.
