// <wtl> project: wtlwireless | file: src/native/MtkAbiCompat.h | author: watchthelight </wtl>
// SPDX-License-Identifier: ISC
// What differs between the two IO80211 generations this kext is built for. build.sh defines MTK_ABI_SEQUOIA when it builds
// against abi/24H*/ (macOS 15 Sequoia); without it the code is the Tahoe 26 (25G241 / 25G332) code, unchanged.
//
// Sequoia 24H32 (headers generated from the kcabi-format dump assembled from thanhnndev/mt7925-macos-wifi abi/native-wifi/24H32,
// BSD-3-Clause-Clear; payload layouts from its frozen `sequoia` branch, kexts/itlmt7925Native, which carried traffic on 24H32):
//   * IO80211InfraProtocol has 190 request handlers (slots 467..656) instead of 200 (470..669): slot numbers differ, the
//     ioctl codes the Family asks isCommandProhibited about are the same (24H32 apple80211RouteInternal table)
//   * setMacAddress(ether_addr&) is pure in IO80211SkywalkInterface (slot 417), not setMacAddress(mloAddrArray&) (470)
//   * setWCL_LINK_STATE_UPDATE is one of the protocol's pure handlers (slot 600), not an IO80211InfraInterface virtual
//   * setWCL_JOIN_ABORT takes void*; IO80211InfraInterface::setLinkState takes four arguments, not five
//   * scan-result / BSS-info beacon message: 0x40-byte header, flags at +0x3c (25G241: 0x44, flags at +0x40)
//   * WCL_EXTENDED_BSS_INFO is 0x1e0 bytes with the WPA/RSN element at +0xdd (25G241: 0x214, +0x113)
//   * WCL learns the hardware MAC (GET 0x167 HW_ADDR, WCLConfigManager::loadHwMacAddr) only from its "firmware loaded"
//     notification, which it sends when the driver posts event 0x31 (u32 1) in the CAN_SEND state, reached through event
//     0x37 "driver available" (AppleBCMWLANCore::powerOn / resetInterfacesComplete do both). Without it a join without a
//     private address (Recovery, Private Wi-Fi Address off) sends SET_MAC_ADDRESS 00:00:00:00:00:00, the MAC agent answers
//     0xe00002c2 and WCLJoinManager::handleJoinRequest drops the join
//   * the 0x27 LQM and 0xaf beacon-counter events and the candidate list offsets were read on 25G241 only: on Sequoia they
//     are off unless asked for (boot-args mtknativelqm=1, mtknativecand=1), as the 24H32 reference never sent them
#pragma once

#if defined(MTK_ABI_SEQUOIA)
#define MTK_SLOT(tahoe, sequoia) (sequoia)
#define MTK_ABI_GENERATION "sequoia"
#else
#define MTK_SLOT(tahoe, sequoia) (tahoe)
#define MTK_ABI_GENERATION "tahoe"
#endif

namespace mtkabi {
#if defined(MTK_ABI_SEQUOIA)
const unsigned kBeaconHeader = 0x40, kBeaconFlags = 0x3c;
const unsigned kExtBssSize = 0x1e0, kExtBssRsn = 0xdd;
const bool kTahoeOnlyEventsDefault = false;   // 0x27 / 0xaf and the candidate BSSID
const bool kWclReadyDefault = true;            // post 0x37 driver available + 0x31 interfaces reset complete (boot-arg mtkwclready)
#else
const unsigned kBeaconHeader = 0x44, kBeaconFlags = 0x40;
const unsigned kExtBssSize = 0x214, kExtBssRsn = 0x113;
const bool kTahoeOnlyEventsDefault = true;
const bool kWclReadyDefault = false;           // read on 24H32 only; Tahoe joins without a private address already
#endif
const unsigned kBeaconHeaderMax = 0x44;       // allocation size, whichever build
} // namespace mtkabi
