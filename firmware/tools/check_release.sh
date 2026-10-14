#!/usr/bin/env bash
# Release guard: run after `idf.py build`. Fails if any simulator/fake symbol or string
# made it into the firmware ELF.
set -euo pipefail
cd "$(dirname "$0")/.."
fail=0
elf=$(ls build/*.elf 2>/dev/null | head -1)
[ -n "$elf" ] || { echo "no build/*.elf (run idf.py build first)"; exit 2; }
NM=${NM:-riscv32-esp-elf-nm}
syms=$("$NM" "$elf" | grep -Ei ' (sim_[a-z_]+|fake_[a-z_]+|hootl[a-z_]*|stwd_poll|kernel_[a-z_]+)$' || true)
if [ -n "$syms" ]; then echo "FAIL: simulator symbols in $elf:"; echo "$syms"; fail=1; fi
if strings "$elf" | grep -Eq 'HOOTL|\(injected\)|sim_kv'; then echo "FAIL: simulator strings in $elf"; fail=1; fi
[ $fail = 0 ] && echo "release check OK: $elf has no simulator code"
exit $fail
