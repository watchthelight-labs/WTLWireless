// <wtl> project: wtlwireless | file: tests/hwtrace/trace_main.cpp | author: watchthelight </wtl>
// Drives the real WTLWireless code against the fake MT7925 and writes the hardware trace.
// usage: hwtrace <trace.txt> [--cov <functions.txt>] [--props] [--pcidev hex] [--ops6] [--oldset]
//        [--chip 7925|7927] [--chipid hex] [--lnkctl hex] [--bootarg key=value]

#include "model.h"
#include <hw_shim.h>
#include <signal.h>
#if __has_include("WTLWireless.hpp")
#include "WTLWireless.hpp"
typedef WTLWireless Core;
#define CORE_SCOPE "WTLWireless::"
#else   // a tree from before the rename (compare.sh with an older ref)
#include "MtkWlan.hpp"
typedef MtkWlan Core;
#define CORE_SCOPE "MtkWlan::"
#endif

void hw_set_props(bool on);
const std::string &hw_trace_text();

namespace {
// built on first use: instrumented static initialisers in the driver call the hook before main
std::set<void *> &covSet() { static std::set<void *> *s = new std::set<void *>; return *s; }
const char *g_tracePath = nullptr;

void writeTrace() {
    if (!g_tracePath) return;
    FILE *f = fopen(g_tracePath, "w");
    if (!f) return;
    const std::string &s = hw_trace_text();
    fwrite(s.data(), 1, s.size(), f);
    fclose(f);
}

void onCrash(int sig) {
    hw_trace("CRASH signal %d", sig);
    writeTrace();
    _exit(3);
}

void writeCoverage(const char *path) {
    std::set<std::string> names;
    for (void *fn : covSet()) {
        Dl_info di;
        if (!dladdr(fn, &di) || !di.dli_sname) continue;
        int st = 0;
        char *dm = abi::__cxa_demangle(di.dli_sname, nullptr, nullptr, &st);
        std::string n = st == 0 && dm ? dm : di.dli_sname;
        free(dm);
        size_t p = n.find('(');
        if (p != std::string::npos && n.compare(0, 11, "(anonymous ") != 0) n.resize(p);
        if (n.compare(0, sizeof(CORE_SCOPE) - 1, CORE_SCOPE) == 0 || n.compare(0, 4, "mtk_") == 0 || n.compare(0, 11, "(anonymous ") == 0 ||
            n.compare(0, 5, "Msg::") == 0)
            names.insert(n);
    }
    FILE *f = fopen(path, "w");
    if (!f) return;
    for (const std::string &n : names) fprintf(f, "%s\n", n.c_str());
    fclose(f);
}

// Tick the driver until done() or n ticks have passed; returns the ticks used
template <class F> int tickUntil(Core *d, int n, F done) {
    int i = 0;
    while (i < n && !done()) { d->tick(); i++; }
    return i;
}
}

extern "C" {
__attribute__((no_instrument_function)) void __cyg_profile_func_enter(void *fn, void *) { covSet().insert(fn); }
__attribute__((no_instrument_function)) void __cyg_profile_func_exit(void *, void *) {}
}

// --ops6: the v6 calls against the older ones, while joined. Extra lines, so never part of the compared trace.
static bool g_ops6 = false;
#ifdef MTK_CORE_API_V6
static void checkOps6(Core *d) {
    const mtk_core_ops_v6 *o = &Core::kOpsV6;
    if (o->v5.v4.v3.v2.v1.version != MTK_CORE_API_V6 || o->v5.v4.v3.v2.v1.size != sizeof(*o)) hw_trace("UNEXPECTED ops6 header");
    mtk_chip_info ci;
    memset(&ci, 0xee, sizeof(ci));
    ci.size = sizeof(ci);
    int rc = o->get_chip_info(d, &ci);
    hw_trace("OPS6 chip rc %d %s id %04x pci %04x bands %u/%u nss %u/%u bw %u/%u he %u/%u eht %u/%u fw '%s'", rc, ci.name, ci.chip_id,
             ci.pci_device, ci.bands, ci.hw_bands, ci.nss, ci.hw_nss, ci.max_bw, ci.hw_max_bw, ci.he, ci.hw_he, ci.eht, ci.hw_eht,
             ci.fw_version);
    if (rc || ci.size != sizeof(ci) || !ci.fw_version[0]) hw_trace("UNEXPECTED ops6 chip info");
    mtk_chip_info part;
    memset(&part, 0xee, sizeof(part));
    part.size = 8;                                         // an older caller: only size, chip_id and pci_device are written
    rc = o->get_chip_info(d, &part);
    if (rc || part.size != 8 || part.chip_id != ci.chip_id || (uint8_t)part.name[0] != 0xee) hw_trace("UNEXPECTED ops6 short chip info");

    uint8_t old[64];
    mtk_chan ch[64];
    uint32_t n1 = 0, n2 = 0, same = 0, dfs = 0, passive = 0, five = 0;
    o->v5.v4.v3.get_channels(d, old, 64, &n1);
    rc = o->get_channels2(d, ch, 64, &n2);
    for (uint32_t i = 0; i < n2 && i < n1; i++) {
        bool ok = ch[i].chan == old[i] && ch[i].center == old[i] && ch[i].bw == MTK_BW_20 &&
                  ch[i].band == (old[i] > 14 ? MTK_CHAN_5G : MTK_CHAN_2G) &&
                  !!(ch[i].flags & MTK_CHAN_F_DFS) == (old[i] >= 52 && old[i] <= 144);
        same += ok;
        dfs += !!(ch[i].flags & MTK_CHAN_F_DFS);
        passive += !!(ch[i].flags & MTK_CHAN_F_PASSIVE);
        five += ch[i].band == MTK_CHAN_5G;
    }
    hw_trace("OPS6 channels rc %d %u (old %u) agree %u 5g %u dfs %u passive %u", rc, n2, n1, same, five, dfs, passive);
    if (rc || !n2 || n1 != n2 || same != n2) hw_trace("UNEXPECTED ops6 channels");

    mtk_link_info li;
    mtk_chan lc;
    o->v5.v4.v3.v2.v1.get_link_info(d, &li);
    rc = o->get_link_chan(d, &lc);
    hw_trace("OPS6 link rc %d up %u band %u chan %u bw %u center %u flags %u", rc, li.up, lc.band, lc.chan, lc.bw, lc.center, lc.flags);
    if (li.up ? (rc || lc.chan != li.channel || lc.bw != li.bw || lc.center != li.center || lc.band != (li.channel > 14 ? 1 : 0)) : rc != -2)
        hw_trace("UNEXPECTED ops6 link chan");
}
#else
static void checkOps6(Core *) { hw_trace("UNEXPECTED ops6 asked of a core without it"); }
#endif

int main(int argc, char **argv) {
    const char *covPath = nullptr;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--cov") && i + 1 < argc) covPath = argv[++i];
        else if (!strcmp(argv[i], "--props")) hw_set_props(true);
        else if (!strcmp(argv[i], "--ops6")) g_ops6 = true;
        else if (!strcmp(argv[i], "--oldset")) g_hw_old_set = true;
        else if (!strcmp(argv[i], "--chip") && i + 1 < argc) g_hw_chip = (uint16_t)strtoul(argv[++i], nullptr, 16);
        else if (!strcmp(argv[i], "--chipid") && i + 1 < argc) g_hw_chipid_raw = (int)strtoul(argv[++i], nullptr, 16);
        else if (!strcmp(argv[i], "--lnkctl") && i + 1 < argc) g_hw_lnkctl = (int)strtoul(argv[++i], nullptr, 16);
        else if (!strcmp(argv[i], "--bootarg") && i + 1 < argc) hw_set_bootarg(argv[++i]);
        else if (!strcmp(argv[i], "--pcidev") && i + 1 < argc) g_hw_pci_device = (uint16_t)strtoul(argv[++i], nullptr, 16);
        else g_tracePath = argv[i];
    }
    if (!g_tracePath) { fprintf(stderr, "usage: hwtrace <trace.txt> [--cov file] [--props] [--pcidev hex]\n"); return 2; }
    for (int s : { SIGSEGV, SIGBUS, SIGABRT, SIGILL, SIGFPE, SIGTRAP }) signal(s, onCrash);

    g_model = new Mt7925Model;
    IOPCIDevice *pci = new IOPCIDevice;
    Core *d = new Core;
    d->init();

    // 1. PCI start: BAR0, ownership handshake, CHIPID gate, then hwStart (WFSYS reset, rings, prefetch, IRQ mask,
    //    patch and RAM download) and hwInit (NIC capability, MDP, DEV_INFO, domain, CLC, SKU, RTS, RX filter)
    hw_trace("PHASE start");
    d->start(pci);
    bool up = d->timer != nullptr && !d->fwDead;

    if (up) {
        // 2. the tick starts the first firmware scan; the fake air answers with three beacons and SCAN_DONE
        hw_trace("PHASE scan");
        tickUntil(d, 3000, [&] { return d->scanEverDone; });

        // 3. join the open 5 GHz AP through the connection manager (as the user client would ask): BSS_INFO, STA_REC,
        //    ROC, auth, assoc, then BSS_INFO and STA_REC for the link
        hw_trace("PHASE join");
        mtk_join_params jp = {};
        jp.version = MTK_USER_API_VERSION;
        jp.key_type = MTK_KEY_NONE;
        jp.ssid_len = (uint8_t)strlen("WTL-OPEN-5G");
        memcpy(jp.ssid, "WTL-OPEN-5G", jp.ssid_len);
        if (d->coreJoin(&jp) != 0) hw_trace("UNEXPECTED coreJoin");
        tickUntil(d, 3000, [&] { return d->joinState == 5 || (d->joinState == 0 && d->joinFailures); });
        tickUntil(d, 50, [] { return false; });
        if (g_ops6) checkOps6(d);

        // 4. leave: deauth, STA_REC remove, BSS_INFO off
        hw_trace("PHASE disconnect");
        d->coreDisconnect();
        tickUntil(d, 20, [] { return false; });

        // 5. a new country: channel domain, CLC and per-rate power again
        hw_trace("PHASE country");
        Core::opSetCountry(d, "DE");
        tickUntil(d, 400, [&] { return !d->countryPending; });

        // 6. register windows: first and last word of every fixed window, the L1 and L2 remap paths, a direct offset
        hw_trace("PHASE regmap");
        static const struct { uint32_t phys, maps, size; } kProbe[] = {
#include "regmap_probe.inc"
        };
        for (const auto &w : kProbe) {
            d->rr(w.phys);
            if (w.maps + w.size <= 0x200000) d->rr(w.phys + w.size - 4);
        }
        for (uint32_t a : { 0x18000000u, 0x18011100u, 0x70010200u, 0x7c3ffffcu, 0x60000000u, 0x83000010u, 0x00001000u })
            d->rr(a);
        d->wr(0x18011100u, 0x2);
        d->wr(0x60000004u, 0x1);

        // 7. supervisor reload: chip reset and firmware download a second time over the existing rings
        hw_trace("PHASE reload");
        int rc = d->reload("hwtrace");
        hw_trace("RELOAD rc %d", rc);
    }

    // 8. restart: quiesce (driver own, DMA off, WFSYS reset), then stop
    hw_trace("PHASE shutdown");
    d->systemWillShutdown(kIOMessageSystemWillRestart);
    hw_trace("PHASE stop");
    d->stop(pci);

    hw_trace("RESULT up %d join_state %u joins %u scans %u links %u", up, d->joinState, d->joins, d->scans, d->joinedOnce);
    writeTrace();
    if (covPath) writeCoverage(covPath);
    return 0;
}
