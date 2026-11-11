// <wtl> project: wtlwireless | file: tests/hwtrace/shim/libkern/OSByteOrder.h | author: watchthelight </wtl>
// The real header first (system headers pull it in), then the 32-bit accessors are redirected into the device model.
#ifndef HWTRACE_OSBYTEORDER_H
#define HWTRACE_OSBYTEORDER_H
#include_next <libkern/OSByteOrder.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t hw_rd32(const volatile void *base, uintptr_t off);
void hw_wr32(volatile void *base, uintptr_t off, uint32_t val);
#ifdef __cplusplus
}
#endif
#define OSReadLittleInt32 hw_rd32
#define OSWriteLittleInt32 hw_wr32
#endif
