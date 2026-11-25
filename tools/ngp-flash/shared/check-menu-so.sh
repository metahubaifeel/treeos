#!/bin/bash
# Gate for the injected menu library: refuse to deploy a .so that cannot be
# fully resolved on THIS machine. Run on the handheld (or with a chroot).
# Usage: check-menu-so.sh PATH/TO/quick-menu.so [more.so...]
set -u
rc=0
for so in "$@"; do
    echo "== $so"
    if [[ ! -f $so ]]; then echo "  MISSING"; rc=1; continue; fi
    # 1) every NEEDED library present and resolvable
    if ! ldd "$so" >/dev/null 2>&1; then echo "  ldd failed"; rc=1; fi
    miss=$(ldd "$so" 2>/dev/null | grep -c 'not found')
    [[ $miss -eq 0 ]] || { echo "  $miss NEEDED libraries not found"; rc=1; }
    # 2) no unresolved symbols against the loaded set
    und=$(ldd -r "$so" 2>&1 | grep -c 'undefined symbol')
    [[ $und -eq 0 ]] || { ldd -r "$so" 2>&1 | grep 'undefined symbol' | head -5; echo "  $und undefined symbols"; rc=1; }
    # 3) actually dlopen it with RTLD_NOW (this is what the player does, eagerly)
    if command -v python3 >/dev/null; then
        python3 - "$so" <<'PY' || rc=1
import ctypes, sys
try:
    ctypes.CDLL(sys.argv[1], mode=ctypes.RTLD_GLOBAL)
except OSError as exc:
    print('  dlopen failed:', exc); sys.exit(1)
print('  dlopen ok, all symbols bound')
PY
    fi
    # 4) record what it drags in, so a missing ttf/sdl shows in the log
    readelf -d "$so" 2>/dev/null | awk '/NEEDED/{gsub(/[\[\]]/,"",$NF); printf "  needs %s\n", $NF}'
done
[[ $rc -eq 0 ]] && echo "GATE PASS" || echo "GATE FAIL"
exit $rc
