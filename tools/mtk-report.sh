#!/bin/bash
# <wtl> project: wtlwireless | file: tools/mtk-report.sh | author: watchthelight </wtl>
# Collect what a bug report about WTLWireless (or the older MtkWlan / MtkWlanNative kexts) needs into one archive on the Desktop.
#   bash mtk-report.sh [--no-redact] [--minutes N]
# No sudo needed; files are created private (umask 077) and slow commands are cut off by a time limit (tmo). Panic report
# files are listed by name and date only. MAC addresses and SSIDs are replaced by stable placeholders unless --no-redact is given; the archive
# never contains keys or passphrases (the driver keeps none in ioreg). Attach the .tar.gz to a GitHub issue.
set -u
umask 077     # the work folder and the archive are for the tester (and whoever they send it to) only
REDACT=1; MIN=30
while [ $# -gt 0 ]; do case "$1" in --no-redact) REDACT=0;; --minutes) MIN=$2; shift;; *) echo "unknown option $1"; exit 2;; esac; shift; done
OUT=${TMPDIR:-/tmp}/mtk-report-$(date +%Y%m%d-%H%M%S); mkdir -p "$OUT"
step() { echo "  $1"; }
# tmo SECONDS command [args]: run the command, kill it after SECONDS (macOS ships no timeout(1)). A background watchdog sleeps,
# then sends TERM and, two seconds later, KILL. Output and exit status are the command's; a kill leaves a note on stderr.
tmo() {
  _s=$1; shift
  "$@" &
  _p=$!
  ( sleep "$_s"; kill "$_p" 2>/dev/null; sleep 2; kill -9 "$_p" 2>/dev/null ) >/dev/null 2>&1 &
  _w=$!
  { wait "$_p"; } 2>/dev/null; _r=$?
  pkill -P "$_w" 2>/dev/null; kill "$_w" 2>/dev/null; { wait "$_w"; } 2>/dev/null
  if [ "$_r" -eq 143 ] || [ "$_r" -eq 137 ]; then echo "[stopped after ${_s}s: $1]" >&2; fi
  return "$_r"
}
echo "collecting into $OUT"
step "system";        { sw_vers; uname -a; sysctl -n kern.bootargs; } > "$OUT/system.txt" 2>&1
step "pci";           tmo 30 ioreg -r -c IOPCIDevice -l -w0 2>/dev/null | grep -B2 -A30 '"vendor-id" = <c314' | grep -E 'IOName|vendor-id|device-id|subsystem|revision-id|pcidebug|IOPCIExpressLinkStatus|name"' > "$OUT/pci-mediatek.txt" 2>&1
step "kexts";         tmo 30 kextstat 2>/dev/null | grep -E 'watchthelight|Mtk|IO80211|IOSkywalk' > "$OUT/kextstat.txt"; for k in WTLWireless WTLWireless.Native WTLWireless.Bt MtkWlan MtkWlanNative MtkBt; do tmo 30 kmutil showloaded --bundle-identifier org.watchthelight.$k 2>/dev/null; done >> "$OUT/kextstat.txt" 2>&1
step "ioreg";         for c in WTLWireless WTLWirelessEth WTLWirelessNative WTLWirelessNativeIf WTLWirelessBt MtkWlan MtkWlanEth MtkWlanNative MtkWlanNativeIf MtkBt; do tmo 30 ioreg -r -c $c -l -w0 2>/dev/null > "$OUT/ioreg-$c.txt"; [ -s "$OUT/ioreg-$c.txt" ] || rm -f "$OUT/ioreg-$c.txt"; done
step "interfaces";    { tmo 20 ifconfig -a; echo; tmo 20 netstat -rn; echo; tmo 20 networksetup -listallhardwareports; echo; tmo 20 networksetup -listnetworkserviceorder; } > "$OUT/network.txt" 2>&1
step "mtkwifi";       { command -v mtkwifi >/dev/null && mtkwifi status --json; command -v mtkwifi >/dev/null && mtkwifi list; } > "$OUT/mtkwifi.txt" 2>&1
step "wifi profile";  tmo 90 system_profiler SPAirPortDataType > "$OUT/airport-profile.txt" 2>&1
step "kernel log";    tmo 120 log show --last "${MIN}m" --predicate 'process == "kernel" AND (eventMessage CONTAINS "WTLWireless" OR eventMessage CONTAINS "Mtk" OR eventMessage CONTAINS "80211")' --style compact > "$OUT/kernel-log.txt" 2>&1
step "wifi daemons";  tmo 120 log show --last "${MIN}m" --info --predicate 'process == "airportd" OR process == "wifid" OR process == "WiFiAgent" OR subsystem BEGINSWITH "com.apple.wifi" OR (subsystem == "com.apple.IPConfiguration" AND eventMessage CONTAINS "en")' --style compact > "$OUT/wifi-daemons.txt" 2>&1
# Names, dates and counts only, no panic text: stat prints the file name and its date (no owner), nvram output is only counted.
step "panic inventory"
{
  echo "kernel panic report files in /Library/Logs/DiagnosticReports (newest five, names and dates only):"
  ls -t /Library/Logs/DiagnosticReports/*.panic /Library/Logs/DiagnosticReports/Kernel-*.panic 2>/dev/null | awk '!seen[$0]++' | head -5 |
    while read -r f; do stat -f '%Sm  %N' -t '%Y-%m-%d %H:%M' "$f"; done
  echo
  echo "NVRAM lines mentioning aapl,panic-info (count only, contents not read): $(tmo 20 nvram -p 2>/dev/null | grep -c aapl,panic-info)"
  echo
  echo "last reboot shutdown (newest ten):"
  tmo 20 last reboot shutdown 2>&1 | head
} > "$OUT/panic-inventory.txt" 2>&1
if [ "$REDACT" = 1 ]; then
  step "redacting MAC addresses and SSIDs"
  python3 - "$OUT" <<'PY'
import os, re, subprocess, sys
root = sys.argv[1]
macs, ssids = {}, {}
mac_re = re.compile(r'\b([0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5})\b')
# netstat prints MACs without leading zeros; uname prints the Bonjour host name; IPv6 addresses carry the ISP prefix and,
# for EUI-64 ones (the router's link-local address), the MAC itself
mac_short_re = re.compile(r'(?<![\w:.-])((?:[0-9A-Fa-f]{1,2}:){5}[0-9A-Fa-f]{1,2})(?![\w:])')
host_re = re.compile(r'(?<![\w.-])([A-Za-z0-9][A-Za-z0-9-]{1,62})\.local\b')
ip6_re = re.compile(r'(?<![\w:])((?:[0-9A-Fa-f]{0,4}:){2,7}[0-9A-Fa-f]{0,4})(?![\w:])')
hosts, ip6s = {}, {}
def mac_short(m):
    v = ':'.join(g.zfill(2) for g in m.group(1).lower().split(':'))
    return mac(re.match(r'(.*)', v))
# the account name, its full name and the Mac's own names (airportd logs "user: <name>"; crash file names carry the host name)
def own(*cmd):
    try: return subprocess.run(cmd, capture_output=True, text=True, timeout=10).stdout.strip()
    except Exception: return ''
acct = os.environ.get('USER') or own('id', '-un')
names = [n for n in {own('id', '-F'), own('scutil', '--get', 'ComputerName'), own('scutil', '--get', 'LocalHostName'), own('hostname', '-s')} if len(n) >= 3]
own_res = [(re.compile(re.escape(n), re.I), 'my-mac') for n in sorted(names, key=len, reverse=True)]
if acct:
    own_res += [(re.compile(r'(/Users/)' + re.escape(acct) + r'\b'), r'\1user'), (re.compile(r'(\buser[:=\s]+)' + re.escape(acct) + r'\b', re.I), r'\1user')]
    if len(acct) >= 6: own_res.append((re.compile(r'\b' + re.escape(acct) + r'\b'), 'user'))
def host(m):
    return hosts.setdefault(m.group(1), 'host-%d' % (len(hosts) + 1)) + '.local'
def ip6(m):
    v = m.group(1)
    if not ('::' in v or v.count(':') == 7) or v.lower() in ('::1', 'fe80::1', 'fe80::', '::'): return v
    return ip6s.setdefault(v.lower(), 'ip6-%d' % (len(ip6s) + 1))
ssid_res = [re.compile(r'(ssid[=:\s"\']+)([^"\',\s<>)]{1,32})', re.I), re.compile(r'("Mtk(?:Target|Want)?Ssid" = ")([^"]*)'), re.compile(r'(SSID\s*:\s*)(\S.*)$', re.M)]
def mac(m):
    k = m.group(1).lower()
    if k in ('ff:ff:ff:ff:ff:ff', '00:00:00:00:00:00'): return k
    return macs.setdefault(k, 'mac-%02d:00:00:00:00:00' % (len(macs) + 1))
def ssid(m):
    s = m.group(2)
    return m.group(1) + ssids.setdefault(s, 'ssid-%d' % (len(ssids) + 1))
for name in os.listdir(root):
    p = os.path.join(root, name)
    if not name.endswith('.txt'): continue
    t = open(p, errors='replace').read()
    t = mac_re.sub(mac, t)
    t = ip6_re.sub(ip6, t)
    t = mac_short_re.sub(mac_short, t)
    t = host_re.sub(host, t)
    for r in ssid_res: t = r.sub(ssid, t)
    for r, rep in own_res: t = r.sub(rep, t)
    open(p, 'w').write(t)
open(os.path.join(root, 'REDACTED.txt'), 'w').write('%d MAC addresses, %d IPv6 addresses, %d host names and %d SSIDs replaced by placeholders (same placeholder for the same value throughout).\n' % (len(macs), len(ip6s), len(hosts), len(ssids)))
PY
fi
A=~/Desktop/$(basename "$OUT").tar.gz
tar -czf "$A" -C "$(dirname "$OUT")" "$(basename "$OUT")" && rm -rf "$OUT"
echo "report: $A"
echo "open an issue at github.com/watchthelight-labs/WTLWireless and attach it; say which card (PCI ids are in pci-mediatek.txt), which macOS build, and what you did"
