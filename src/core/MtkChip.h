// <wtl> project: wtlwireless | file: src/core/MtkChip.h | author: watchthelight </wtl>
// SPDX-License-Identifier: ISC
// What differs between MT792x chips, as data: one row per chip in MtkChip.cpp. Code that is the same for every
// chip we drive keeps its constants where they are used.
#pragma once
#include <stdint.h>

struct MtkRegMap { uint32_t phys, maps, size; };      // chip bus address range -> BAR0 offset
struct MtkPrefetch { uint16_t ofs; uint32_t val; bool readBack; };   // one write at WFDMA0 + ofs, in table order; readBack writes what the register holds
struct MtkFwBlob { const unsigned char *data; const unsigned int *len; const char *sha256, *name; };

// Reset through CBTOP (MT7927): all BAR0 offsets below 0x200000, so no remap is involved
struct MtkCbtop {
    uint32_t rguRst;                    // WF subsystem reset, bit 4
    uint32_t mcuOwnSet;                 // written 1 after the reset releases
    uint32_t romcode;                   // reads romcodeIdle in its low 16 bits once the MCU ROM runs
    uint16_t romcodeIdle;
    uint32_t remapReg;                  // PCIE2AP window: low 16 bits point at the semaphore block
    uint16_t remapVal;
    uint32_t semaOwn, semaRep;          // read through the window before and after the reset
};

enum : uint32_t {
    MTK_Q_ASPM_OFF   = 1u << 0,         // clear L0s and L1 in the device's PCIe Link Control before any register access
    MTK_Q_WFDMA_EXT  = 1u << 1,         // DTX and DRX pointers reset before the rings, none after; GLO_CFG bits 26 and 9 set, 20 clear
    MTK_Q_BAND_IDX   = 1u << 2,         // BSS info carries band_idx 0 (2.4 GHz) or 1 (5 and 6 GHz) instead of 0xff
    MTK_Q_DBDC       = 1u << 3,         // UNI command SET_DBDC_PARMS after the MAC init, a failure is only published
};

enum : uint8_t { MTK_BAND_2G = 1u << 0, MTK_BAND_5G = 1u << 1, MTK_BAND_6G = 1u << 2 };

struct MtkChip {
    uint16_t id;                        // low word of MT_HW_CHIPID
    const char *name;
    const uint16_t *pciIds;             // device IDs under vendor 14c3, 0 ends the list
    const MtkRegMap *fixedMap;
    uint16_t nFixedMap;
    MtkFwBlob patch, ram;
    uint32_t wfsysRst;                  // WFSYS_SW_RST_B: bit 0 resets, bit 4 reports done
    uint8_t rxqMcu, rxqData;            // WFDMA0 RX ring numbers
    uint32_t irqRxMcu, irqRxData;       // their bits in HOST_INT_ENA
    const MtkPrefetch *prefetch;
    uint8_t nPrefetch;
    uint8_t bands, nss, maxBw;          // what the hardware can do; maxBw as in mtk_link_info (2 = 80, 3 = 160 MHz)
    bool he, eht;
    // chips with a different reset, an extra RX ring and quirks (MT7927); the MT7925 row leaves these zero
    const MtkCbtop *cbtop;
    uint8_t rxqWa;                      // extra RX ring that must be drained (0 = none)
    uint32_t irqRxWa;
    uint32_t quirks;
    const uint16_t *chipIdAlt;          // other raw CHIPID low words the hardware is known to report (0 is one of them)
    uint8_t nChipIdAlt;
    const char *optInArg;               // boot-arg that must be 1 for the chip to run; nullptr = verified on our hardware
};

// The row for a PCI device ID, or nullptr for a card this driver must not touch
const MtkChip *mtkChipForPci(uint16_t device);
