#!/usr/bin/env python3
# <wtl> project: wtlwireless | file: tools/abi-plist.py | author: watchthelight </wtl>
# SPDX-License-Identifier: ISC
# Replace the MtkNativeAbi allow-list of a built kext's Info.plist with the builds whose headers the binary was compiled
# against. build.sh calls it for a Sequoia build (abi/24H*/): the source Info.plist lists the Tahoe builds, and a binary built
# against the 24H32 headers must never engage on Tahoe (or the reverse).
#   abi-plist.py OUT/Info.plist abi/24H32/abi.json [more abi.json ...]
import json
import plistlib
import sys


def entry(path):
    a = json.load(open(path))
    if not a.get('xnu', '').startswith('xnu-'):
        # a short kernel string such as "Darwin Kernel Version 24.6.0" would let every 24.6.x kernel through
        sys.exit('FAIL: %s has no xnu tag; put the full `uname -v` of that build into the dump\'s kernel_version' % path)
    return {'Build': a['build'], 'Xnu': a['xnu'], 'Sizes': {k: int(v) for k, v in sorted(a['sizes'].items())}}


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__ or 'usage: abi-plist.py Info.plist abi.json...')
    plist = argv[0]
    with open(plist, 'rb') as f:
        p = plistlib.load(f)
    builds = [entry(a) for a in argv[1:]]
    for pers in p.get('IOKitPersonalities', {}).values():
        if 'MtkNativeAbi' in pers:
            pers['MtkNativeAbi']['Builds'] = builds
    with open(plist, 'wb') as f:
        plistlib.dump(p, f)
    print('MtkNativeAbi: %s' % ', '.join('%s (%s)' % (b['Build'], b['Xnu']) for b in builds))


if __name__ == '__main__':
    main(sys.argv[1:])
