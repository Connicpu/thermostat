# Rev 8 response to the pre-order review (2026-10-13 PT)

Every Critical and Should-fix item is resolved below, except S4, which is resolved by field evidence.
- ERC: 0 errors.
- DRC with schematic parity: 0 errors and 0 unconnected. The only warnings are 18 `lib_footprint_mismatch`, all from local silk trims.

## Critical
**C1 J1 position: FIXED on the real board.**
- J1 (15EDGRC right-angle, B.Cu) now sits at board (70,58), pins along x, with the mating face toward −y (courtyard y 50.0–59.2).
- The fab files prove it:
  - `PTH.drl` has 1.4 mm holes at X63.0/66.5/70.0/73.5/77.0, Y6.0 (bottom-left origin).
  - `B_Cu.gbl` and `F_Cu.gtl` flash pads at the same coordinates (`X63000000Y6000000D03` …).
  - No pads remain at X9.
- Root cause: the 09:36 board came from an older pass (`pass2b.sh`) that never re-ran `gen_pcb2.py`.
  - The full pipeline (`route2.sh`) now regenerates from `gen_pcb2.py`.
  - `gen_pcb2.py` asserts J1 is at (70,58) on B.Cu.
  - `gen_pcb2.py` now also sets the aux/grid origin (100,164), because it had been lost on regeneration.
- The case `term=[70,58]` matches. `case-section-j1.png` shows the header/plug and C43 clearing the backplate by about 2.5 mm.

**C2 D3 backfeed diode: FIXED.**
- B5819W (40 V) → SS210 (C14996): 100 V, 2 A, SMA. It is a basic part (no extended fee) and is the same part as D4.
- Topology unchanged: VBUS → D3 → VIN_HV (buck input).

## Should-fix
- **S1 bulk: FIXED.** Added C43, 47 µF 63 V radial 8×9 mm THT (JBLH2470M063D090RLM, C19270693, $0.25), on the BACK at (55,58), hand-soldered like J1.
  - It is the cheapest sensible option: a bottom-side SMD part would trigger JLC double-sided assembly fees.
  - It is excluded from the JLC BOM/CPL.
  - Height is 9.5 mm against the 12 mm standoff gap.
- **S2 SHDN: FIXED.** R50, 100 k 0402 (C25741, basic), from VIN_HV to U1 pin 4 (new net BUCK_EN).
- **S3 antenna: FIXED.** Added board rule area `ANT_KEEPOUT` at x 61–89, y 0–6.4 on all copper layers (no tracks, vias or pour).
  - SW1/SW2/R6 moved 2 mm right to make room; RESET is now at (93,5), and the case pinhole moved with it.
  - Zone hit-test: no filled copper in that area on F.Cu or B.Cu.
- **S4 opto: NOT CHANGED (field evidence).** Kept MOC3063S zero-cross with snubbers DNP.
  - The user's V1 thermostat has run the same zero-cross opto family (MOC3033) with no snubbers on this exact HVAC system.
  - **Triac changed instead:** BT136S-600D (sensitive gate, 5 mA, more prone to dV/dt false triggering on contactor coils) → **ST T435-600B** (C145128). It is standard-gate and Snubberless: 4 A, 600 V, IGT 35 mA, high static/commutating dV/dt. V1 used a standard-gate BT136-600.
    - Same DPAK pinout (1 = A1/MT1, tab = A2/MT2, 3 = G).
    - In-stock standard-gate alternatives were weak: BT136S-600 has 0 stock; the "BT136S" C23427 is actually IGT 10 mA; BT137S-600,118 has only 66 in stock at $0.98.
  - **Gate drive check:** R22/R32/R42 = 100 Ω, R23/33/43 = 1 k gate–MT1, opto on-drop ≈ 3 V, VGT ≈ 1.3 V.
    - Ig = (V − 3 − 1.3)/100 − 1.3/1k ≥ 35 mA needs V ≥ about 8 V. That is reached 0.6 ms after the zero cross at 24 VAC, so the triac latches early in each half cycle.
    - Opto peak current is ≤ 0.34 A, under the 1 A ITSM.
    - 100 Ω is kept rather than V1's 180 Ω. 180 Ω would need about 11.8 V, which is still fine, but 100 Ω fires earlier.
- **S5 PTC: FIXED.** F1 → mSMD050-60V (C70113), 1812, 0.5 A hold, 60 V.
- **S6 USB-C: FIXED.** J2 moved to y = 60.35, so the shell front is flush with the board edge.
  - CPL offset re-verified independently: EasyEDA C165948 pads sit at y = −2.47 versus KiCad −4.045. The EasyEDA origin is therefore KiCad local y −1.575, the same as `export_jlc.py`.
  - CPL J2 = (121.000, 5.225).
- **S7 HV clearance: FIXED.** Custom rule `hv_pour_clearance`: a zone keeps ≥ 1.0 mm from HVAC_*, R_FUSED, OPTO_MT2_*, GATE_* and SNUB_*.
  - Verified by collision test: no HV pad or track lies within 0.99 mm of the GND fill on either layer.
  - I did not add a pour-free region over the opto output/triac area. The 1 mm clearance is the protection.

## 24 VAC supply: extra scrutiny (calculations)
**Input envelope.**
- Low line 18 VAC: Vpk = 25.5 V. After the bridge (2 × 0.9 V at the charging peak) and PTC (~0.3 V), about 23.4 V.
- Nominal 24 VAC: about 32 V.
- High line 30 VAC, light load: 42.4 − 1.6 ≈ 40.8 V.

**Ratings (all ≥ 60 V).**

| Part | Rating | Stress | Margin |
|---|---|---|---|
| D1 MB10S bridge | 1000 V / 1 A, IFSM 35 A | ~41 V | large |
| D3 SS210 | 100 V | 40.8 V reverse | 41 % used |
| D4 SS210 | 100 V | — | — |
| F1 mSMD050-60V | 60 V | 42.4 V pk while tripped | 1.4× |
| C1/C41/C42/C2 X7R | 100 V | — | — |
| C43 electrolytic | 63 V | 40.8 V | 65 % |
| U1 LMR16006 | 60 V recommended, 65 V abs | see TVS below | — |
| Q2–Q4 T435 | 600 V | — | — |
| U6–U8 MOC3063 | 600 V | — | — |

**TVS (SMBJ40A, D2, on VIN_HV next to the bulk caps, upstream of U1).**
- V_BR min 44.4 V, above the 40.8 V high-line peak, so it never conducts in normal operation.
- Clamp: about 47–50 V at ≤ 1 A, 64.5 V only at 9.3 A.
- A Class-2 transformer (about 1.5–3 Ω source impedance) plus the PTC limits realistic surge current to a few amps. The clamp therefore stays near 50–55 V, under the 60 V recommended max.
- Only a full 9.3 A pulse would reach 64.5 V, still under the 65 V absolute max. This was accepted, not changed (review N1).
- Option if wanted: 2.2–4.7 Ω (1206) in series between the bridge and the bulk caps, at the cost of ≈ 0.1 V drop.

**Bulk ripple.**
- C_eff ≈ 4 × 4.7 µF X7R at 25–40 V bias (≈ 3 µF each → 12 µF) + 47 µF electrolytic ≈ **59 µF**.
- Worst-case load: ESP TX burst 200 mA + 3 opto LEDs 27 mA + EPD at 3.3 V ≈ 0.75 W out; at η ≈ 85 % that is **0.88 W in**. Hold-up per half cycle is about 7 ms (8.33 ms minus conduction).
- **18 VAC:** E = 0.88 × 0.007 = 6.2 mJ. Vmin = √(23.4² − 2·6.2m/59µ) = √(548 − 210) = **18.4 V**, about 5 V ripple. That is far above the buck's needs (3.3 V + dropout, UVLO ~4 V).
  - Without C43 (12 µF) the rail would collapse toward UVLO, confirming S1.
- **30 VAC:** I ≈ 0.88/39 = 23 mA, ΔV = I·t/C ≈ 2.7 V. The peak of 40.8 V stays below TVS V_BR.
- C43 ripple current is about 3 × I_avg ≈ 0.1–0.15 A rms at 120 Hz. **Confirm against the JBLH datasheet ripple rating** (typically ~100–150 mA for 8×9 63 V). The ceramics share the high-frequency part.

**Fuse.**
- Average input is 25–45 mA, and the rms charging current is ≈ 3× that, about 0.15 A. That is well under the 0.5 A hold, and the 1.0 A trip covers a shorted bulk cap or buck.
- The fuse protects the PSU only. Triac load current returns through the equipment and is not fused (by design, same as V1).

**Buck (LMR16006X, 0.7 MHz).**
- V_OUT = 0.765 × (1 + 33k/10k) = 3.29 V.
- Ripple: ΔI_L at 40 V = (40 − 3.3) × D/(L·f) = 36.7 × 0.092/(22 µH × 0.7 MHz) = **0.22 A pk-pk**. Peak I_L at 0.3 A load ≈ 0.41 A, against L1 SWPA4030S220MT Isat ≈ 1.0 A, which is fine in normal operation.
- On a hard output short the LMR's 1.2–1.7 A limit can saturate L1 (review N2). This was not changed. A 5040-size 22 µH (Isat ≥ 1.6 A) is the drop-in if wanted.
- Layout as built:
  - C2 (input) to U1 VIN: 2.7 mm. C2 GND to U1 GND: 5.4 mm.
  - D4 cathode to SW: 5.8 mm. D4 anode to U1 GND: 8.6 mm.
  - L1 to SW: 7.2 mm. R1/R2 to FB: about 5 mm.
  - The hot loop (C2 → U1 → D4 → GND) is about 7 mm across, closed through solid GND pour on both layers. That is acceptable at 0.7 MHz and < 0.5 A, but is **not** the datasheet ideal (review N3, not re-laid out this rev).

**USB / 24 V diode-OR.**
- With 24 VAC present, D3 (SS210) holds off 32–41 V with 100 V rating. Its leakage (tens of µA at 25 °C, up to ~0.5 mA hot) only lifts VBUS a few volts into C6 and USBLC6. No current flows back into a USB host, because D3 is reverse-biased.
- On USB alone, VIN ≈ 5 − 0.45 = 4.55 V into the buck (UVLO 4 V), so the 3.3 V rail holds at light load. That is enough for flashing.
- The silk warning "NO USB WHILE 24VAC IS WIRED" stays, because of the GND/HVAC-C reference (unchanged).

**Thermal.** U1/L1 are at (39–45, 50–58), **89 mm** from the SHT40. The triacs are 110 mm away, and the island slots, DRC net rule and pour cutout are unchanged.

**Test points (new, bare back-side pads, no BOM cost).**
- TP1 VIN_HV (30,61.5)
- TP2 GND (33.5,61.5)
- TP3 +3V3 (47,62.2)
- TP4 VBUS/5V (114.5,46.5)
