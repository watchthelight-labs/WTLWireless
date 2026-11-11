// <wtl> project: mtkwlan | file: src/MtkChip.h | author: watchthelight </wtl>
// SPDX-License-Identifier: ISC
// What differs between MT792x chips, as data: one row per chip in MtkChip.cpp. Code that is the same for every
// chip we drive keeps its constants where they are used.
#pragma once
#include <stdint.h>

struct MtkRegMap { uint32_t phys, maps, size; };      // chip bus address range -> BAR0 offset
struct MtkPrefetch { uint16_t ofs; uint32_t val; };   // one write at WFDMA0 + ofs, in table order
struct MtkFwBlob { const unsigned char *data; const unsigned int *len; const char *sha256, *name; };

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
};

// The row for a PCI device ID, or nullptr for a card this driver must not touch
const MtkChip *mtkChipForPci(uint16_t device);
