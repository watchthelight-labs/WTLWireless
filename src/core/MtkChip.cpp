// <wtl> project: wtlwireless | file: src/core/MtkChip.cpp | author: watchthelight </wtl>
// SPDX-License-Identifier: ISC AND BSD-3-Clause-Clear
// The chip table. MT7925 values are ported from Linux mt76 v6.18 (mt7925/pci.c fixed_map, mt792x_dma.c prefetch,
// mt7925/regs.h and mt792x_regs.h interrupt bits, mt7925/init.c capabilities):
//   Copyright (C) 2023 MediaTek Inc. (ISC, see LICENSE in this folder)
// MT7927 values come from Linux master 79eb6679ef6a (2026-11-11), files mt7925/pci.c, mt7925/regs.h, mt792x_regs.h,
// mt792x_dma.c, mt792x.h and mt7925/mt7925.h, which carry SPDX BSD-3-Clause-Clear:
//   Copyright (C) 2023 MediaTek Inc.
// A tester's own port to macOS confirmed the register values on hardware. That port has no licence, so nothing is
// copied from it.

#include "MtkChip.h"

extern "C" {
extern const unsigned char mtk_fw_patch[];
extern const unsigned int mtk_fw_patch_len;
extern const char mtk_fw_patch_sha256[];
extern const char mtk_fw_patch_name[];
extern const unsigned char mtk_fw_ram[];
extern const unsigned int mtk_fw_ram_len;
extern const char mtk_fw_ram_sha256[];
extern const char mtk_fw_ram_name[];
extern const unsigned char mtk_fw_patch_7927[];
extern const unsigned int mtk_fw_patch_7927_len;
extern const char mtk_fw_patch_7927_sha256[];
extern const char mtk_fw_patch_7927_name[];
extern const unsigned char mtk_fw_ram_7927[];
extern const unsigned int mtk_fw_ram_7927_len;
extern const char mtk_fw_ram_7927_sha256[];
extern const char mtk_fw_ram_7927_name[];
}

namespace {
// mt7925/pci.c fixed_map[]
const MtkRegMap kMap7925[] = {
    { 0x830c0000, 0x000000, 0x0001000 }, // WF_MCU_BUS_CR_REMAP
    { 0x54000000, 0x002000, 0x0001000 }, // WFDMA PCIE0 MCU DMA0
    { 0x55000000, 0x003000, 0x0001000 }, // WFDMA PCIE0 MCU DMA1
    { 0x56000000, 0x004000, 0x0001000 }, // WFDMA reserved
    { 0x57000000, 0x005000, 0x0001000 }, // WFDMA MCU wrap CR
    { 0x58000000, 0x006000, 0x0001000 }, // WFDMA PCIE1 MCU DMA0 (MEM_DMA)
    { 0x59000000, 0x007000, 0x0001000 }, // WFDMA PCIE1 MCU DMA1
    { 0x820c0000, 0x008000, 0x0004000 }, // WF_UMAC_TOP (PLE)
    { 0x820c8000, 0x00c000, 0x0002000 }, // WF_UMAC_TOP (PSE)
    { 0x820cc000, 0x00e000, 0x0002000 }, // WF_UMAC_TOP (PP)
    { 0x74030000, 0x010000, 0x0001000 }, // PCIe MAC
    { 0x820e0000, 0x020000, 0x0000400 }, // WF_LMAC_TOP BN0 (WF_CFG)
    { 0x820e1000, 0x020400, 0x0000200 }, // WF_LMAC_TOP BN0 (WF_TRB)
    { 0x820e2000, 0x020800, 0x0000400 }, // WF_LMAC_TOP BN0 (WF_AGG)
    { 0x820e3000, 0x020c00, 0x0000400 }, // WF_LMAC_TOP BN0 (WF_ARB)
    { 0x820e4000, 0x021000, 0x0000400 }, // WF_LMAC_TOP BN0 (WF_TMAC)
    { 0x820e5000, 0x021400, 0x0000800 }, // WF_LMAC_TOP BN0 (WF_RMAC)
    { 0x820ce000, 0x021c00, 0x0000200 }, // WF_LMAC_TOP (WF_SEC)
    { 0x820e7000, 0x021e00, 0x0000200 }, // WF_LMAC_TOP BN0 (WF_DMA)
    { 0x820cf000, 0x022000, 0x0001000 }, // WF_LMAC_TOP (WF_PF)
    { 0x820e9000, 0x023400, 0x0000200 }, // WF_LMAC_TOP BN0 (WF_WTBLOFF)
    { 0x820ea000, 0x024000, 0x0000200 }, // WF_LMAC_TOP BN0 (WF_ETBF)
    { 0x820eb000, 0x024200, 0x0000400 }, // WF_LMAC_TOP BN0 (WF_LPON)
    { 0x820ec000, 0x024600, 0x0000200 }, // WF_LMAC_TOP BN0 (WF_INT)
    { 0x820ed000, 0x024800, 0x0000800 }, // WF_LMAC_TOP BN0 (WF_MIB)
    { 0x820ca000, 0x026000, 0x0002000 }, // WF_LMAC_TOP BN0 (WF_MUCOP)
    { 0x820d0000, 0x030000, 0x0010000 }, // WF_LMAC_TOP (WF_WTBLON)
    { 0x40000000, 0x070000, 0x0010000 }, // WF_UMAC_SYSRAM
    { 0x00400000, 0x080000, 0x0010000 }, // WF_MCU_SYSRAM
    { 0x00410000, 0x090000, 0x0010000 }, // WF_MCU_SYSRAM (configure register)
    { 0x820f0000, 0x0a0000, 0x0000400 }, // WF_LMAC_TOP BN1 (WF_CFG)
    { 0x820f1000, 0x0a0600, 0x0000200 }, // WF_LMAC_TOP BN1 (WF_TRB)
    { 0x820f2000, 0x0a0800, 0x0000400 }, // WF_LMAC_TOP BN1 (WF_AGG)
    { 0x820f3000, 0x0a0c00, 0x0000400 }, // WF_LMAC_TOP BN1 (WF_ARB)
    { 0x820f4000, 0x0a1000, 0x0000400 }, // WF_LMAC_TOP BN1 (WF_TMAC)
    { 0x820f5000, 0x0a1400, 0x0000800 }, // WF_LMAC_TOP BN1 (WF_RMAC)
    { 0x820f7000, 0x0a1e00, 0x0000200 }, // WF_LMAC_TOP BN1 (WF_DMA)
    { 0x820f9000, 0x0a3400, 0x0000200 }, // WF_LMAC_TOP BN1 (WF_WTBLOFF)
    { 0x820fa000, 0x0a4000, 0x0000200 }, // WF_LMAC_TOP BN1 (WF_ETBF)
    { 0x820fb000, 0x0a4200, 0x0000400 }, // WF_LMAC_TOP BN1 (WF_LPON)
    { 0x820fc000, 0x0a4600, 0x0000200 }, // WF_LMAC_TOP BN1 (WF_INT)
    { 0x820fd000, 0x0a4800, 0x0000800 }, // WF_LMAC_TOP BN1 (WF_MIB)
    { 0x820c4000, 0x0a8000, 0x0004000 }, // WF_LMAC_TOP BN1 (WF_MUCOP)
    { 0x820b0000, 0x0ae000, 0x0001000 }, // [APB2] WFSYS_ON
    { 0x80020000, 0x0b0000, 0x0010000 }, // WF_TOP_MISC_OFF
    { 0x81020000, 0x0c0000, 0x0010000 }, // WF_TOP_MISC_ON
    { 0x7c020000, 0x0d0000, 0x0010000 }, // CONN_INFRA, wfdma
    { 0x7c060000, 0x0e0000, 0x0010000 }, // CONN_INFRA, conn_host_csr_top
    { 0x7c000000, 0x0f0000, 0x0010000 }, // CONN_INFRA
    { 0x70020000, 0x1f0000, 0x0010000 }, // Reserved for CBTOP, can't switch
    { 0x7c500000, 0x060000, 0x2000000 }, // remap
};

constexpr uint32_t pf(uint32_t base, uint32_t depth) { return (base << 16) | depth; }

// mt7925 branch of mt792x_dma_prefetch: RX rings 0 to 3, TX rings 0 to 3, then TX 15 and 16
const MtkPrefetch kPrefetch7925[] = {
    { 0x680, pf(0x0000, 0x4) },
    { 0x684, pf(0x0040, 0x4) },
    { 0x688, pf(0x0080, 0x4) },
    { 0x68c, pf(0x00c0, 0x4) },
    { 0x600, pf(0x0100, 0x10) },
    { 0x604, pf(0x0200, 0x10) },
    { 0x608, pf(0x0300, 0x10) },
    { 0x60c, pf(0x0400, 0x10) },
    { 0x63c, pf(0x0500, 0x4) },
    { 0x640, pf(0x0540, 0x4) },
};

// mt7927_dma_prefetch_setup: PREFETCH_CTRL is written back as read, CFG0 to CFG3 sit at 0xd70f0 (WFDMA0 + 0x30f0),
// then RX rings 4, 6 and 7 and TX rings 16, 15 and 0
const MtkPrefetch kPrefetch7927[] = {
    { 0x3030, 0, true },
    { 0x30f0, 0x660077 },
    { 0x30f4, 0x1100 },
    { 0x30f8, 0x30004f },
    { 0x30fc, 0x542200 },
    { 0x690, pf(0x0000, 0x8) },
    { 0x698, pf(0x0080, 0x8) },
    { 0x69c, pf(0x0100, 0x4) },
    { 0x640, pf(0x0140, 0x4) },
    { 0x63c, pf(0x0180, 0x10) },
    { 0x600, pf(0x0280, 0x4) },
};

// mt7927_wfsys_reset and mt792x_regs.h MT7927_* (CBInfra block)
const MtkCbtop kCbtop7927 = {
    0x1f8600, 0x1f5034, 0xc1604, 0x1d1e, 0x21008, 0x1807, 0x40000, 0x40400,
};

const uint16_t kPci7925[] = { 0x7925, 0x0717, 0 };
const uint16_t kPci7927[] = { 0x7927, 0x6639, 0x0738, 0 };
// raw CHIPID words seen from MT7927 boards: the real one, the MT6639 die name, the PCI ID, and zero
const uint16_t kAlt7927[] = { 0x6639, 0x0738, 0x0000 };

const MtkChip kChips[] = {
    {
        0x7925, "MT7925", kPci7925,
        kMap7925, sizeof(kMap7925) / sizeof(kMap7925[0]),
        { mtk_fw_patch, &mtk_fw_patch_len, mtk_fw_patch_sha256, mtk_fw_patch_name },
        { mtk_fw_ram, &mtk_fw_ram_len, mtk_fw_ram_sha256, mtk_fw_ram_name },
        0x7c000140,
        0, 2,
        1u << 0, 1u << 2,
        kPrefetch7925, sizeof(kPrefetch7925) / sizeof(kPrefetch7925[0]),
        MTK_BAND_2G | MTK_BAND_5G | MTK_BAND_6G, 2, 3,
        true, true,
        nullptr, 0, 0, 0, nullptr, 0, nullptr,
    },
    {
        // Matched but opt-in: it runs only with boot-arg mtk7927=1 and has been seen on one tester's machine, not here.
        // Same fixed map as MT7925 (mt7925/pci.c uses it for both). Width stays at 160 MHz: the hardware does 320 MHz
        // and MLO, which this driver does not offer.
        0x7927, "MT7927", kPci7927,
        kMap7925, sizeof(kMap7925) / sizeof(kMap7925[0]),
        { mtk_fw_patch_7927, &mtk_fw_patch_7927_len, mtk_fw_patch_7927_sha256, mtk_fw_patch_7927_name },
        { mtk_fw_ram_7927, &mtk_fw_ram_7927_len, mtk_fw_ram_7927_sha256, mtk_fw_ram_7927_name },
        0,                                          // no WFSYS_SW_RST_B: the reset goes through CBTOP
        6, 4,                                       // MT7927_RXQ_MCU_WM, MT7927_RXQ_BAND0
        1u << 14, 1u << 12,                         // MT7927_RX_DONE_INT_ENA6, MT7927_RX_DONE_INT_ENA4
        kPrefetch7927, sizeof(kPrefetch7927) / sizeof(kPrefetch7927[0]),
        MTK_BAND_2G | MTK_BAND_5G | MTK_BAND_6G, 2, 3,
        true, true,
        &kCbtop7927, 7, 1u << 15,                   // MT7927_RXQ_DATA2, MT7927_RX_DONE_INT_ENA7
        MTK_Q_ASPM_OFF | MTK_Q_WFDMA_EXT | MTK_Q_BAND_IDX | MTK_Q_DBDC,
        kAlt7927, sizeof(kAlt7927) / sizeof(kAlt7927[0]), "mtk7927",
    },
};
} // namespace

const MtkChip *mtkChipForPci(uint16_t device) {
    for (const MtkChip &c : kChips)
        for (const uint16_t *id = c.pciIds; *id; id++)
            if (*id == device)
                return &c;
    return nullptr;
}
