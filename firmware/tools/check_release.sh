#!/usr/bin/env bash
# Release guard: run after `idf.py build`. Fails if the production config enables the
# simulation option or if any simulator/fake symbol made it into the firmware ELF.
set -euo pipefail
cd "$(dirname "$0")/.."
fail=0
for f in sdkconfig.defaults build/config/sdkconfig.h; do
    [ -f "$f" ] || { echo "missing $f (run idf.py build first)"; exit 2; }
    if grep -Eq '^(CONFIG_THERMOSTAT_SIM=y|#define CONFIG_THERMOSTAT_SIM 1)' "$f"; then
        echo "FAIL: CONFIG_THERMOSTAT_SIM is enabled in $f"; fail=1
    fi
done
elf=$(ls build/*.elf | head -1)
NM=${NM:-riscv32-esp-elf-nm}
syms=$("$NM" "$elf" | grep -Ei ' (sim_[a-z_]+|fake_[a-z_]+|hootl[a-z_]*|stwd_poll|kernel_[a-z_]+)$' || true)
if [ -n "$syms" ]; then echo "FAIL: simulator symbols in $elf:"; echo "$syms"; fail=1; fi
if strings "$elf" | grep -Eq 'HOOTL|\(injected\)|sim_kv'; then echo "FAIL: simulator strings in $elf"; fail=1; fi
[ $fail = 0 ] && echo "release check OK: $elf has no simulator code, CONFIG_THERMOSTAT_SIM off"
exit $fail
