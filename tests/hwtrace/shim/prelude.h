// <wtl> project: wtlwireless | file: tests/hwtrace/shim/prelude.h | author: watchthelight </wtl>
// Force-included in every C++ unit: host headers first, then private/protected opened so the harness can call
// driver internals without touching the driver sources. All units see the same layout.
#ifndef HWTRACE_PRELUDE_H
#define HWTRACE_PRELUDE_H
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <algorithm>
#include <functional>
#include <memory>
#include <new>
#include <typeinfo>
#include <type_traits>
#include <sys/types.h>
#include <strings.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cxxabi.h>
#define private public
#define protected public
#endif
