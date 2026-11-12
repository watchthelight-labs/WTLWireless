# How WTLWireless works

This is the theory of operation, written so I can answer "why is it done this way" for any part of it. Where a function
is ported from Linux mt76 or btmtk, or from itlmt7925, the source function is named in a comment next to it and
`NOTICE.md` lists it per file.

## One bundle, three parts

| Part | Bundle | Sources | Attaches to | Does |
|---|---|---|---|---|
| core | `WTLWireless.kext` | `src/core` | the card's PCI device | firmware, DMA, scan, join, WPA2, the 802.11 data path, and an Ethernet port when nothing else takes the front end |
| Wi-Fi menu | `Contents/PlugIns/WTLWirelessNative.kext` | `src/native` | the core | an `IO80211Controller` and interface, so macOS runs the scans and joins itself |
| Bluetooth | `Contents/PlugIns/WTLWirelessBt.kext` | `src/bt` | USB interface 0 of the card's Bluetooth half | downloads the Bluetooth firmware and lets go |

Until 0.14.0 these were three kexts with three versions (MtkWlan 0.13.0, MtkWlanNative 0.4.17 and MtkBt 0.2.4). The
code is the same; the classes and bundle IDs were renamed and the three now share one version. Version numbers below
0.14.0 in `CHANGELOG.md` belong to the old kext they are listed under.

The plugins are ordinary kexts that happen to live inside the core's bundle. OpenCore does not look into `PlugIns` on
its own, so `config.plist` carries one `Kernel > Add` entry per part, the core first. Each part fails alone:

- The Wi-Fi menu part is built against one set of macOS builds and declines on any other. The core then brings up its
  Ethernet port, and `mtkwifi` joins networks from the command line.
- The Bluetooth part needs nothing from the core and can be injected without it. Its entry can also be left out.
- The core never loads next to the old kexts: if the `MtkWlan` class is registered in the kernel, it leaves the card
  to the old driver and publishes `MtkConflict` on the PCI device. Two drivers on one card would both write its
  registers.

The registry properties (`Mtk*`, `MtkNative*`, `MtkBt*`) and the boot-args kept their names in 0.14.0, so scripts and
reports written for the old kexts still read the right values. The classes to ask for are now `WTLWireless`,
`WTLWirelessNative` and `WTLWirelessBt`.

`build.sh` compiles the three parts with the Command Line Tools, links each with its own `kmod_info`, and copies the
two plugins into the core's bundle. It stops when the three `Info.plist` versions, the `Makefile` and the version
strings in the sources disagree. The `Makefile` cross-builds the core alone on Linux.

## The Wi-Fi core (`src/core`)

File names in this part refer to `src/core/`. Where a function is ported from Linux mt76 or from itlmt7925, the source function is named in a comment next to it and `NOTICE.md` lists it per file.

### What macOS sees without the Wi-Fi menu part

The MT7925 is a PCIe device (`14C3:7925`) with one 64-bit memory BAR. Apple's Wi-Fi stack (IO80211Family) is private and tied to Broadcom hardware, so the core does not use it. Instead it does all of 802.11 itself and presents an `IOEthernetController` to macOS. The system sees an Ethernet card named `en1`, runs DHCP on it, and knows nothing about networks, passphrases or signal strength. That is why there is no Wi-Fi menu and why `tools/mtkwifi` exists.

Two classes:

| Class | Base | Files | Owns |
|---|---|---|---|
| `WTLWireless` (the core) | `IOService` | WTLWireless, MtkChip, MtkRegs, MtkDma, MtkFw, MtkScan, MtkJoin, MtkSec, MtkNet, MtkBa, MtkPower, MtkUserClient | PCI attach, BAR0, MSI, DMA rings, the MCU, firmware, scan, join, WPA2, 802.11 TX/RX, block ack, power tables, the work loop and its 1 ms timer, all `Mtk*` registry properties |
| `WTLWirelessEth` (the front end) | `IOEthernetController` | WTLWirelessEth | the `IOEthernetInterface` (`en1`), a gated output queue of 64 packets, the medium, link status, the MAC address |

The personality matches `IOClass WTLWireless` on `IOPCIPrimaryMatch 0x792514c3 0x071714c3`. After bring-up the core creates the front end by class name, attaches it as its child and starts it. The two talk through a plain C table (`MtkCoreApi.h`), fetched with `callPlatformFunction("mtk-core-ops-v2", ...)`. The table is versioned both ways (`version`, `size`) so that a different front end, built as a separate kext, can attach later without a C++ link against the core. The core offers `tx`, `set_enabled`, `get_mac`, `get_link_info`, `scan`, `results`, `join`, `disconnect`, `status`; the front end gives back `rx_alloc`, `rx`, `rx_flush`, `link`, `tx_space`, `scan_done`, `join_result`.

### Bring-up

Everything runs from `WTLWireless::start` on the provider's start thread, then hands over to the 1 ms timer. Each step publishes a registry property (`MtkState` and one `MtkW<n>...` value per step) because the early work was done in macOS Recovery, where the kernel log is tiny and the release kernel ignores `msgbuf=`. `ioreg -r -c WTLWireless -l -w0` still is the main diagnostic.

1. **PCI and registers** (`WTLWireless.cpp`, `MtkChip.cpp`, `MtkRegs.cpp`). Look up the card's PCI device ID in the chip table (`MtkChip.cpp`); the row holds what differs between MT792x chips (register map, firmware, reset register, RX rings, prefetch table), and a card without a row is left alone. A row can be opt-in: the MT7927 row (0.15.0, written from Linux and never run on hardware here) is taken only with boot-arg `mtk7927=1`; without it `start` publishes `MtkChipMatched` and `MtkChipUnsupported` and returns before any register or config-space access. Enable memory and bus master, map BAR0. The chip's register space is reached through two remap windows (L1 and L2) exactly as `mt7925/pci.c` does; `MtkRegs.cpp` keeps the same helper shape. Then the driver ownership handshake from `mt792x_core.c`: the firmware owns the chip at power-on and the host has to claim it before touching WFDMA.
2. **WFDMA rings and MSI** (`MtkFw.cpp`, `MtkDma.cpp`). A WFSYS reset (`wfsysReset`: the reset register on MT7925; on MT7927, which has none, the CBTOP sequence of `mt7927_wfsys_reset`, and that chip also gets a third RX ring, its own prefetch table and two more GLO_CFG bits), then the TX and RX rings (`mt792x_dma.c`, `dma.c`): descriptors in IOKit DMA memory (`IODMACommand`, 32-bit addressable), one MCU command ring, one data ring, the MCU event ring and the data RX ring. MSI is enabled and the handler only counts and acknowledges; all real work happens in the timer. This is the one place the driver is deliberately unlike Linux: RX is polled every millisecond from the work loop, which costs a little latency and keeps every path under one gate.
3. **Firmware** (`MtkFw.cpp`). Two blobs from linux-firmware, embedded at build time by `tools/gen_firmware.py` with their SHA-256 (checked against `firmware/mt7925/SHA256SUMS`, and published in the registry; the MT7927 pair in `firmware/mt7927/` is embedded the same way and the chip row says which pair to send): the patch (`WIFI_MT7925_PATCH_MCU_1_1_hdr.bin`) through the patch semaphore, then the RAM code (`WIFI_RAM_CODE_MT7925_1_1.bin`) region by region, then the "firmware start" command, as in `mt7925/mcu.c` and `mt76_connac_mcu.c`. Done when the firmware reports ready in the MCU state register.
4. **Station setup** (`MtkScan.cpp`, `hwInit`). UNI commands over the MCU ring: NIC capability (which tells the firmware version, the MAC address from EEPROM and the HW type), the EEPROM read, the regulatory domain (`US`), DEV_INFO and the first BSS_INFO for a station interface. The short command sequence after firmware load follows itlmt7925, which was the first driver to get this card up on macOS; every value was checked against Linux and where the two differ the driver follows Linux.
5. **Power tables** (`MtkPower.cpp`, 0.7.0). The RAM firmware carries country limit tables (CLC). The driver finds the region matching the EEPROM HW type and sends the `US` rules with UNI SET_POWER_LIMIT, then per-rate limits (SKU) for every channel, three channels per message, as `mt7925_mcu_set_clc` and `mt7925_mcu_rate_txpower_band` do. Without this the firmware transmits at a conservative default and upload speeds stay low. `MtkPower = false` in `Info.plist` skips it.
6. **Front end** (`WTLWirelessEth.cpp`). Started right after bring-up, before any network exists, with the link published as valid and down. configd then sees "no carrier" and waits, instead of starting DHCP on a dead interface. The MAC address is the firmware's with the locally administered bit set, so the interface address and the on-air address match.

From here on the timer runs: every millisecond it drains the RX rings, dispatches MCU events, and services the scan, join, security, connection manager and block-ack machines in turn.

Steps 2 and 3 live in `hwStart` so they can run again (0.12.0). If they fail at attach, usually because a warm restart left the old firmware running with its DMA pointed at freed memory, `bringUp` does one more WFSYS reset and a second try, then a PCIe function level reset (`pciFlr`, only if the card advertises FLR) and a third. On restart and power off the core is in the power plane and `systemWillShutdown` quiesces the chip on the work-loop gate: interrupts off, driver own, DMA off, WFSYS reset, bus master off, as `mt7925_pci_shutdown` does. `MtkAttachRecover`, `MtkFlr` and `MtkPmJoined` show what happened.

### The link supervisor

`stallService` runs once a second and feeds counters to `MtkSuper.c`, a small state machine with no kernel calls (host-tested in `tests/mtk_super_test.c`). While the port is open:

- No frame received for `MtkStallSec` seconds (default 8): send one null frame with Tx status. Acknowledged means a quiet link and nothing happens. Not acknowledged means the AP dropped us without a deauth, and the link goes down the same way as after a deauth, so the manager rejoins. No status at all means the chip is stuck.
- Frames queued and no Tx token released for `MtkStallSec` seconds, or `fwDead` set: the chip is stuck.

A stuck chip gets `reload`: the link goes down (a native front end sees a link loss), WFSYS reset, the rings reset in place, the firmware loaded again, `hwInit` with the current station address kept, then a scan and a rejoin of the wanted network. The front end and its interface stay attached. At most three reloads per ten minutes. `MtkStall` has the counters and `MtkStallPostMortem` the ring indexes and PCI state from just before the last reload. `MtkStallSec = 0` or boot-arg `mtkstall=0` turns it off, and `kMtkUcTune` key 4 freezes the data ring on purpose to test the path. The idea of a supervisor that resets and reloads comes from thanhnndev's ga403wm-hackintosh [5f00557](https://github.com/thanhnndev/ga403wm-hackintosh/commit/5f00557b5ed4377b6cb0f45a4e1c9cab5503163f).

### Scanning

`scanStart` sends a hardware scan (UNI SCAN, all channels including DFS on 5 GHz, with the SSIDs of saved networks as directed probes). The firmware returns beacons and probe responses as frames on the MCU ring; `rxFrame` classifies them, `mtkParseFrame` pulls out SSID, channel, capability, RSN or WPA element and the HT/VHT elements. Each network goes into a cache of 48 entries keyed by BSSID with its security type (`mtk_rsn_parse` in `MtkSupp.c` decides whether the driver can join it: AKM PSK with pairwise CCMP, group CCMP or TKIP), RSSI from the RCPI in the RX descriptor, and a last-seen time. Entries unseen for 60 s are dropped. Beacons heard while associated keep the cache fresh. Scans start only while idle or connected, never during an authentication, and no join starts while a scan runs.

### Joining

`joinStart` / `joinService` is a state machine with deadlines and four tries per phase:

1. **ROC**: ask the firmware to stay on the AP's channel (remain-on-channel).
2. **Authentication**: open-system auth frame, wait for the response.
3. **Association**: association request with the supported rates, HT and VHT capabilities (80 MHz, 2 streams) and, for WPA2, the RSN element (group cipher = the AP's, pairwise CCMP, AKM PSK). Wait for the response, read the AID.
4. **Handshake** (WPA2 only): see below.
5. **Up**: BSS_INFO with the connected state, STA_REC with the HT/VHT/rate TLVs in the order Linux sends them (the firmware rejects other orders), the EDCA parameters from the association response, then the firmware does rate control. The port opens and the front end reports link up at the negotiated speed.

A deauthentication from the AP during the join counts as a failure. Right after our handshake message 2 it is reported as "wrong passphrase" (that is how most APs answer a bad key); otherwise as "AP left". Both back off.

### WPA2-PSK

Three files, with no OS calls in the first two so they can be built and tested on the host:

- `MtkCrypto.c`: SHA-1, HMAC-SHA1, PBKDF2 (passphrase to PMK), the 802.11 PRF (PMK to PTK), AES-128 and RFC 3394 key unwrap (for the GTK). Checked by `tests/mtk_wpa_test.c` against the FIPS, RFC and IEEE test vectors.
- `MtkSupp.c`: the supplicant state machine for the 4-way handshake and the group key handshake. The rules it enforces come from IEEE 802.11-2020 12.7.6 and from reading hostap's `wpa_supplicant`: a frame is dropped unless its replay counter is above the last one that carried a verified MIC; message 3's ANonce must equal message 1's, its MIC must verify, and the RSN element inside it must equal the AP's beacon element (downgrade check); message 4 is sent before the PTK is installed; a key equal to the installed one is never installed again (the KRACK rule); one SNonce per handshake. The host tests run a simulated authenticator through the normal, retransmit, wrong-key and rekey cases and compare every derived value with an independent Python implementation (`tests/oracle.py`).
- `MtkSec.cpp`: the glue. EAPOL frames from the AP are queued by `rxFrame` and fed to the supplicant from the timer. The supplicant's callbacks only queue a frame to send or stash a key; nothing in them touches the rings or the firmware. `secInstallService` then installs the keys from the timer in the order Linux and itlmt7925 use: wait until message 4 is on the air (its TX-free seen, 100 ms bound), send BSS_INFO with the WPA2 security values, the pairwise key (STA_REC_KEY_V3 on WCID 1), the group key (WCID 19; 16 bytes CCMP or a 32-byte TKIP key with the MIC halves swapped), then open the port. A later group-key handshake installs the new GTK the same way without touching the port.

**Port gating** is the driver's, not the firmware's. While the port is closed `txEthernet` drops everything but EAPOL. Once the PTK is installed every data frame carries the Protected bit and TXD3 PROTECT_FRAME, and the firmware adds the CCMP header and packet number. On receive, protected data must have been decrypted by the hardware with the expected cipher (RXD2 SEC_MODE), plaintext data on a WPA2 association is dropped, and the CCMP packet number (from RX descriptor group 1) is checked when the frame leaves the block-ack reorder buffer in order, as `mt76_check_ccmp_pn` does: one counter per key and TID, reset by a PTK install, seeded from the key RSC by a GTK install. Every drop has a counter in `MtkW2Sec`.

The PMK is derived in user space by `mtkwifi add`, stored root-only, and handed to the driver through the user client at join time. The driver never publishes a key, PMK or passphrase anywhere.

### The connection manager

`cmService` decides what to join. A wanted network (from `mtkwifi join` or a front end's `join`) is kept: rejoined 500 ms after a loss, retried with backoff (5, 10, 20, 40, 60 s) after failures, rescanned every 5 s while out of range (15 s after the first 30 s). When nothing is wanted or the wanted network has been out of range for 30 s, the driver may join an open network whose name is listed exactly in `MtkAutoOpenSsids`, but not before 20 s after the first scan (so the autojoin daemon gets its turn first) and not while a user-space scan is pending. While on that fallback it keeps looking for the wanted network after every scan (one every 30 s) and switches back when it reappears. `mtkwifi disconnect` turns the fallback off until the next `join`.

`MtkAutoNetworks` (0.12.0) does the same for WPA2 networks on machines without the native menu: entries of `Ssid` plus `Pmk` (64 hex digits from `mtkwifi pmk`). When one is in range and nothing is wanted, it becomes the wanted network as if it had been joined through the user client. The keys are removed from the registry after start. Neither list is used while the native front end drives the joins.

### The data path

Ethernet frames from the output queue become 802.11 data frames: `txEthernet` builds the 802.11 header (ToDS, our MAC, the BSSID) and the LLC/SNAP header, then the TX descriptor (`write_txwi` and `write_hw_txp` from mt76) and puts the frame on the data ring. TX completions arrive as TX-free events with the sampled rate, power and try count, which feed the `MtkW8*` counters. In flight is capped at 32 frames and the output queue at 64, which is what took upload latency from over a second to tens of milliseconds in 0.5.1.

Received data frames are translated back to Ethernet (`rxFrame`, then `rx` on the front end with packets the front end allocated through `rx_alloc`). EAPOL goes to the supplicant queue, block-ack action frames to the block-ack machine, deauthentication to a pending-teardown flag that the timer acts on, beacons to the cache. A keepalive null frame goes out when nothing has been sent for a while, and a rejoin starts after a link loss.

### Block ack

`MtkBa.cpp`. On the receive side the driver accepts the AP's ADDBA request, gives the firmware a recipient STA_REC_BA, and reorders by sequence number with the algorithm from mt76's `agg-rx.c` (release head, release on timeout at HZ/10, BlockAckReq handling). Frames waiting in the buffer are the front end's mbufs; dropped ones are freed. On the transmit side, once data flows the driver sends an ADDBA request for TID 0 and, on success, an originator STA_REC_BA; the firmware then builds A-MPDUs itself. `MtkRxBa` and `MtkTxBa` in `Info.plist` switch either side off, in which case ADDBA requests are declined.

### The user client

`WTLWirelessUserClient` (`MtkUserClient.cpp`) is created per process that opens the service. Its five methods (scan, results, join, disconnect, status) run under the core's work loop gate through `runAction`; they set flags or copy snapshots and never issue a firmware command themselves, so a call waits at most for one command to finish (3 s worst case). Scan, join and disconnect need admin; status and results do not. The data shapes are fixed-size integers in `MtkUserApi.h`, the same in the kernel and in a 64-bit user process.

### Threading

One gate. The core makes the work loop; the front end returns the same loop from `getWorkLoop()`, so its output queue and the IONetworkController command gate serialise with the timer.

| Path | Context |
|---|---|
| `hwInit` (NIC capability, domain, power) | provider start thread, before the timer runs |
| `tick`: ring drains, pending teardown, `secService`, `joinService`, `scanService`, `cmService`, block ack, keepalive, link publication | core work loop, timer action; the timer is re-armed first |
| `uniCmd` (every firmware command) | tick or bring-up thread only; a call from inside `dispatchRx` fails with -13 and is counted; three timeouts in a row set `fwDead` and stop the machines |
| `rxFrame` | classifies, queues and delivers only |
| user client calls | `workLoop->runAction`; set flags or copy snapshots |
| supplicant callbacks | queue an EAPOL frame or stash a key; never poll, never command |
| `tx` from the output queue, `set_enabled` from enable/disable | the core gate |
| MSI handler | primary interrupt: count and acknowledge |

Callbacks must not sleep and must not call back into `tx` or `set_enabled`. The front end's `stop` removes its callbacks under the gate before detaching `en1`, so a running tick never calls into a dying front end.

### Registry properties

`ioreg -r -c WTLWireless -l -w0` shows `MtkState` (the current step as text), `MtkScan` (one line per cached network: SSID, channel, RSSI, security, `!` when not joinable), `MtkW1Bss` / `MtkW1Scan` / `MtkW1Join` (the machines' counters and last results), `MtkW2Sec` (handshake messages seen, bad MICs, replays, reinstall skips, missing PNs, ICV errors, cipher mismatches, unprotected / port / translation drops, key installs), `MtkCm` (connection manager decisions), `MtkW2Cmd` (in-dispatch violations, timeouts, failures, dead), the command results `MtkW2BssSec` / `MtkW2PtkInstall` / `MtkW2GtkInstall`, and the older `MtkW4*` to `MtkW9*` sets (scan, join, data path, rates, block ack). Never a key.

### What is not there

- WPA3-SAE, PSK-SHA256 (EAPOL descriptor version 3), management frame protection, fast transition and enterprise authentication.
- Roaming between BSSIDs of one SSID.
- HE (802.11ax) and EHT; the association is VHT at 80 MHz.
- AirDrop and Continuity (no AWDL interface), also with the Wi-Fi menu part.
- Sleep and wake. The laptop this runs on has sleep disabled.
- Matching a SCAN_DONE or ROC grant event to the request that caused it; the machines use deadlines instead.
- Interrupt-driven RX.

### Testing

`bash tests/run.sh` builds `MtkCrypto.c`, `MtkSupp.c` and `MtkSuper.c` with the host clang and UBSan, runs the vectors, the simulated handshakes and every supervisor decision, and compares the printed values with `tests/oracle.py`. `tests/hwtrace/` compiles the driver itself for the host and runs bring-up, a scan, a join and the shutdown against a fake MT7925, writing every register access, firmware command and DMA ring setup as a line of text. `tests/hwtrace/compare.sh <tag>` builds the sources at that tag next to the working tree and diffs the two traces, which is how a change that should not touch the hardware gets checked before a boot (`tests/hwtrace/README.md` says what the fake chip guesses). On the machine, `tools/wifi.sh` records the interface state, runs a router ping, a download and an upload through Cloudflare's speed endpoint with ping alongside, and saves every `Mtk*` property. The numbers in the README come from that script.

## The Wi-Fi menu part (`src/native`)

The core owns the hardware: PCIe, firmware, DMA rings, scanning, association, WPA2, block ack. It exports a C ops
table (`src/core/MtkCoreApi.h`) through `callPlatformFunction`: scan, get_bss, join, disconnect, get_status, set_mode,
set_mac, tx_bytes, plus callbacks for RX frames, link state, scan done and join result. This part is the front end
macOS talks to. It asks the core for table version 6 (chip name, stream count and channel bands come from the core, so
nothing here names an MT7925) and falls back to 5, 4 and 3. It reaches the core only through that table; the core is
in its `OSBundleLibraries` so that the kernel loads the core first.

`WTLWirelessNative` (an `IO80211Controller`) matches the core, creates a CoreCapture logger and fault reporter (the Family
asks for both and panics without them), runs the Family's base start, then creates `WTLWirelessNativeIf` (an
`IO80211InfraProtocol`: `IO80211InfraInterface` plus the 200 pure request handlers the Family routes ioctls to) and
starts it with a RegistrationInfo block: one packet pool, four Tx submission queues, Tx and Rx completion queues. The
interface binds the core (callbacks version 4) and tells it that the native front end owns the data path and the joins.

### Requests

Userland ioctls reach `IO80211Controller::apple80211Request`; the Family's dispatcher first asks the
interface `isCommandProhibited(code)` and only then calls the handler slot. Everything we do not implement answers
ENOTSUP, which userland treats as "not supported" and carries on; the allow-list in `src/native/WTLWirelessNative.cpp` names what we
answer. Scan: `setWCL_SCAN_REQ` runs the core scan and the tick posts one 0xc9 event per cached BSS (a beacon message:
0x44-byte header, then the IEs), then 0xed. Join: `setWCL_ASSOCIATE` turns the candidate (SSID, cipher, key, BSSID)
into a core join; the result is announced as 0xd8 link status (the only event that takes the Family's net manager out
of LINK_DOWN), then 0x4e, 0xd3, 0xd4, 0xd5; the Family's link-up then reads the BSS info and extended BSS info from the
core's cache and sends its link-path configuration, which we accept. A link the core loses is reported as 0xd8 down.

### Data path

RX: the core hands Ethernet frames as mbufs under its gate; the interface copies them into a ring and
a 2 ms timer on the Family's work queue moves them into pool packets on the Rx completion queue. TX: the Tx submission
dequeue callback hands packet bytes to the core under the core's gate and completes the packets on the next tick. The
two gates never nest.

### Addresses

macOS gives a Wi-Fi interface a private address per network (through ioctl 0x170 before each join, or
directly on the BSD interface). The firmware only receives unicast for the address it was given, so the interface
follows every address change into the core (`set_mac`), which re-adds its station interface with it.

### ABI

Apple ships no headers for any of this. `tools/kcabi` dumps class sizes, vtables and the request dispatch
tables from the kernel collection of the running build; `genhdr.py` generates the headers and the stub file; the build
checks every override against Apple's slot numbers and every undefined symbol against the collection.
`src/native/Info.plist` lists the builds the headers were generated for (`MtkNativeAbi`) and the part declines on any
other build. When it declines, or claims the core and has not registered after 90 s, the core starts its own
Ethernet port, so an unknown macOS build still has a network interface. `docs/NOTES-NATIVE.md` is the lab notebook: what each version found in Apple's binaries.

## The Bluetooth part (`src/bt`)

One file, `src/bt/WTLWirelessBt.cpp`, plus the firmware blob that `tools/gen_bt_firmware.py` embeds.

### Why a loader is needed

The MT7925's Bluetooth function is a USB device (`0489:E111` on the Legion; other cards use other ids) with three interfaces: `BT_ACL_If` (interface 0: interrupt IN for HCI events, bulk IN and OUT for ACL), `BT_SCO_If` and `BT_ISO_If`. After power-on the chip runs only its boot ROM. It enumerates fine, but HCI commands get no answer until the RAM code has been written into it through the vendor WMT protocol. Linux does that in `btmtk.c` every time the device appears. macOS has no equivalent, so without a loader `bluetoothd` sees a controller that never answers Reset and gives up.

### Where it sits in the stack

Apple's `AppleUSBHostCompositeDevice` matches the device (class EF/02/01, interface association) and sets configuration 1, which publishes the three interfaces as `IOUSBHostInterface` nubs with no driver attached. On macOS 15 and 26 there is no in-kernel Bluetooth USB transport: `bluetoothd` opens the interface from user space through IOUSBLib. So `WTLWirelessBt` is an interface driver on `IOUSBHostInterface`, configuration 1, interface 0, with personalities that match every MT7925 Bluetooth id Linux `btusb` knows (the 0489, 13D3 and 0E8D families, listed in the README). It opens the interface, does its work, closes it, and stays attached but idle. Apple's normal path is untouched, and `bluetoothd` later opens the same interface as if nothing had happened. A device-level driver (the BrcmPatchRAM3 approach) would have to replace the composite driver and configure the device itself for no gain.

### The sequence

Everything runs in a kernel thread started from `start()`, so the boot is never held up, and every wait has a deadline.

1. Open interface 0, retrying for up to 10 s if something else holds it (`MtkBtOpenTries`).
2. Endpoints: walk the interface descriptor for the interrupt IN, bulk IN and bulk OUT pipes (`MtkBtEndpoints`). Start a read on the interrupt IN pipe and keep it running, dropping whatever arrives (`MtkBtIntrPackets`), so events cannot back up and stall the firmware during setup.
3. Identify: vendor request 0x63 reads the chip id (register 0x70010200), firmware version (0x80021004) and flavor (0x70010020). Anything but 0x7925 ends here.
4. Sleep protection bypass: read `0x18011100` through the uhw path, set bit 1, write it back. The MT7925 BT core ignores WMT commands while sleep protection is active, which is the state the `0E8D:0717` card wakes up in on AM5 desktop boards; the Linux fix for that card does the same read-modify-write here and again after activation. The step is repeated after each subsystem reset. A failed access is logged in `MtkBtSlpProt` and the start goes on without it.
5. Download `BT_RAM_CODE_MT7925_1_1_hdr.bin` (embedded at build time with its SHA-256). The blob has a 32-byte header, a 64-byte global descriptor and a section map of up to 16 entries; this one has 5 sections, 3 with payload (74,176, 332,224 and 41,472 bytes). For each section: a WMT PATCH_DWNLD query carrying the 52-byte section spec, which the firmware answers with download, already loaded, or busy. Already loaded is skipped (as Linux does), busy is retried every 100 ms up to 20 times, otherwise the section goes out in 250-byte blocks with a first / middle / last flag. Then 110 ms for activation.
6. Sleep protection bypass again: activation turns the protection back on, so the same write as step 4 goes out before FUNC_CTRL.
7. Endpoint reset option: register 0x74011890 = 0x00010001, as `btmtk_usb_setup` does for the 7925.
8. FUNC_CTRL 1: turn the Bluetooth function on; the status word from the event is recorded (`MtkBtFuncCtrlStatus`).
9. Close: stop the interrupt read, close the interface, `MtkBtState` = `ready`.

WMT commands are HCI vendor command 0xFC6F sent as a class request on the default control endpoint. The WMT event (HCI event 0xE4) is read back with a vendor IN request (bRequest 1, wValue 0x30), polled for at most 2 s per command.

### Warm reboots

macOS never turns the Bluetooth function off at shutdown (Linux does, in `btmtk_usb_shutdown`), so after a restart the chip may still be running the previous boot's firmware. Two forms were seen: sections 0 and 1 answered "already loaded" and then section 2 stopped answering at block 55, with about a thousand interrupt packets of firmware chatter in between; and a first WMT query that got no event at all. The loader handles that three ways:

- A pass that fails after the chip already holds part of the firmware (a section reported loaded or sent in this pass), any failed block, and a first query without an event that HCI does not explain all count as half-loaded. Then the MT7925 subsystem reset from `btmtk_usb_subsys_reset` runs (CONNV3 reset register, endpoint reset option, UDMA interrupt status clear, poll RST_DONE for 1 s, re-read the chip id), the interface 0 endpoints are resynced with CLEAR_FEATURE(ENDPOINT_HALT), and the whole download runs again. At most 3 passes per instance (`MtkBtTries`), and at most 2 resets per boot across all WTLWirelessBt instances. Linux follows the reset with a USB port reset and re-enumeration; WTLWirelessBt does not, but on some boards the reset itself drops the chip off the USB bus and it comes back as a new device with a new WTLWirelessBt instance. The per-boot cap is what stops that from turning into a reset loop (#1).
- If the very first WMT query gets no event, an HCI Read Local Version (0x1001) is sent. A Command Complete on the interrupt endpoint means the firmware is up: the device is left as it is and the state is `ready` (`MtkBtWarmPath` says so).
- At system power off and restart (IOPMrootDomain priority interest, `kIOMessageSystemWillPowerOff` / `kIOMessageSystemWillRestart`) and in `stop()`, the loader opens the interface again and sends WMT FUNC_CTRL 0, so the next boot starts with the function off and the WMT channel quiet. By then launchd has ended `bluetoothd`; if the interface is still taken, or the provider is going away, the step is skipped (`MtkBtFuncOff`).

The laptop this runs on has booted with `MtkBtTries` = 1 every time since the handling went in, so on that machine the reset path has not run. On an ASUS B860 board it ran once and the controller came up `ready` (README, "What it can't do").

### Failures and known risks

Any failure ends with `MtkBtState` = `error`, the step name in `MtkBtStep`, the error line in `MtkBtLastError`, the whole trace in `MtkBtLog`, and the interface closed. There are no deliberate panics.


- The uhw register reads (request type 0xDE) run on every start (the sleep protection bypass) and on the subsystem reset path. If IOUSBHost rejects the odd recipient bits, the bypass is skipped with a `uhw ... read failed` line and `MtkBtSlpProt` = `read failed`, and the reset path fails cleanly the same way.
- If `bluetoothd` opens the interface before WTLWirelessBt (it should not at boot, since kexts start first), WTLWirelessBt waits 10 s and gives up with `interface busy`.
- The FUNC_CTRL status word this firmware returns is `0xffff`, not the `0x0404` Linux checks for, and Bluetooth still comes on. The value is recorded, not acted on.
