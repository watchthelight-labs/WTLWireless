// <wtl> project: wtlwireless | file: tests/hwtrace/model.cpp | author: watchthelight </wtl>
// Fake MT7925. Register addresses and message layouts here come from the Linux mt76 sources and are written down
// independently of the driver, so a changed driver constant shows up as a different trace or a failed handshake.
// Every place the behaviour is a guess is marked GUESS and listed in the README.

#include "model.h"
#include <hw_shim.h>

Mt7925Model *g_model = nullptr;

namespace {
// BAR0 offsets of the registers the model answers (mt7925/pci.c fixed_map applied by hand)
constexpr uint32_t R_LPCTL = 0x0e0010;        // CONN_ON_LPCTL  0x7c060010
constexpr uint32_t R_CONN_ON_MISC = 0x0e00f0; // 0x7c0600f0
constexpr uint32_t R_WFSYS_RST = 0x0f0140;    // WFSYS_SW_RST_B 0x7c000140
constexpr uint32_t R_REMAP_L1 = 0x155024, R_REMAP_L2 = 0x0120, REMAP_BASE_L1 = 0x130000;
constexpr uint32_t R_GLO_CFG = 0xd4208;
constexpr uint32_t TX_RING0 = 0xd4300, RX_RING0 = 0xd4500;
constexpr uint32_t CHIP_HW_CHIPID = 0x70010200, CHIP_HW_REV = 0x70010204;
constexpr uint32_t TXQ_BAND0 = 0, TXQ_MCU_WM = 15, TXQ_FWDL = 16, RXQ_MCU_WM = 0, RXQ_BAND0 = 2;

uint32_t fnv(const uint8_t *p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}
uint16_t g16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
uint32_t g32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
void p16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
void p32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }

// Networks the fake air offers during a scan (made-up addresses and names)
struct Ap { const char *ssid; uint8_t ch; uint8_t bssid[6]; int kind; };   // kind 0 open b/g, 1 open 5 GHz HT+VHT, 2 WPA2-PSK
const Ap kAps[] = {
    { "WTL-OPEN-5G", 36, { 0x02, 0x00, 0x5e, 0x00, 0x00, 0x01 }, 1 },
    { "WTL-OPEN-24", 6, { 0x02, 0x00, 0x5e, 0x00, 0x00, 0x02 }, 0 },
    { "WTL-WPA2", 11, { 0x02, 0x00, 0x5e, 0x00, 0x00, 0x03 }, 2 },
};
const uint8_t kStaMac[6] = { 0x02, 0x00, 0x5e, 0x10, 0x00, 0x01 };   // the "card's" address in the NIC capability reply
}

uint64_t Mt7925Model::key(uint32_t off) const { return off; }

// BAR offset behind the remap windows -> chip address (mt7925 reg_map_l1 / reg_map_l2)
uint32_t Mt7925Model::chipAddr(uint32_t off, bool *mapped) const {
    *mapped = false;
    if (off < REMAP_BASE_L1 || off >= REMAP_BASE_L1 + 0x10000) return off;
    *mapped = true;
    auto l1 = regs.find(R_REMAP_L1);
    uint32_t hi = l1 == regs.end() ? 0 : l1->second >> 16;
    if (hi == 0x1850) {
        auto l2 = regs.find(R_REMAP_L2);
        return (l2 == regs.end() ? 0 : l2->second) + (off - REMAP_BASE_L1);
    }
    return (hi << 16) | (off - REMAP_BASE_L1);
}

uint32_t Mt7925Model::read(uint32_t off) {
    uint32_t v = 0;
    bool mapped;
    uint32_t chip = chipAddr(off, &mapped);
    if (off >= TX_RING0 && off < TX_RING0 + NTX * 0x10) {
        const ModelRing &r = tx[(off - TX_RING0) / 0x10];
        uint32_t f = (off & 0xf) / 4;
        v = f == 0 ? r.base : f == 1 ? r.size : f == 2 ? r.cpu : r.dma;
    } else if (off >= RX_RING0 && off < RX_RING0 + NRX * 0x10) {
        const ModelRing &r = rx[(off - RX_RING0) / 0x10];
        uint32_t f = (off & 0xf) / 4;
        v = f == 0 ? r.base : f == 1 ? r.size : f == 2 ? r.cpu : r.dma;
    } else if (off == R_LPCTL) {
        if (ownPending) { v = ownSync; ownSync = ownTarget; ownPending = false; }
        else v = ownSync;
        v <<= 2;                                   // PCIE_LPCR_HOST_OWN_SYNC is bit 2
    } else if (off == R_WFSYS_RST) {
        uint32_t s = regs.count(off) ? regs[off] : 0;
        if (rstPending) { rstPending = false; rstDone = true; v = s & ~0x10u; }
        else v = s | (rstDone ? 0x10u : 0);        // bit 4: WFSYS init done
    } else if (off == R_CONN_ON_MISC) {
        if (n9Pending) { n9Pending = false; n9Ready = true; v = 0; }
        else v = n9Ready ? 3 : 0;                  // FW_N9_RDY bits 1:0
    } else if (off == R_GLO_CFG) {
        v = (regs.count(off) ? regs[off] : 0) & ~0xau;   // GUESS: the DMA engines are never busy
    } else if (mapped && chip == CHIP_HW_CHIPID) {
        v = 0x00007925;                            // GUESS: upper half of the register
    } else if (mapped && chip == CHIP_HW_REV) {
        v = 0x00008a00;                            // GUESS: revision value
    } else {
        uint64_t k = mapped ? (1ull << 32 | chip) : key(off);
        auto it = regs.find(k);
        v = it == regs.end() ? 0 : it->second;     // GUESS: plain registers read back what was written
    }
    hw_trace("R 0x%06x -> 0x%08x", off, v);
    return v;
}

void Mt7925Model::write(uint32_t off, uint32_t val) {
    hw_trace("W 0x%06x 0x%08x", off, val);
    bool mapped;
    uint32_t chip = chipAddr(off, &mapped);
    if (off >= TX_RING0 && off < TX_RING0 + NTX * 0x10) { ringWrite(true, (off - TX_RING0) / 0x10, (off & 0xf) / 4, val); return; }
    if (off >= RX_RING0 && off < RX_RING0 + NRX * 0x10) { ringWrite(false, (off - RX_RING0) / 0x10, (off & 0xf) / 4, val); return; }
    if (off == R_LPCTL) {
        if (val & 1) { ownPending = true; ownTarget = true; }         // SET_OWN: firmware takes the chip
        if (val & 2) { ownPending = true; ownTarget = false; }        // CLR_OWN: driver takes it
        return;
    }
    if (off == R_WFSYS_RST) {
        bool was = regs.count(off) && (regs[off] & 1);
        if (!(val & 1)) { rstDone = false; rstPending = false; n9Ready = n9Pending = semHeld = false; }   // reset asserted: firmware gone
        else if (!was) rstPending = true;
        regs[off] = val & ~0x10u;
        return;
    }
    regs[mapped ? (1ull << 32 | chip) : key(off)] = val;
}

void Mt7925Model::ringWrite(bool isTx, int idx, int field, uint32_t val) {
    ModelRing &r = isTx ? tx[idx] : rx[idx];
    switch (field) {
    case 0: r.base = val; break;
    case 1: r.size = val; hw_trace("RING %s %d base 0x%08x cnt %u", isTx ? "tx" : "rx", idx, r.base, val); break;
    case 2:
        r.cpu = val;
        if (isTx) txKick(idx);
        break;
    default: r.dma = val; break;
    }
}

// The engine fetches descriptors only while TX_DMA_EN is set (GLO_CFG bit 0)
void Mt7925Model::txKick(int idx) {
    ModelRing &r = tx[idx];
    auto g = regs.find(R_GLO_CFG);
    if (g == regs.end() || !(g->second & 1) || !r.size) return;
    while (r.dma != r.cpu) {
        const uint32_t *d = (const uint32_t *)(hw_dma_host(r.base) + r.dma * 16);
        uint32_t len = (d[1] >> 16) & 0x3fff;
        const uint8_t *buf = hw_dma_host(d[0]);
        if (idx == TXQ_MCU_WM) mcuTx(buf, len);
        else if (idx == TXQ_FWDL) {
            hw_trace("FWDL off %u len %u fnv %08x", fwdlOff, len, fnv(buf, len));
            fwdlOff += len;
        } else if (idx == TXQ_BAND0) dataTx(buf, len);
        else hw_trace("UNEXPECTED tx ring %d", idx);
        r.dma = (r.dma + 1) % r.size;
    }
}

// RX ring: the next armed buffer (dma != cpu) takes the packet. Only while RX_DMA_EN (GLO_CFG bit 2) is set.
bool Mt7925Model::rxInject(int ring, const uint8_t *data, uint32_t len) {
    ModelRing &r = rx[ring];
    auto g = regs.find(R_GLO_CFG);
    if (g == regs.end() || !(g->second & 4) || !r.size || r.dma == r.cpu) {
        hw_trace("RXDROP ring %d len %u", ring, len);
        return false;
    }
    uint32_t *d = (uint32_t *)(hw_dma_host(r.base) + r.dma * 16);
    uint32_t cap = (d[1] >> 16) & 0x3fff;
    if (len > cap) { hw_trace("RXDROP ring %d len %u cap %u", ring, len, cap); return false; }
    memcpy(hw_dma_host(d[0]), data, len);
    d[1] = (len << 16) | (1u << 30) | (1u << 31);       // SD_LEN0, LAST_SEC0, DMA_DONE
    r.dma = (r.dma + 1) % r.size;
    return true;
}

// MCU event on RX ring 0: RXD type 7 (event), id at 36, sequence at 37, body from bodyAt
void Mt7925Model::mcuReply(uint8_t eid, uint8_t seq, const std::vector<uint8_t> &body, uint32_t bodyAt) {
    std::vector<uint8_t> pkt(std::max<size_t>(48, bodyAt + body.size()), 0);
    p32(pkt.data(), (7u << 27) | (uint32_t)pkt.size());
    pkt[36] = eid;
    pkt[37] = seq;
    memcpy(pkt.data() + bodyAt, body.data(), body.size());
    hw_trace("RXMCU eid 0x%02x seq %u len %zu fnv %08x", eid, seq, pkt.size(), fnv(pkt.data(), pkt.size()));
    rxInject(RXQ_MCU_WM, pkt.data(), (uint32_t)pkt.size());
}

void Mt7925Model::mcuTx(const uint8_t *m, uint32_t len) {
    bool legacy = g16(m + 34) == 0x8000;               // legacy txd carries pq_id here, UNI txd carries the command id
    uint8_t seq = m[39];
    uint32_t hdrLen = legacy ? 64 : 48;
    if (len < hdrLen) { hw_trace("UNEXPECTED short mcu message %u", len); return; }
    const uint8_t *pl = m + hdrLen;
    uint32_t plen = len - hdrLen;
    if (legacy) {
        uint8_t cid = m[36];
        hw_trace("MCU legacy cid 0x%02x seq %u len %u hdr %08x fnv %08x", cid, seq, plen, fnv(m, hdrLen), fnv(pl, plen));
        uint8_t status = 0;
        switch (cid) {
        case 0x10: {                                   // PATCH_SEM_CONTROL: GET -> 2 (not downloaded, semaphore taken), RELEASE -> 3
            uint32_t op = plen >= 4 ? g32(pl) : 0;
            if (op == 1) { status = semHeld ? 1 : 2; semHeld = true; }
            else { status = 3; semHeld = false; }
            break;
        }
        case 0x01: case 0x05: fwdlOff = 0; break;      // TARGET_ADDRESS_LEN_REQ / PATCH_START_REQ: a new region follows
        case 0x02: n9Pending = true; break;            // FW_START_REQ: the N9 comes up (GUESS: after one status read)
        default: break;                                // PATCH_FINISH_REQ and the rest: status 0
        }
        mcuReply(cid, seq, std::vector<uint8_t>{ status }, 40);
        return;
    }
    uint16_t cid = g16(m + 34);
    uint8_t opt = m[43];
    hw_trace("MCU uni cid 0x%02x seq %u opt 0x%02x len %u hdr %08x fnv %08x", cid, seq, opt, plen, fnv(m, hdrLen), fnv(pl, plen));
    uint16_t tag = plen >= 6 ? g16(pl + 4) : 0xffff;
    std::vector<uint8_t> body;
    if (cid == 0x0e && tag == 3) {                     // CHIP_CONFIG NIC_CAPA: one element, the MAC address (tag 7)
        body = { 1, 0, 0, 0, 7, 0, 12, 0, kStaMac[0], kStaMac[1], kStaMac[2], kStaMac[3], kStaMac[4], kStaMac[5], 0, 0 };
        mcuReply(0x01, seq, body, 44);
    } else if (cid == 0x2d && tag == 1 && !(opt & 4)) {   // EFUSE_CTRL read: 32 data bytes at 44 + 48 (GUESS: all zero, HW_TYPE encap 0)
        body.assign(48 + 32, 0);
        mcuReply(0x01, seq, body, 44);
    } else {
        mcuReply(0x01, seq, body, 44);                 // GUESS: every other UNI command is accepted with a bare result event
    }
    if (cid == 0x27 && tag == 0 && plen >= 10) {       // ROC acquire: the channel is granted (event 0x27, token at 53, status at 54)
        std::vector<uint8_t> ev(8, 0);
        ev[5] = pl[9];
        mcuReply(0x27, 0, ev, 48);
    }
    if (cid == 0x16) {                                 // SCAN_REQ: beacons on the data ring, then SCAN_DONE
        scanBeacons();
        mcuReply(0x0e, 0, std::vector<uint8_t>{}, 44);
    }
}

// Beacon frame for the fake air: RXD 32 bytes, 24-byte header, fixed fields, elements
static std::vector<uint8_t> beacon(const Ap &a) {
    std::vector<uint8_t> f(32 + 36, 0);
    uint8_t *h = f.data() + 32;
    h[0] = 0x80;
    memset(h + 4, 0xff, 6);
    memcpy(h + 10, a.bssid, 6);
    memcpy(h + 16, a.bssid, 6);
    p16(h + 32, 100);
    p16(h + 34, a.kind == 2 ? 0x0411 : 0x0401);        // ESS, short preamble, Privacy for WPA2
    auto ie = [&](uint8_t id, std::initializer_list<uint8_t> b) {
        f.push_back(id);
        f.push_back((uint8_t)b.size());
        f.insert(f.end(), b);
    };
    f.push_back(0); f.push_back((uint8_t)strlen(a.ssid));
    f.insert(f.end(), a.ssid, a.ssid + strlen(a.ssid));
    if (a.ch <= 14) ie(1, { 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24 });
    else ie(1, { 0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c });
    ie(3, { a.ch });
    if (a.kind == 2) ie(48, { 1, 0, 0x00, 0x0f, 0xac, 4, 1, 0, 0x00, 0x0f, 0xac, 4, 1, 0, 0x00, 0x0f, 0xac, 2, 0, 0 });
    if (a.kind == 1) {
        f.push_back(45); f.push_back(26);              // HT Capabilities: 20/40, SGI, 2 streams
        uint8_t ht[26] = { 0x6f, 0x01, 0x17, 0xff, 0xff };
        f.insert(f.end(), ht, ht + 26);
        ie(61, { a.ch, 0x05, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 });   // HT Operation: 40 MHz, secondary above
        ie(191, { 0xb2, 0x01, 0x80, 0x33, 0xfa, 0xff, 0, 0, 0xfa, 0xff, 0, 0 });              // VHT Capabilities, 2 streams MCS 0-9
        ie(192, { 1, 42, 0, 0xfc, 0xff });                                                     // VHT Operation: 80 MHz
        ie(0xdd, { 0x00, 0x50, 0xf2, 0x02, 0x01, 0x01, 0x80, 0x00,                             // WMM parameter element
                   0x03, 0xa4, 0x00, 0x00, 0x27, 0xa4, 0x00, 0x00, 0x42, 0x43, 0x5e, 0x00, 0x62, 0x32, 0x2f, 0x00 });
    }
    p32(f.data(), (2u << 27) | (uint32_t)f.size());    // RXD0: normal packet
    return f;
}

void Mt7925Model::scanBeacons() {
    for (const Ap &a : kAps) {
        std::vector<uint8_t> f = beacon(a);
        hw_trace("RXDATA beacon ch %u len %zu fnv %08x", a.ch, f.size(), fnv(f.data(), f.size()));
        rxInject(RXQ_BAND0, f.data(), (uint32_t)f.size());
    }
}

// TX data ring: the descriptor points at the TXD+txp block, whose txp names the 802.11 frame (mt7925 txp ptr[0])
void Mt7925Model::dataTx(const uint8_t *d, uint32_t len) {
    if (len < 48) { hw_trace("UNEXPECTED short txd %u", len); return; }
    uint32_t bufIova = g32(d + 40), flen = g16(d + 44) & 0x7fff;
    const uint8_t *f = hw_dma_host(bufIova);
    hw_trace("TXDATA txd %08x len %u frame %08x", fnv(d, len), flen, fnv(f, flen));
    if (flen >= 24) apFrame(f, flen);
}

// The fake AP answers open-system authentication and association requests (GUESS: always success, AID 1)
void Mt7925Model::apFrame(const uint8_t *f, uint32_t len) {
    if (f[0] != 0xb0 && f[0] != 0x00) return;
    std::vector<uint8_t> r(32 + 24 + 6, 0);
    uint8_t *h = r.data() + 32;
    h[0] = f[0] == 0xb0 ? 0xb0 : 0x10;
    memcpy(h + 4, f + 10, 6);                          // to the station
    memcpy(h + 10, f + 4, 6);                          // from the BSSID the request went to
    memcpy(h + 16, f + 16, 6);
    if (f[0] == 0xb0) { p16(h + 24, 0); p16(h + 26, 2); p16(h + 28, 0); }   // algorithm 0, transaction 2, success
    else { p16(h + 24, 0x0401); p16(h + 26, 0); p16(h + 28, 0xc001); }      // capability, success, AID 1 with the two top bits set
    p32(r.data(), (2u << 27) | (uint32_t)r.size());
    hw_trace("RXDATA %s response len %zu fnv %08x", f[0] == 0xb0 ? "auth" : "assoc", r.size(), fnv(r.data(), r.size()));
    rxInject(RXQ_BAND0, r.data(), (uint32_t)r.size());
}
