#!/bin/sh
# Builds and runs mixer-probe against the AudioMixer.c of a given tree.
#
#   sh build-probe.sh <blueMSX/Src of the tree under test> [output directory]
#
# The output directory defaults to a fresh temporary one, so nothing is left in
# the working tree. Exit status is the probe's: non-zero if any check fails.
#
# MIXER_C, if set, names an AudioMixer.c to build instead of the tree's own,
# for running the probe on a deliberately broken copy kept outside the tree.
set -e

if [ -z "$1" ]; then
    echo "usage: $0 <blueMSX/Src> [outdir]" >&2
    exit 2
fi

here=$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)
src=$(cd "$1" && pwd -W 2>/dev/null || pwd)
mixer=${MIXER_C:-$1/SoundChips/AudioMixer.c}
mixer="$(cd "$(dirname "$mixer")" && (pwd -W 2>/dev/null || pwd))/$(basename "$mixer")"
out=${2:-$(mktemp -d)}
mkdir -p "$out"
out=$(cd "$out" && pwd -W 2>/dev/null || pwd)

vswhere="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
vsdir=$("$vswhere" -products '*' -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | head -1 | tr -d '\r')
if [ -z "$vsdir" ]; then
    echo "no Visual Studio with the C++ tools found" >&2
    exit 2
fi

bat="$out/build.bat"
{
    printf '@echo off\r\n'
    printf 'call "%s/VC/Auxiliary/Build/vcvars64.bat" >nul || exit /b 1\r\n' "$vsdir"
    printf 'cd /d "%s" || exit /b 1\r\n' "$out"
    printf 'cl /nologo /W3 /O2 /D_CRT_SECURE_NO_WARNINGS'
    for d in SoundChips Common Board Arch Utils Memory Media VideoChips Emulator Debugger Language; do
        printf ' /I"%s/%s"' "$src" "$d"
    done
    printf ' "%s/mixer-probe.c" "%s/mixer-host-stub.c" "%s" /Fe:mixer-probe.exe || exit /b 1\r\n' "$here" "$here" "$mixer"
} > "$bat"

cmd //c "$(cygpath -w "$bat")" > "$out/build.log" 2>&1 || { cat "$out/build.log"; exit 2; }
"$out/mixer-probe.exe"
