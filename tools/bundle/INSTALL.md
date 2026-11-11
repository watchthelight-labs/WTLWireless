# WTLWireless: install

`WTLWireless.kext` gives a MediaTek MT7925 (Filogic 360, PCI `14C3:7925`, or the AMD RZ717 `14C3:0717`) Wi-Fi and
Bluetooth on a Hackintosh. It is one kext with two more inside it:

| Entry in `Kernel > Add` | What it is |
|---|---|
| `WTLWireless.kext` | the Wi-Fi driver |
| `WTLWireless.kext/Contents/PlugIns/WTLWirelessNative.kext` | macOS's own Wi-Fi menu on top of the driver (Tahoe 26.7.1 to 26.7.3 and the 26.7.3 developer beta 25G416) |
| `WTLWireless.kext/Contents/PlugIns/WTLWirelessBt.kext` | the Bluetooth firmware loader |

`LICENSE`, `NOTICE.md` and `LICENSES/` must stay with the kext. Sources, status and the comparison with other
MediaTek drivers: https://github.com/watchthelight-labs/WTLWireless

## New install

1. Copy `WTLWireless.kext` into `EFI/OC/Kexts/`. Copy the folder whole; the two inner kexts stay where they are.
2. Add three entries to `config.plist` under `Kernel > Add`, in this order. OpenCore does not load a plugin unless
   it has an entry of its own, and the driver has to come before the Wi-Fi menu entry.

   ```
   BundlePath       WTLWireless.kext
   ExecutablePath   Contents/MacOS/WTLWireless
   PlistPath        Contents/Info.plist
   Enabled          true

   BundlePath       WTLWireless.kext/Contents/PlugIns/WTLWirelessNative.kext
   ExecutablePath   Contents/MacOS/WTLWirelessNative
   PlistPath        Contents/Info.plist
   MinKernel        25.0.0
   MaxKernel        25.99.99
   Enabled          true

   BundlePath       WTLWireless.kext/Contents/PlugIns/WTLWirelessBt.kext
   ExecutablePath   Contents/MacOS/WTLWirelessBt
   PlistPath        Contents/Info.plist
   MinKernel        21.0.0
   Enabled          true
   ```

   `config-snippet.plist` next to this file has the same three entries as plist XML to paste. ProperTree's OC
   Snapshot finds the plugins by itself; check that the driver's entry ends up above the other two. None of the three
   needs Lilu.
3. Add `mtkfe=native mtknativeif=1` to `boot-args` (`NVRAM > Add > 7C436110-AB2A-4BBB-A880-FE41995C9F82`). If
   `boot-args` is already in NVRAM, OpenCore keeps the old value (its log says `ignored, exists`): list `boot-args`
   under `NVRAM > Delete` for the same GUID, or pick Reset NVRAM in the picker once.
4. For Bluetooth, three more things:
   - BlueToolFixup (from acidanthera's BrcmPatchRAM release, needs Lilu), with the two NVRAM keys its 2.7.0 changelog
     asks for, in `NVRAM > Add` and `NVRAM > Delete` under `7C436110-AB2A-4BBB-A880-FE41995C9F82`:

     ```
     bluetoothExternalDongleFailed      Data  00
     bluetoothInternalControllerInfo    Data  0000000000000000000000000000   (exactly 14 zero bytes)
     ```

   - On Tahoe, [WTLLegionBtFix](https://github.com/watchthelight-labs/WTLLegionBtFix) after BlueToolFixup. Without it
     `bluetoothd` crashes every few seconds once an LE device is near.
   - The USB port the card's Bluetooth half sits on has to be in your USB map.
5. Reboot. The Wi-Fi icon appears in the menu bar; join your network from it and macOS remembers it.

If you only want Wi-Fi, leave the third entry out. The Bluetooth entry links nothing from the driver, so it does not
need the other two; only the full set has been booted so far.

## Coming from MtkWlan.kext, MtkWlanNative.kext and MtkBt.kext

WTLWireless 0.14.0 is those three kexts (0.13.0, 0.4.17 and 0.2.4) in one bundle under new names.

1. Copy `WTLWireless.kext` into `EFI/OC/Kexts/`.
2. In `Kernel > Add`, remove the entries for `MtkWlan.kext`, `MtkWlanNative.kext` and `MtkBt.kext` and add the three
   above. The driver's entry goes where `MtkWlan.kext` was, before the Wi-Fi menu entry.
3. Delete the three old kext folders from `EFI/OC/Kexts/`.
4. Leave everything else alone: `boot-args`, the Bluetooth NVRAM keys, BlueToolFixup and LegionBtFix are unchanged,
   and macOS keeps its saved networks and paired devices.

Do not run old and new together. If `MtkWlan.kext` is still loaded, the new driver leaves the card to it and says so
in the `MtkConflict` property of the card's PCI device, and Wi-Fi keeps running on the old kexts.

Commands that name a class change (`ioreg -rc WTLWireless`, `WTLWirelessNative`, `WTLWirelessBt`). The property
names (`MtkState`, `MtkNative...`, `MtkBtState`) are the same as before.

## Check after the reboot

```
kmutil showloaded | grep WTLWireless                    # three lines: WTLWireless, .Native, .Bt
ioreg -rc WTLWireless -l -w0 | grep '"MtkState"'        # ends in "pass"
ioreg -rc WTLWirelessBt -l -w0 | grep '"MtkBtState"'    # "ready"
```

## When the Wi-Fi menu does not appear

The Wi-Fi menu part is built for the macOS builds named above and stays out on any other one, Sequoia (15.x)
included. The driver then shows an Ethernet port (`en1`) by itself, so the machine is never left without a network
interface by a wrong kext. Join with the `mtkwifi` tool in the `Recovery` folder of this zip
(`sudo ./mtkwifi join`, or `add` first for a new network) until a release for the new build is out.

Problems: run `tools/mtk-report.sh` from the repository and open an issue with the archive it writes.
