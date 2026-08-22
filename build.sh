#!/usr/bin/env bash
# CH375USB 0.5.1 Linux build
# Builds the unified DOS/Windows-aware SYS and the Win16 CH375MOU.DRV bridge.
# Author: Davide "gat" - https://github.com/davidegat
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail
cd "$(dirname "$0")"

ROOT="$PWD"
TOOLROOT="$ROOT/.toolchains"
OWROOT="$TOOLROOT/openwatcom"
BUILD="$ROOT/.build"
OW_URL="https://openwatcom.org/ftp/source/ow_portable_v2_stable.zip"

mkdir -p "$TOOLROOT" "$BUILD"

have_downloader() {
    command -v curl >/dev/null 2>&1 || command -v wget >/dev/null 2>&1
}

download() {
    local url="$1" out="$2"
    if command -v curl >/dev/null 2>&1; then
        curl -fL --retry 3 --connect-timeout 20 "$url" -o "$out"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$out" "$url"
    else
        echo "ERROR: need curl or wget to download build tools." >&2
        exit 1
    fi
}

# NASM is intentionally accepted from PATH. It is small and commonly packaged,
# while Open Watcom is bootstrapped locally below exactly as in the earlier
# CH375WIN build flow.
if ! command -v nasm >/dev/null 2>&1; then
    echo "ERROR: nasm not found in PATH." >&2
    echo "Install NASM, then rerun ./build.sh." >&2
    exit 1
fi

if ! command -v unzip >/dev/null 2>&1; then
    echo "ERROR: unzip not found in PATH." >&2
    exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "ERROR: python3 not found in PATH (needed to validate the final NE driver image)." >&2
    exit 1
fi

find_ow_tool() {
    local name="$1"
    local p
    for p in \
        "$OWROOT/binl/$name" \
        "$OWROOT/binl64/$name" \
        "$OWROOT/binw/$name"; do
        if [ -x "$p" ]; then
            printf '%s\n' "$p"
            return 0
        fi
    done
    return 1
}

bootstrap_openwatcom() {
    if find_ow_tool wasm >/dev/null 2>&1 && find_ow_tool wlink >/dev/null 2>&1; then
        return 0
    fi

    have_downloader || {
        echo "ERROR: Open Watcom is missing and neither curl nor wget is available." >&2
        exit 1
    }

    echo "Open Watcom V2 not found locally; downloading portable stable toolchain..."
    local zip="$TOOLROOT/ow_portable_v2_stable.zip"
    rm -f "$zip"
    download "$OW_URL" "$zip"

    rm -rf "$OWROOT" "$TOOLROOT/.ow-unpack"
    mkdir -p "$TOOLROOT/.ow-unpack"
    unzip -q "$zip" -d "$TOOLROOT/.ow-unpack"

    # The portable archive has changed top-level directory names over time.
    # Locate the directory that actually contains binl/wasm and retain it under
    # the stable project-local path .toolchains/openwatcom/.
    local wasm_path ow_candidate
    wasm_path="$(find "$TOOLROOT/.ow-unpack" -type f -path '*/binl/wasm' -print -quit)"
    if [ -z "$wasm_path" ]; then
        wasm_path="$(find "$TOOLROOT/.ow-unpack" -type f -path '*/binl64/wasm' -print -quit)"
    fi
    if [ -z "$wasm_path" ]; then
        echo "ERROR: downloaded Open Watcom archive does not contain wasm." >&2
        exit 1
    fi
    ow_candidate="$(dirname "$(dirname "$wasm_path")")"
    mv "$ow_candidate" "$OWROOT"
    rm -rf "$TOOLROOT/.ow-unpack"
}

bootstrap_openwatcom

WASM="$(find_ow_tool wasm)"
WLINK="$(find_ow_tool wlink)"

# Open Watcom tools use WATCOM/EDPATH/INCLUDE for some defaults. Keep everything
# project-local: no system-wide installation and no DDK dependency.
export WATCOM="$OWROOT"
export EDPATH="$OWROOT/eddat"
if [ -d "$OWROOT/h" ]; then
    export INCLUDE="$OWROOT/h${INCLUDE:+:$INCLUDE}"
fi

# WLINK's named SYSTEM definitions (including windows_dll) live in
# wlink.lnk/wlsystem.lnk beside the linker.  Calling wlink by absolute path is
# NOT enough: Open Watcom searches PATH for these files.  An earlier build configuration
# omitted this and silently linked CH375MOU as a normal Windows executable.
OW_BINDIR="$(dirname "$WLINK")"
export PATH="$OW_BINDIR:$PATH"
if [ -f "$OW_BINDIR/wlink.lnk" ]; then
    export WLINK_LNK="$OW_BINDIR/wlink.lnk"
fi

printf '\n============================================================\n'
printf ' CH375USB / CH375MOU BUILD\n'
printf '============================================================\n\n'
printf 'Project directory : %s\n' "$ROOT"
printf 'NASM              : %s\n' "$(command -v nasm)"
printf 'WASM              : %s\n' "$WASM"
printf 'WLINK             : %s\n\n' "$WLINK"

rm -f CH375USB.SYS CH375W.SYS CH375MOU.DRV CH375MOU.MAP
rm -f CH375MOU.OBJ

# Unified release SYS.  WIN_MOUSE_INT33_BRIDGE defaults to 1 in the source:
# normal DOS behavior is preserved, and the Windows-specific path wakes only
# after the enhanced-mode notification.  There is no separate CH375W.SYS.
nasm -f bin CH375USB.ASM -o CH375USB.SYS
nasm -f bin MTEST.ASM -o MTEST.COM

# Our independently-written Win16 mouse.drv-compatible bridge.
# WASM emits OMF; WLINK emits a 16-bit Windows NE driver/DLL image.
# The ASM deliberately has a plain END (no MODEND start address).  The linker
# file uses FORMAT WINDOWS DLL INITGLOBAL plus OPTION START=Initialize, so there
# is exactly one initializer and no Open Watcom C DLL startup is involved.
"$WASM" -q -2 -fo=CH375MOU.OBJ CH375MOU.ASM

LINK_LOG="$BUILD/CH375MOU.wlink.log"
set +e
"$WLINK" @CH375MOU.LNK 2>&1 | tee "$LINK_LOG"
link_rc=${PIPESTATUS[0]}
set -e
if [ "$link_rc" -ne 0 ]; then
    echo "ERROR: WLINK failed with exit code $link_rc." >&2
    exit "$link_rc"
fi
if grep -q 'Warning!' "$LINK_LOG"; then
    echo "ERROR: WLINK emitted warnings; refusing to accept CH375MOU.DRV." >&2
    echo "See $LINK_LOG" >&2
    exit 1
fi
rm -f CH375MOU.OBJ

# Do not trust a zero WLINK exit status alone.  WLINK can
# warn about an unknown SYSTEM and still emit a superficially valid NE file.
# Verify the final image is really a Win16 DLL/driver before reporting success.
python3 - CH375MOU.DRV <<'PYVERIFY'
import struct, sys
from pathlib import Path
p = Path(sys.argv[1])
b = p.read_bytes()
if len(b) < 0x40 or b[:2] != b'MZ':
    raise SystemExit('ERROR: CH375MOU.DRV has no MZ header')
ne = struct.unpack_from('<I', b, 0x3c)[0]
if ne + 0x40 > len(b) or b[ne:ne+2] != b'NE':
    raise SystemExit('ERROR: CH375MOU.DRV is not a Win16 NE image')
app_flags = b[ne + 0x0d]
autodata = struct.unpack_from('<H', b, ne + 0x0e)[0]
seg_count = struct.unpack_from('<H', b, ne + 0x1c)[0]
target_os = b[ne + 0x36]
ip = struct.unpack_from('<H', b, ne + 0x14)[0]
cs = struct.unpack_from('<H', b, ne + 0x16)[0]
if target_os != 2:
    raise SystemExit(f'ERROR: NE target OS is {target_os}, expected Windows (2)')
if not (app_flags & 0x80):
    raise SystemExit(f'ERROR: NE application flags 0x{app_flags:02X}: DLL/driver bit 0x80 is NOT set')
if seg_count < 2:
    raise SystemExit(f'ERROR: NE image has only {seg_count} segment(s); expected code + writable data')
if autodata == 0:
    raise SystemExit('ERROR: NE automatic data segment is zero')
if cs == 0:
    raise SystemExit('ERROR: NE initializer CS is zero')
print(f'NE validation      : Windows DLL/driver, flags=0x{app_flags:02X}, segments={seg_count}, autodata={autodata}, entry={cs:04X}:{ip:04X}')
PYVERIFY

printf '\nBuilt outputs:\n'
printf '  CH375USB.SYS  %8s bytes\n' "$(stat -c%s CH375USB.SYS)"
printf '  CH375MOU.DRV  %8s bytes\n' "$(stat -c%s CH375MOU.DRV)"
printf '  CH375MOU.MAP  %8s bytes\n' "$(stat -c%s CH375MOU.MAP)"
printf '  MTEST.COM      %8s bytes\n' "$(stat -c%s MTEST.COM)"
printf '\nSHA-256:\n'
sha256sum CH375USB.SYS CH375MOU.DRV

printf '\nWindows 95 mouse setup:\n'
printf '  1. Load CH375USB.SYS before WIN. Do NOT load CuteMouse/CTMOUSE.\n'
printf '  2. Copy CH375MOU.DRV to C:\\WINDOWS\\SYSTEM\\CH375MOU.DRV\n'
printf '  3. In C:\\WINDOWS\\SYSTEM.INI, [boot]: mouse.drv=CH375MOU.DRV\n'
printf '  4. Restart Windows.\n'
