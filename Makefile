# <wtl> project: wtlwireless | file: Makefile | author: watchthelight </wtl>
# Cross-builds the core alone on Linux (or WSL): out/linux/WTLWireless.kext, without the two plugins.
# Toolchain: tools/build-toolchain.sh. The bundle is built on macOS with ./build.sh, which also takes
# its source list and version from this file.
#   make
#   make SDK=/path/to/MacKernelSDK
NAME         := WTLWireless
KEXT_ID      := org.watchthelight.WTLWireless
KEXT_VERSION := 0.14.0

SDK    ?= $(CURDIR)/MacKernelSDK
LD     := LD_LIBRARY_PATH=/opt/dispatch/lib /opt/cctools/bin/x86_64-apple-darwin-ld
CC     := clang
CXX    := clang++
TARGET := -target x86_64-apple-macos10.15
RES    := $(shell clang -print-resource-dir)/include

SRCS := src/core/WTLWireless.cpp src/core/MtkChip.cpp src/core/MtkRegs.cpp src/core/MtkDma.cpp src/core/MtkFw.cpp \
        src/core/MtkScan.cpp src/core/MtkJoin.cpp src/core/MtkSec.cpp src/core/MtkNet.cpp src/core/MtkBa.cpp \
        src/core/MtkPower.cpp src/core/WTLWirelessEth.cpp src/core/MtkUserClient.cpp \
        src/core/MtkCrypto.c src/core/MtkSupp.c src/core/MtkSuper.c out/gen/MtkFirmware.c

KFLAGS := $(TARGET) -mkernel -fapple-kext -nostdinc -fno-builtin -fno-common -fno-stack-protector \
          -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT \
          -isystem $(RES) -I$(SDK)/Headers -Isrc/core -O2 -g0 -Wall -Wno-unused-parameter
CFLAGS   += $(KFLAGS) -std=gnu11
# the last three flags keep clang from emitting __cxa_atexit, which the kernel does not export
CXXFLAGS += $(KFLAGS) -std=gnu++17 -nostdinc++ -fno-exceptions -fno-rtti -fno-use-cxa-atexit \
            -fno-register-global-dtors-with-atexit -mllvm -disable-atexit-based-global-dtor-lowering \
            -Wno-inconsistent-missing-override

OBJS := $(addprefix out/linux/obj/,$(notdir $(SRCS:=.o))) out/linux/obj/kmod_info.o
BIN  := out/linux/$(NAME).kext/Contents/MacOS/$(NAME)

all: $(BIN) out/linux/$(NAME).kext/Contents/Info.plist check

out/linux/obj/%.cpp.o: src/core/%.cpp src/core/*.hpp src/core/*.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

out/linux/obj/%.c.o: src/core/%.c src/core/*.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

out/linux/obj/MtkFirmware.c.o: out/gen/MtkFirmware.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

out/linux/obj/kmod_info.o: src/kmod_info.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DKEXT_ID=$(KEXT_ID) -DKEXT_VERSION='"$(KEXT_VERSION)"' -c $< -o $@

# firmware blobs from linux-firmware, checked against firmware/mt7925/SHA256SUMS
out/gen/MtkFirmware.c: tools/gen_firmware.py firmware/mt7925/SHA256SUMS
	python3 tools/gen_firmware.py $@

$(BIN): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) -arch x86_64 -kext -platform_version macos 10.15 11.0 -o $@ $(OBJS) \
	  $(SDK)/Library/universal/libkmodc++.a $(SDK)/Library/x86_64/libkmod.a

out/linux/$(NAME).kext/Contents/Info.plist: src/core/Info.plist
	@mkdir -p $(dir $@)
	cp $< $@

# offline sanity: MH_KEXT_BUNDLE, kmod_info present, no C++ runtime symbols the kernel lacks
check: $(BIN)
	@llvm-objdump --macho --private-header $(BIN) | grep -q KEXTBUNDLE && echo "filetype: MH_KEXT_BUNDLE"
	@llvm-nm $(BIN) | grep -q ' _kmod_info' && echo "kmod_info: present"
	@echo "undefined symbols: $$(llvm-nm -u $(BIN) | wc -l)"
	@! llvm-nm -u $(BIN) | grep -E '^_{2,3}(cxa_|Unwind|gxx_)' || (echo "FAIL: C++ runtime symbols the kernel does not export"; false)

# host tests (clang, UBSan, Python oracle); they write only to out/tests/
test:
	bash tests/run.sh
	bash tests/native/run.sh

clean:
	rm -rf out

.PHONY: all check test clean
