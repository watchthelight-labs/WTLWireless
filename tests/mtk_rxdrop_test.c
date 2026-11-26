// <wtl> project: wtlwireless | file: tests/mtk_rxdrop_test.c | author: watchthelight </wtl>
// Host test for the AWDL phase 0 drop ring (MtkRxDrop.h): counts by reason, ring wrap, 32 byte cap, formatting.
#include "../src/core/MtkRxDrop.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

int main(void) {
    mtk_rxdrop d;
    memset(&d, 0, sizeof(d));
    uint8_t f[64];
    for (int i = 0; i < 64; i++) f[i] = (uint8_t)i;
    char s[2 + 2 * MTK_RXDROP_BYTES + 8];
    CHECK(!mtk_rxdrop_format(&d, 0, s, sizeof(s)));              // empty ring
    mtk_rxdrop_note(&d, MTK_RXDROP_MGMT_UNUSED, f, 64);          // capped at 32 bytes
    CHECK(d.e[0].len == MTK_RXDROP_BYTES);
    mtk_rxdrop_note(&d, MTK_RXDROP_CTRL_OTHER, f, 10);           // shorter than the cap
    CHECK(d.e[1].len == 10);
    mtk_rxdrop_note(&d, 99, f, 10);                              // unknown reason ignored
    CHECK(d.head == 2);
    CHECK(d.count[MTK_RXDROP_MGMT_UNUSED] == 1 && d.count[MTK_RXDROP_CTRL_OTHER] == 1);
    CHECK(mtk_rxdrop_format(&d, 0, s, sizeof(s)));
    CHECK(!strcmp(s, "0:00010203040506000000"));                 // newest first, addr1 low bytes zeroed
    f[1] = 3;                                                    // ToDS and FromDS: four addresses
    mtk_rxdrop_note(&d, MTK_RXDROP_MGMT_UNUSED, f, 64);
    const uint8_t *b4 = d.e[(d.head - 1) % MTK_RXDROP_RING].b;
    for (int i = 0; i < MTK_RXDROP_BYTES; i++) {                 // every byte: low half of addr1..4 zero, the rest untouched
        int low = (i >= 7 && i <= 9) || (i >= 13 && i <= 15) || (i >= 19 && i <= 21) || (i >= 27 && i <= 29);
        CHECK(b4[i] == (low ? 0 : (i == 1 ? 3 : i)));
    }
    memset(f, 0xAA, sizeof(f)); f[1] = 3;                        // no byte value may hide a missed position
    mtk_rxdrop_note(&d, MTK_RXDROP_MGMT_UNUSED, f, 64);
    b4 = d.e[(d.head - 1) % MTK_RXDROP_RING].b;
    CHECK(b4[27] == 0 && b4[28] == 0 && b4[29] == 0 && b4[22] == 0xAA && b4[23] == 0xAA && b4[26] == 0xAA && b4[30] == 0xAA);
    for (int i = 0; i < 64; i++) f[i] = (uint8_t)i;
    f[1] = 1;
    for (int i = 0; i < 20; i++) mtk_rxdrop_note(&d, MTK_RXDROP_DATA_NOPAYLOAD, f, 24);   // wraps the ring
    CHECK(d.head == 24 && d.count[MTK_RXDROP_DATA_NOPAYLOAD] == 20);
    CHECK(mtk_rxdrop_format(&d, MTK_RXDROP_RING - 1, s, sizeof(s)));
    CHECK(s[0] == '3');
    CHECK(!mtk_rxdrop_format(&d, MTK_RXDROP_RING, s, sizeof(s)));    // older than the ring holds
    printf("mtk_rxdrop_test ok\n");
    return 0;
}
