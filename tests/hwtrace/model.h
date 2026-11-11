// <wtl> project: mtkwlan | file: tests/hwtrace/model.h | author: watchthelight </wtl>
// Fake MT7925 for the hwtrace harness: BAR0 register file, WFDMA ring engine, MCU firmware and a tiny AP.
#ifndef HWTRACE_MODEL_H
#define HWTRACE_MODEL_H

#include <cstdint>
#include <map>
#include <vector>

struct ModelRing {
    uint32_t base = 0, size = 0, cpu = 0, dma = 0;
};

class Mt7925Model {
public:
    uint32_t read(uint32_t off);
    void write(uint32_t off, uint32_t val);
    // AP side: the harness asks for the join target, the model answers frames the driver sends
    uint32_t events = 0;

private:
    static constexpr int NTX = 32, NRX = 16;
    std::map<uint64_t, uint32_t> regs;   // key: BAR offset, or (1<<32 | chip address) behind the remap windows
    ModelRing tx[NTX], rx[NRX];

    // latency model: a handshake bit flips on the read after the one that first sees the old value
    bool ownSync = false, ownPending = false, ownTarget = false;
    bool rstDone = false, rstPending = false;
    bool n9Ready = false, n9Pending = false;
    bool semHeld = false;
    uint32_t fwdlOff = 0;

    uint64_t key(uint32_t off) const;
    uint32_t chipAddr(uint32_t off, bool *mapped) const;
    void ringWrite(bool isTx, int idx, int field, uint32_t val);
    void txKick(int idx);
    void mcuTx(const uint8_t *msg, uint32_t len);
    void dataTx(const uint8_t *desc, uint32_t len);
    bool rxInject(int ring, const uint8_t *data, uint32_t len);
    void mcuReply(uint8_t eid, uint8_t seq, const std::vector<uint8_t> &body, uint32_t bodyAt);
    void scanBeacons();
    void apFrame(const uint8_t *f, uint32_t len);
};

extern Mt7925Model *g_model;

#endif
