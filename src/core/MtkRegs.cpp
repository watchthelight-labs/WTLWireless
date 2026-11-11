// <wtl> project: wtlwireless | file: src/core/MtkRegs.cpp | author: watchthelight </wtl>
// SPDX-License-Identifier: ISC
// Register access for MT792x over PCIe BAR0, including the bus address remapping (fixed map per chip in MtkChip.cpp).
// Ported from Linux drivers/net/wireless/mediatek/mt76/mt7925/pci.c (v6.18):
//   Copyright (C) 2023 MediaTek Inc. (ISC, see LICENSE in this folder)
// Linux mt76 accessors map 1:1: bus_ops->rr/wr = rawRead/rawWrite, mt7925_rr/wr/rmw = rr/wr/rmw.

#include "WTLWireless.hpp"

namespace {
// mt7925/regs.h
constexpr uint32_t MT_HIF_REMAP_L1        = 0x155024;
constexpr uint32_t MT_HIF_REMAP_L1_MASK   = 0xffff0000; // GENMASK(31, 16)
constexpr uint32_t MT_HIF_REMAP_BASE_L1   = 0x130000;
constexpr uint32_t MT_HIF_REMAP_L2        = 0x0120;
constexpr uint32_t MT_HIF_REMAP_BASE_L2   = 0x18500000; // CONFIG_MT76_DEV is not a Kconfig symbol
} // namespace

uint32_t WTLWireless::rawRead(uint32_t off) {
    if (!mmio || off + 4 > mmioLen) { badAccess++; return 0xffffffff; }
    return OSReadLittleInt32((void *)mmio, off);
}

void WTLWireless::rawWrite(uint32_t off, uint32_t val) {
    if (!mmio || off + 4 > mmioLen) { badAccess++; return; }
    OSWriteLittleInt32((void *)mmio, off, val);
}

uint32_t WTLWireless::rawRmw(uint32_t off, uint32_t mask, uint32_t val) {
    uint32_t cur = rawRead(off) & ~mask;
    rawWrite(off, cur | val);
    return cur;
}

void WTLWireless::remapRestore() {
    if (backupL1) { rawWrite(MT_HIF_REMAP_L1, backupL1); backupL1 = 0; }
    if (backupL2) { rawWrite(MT_HIF_REMAP_L2, backupL2); backupL2 = 0; }
}

uint32_t WTLWireless::mapL1(uint32_t addr) {
    uint32_t offset = addr & 0xffff;
    uint32_t base = addr >> 16;
    backupL1 = rawRead(MT_HIF_REMAP_L1);
    rawRmw(MT_HIF_REMAP_L1, MT_HIF_REMAP_L1_MASK, base << 16);
    rawRead(MT_HIF_REMAP_L1); // read to push the write
    return MT_HIF_REMAP_BASE_L1 + offset;
}

uint32_t WTLWireless::mapL2(uint32_t addr) {
    uint32_t base = MT_HIF_REMAP_BASE_L2 >> 16;
    backupL2 = rawRead(MT_HIF_REMAP_L1);
    rawRmw(MT_HIF_REMAP_L1, MT_HIF_REMAP_L1_MASK, base << 16);
    rawWrite(MT_HIF_REMAP_L2, addr);
    rawRead(MT_HIF_REMAP_L1); // read to push the write
    return MT_HIF_REMAP_BASE_L1;
}

uint32_t WTLWireless::regAddr(uint32_t addr) {
    if (addr < 0x200000)
        return addr;

    remapRestore();

    for (uint16_t i = 0; chip && i < chip->nFixedMap; i++) {
        const MtkRegMap &m = chip->fixedMap[i];
        if (addr < m.phys)
            continue;
        uint32_t ofs = addr - m.phys;
        if (ofs > m.size)
            continue;
        return m.maps + ofs;
    }

    if ((addr >= 0x18000000 && addr < 0x18c00000) ||
        (addr >= 0x70000000 && addr < 0x78000000) ||
        (addr >= 0x7c000000 && addr < 0x7c400000))
        return mapL1(addr);

    return mapL2(addr);
}

uint32_t WTLWireless::rr(uint32_t reg) { return rawRead(regAddr(reg)); }
void WTLWireless::wr(uint32_t reg, uint32_t val) { rawWrite(regAddr(reg), val); }
uint32_t WTLWireless::rmw(uint32_t reg, uint32_t mask, uint32_t val) { return rawRmw(regAddr(reg), mask, val); }

// mt76/util.c ____mt76_poll_msec
bool WTLWireless::pollMsec(uint32_t reg, uint32_t mask, uint32_t val, int timeoutMs, int tickMs) {
    int timeout = timeoutMs / tickMs;
    do {
        if ((rr(reg) & mask) == val)
            return true;
        IOSleep(tickMs);
    } while (timeout-- > 0);
    return false;
}
