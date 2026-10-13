# Rev 9 - supply margin fixes + final independent review (2026-10-13)

## Changes
| Ref | Part | LCSC | Stock (JLC, 13 Oct) | Notes |
|---|---|---|---|---|
| R51 (new) | Vishay CRCW12063R30FKEAHP, 3.3 Ω 1 % 1206, 0.75 W, pulse-proof | C844038 (extended) | 27,862 | D1 + (new net VIN_RECT) -> VIN_HV. No basic 1206 2.2-4.7 Ω exists at JLC. |
| L1 | Sunlord SWPA6045S220MT, 22 µH, Isat 2.2 A / Irms 1.8 A, 89 mΩ, 6×6×4.5 | C83454 (extended) | 40,752 | was SWPA4030S220MT (Isat 1.0 A, C83472). FP Inductor_SMD:L_Sunlord_SWPA6045S |

Layout: R51 at (20.0,48.6) rot 90 between F1/D1 and the bulk ceramics; VIN_RECT is a 0.5 mm hand-routed trace around D1's GND pad (that pad is fed by the GND pour). L1 at (45,57.8). C4/C5 moved to x=50.3. R50 rotated 180° so EN routes cleanly. No change to J1 (back, 70,58), antenna keepout, island, HV pour clearance (1 mm), MOC3063 + DNP snubbers, no serial header, cat logo.

## Calculations
Time-step simulation: transformer source 2 Ω, PTC 0.5 Ω, bridge 2×0.9 V, 59 µF effective bulk (4×4.7 µF X7R derated + 47 µF), 0.88 W constant-power load (worst-case ESP TX + optos + EPD).

| Line | R51 | Vmin | Vmax | I rms | I pk | P(R51) |
|---|---|---|---|---|---|---|
| 18 VAC | 0 | 19.0 V | 23.5 V | 96 mA | 300 mA | - |
| 18 VAC | 3.3 Ω | **18.8 V** | 23.2 V | 91 mA | 255 mA | 27 mW |
| 24 VAC | 3.3 Ω | 28.4 V | 31.7 V | 72 mA | 227 mA | 17 mW |
| 30 VAC, 0.88 W | 3.3 Ω | 37.5 V | 40.2 V | 61 mA | 210 mA | 12 mW |

- Low-line cost of R51 is only about 0.2-0.3 V. 18.8 V minimum is far above the buck's ~4 V UVLO. Even at the 0.15 A rms assumed in the brief, P = 0.15² × 3.3 = 74 mW (10 % of 0.75 W).
- **TVS clamp at U1 VIN.** SMBJ40A: V_BR max 49.1 V, Vc 64.5 V at 9.3 A, so R_dyn ≈ 1.66 Ω. Take the surge that drove 9.3 A before (Voc ≈ 88 V behind 2 Ω + PTC). With R51 it now drives (88 − 49.1)/(2 + 0.5 + 3.3 + 1.66) ≈ **5.2 A**, so Vc ≈ **57.7 V**. Over source impedances of 1.5-3 Ω the range is 57.2-58.5 V, under LMR16006's 60 V recommended max and 65 V absolute max. This ignores the 59 µF bulk, which absorbs short spikes, so it is conservative.
- R51 surge pulse: about 80-110 W for the length of the surge. Cold-start inrush: 59 µF to 41 V through about 5.8 Ω total gives about 7 A peak, τ ≈ 0.34 ms, with about 28 mJ in R51. That is within the CRCW-HP 1206 single-pulse rating. **Bench check suggested**: power cycle 20× at 30 VAC.
- Fault case: on a shorted bulk cap, R51 sees several watts until the PTC trips, and may fuse open first. It fails open, which is safe.
- **L1.** At 40 V in, ΔI_L = 36.7 × 0.0825/(22 µH × 0.7 MHz) ≈ 0.20 A p-p. Peak is 0.40 A at 0.3 A load. LMR16006X peak current limit is 1.2 A typ (datasheet: 22 µH, ≥1.6 A rating recommended), against Isat 2.2 A, so no saturation on a short. The DCR change from 0.17 Ω to 0.089 Ω reduces loss.
- Buck loop after the change: U1 SW to L1 is 7.0 mm, L1 to C4/C5 is 3.2/3.6 mm, D4 K to SW is 5.8 mm, C2 to VIN is 2.7 mm. This is unchanged except L1 is now bigger.

## Checks
- ERC 0. DRC (`--schematic-parity --severity-all`): **0 errors, 0 unconnected, 0 parity**. The only items are 17 `lib_footprint_mismatch` warnings, the same benign custom-lib warnings as rev 8. Silkscreen overlap and edge warnings: 0.
- Gerbers, drill, BOM (37 lines), CPL (83 placements) regenerated. The aux/drill origin is the board bottom-left. R51 CPL is (20.0, 15.4) Top 90; L1 is (45.0, 6.2) Top 0.
- C43 (THT, back, hand-solder) is not in the JLC BOM/CPL, by design. Order it separately: JBLH2470M063D090RLM, C19270693.

## Final independent review (power emphasis)
**Critical: none found.**

**Should-fix: none blocking.** Recommended bench steps at bring-up:
1. Bring up in stages: USB only, then bench 24 VAC (18/24/30 VAC), then the wall. Measure TP1 VIN_HV ripple at 18 VAC under radio TX. Expect at least 18 V.
2. Power-cycle at 30 VAC 20×, then check R51 is still 3.3 Ω (inrush pulse).

**Notes / accepted:**
- N1. R50 100k (0402, 50 V working) sits on VIN_HV. That is about 41 V steady and about 58 V for microseconds during a clamped surge. It is acceptable for a high-value resistor at µA current. An 0603 (75 V) would be cleaner, but it needs a reroute, so it was not changed.
- N2. On USB alone, VIN is about 4.55 V, so 3.3 V may sag during Wi-Fi/Zigbee TX bursts. It is fine for flashing, which is the only USB use.
- N3. The buck hot loop is about 7 mm through solid GND (the same as rev 8). This is acceptable at 0.7 MHz and < 0.5 A.
- N4. D2 TVS is about 12 mm of 0.5 mm trace from R51/C1, and about 23 mm from U1. C2 (4.7 µF) sits right at U1, so this is acceptable.
- N5. The removed autorouter GND stub at D1-: D1's GND pad and D2 GND are now pour-connected only (full connection, both layers). DRC confirms continuity.
- N6. The CPL footprint for L1 is the KiCad Sunlord footprint. Check its rotation in the JLC placement preview (it is symmetric, so 0/180 are equivalent).
