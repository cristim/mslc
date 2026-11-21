#!/bin/sh
# Records whether this machine has a Metal device and prints `metal=yes|no` (also to
# $GITHUB_OUTPUT when set). Used by .github/workflows/diffcheck.yml, and runnable by hand
# to answer "does a hosted macOS runner expose a Metal device?" from the log.
set -u

echo "## Metal probe"
sw_vers 2>&1
uname -m
echo "--- system_profiler SPDisplaysDataType"
system_profiler SPDisplaysDataType 2>&1 | head -30
echo "--- MTLCreateSystemDefaultDevice / MTLCopyAllDevices"
cat > "${TMPDIR:-/tmp}/metal-probe.swift" <<'EOF'
import Metal
let all = MTLCopyAllDevices()
print("devices: \(all.count)")
for d in all { print("  \(d.name) lowPower=\(d.isLowPower) headless=\(d.isHeadless)") }
print(MTLCreateSystemDefaultDevice() == nil ? "default: none" : "default: present")
EOF
out=$(xcrun swift "${TMPDIR:-/tmp}/metal-probe.swift" 2>&1)
echo "$out"
if echo "$out" | grep -q '^default: present'; then verdict=yes; else verdict=no; fi
echo "metal=$verdict"
if [ -n "${GITHUB_OUTPUT:-}" ]; then echo "metal=$verdict" >> "$GITHUB_OUTPUT"; fi
