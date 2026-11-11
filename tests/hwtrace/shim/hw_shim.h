// <wtl> project: mtkwlan | file: tests/hwtrace/shim/hw_shim.h | author: watchthelight </wtl>
// Host stand-ins for the kernel and IOKit pieces MtkWlan uses. Only what the bring-up, scan, join and power paths
// need; anything else is reported as UNEXPECTED in the trace instead of silently succeeding.
#ifndef HW_SHIM_H
#define HW_SHIM_H

#include <libkern/OSByteOrder.h>
#include <sys/random.h>

// ---- harness services (host_shim.cpp)
void hw_trace(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void hw_unexpected(const char *what);
void hw_property(const char *key, const std::string &val);
extern uint16_t g_hw_pci_device;       // PCI device ID the fake card reports (--pcidev)
uint8_t *hw_dma_alloc(uint32_t size, uint32_t align, uint32_t *iova);
uint8_t *hw_dma_host(uint32_t iova);
uint8_t *hw_bar();
uint32_t hw_bar_len();
uint32_t hw_pci_cmd(uint32_t set, uint32_t clr);
uint64_t hw_uptime();

// ---- basic types
typedef int IOReturn;
typedef uint32_t IOOptionBits;
typedef uint32_t UInt32;
typedef uint64_t UInt64;
typedef uint8_t UInt8;
typedef uint64_t mach_vm_size_t;
typedef uint64_t mach_vm_address_t;
typedef uint64_t IOPhysicalAddress;
typedef uint64_t IOByteCount;
#define kernel_task ((void *)0)
constexpr IOReturn kIOReturnSuccess = 0;
constexpr IOReturn kIOReturnError = (IOReturn)0xe00002bc;
constexpr IOReturn kIOReturnNoMemory = (IOReturn)0xe00002bd;
constexpr IOReturn kIOReturnBadArgument = (IOReturn)0xe00002c2;
constexpr IOReturn kIOReturnUnsupported = (IOReturn)0xe00002c7;
constexpr IOReturn kIOReturnNotReady = (IOReturn)0xe00002d0;
constexpr IOReturn kIOReturnBusy = (IOReturn)0xe00002d5;

// ---- libkern object model
#define OSDeclareDefaultStructors(x)
#define OSDefineMetaClassAndStructors(a, b)
#define OSDynamicCast(T, o) (dynamic_cast<T *>(o))
#define OSSafeReleaseNULL(x) do { if (x) { (x)->release(); (x) = nullptr; } } while (0)

class OSObject {
public:
    int rc = 1;
    virtual ~OSObject() {}
    virtual bool init() { return true; }
    virtual void free() {}
    void retain() { rc++; }
    void release() { if (--rc == 0) { free(); delete this; } }
};
class OSMetaClass {
public:
    static OSObject *allocClassWithName(const char *) { return nullptr; }
};
class OSString : public OSObject {
public:
    std::string s;
    unsigned getLength() const { return (unsigned)s.size(); }
    const char *getCStringNoCopy() const { return s.c_str(); }
    bool isEqualTo(const char *o) const { return s == o; }
};
class OSSymbol : public OSString {};
class OSNumber : public OSObject {
public:
    uint64_t v = 0;
    uint32_t unsigned32BitValue() const { return (uint32_t)v; }
    uint8_t unsigned8BitValue() const { return (uint8_t)v; }
};
class OSBoolean : public OSObject {
public:
    bool b = false;
    bool isTrue() const { return b; }
};
class OSArray : public OSObject {
public:
    std::vector<OSObject *> items;
    unsigned getCount() const { return (unsigned)items.size(); }
    OSObject *getObject(unsigned i) const { return i < items.size() ? items[i] : nullptr; }
};
class OSDictionary : public OSObject {
public:
    OSObject *getObject(const char *) const { return nullptr; }
};

// ---- event sources and the work loop
class IOEventSource : public OSObject {};
class IOTimerEventSource : public IOEventSource {
public:
    typedef void (*Action)(OSObject *, IOTimerEventSource *);
    OSObject *owner = nullptr;
    Action action = nullptr;
    static IOTimerEventSource *timerEventSource(OSObject *o, Action a) {
        auto *t = new IOTimerEventSource;
        t->owner = o;
        t->action = a;
        return t;
    }
    IOReturn setTimeoutUS(uint32_t) { return kIOReturnSuccess; }
    IOReturn cancelTimeout() { return kIOReturnSuccess; }
};
class IOWorkLoop : public OSObject {
public:
    typedef IOReturn (*Action)(OSObject *, void *, void *, void *, void *);
    static IOWorkLoop *workLoop() { return new IOWorkLoop; }
    IOReturn addEventSource(IOEventSource *) { return kIOReturnSuccess; }
    IOReturn removeEventSource(IOEventSource *) { return kIOReturnSuccess; }
    IOReturn runAction(Action a, OSObject *t, void *a0 = nullptr, void *a1 = nullptr, void *a2 = nullptr, void *a3 = nullptr) {
        return a(t, a0, a1, a2, a3);
    }
};

// ---- power management constants
#define kIOPMPowerStateVersion1 1
#define kIOPMPowerOn 1
#define kIOPMAckImplied 0
#define kIOMessageSystemWillPowerOff 0xe0000250
#define kIOMessageSystemWillRestart 0xe0000310
struct IOPMPowerState {
    unsigned long version, capabilityFlags, outputPowerCharacter, inputPowerRequirement, staticPower, stateOrder,
        powerToAttain, timeToAttain, settleUpTime, timeToLower, settleDownTime, powerDomainBudget;
};

class IOMapper : public OSObject {
public:
    static IOMapper *copyMapperForDevice(void *) { return nullptr; }
};

// ---- IOService: properties only feed STEP lines (and PROP lines when enabled)
class IOService : public OSObject {
public:
    virtual bool start(IOService *) { return true; }
    virtual void stop(IOService *) {}
    virtual IOReturn setPowerState(unsigned long, IOService *) { return kIOReturnSuccess; }
    virtual void systemWillShutdown(IOOptionBits) {}
    virtual IOWorkLoop *getWorkLoop() const { return nullptr; }
    virtual IOReturn callPlatformFunction(const OSSymbol *, bool, void *, void *, void *, void *) { return kIOReturnUnsupported; }
    bool setProperty(const char *k, const char *v) { hw_property(k, v); return true; }
    bool setProperty(const char *k, uint64_t v, unsigned) { char b[32]; snprintf(b, sizeof b, "%llu", (unsigned long long)v); hw_property(k, b); return true; }
    bool setProperty(const char *k, bool v) { hw_property(k, v ? "true" : "false"); return true; }
    bool setProperty(const char *k, OSObject *) { hw_property(k, "(object)"); return true; }
    OSObject *getProperty(const char *) const { return nullptr; }
    void removeProperty(const char *) {}
    bool attach(IOService *) { return true; }
    void registerService() {}
    void PMinit() {}
    void PMstop() {}
    void joinPMtree(IOService *) {}
    IOReturn registerPowerDriver(IOService *, IOPMPowerState *, unsigned long) { return kIOReturnSuccess; }
};

// ---- PCI device
#define kIOPCIConfigVendorID 0
#define kIOPCIConfigDeviceID 2
#define kIOPCIConfigCommand 4
#define kIOPCIConfigSubSystemVendorID 0x2c
#define kIOPCIConfigSubSystemID 0x2e
#define kIOPCIConfigBaseAddress0 0x10
#define kIOPCIPCIExpressCapability 0x10
#define kIOInterruptTypeLevel 1
#define kIOInterruptTypePCIMessaged 0x10000
typedef void (*IOInterruptAction)(OSObject *, void *, IOService *, int);
class IOMemoryMap : public OSObject {
public:
    uintptr_t getVirtualAddress() const { return (uintptr_t)hw_bar(); }
    uint64_t getLength() const { return hw_bar_len(); }
    uint64_t getPhysicalAddress() const { return 0xa0000000; }
};
class IOPCIDevice : public IOService {
public:
    // the card in the Legion: 14c3:7925; subsystem IDs read 0 so no machine detail enters the trace
    uint16_t configRead16(uint32_t off) {
        switch (off) {
        case kIOPCIConfigCommand: return (uint16_t)hw_pci_cmd(0, 0);
        case kIOPCIConfigVendorID: return 0x14c3;
        case kIOPCIConfigDeviceID: return g_hw_pci_device;
        default: return 0;
        }
    }
    uint32_t configRead32(uint32_t) { return 0; }
    void configWrite16(uint32_t off, uint16_t v) { hw_trace("PCI cfgw16 0x%02x 0x%04x", off, v); }
    void setMemoryEnable(bool on) { hw_pci_cmd(on ? 2 : 0, on ? 0 : 2); hw_trace("PCI memory %d", on); }
    void setBusMasterEnable(bool on) { hw_pci_cmd(on ? 4 : 0, on ? 0 : 4); hw_trace("PCI busmaster %d", on); }
    IOMemoryMap *mapDeviceMemoryWithRegister(uint8_t) { return new IOMemoryMap; }
    IOReturn getInterruptType(int i, int *t) { if (i != 0) return kIOReturnBadArgument; *t = kIOInterruptTypePCIMessaged; return kIOReturnSuccess; }
    IOReturn registerInterrupt(int i, OSObject *, IOInterruptAction, void *) { hw_trace("PCI irq register %d", i); return kIOReturnSuccess; }
    IOReturn unregisterInterrupt(int i) { hw_trace("PCI irq unregister %d", i); return kIOReturnSuccess; }
    IOReturn enableInterrupt(int i) { hw_trace("PCI irq enable %d", i); return kIOReturnSuccess; }
    IOReturn disableInterrupt(int i) { hw_trace("PCI irq disable %d", i); return kIOReturnSuccess; }
    bool findPCICapability(uint32_t, uint8_t *off) { if (off) *off = 0; return false; }
    void saveDeviceState() { hw_unexpected("saveDeviceState"); }
    void restoreDeviceState() { hw_unexpected("restoreDeviceState"); }
};

// ---- DMA memory: one block per call, device address from a bump allocator the model can resolve
#define kIODirectionInOut 3
#define kIOMemoryPhysicallyContiguous 0x10
#define kIOMemoryMapperNone 0x800
class IOBufferMemoryDescriptor : public OSObject {
public:
    uint8_t *mem = nullptr;
    uint32_t iova = 0, size = 0;
    static IOBufferMemoryDescriptor *inTaskWithPhysicalMask(void *, IOOptionBits, mach_vm_size_t sz, mach_vm_address_t) {
        auto *b = new IOBufferMemoryDescriptor;
        b->size = (uint32_t)sz;
        b->mem = hw_dma_alloc(b->size, 4096, &b->iova);
        return b;
    }
    IOReturn prepare() { return kIOReturnSuccess; }
    IOReturn complete() { return kIOReturnSuccess; }
    void *getBytesNoCopy() { return mem; }
    uint64_t getPhysicalSegment(uint64_t, uint64_t *, IOOptionBits) { return iova; }
};
class IODMACommand : public OSObject {
public:
    enum MappingOptions { kMapped = 1 };
    struct Segment64 { UInt64 fIOVMAddr, fLength; };
    IOBufferMemoryDescriptor *md = nullptr;
    static IODMACommand *withSpecification(int, UInt8, UInt64, MappingOptions, UInt64, UInt32, IOMapper *, void * = nullptr) {
        return new IODMACommand;
    }
    IOReturn setMemoryDescriptor(IOBufferMemoryDescriptor *m, bool) { md = m; return kIOReturnSuccess; }
    IOReturn clearMemoryDescriptor(bool) { md = nullptr; return kIOReturnSuccess; }
    IOReturn gen64IOVMSegments(UInt64 *, Segment64 *seg, UInt32 *n) {
        seg->fIOVMAddr = md->iova;
        seg->fLength = md->size;
        *n = 1;
        return kIOReturnSuccess;
    }
};
#define kIODMACommandOutputHost64 0

// ---- logging, time, boot args, kernel services
inline void IOLog(const char *, ...) {}
void IOSleep(unsigned ms);
void IODelay(unsigned us);
inline void clock_get_uptime(uint64_t *t) { *t = hw_uptime(); }
inline void absolutetime_to_nanoseconds(uint64_t a, uint64_t *ns) { *ns = a; }
inline bool PE_parse_boot_argn(const char *, void *, int) { return false; }
// the machine boots with mtkfe=native
inline bool PE_parse_boot_arg_str(const char *k, char *out, int n) {
    if (strcmp(k, "mtkfe")) return false;
    strlcpy(out, "native", (size_t)n);
    return true;
}
typedef void *thread_call_t;
typedef void (*thread_call_func_t)(void *, void *);
inline thread_call_t thread_call_allocate(thread_call_func_t, void *) { return (thread_call_t)1; }
inline bool thread_call_enter(thread_call_t) { hw_unexpected("thread_call_enter"); return false; }
inline bool thread_call_cancel(thread_call_t) { return false; }
inline bool thread_call_free(thread_call_t) { return true; }

// ---- mbufs: no data path is driven, so any use is flagged
typedef struct hw_mbuf *mbuf_t;
#define MBUF_DONTWAIT 1
inline void mbuf_freem(mbuf_t) { hw_unexpected("mbuf_freem"); }
inline int mbuf_copydata(mbuf_t, size_t, size_t, void *) { hw_unexpected("mbuf_copydata"); return 1; }
inline int mbuf_copyback(mbuf_t, size_t, size_t, const void *, int) { hw_unexpected("mbuf_copyback"); return 1; }
inline size_t mbuf_pkthdr_len(mbuf_t) { hw_unexpected("mbuf_pkthdr_len"); return 0; }

#endif
