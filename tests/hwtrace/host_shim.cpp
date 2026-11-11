// <wtl> project: wtlwireless | file: tests/hwtrace/host_shim.cpp | author: watchthelight </wtl>
// Host side of the shim headers: the trace sink, the DMA address space, and the register accessors that route
// BAR0 accesses into the device model.

#include "model.h"
#include <hw_shim.h>

namespace {
std::string g_out;
bool g_props = false;
struct Block { uint8_t *mem; uint32_t size; };
std::map<uint32_t, Block> g_blocks;        // device address -> block
uint32_t g_nextIova = 0x20000000;          // fixed start, so device addresses are the same on every run
constexpr uint32_t BAR_LEN = 0x200000;     // GUESS: BAR0 of the MT7925 is 2 MB (regAddr() treats 0x200000 as the limit)
uint8_t *g_barMem = nullptr;
uint32_t g_pciCmd = 0;
uint64_t g_clock = 0;
uint32_t g_rand = 0;
}

void hw_trace(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    g_out += buf;
    g_out += '\n';
}

void hw_unexpected(const char *what) { hw_trace("UNEXPECTED %s", what); }

uint16_t g_hw_pci_device = 0x7925;
bool g_hw_old_set = false;

const OSMetaClass *OSMetaClass::getMetaClassWithName(const OSSymbol *name) {
    static OSMetaClass old;
    return g_hw_old_set && name && name->s == "MtkWlan" ? &old : nullptr;
}

void hw_property(const char *key, const std::string &val) {
    if (!strcmp(key, "MtkState")) hw_trace("STEP %s", val.c_str());
    else if (g_props) hw_trace("PROP %s %s", key, val.c_str());
}

void hw_set_props(bool on) { g_props = on; }
const std::string &hw_trace_text() { return g_out; }

uint8_t *hw_dma_alloc(uint32_t size, uint32_t align, uint32_t *iova) {
    uint32_t a = (g_nextIova + align - 1) & ~(align - 1);
    uint8_t *m = (uint8_t *)calloc(1, size);
    g_blocks[a] = { m, size };
    g_nextIova = a + size;
    *iova = a;
    return m;
}

uint8_t *hw_dma_host(uint32_t iova) {
    auto it = g_blocks.upper_bound(iova);
    if (it != g_blocks.begin()) {
        --it;
        if (iova - it->first < it->second.size) return it->second.mem + (iova - it->first);
    }
    hw_trace("UNEXPECTED dma address 0x%08x", iova);
    static uint8_t scratch[8192];
    return scratch;
}

uint8_t *hw_bar() {
    if (!g_barMem) g_barMem = (uint8_t *)calloc(1, BAR_LEN);
    return g_barMem;
}
uint32_t hw_bar_len() { return BAR_LEN; }

uint32_t hw_pci_cmd(uint32_t set, uint32_t clr) {
    g_pciCmd = (g_pciCmd | set) & ~clr;
    return g_pciCmd;
}

uint64_t hw_uptime() { return g_clock += 1000000; }

void IOSleep(unsigned ms) { hw_trace("SLEEP %ums", ms); }
void IODelay(unsigned us) { hw_trace("DELAY %uus", us); }

extern "C" {
uint32_t hw_rd32(const volatile void *base, uintptr_t off) {
    if (base == g_barMem && g_model) return g_model->read((uint32_t)off);
    uint32_t v;
    memcpy(&v, (const uint8_t *)base + off, 4);
    return v;
}
void hw_wr32(volatile void *base, uintptr_t off, uint32_t val) {
    if (base == g_barMem && g_model) { g_model->write((uint32_t)off, val); return; }
    memcpy((uint8_t *)base + off, &val, 4);
}
void read_random(void *buf, unsigned len) {
    uint8_t *p = (uint8_t *)buf;
    for (unsigned i = 0; i < len; i++) p[i] = (uint8_t)(g_rand++ * 37 + 11);
}
}
