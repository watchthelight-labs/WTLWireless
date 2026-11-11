// <wtl> project: wtlwireless | file: tests/hwtrace/shim/sys/random.h | author: watchthelight </wtl>
// The real header plus the kernel's read_random, which the harness feeds from a fixed counter.
#ifndef HWTRACE_SYS_RANDOM_H
#define HWTRACE_SYS_RANDOM_H
#include_next <sys/random.h>
#ifdef __cplusplus
extern "C" {
#endif
void read_random(void *buf, unsigned len);
#ifdef __cplusplus
}
#endif
#endif
