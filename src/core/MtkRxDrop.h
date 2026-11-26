// <wtl> project: wtlwireless | file: src/core/MtkRxDrop.h | author: watchthelight </wtl>
// SPDX-License-Identifier: ISC
// Phase 0 of the AWDL work (research/related/2026-11-12/owly-awdl.md): count the frames rxFrame drops without a word, by reason,
// and keep the first bytes of the last few of them. Only the first 32 bytes are kept and the low three bytes of every address in them are zeroed, so ioreg never shows a station MAC. Header only so the host test can include it. No allocation. Single writer:
// the RX path (rxFrame) is the only caller of mtk_rxdrop_note; a reader on another thread can see one torn entry, which is
// accepted (best effort, no lock). Enabled by boot-arg mtkrxdrop=1 (default off); it changes which frames are accepted in no way.
#ifndef MTK_RXDROP_H
#define MTK_RXDROP_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#ifndef KERNEL
#include <stdio.h>      // the kext gets snprintf from libkern
#endif

enum {
    MTK_RXDROP_CTRL_OTHER = 0,      // control frame that is not a BlockAckReq for us
    MTK_RXDROP_MGMT_UNUSED,         // management frame no branch of rxFrame took (this is where AWDL action frames end)
    MTK_RXDROP_DATA_OTHER_BSS,      // not data, or not FromDS from our AP
    MTK_RXDROP_DATA_NOPAYLOAD,      // null / no-data subtype
    MTK_RXDROP_DATA_UNICAST_OTHER,  // unicast to another station
    MTK_RXDROP_REASONS
};
#define MTK_RXDROP_RING 8           // entries kept
#define MTK_RXDROP_BYTES 32         // bytes of 802.11 header kept per entry

typedef struct {
    uint8_t reason, len;            // len = bytes stored in b
    uint8_t b[MTK_RXDROP_BYTES];
} mtk_rxdrop_entry;

typedef struct {
    uint32_t count[MTK_RXDROP_REASONS];
    uint32_t head;                  // total entries written; slot = head % MTK_RXDROP_RING
    mtk_rxdrop_entry e[MTK_RXDROP_RING];
} mtk_rxdrop;

static inline void mtk_rxdrop_note(mtk_rxdrop *d, unsigned reason, const uint8_t *f, uint32_t flen) {
    if (reason >= MTK_RXDROP_REASONS) return;
    d->count[reason]++;
    mtk_rxdrop_entry *e = &d->e[d->head % MTK_RXDROP_RING];
    uint32_t n = flen < MTK_RXDROP_BYTES ? flen : MTK_RXDROP_BYTES;
    e->reason = (uint8_t)reason;
    e->len = (uint8_t)n;
    memcpy(e->b, f, n);
    // addresses never leave the frame: keep the OUI (and the group bit), zero the last three bytes of addr1..3, and of addr4 when present
    uint32_t nad = (n > 1 && (f[1] & 3) == 3) ? 4 : 3;
    for (uint32_t a = 0; a < nad; a++) {
        uint32_t at = a < 3 ? 4 + 6 * a : 24;       // addr4 sits after the 2 byte sequence control
        for (uint32_t i = at + 3; i < at + 6 && i < n; i++) e->b[i] = 0;
    }
    d->head++;
}

// "<reason>:<hex bytes>" of the n-th newest entry (0 = newest) into out; returns 0 when there is no such entry
static inline int mtk_rxdrop_format(const mtk_rxdrop *d, uint32_t n, char *out, size_t cap) {
    if (n >= d->head || n >= MTK_RXDROP_RING || cap < 8) return 0;
    const mtk_rxdrop_entry *e = &d->e[(d->head - 1 - n) % MTK_RXDROP_RING];
    size_t k = (size_t)snprintf(out, cap, "%u:", (unsigned)e->reason);
    for (uint32_t i = 0; i < e->len && i < MTK_RXDROP_BYTES && k + 3 < cap; i++) k += (size_t)snprintf(out + k, cap - k, "%02x", e->b[i]);
    return 1;
}
#endif
