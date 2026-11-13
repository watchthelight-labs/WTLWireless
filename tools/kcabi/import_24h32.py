#!/usr/bin/env python3
# <wtl> project: wtlwireless | file: tools/kcabi/import_24h32.py | author: watchthelight </wtl>
# SPDX-License-Identifier: ISC
#   python3 -I tools/kcabi/import_24h32.py <mt7925-macos-wifi checkout, branch sequoia>/abi/native-wifi/24H32 abi/native-wifi/24H32
# then: genhdr.py gen abi/native-wifi/24H32 abi/24H32 --iface-class WTLWirelessNativeIf --impl src/native/WTLWirelessNativeIf.cpp src/native/WTLWirelessNativeReq.cpp
# Assemble a kcabi-format dump of 24H32 from thanhnndev/mt7925-macos-wifi abi/native-wifi/24H32 (top level + round*/)
import os, re, sys, json, glob, shutil
src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
def find(name):
    c = [os.path.join(src, name)] + sorted(glob.glob(os.path.join(src, 'round*', name)), reverse=True)
    for p in c:
        if os.path.exists(p): return p
    return None
classes = ['IOSkywalkNetworkInterface','IOSkywalkEthernetInterface','IO80211SkywalkInterface','IO80211InfraInterface',
           'IO80211InfraProtocol','IO80211Controller','IO80211WorkQueue','IO80211CommandGate','AppleBCMWLANSkywalkInterface',
           'AppleBCMWLANInfraProtocol','AppleBCMWLANCore','IOSkywalkPacketBufferPool','IOSkywalkPacketQueue',
           'IOSkywalkTxSubmissionQueue','IOSkywalkRxSubmissionQueue','IOSkywalkTxCompletionQueue','IOSkywalkRxCompletionQueue',
           'IOSkywalkPacket','CCPipe','CCStream','CCLogStream','CCLogPipe','CCFaultReporter']
for c in classes:
    for kind in ('vtable', 'methods'):
        p = find('%s.%s.txt' % (c, kind))
        if p: shutil.copy(p, os.path.join(out, '%s.%s.txt' % (c, kind)))
        else: print('missing', c, kind)
# IOSkywalkInterface: not dumped on 24H32. Its slots are the prefix of IOSkywalkNetworkInterface's (IOService + 0x10e..283);
# rows the child overrides are renamed back: new IOSkywalkInterface virtuals to IOSkywalkInterface::, IOService ones to IOService::.
rows = [l.rstrip('\n').split('\t') for l in open(os.path.join(out, 'IOSkywalkNetworkInterface.vtable.txt')) if l[0].isdigit()]
LAST, SVC_LAST = 283, 267
o = ['# IOSkywalkInterface: %d slots (synthesized from IOSkywalkNetworkInterface, 24H32)' % (LAST + 1)]
for s, off, sym in rows:
    s = int(s)
    if s > LAST: break
    if '25IOSkywalkNetworkInterface' in sym:
        sym = sym.replace('25IOSkywalkNetworkInterface', '18IOSkywalkInterface' if s > SVC_LAST else '9IOService')
    o.append('%d\t%s\t%s' % (s, off, sym))
open(os.path.join(out, 'IOSkywalkInterface.vtable.txt'), 'w').write('\n'.join(o) + '\n')
# sizes from the MetaClass constructors (mov ecx, size before OSMetaClass::OSMetaClass)
sz = {'IOSkywalkInterface': 0xb0}
for p in glob.glob(os.path.join(src, '**', '*-MetaClass.asm.txt'), recursive=True):
    m = re.search(r'ecx, (0x[0-9a-f]+)', open(p).read())
    if m: sz[os.path.basename(p)[:-len('-MetaClass.asm.txt')]] = int(m.group(1), 16)
open(os.path.join(out, 'sizes.txt'), 'w').write('# class\tsize (MetaClass constructor, 24H32; IOSkywalkInterface from the SDK)\n' +
    ''.join('%s\t%#x\n' % (k, v) for k, v in sorted(sz.items())))
s = json.load(open(os.path.join(src, 'source.json')))
json.dump(dict(kc=s['kc'], sha256=s['sha256'], product_version='15.8.1', build='24H32',
               kernel_version='Darwin Kernel Version 24.6.0: Sun Aug 23 20:30:56 PDT 2026; root:xnu-11417.140.69.712.69~1/RELEASE_X86_64',
               source='thanhnndev/mt7925-macos-wifi abi/native-wifi/24H32 (BSD-3-Clause-Clear)'),
          open(os.path.join(out, 'source.json'), 'w'), indent=2)
print('ok', len(sz), 'sizes')
