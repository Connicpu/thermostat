# Thermostat V3 PCB: independent pre-order review
Reviewed 2026-10-13 PT, against `kicad/thermostat-v3.kicad_pcb` / `.kicad_sch` (git `8711fb3`). Fab outputs were re-checked against the board.
I did not modify the design. All checks ran on copies in `/workspace/rv/`.
Image: `review/annot-top.png` (top copper and silk, with case features and findings overlaid). Reports: `review/erc.rpt`, `review/drc.rpt`.

**Verdict: do not order yet.** There is one mechanical blocker (C1) and one component-rating problem (C2). Both are cheap to fix.

---
## CRITICAL (fix before ordering)

### C1. J1 is not where the case (and part-plan rev 7) expects it
- **Evidence.** In the PCB, J1 (`HVAC_TERM_15EDGRC`, B.Cu) sits at abs (109,131), rot −90. That is board (9, 24–38) with the pins running along **y** at the **left edge**.
  - The fab PTH drill file confirms it: 1.4 mm holes at X9.0, Y26.0…40.0 (bottom-left origin).
  - The copper in `fab/gerbers` is byte-identical to a fresh export from the board, so the fab files carry this position.
  - `case/thermostat_case.scad` has `term = [70,58]` ("pins along x, mating face toward -y") and a centred 25×20 mm wire hole at board (57.5–82.5, 22–42).
  - `part-plan.md` Rev 7 says J1 sits at board (70,58).
  - The rev7 commit (`c4b7ae0`) changed J1's silk and gerbers but never moved the footprint to (70,58).
- **Impact.** The wires come through the centre hole but the terminal is about 60 mm away at the left edge, inside a 12 mm gap. The plug body is about 11.5 mm deep. Depending on its mating direction it can hit the backplate lip/wall or the H1/H3 standoffs. None of the case's rear clearances were designed around this spot.
- **Fix.** Pick one source of truth:
  - **Recommended:** move J1 on the PCB to (70,58), pins along x, mating face toward −y, then re-route R/C/W/Y/G (1.0 mm). Nothing else sits at that spot on the bottom side, and the top side there is only the bottom-edge area near L1, D7 and D8, so check pin-to-part clearance on the top too.
  - Or: update the case (`term`, wire hole, rear clearances) for J1 at the left edge, and add the right wire path.
  - Either way, add a check that pulls the J1 position from the board into the case file.

### C2. D3 (USB diode-OR) is a B5819W rated 40 V. It is reverse-biased by the full rectified 24VAC.
- **Evidence.** Netlist: D3 A = VBUS, K = VIN_HV. With USB unplugged, D3's reverse voltage ≈ VIN_HV.
  - The LCSC listing for C8598 shows "1A 40V".
  - VIN_HV ≈ √2·V_AC − 2·V_F(bridge). That is ≈32 V at 24 VAC, ≈38 V at a 28 VAC high line, and ≈41 V at 30 VAC with a lightly loaded transformer.
  - In a surge the SMBJ40A clamps at 44.4–49.1 V (V_BR), and up to 64.5 V at 9.3 A (C114003 listing).
  - TI SNVSA24 §9.2.2.4 recommends a diode rating 25 % above V_IN max.
- **Impact.** In normal high-line operation the diode runs at 95–100 % of its rating, and surges go past it. A Schottky that fails short dumps 30–60 V onto VBUS. That kills USBLC6 (5.25 V max), C6 (25 V) and any attached USB host.
- **Fix (pick one).**
  - **MBR2H100SFT3G** (C96515): 100 V, 2 A, same SOD-123 footprint, 34k in stock, extended.
  - **SS210** (C14996): already on the BOM as a basic part, but it is SMA and needs a footprint change.
  - The EPD pump diodes D6/D7/D8 (B5819W) are fine at 40 V.

---
## SHOULD-FIX (strongly recommended for this spin)

### S1. Bulk hold-up on VIN_HV is thin
- **Evidence.** The only bulk capacitance is C1, C41 and C42 plus C2: 4 × 4.7 µF 100 V X7R 1210 (FS32X475K101). With DC bias that is ≈13 µF effective (designer's own estimate).
  - The rev-6 ripple calculation assumed a 15 mA input. The ESP32-C6 module lists 382 mA peak (LCSC C5366877), and an 802.15.4 TX/RX burst plus 3 opto LEDs (≈30 mA) plus the EPD boost is realistically 150–250 mA at 3.3 V. That is ≈0.6–1 W in.
  - Energy needed per 8.3 ms half-cycle at 0.7 W ≈ 5.8 mJ.
  - Energy available from 13 µF falling to the 4 V UVLO (TI SNVSA24 §7.5): ≈4.7 mJ at 20.4 VAC low line, ≈6.7 mJ at 24 VAC.
- **Impact.** At low line, or with a sagging transformer while the compressor contactor pulls in, long RF activity or boot calibration can brown the rail out and cause resets. (Each reset also drops the outputs, then the reset logic re-arms.)
- **Fix.** There is **12 mm of free height on the back side** (the standoff), so a hand-soldered THT or bottom-side 47–100 µF/63 V electrolytic on VIN_HV costs no front height. Otherwise add a ≤5.5 mm-tall SMD 33–47 µF/63 V part outside the panel footprint. At minimum, bench-test at 20 VAC with a continuous Zigbee TX.

### S2. LMR16006 SHDN is tied straight to VIN
- **Evidence.** The netlist has U1.4 (~SHDN) on VIN_HV with no resistor.
  - TI SNVSA24 §8.3.4: *"If the shutdown function is not to be used the SHDN pin may be tied to VIN via 100 kΩ resistor. The maximum voltage to the SHDN pin should not exceed 60 V."*
  - The TVS clamp can reach 64.5 V. Absolute max is 65 V, recommended max is 60 V.
- **Fix.** Put 100 kΩ in series (0402 basic, C25741), or leave SHDN floating (it has an internal pull-up). VIN itself also sits only 0.5 V under absolute max at full TVS clamp current; see N1.

### S3. Antenna keepout is the bare minimum and the GND pour runs right up to it
- **Evidence.** U3's footprint rule area is 166–184 × 100–106 mm, only the module width. Filled GND on **both** F.Cu and B.Cu hits at x = 184 and directly beside the antenna (zone hit-test).
  - Espressif's ESP32-C6-WROOM-1 placement guidance prefers the antenna overhanging the edge. Failing that, keep a copper-free area that extends past the antenna on both sides.
- **Fix.** Widen the keepout by at least 5 mm on each side (x 161–189, y 100–106, both layers). SW1 at x = 191 stays clear. The pour also has to be refilled after the change.

### S4. MOC3063 zero-cross opto driving inductive 24VAC loads
- **Evidence.** The MOC306x only triggers when the voltage across MT1–MT2 is inside its inhibit window (≈20 V max per the datasheet).
  - With a contactor or relay coil (PF ≈0.3–0.5), current lags voltage by 60–70°.
  - When the triac drops out at current zero, the line is already at ≈29–32 V (34 V peak). The opto won't re-fire until the next voltage zero.
  - The result can be a reduced conduction angle and coil buzz or dropout. This is a known weakness of zero-cross drivers on inductive loads, and snubbers C20–22/R24–44 are DNP.
- **Fix.** Bench-test one channel with a real 24VAC contactor before deploying. Alternatively use a random-phase driver: MOC3023/MOC3021 (same SMD-6 pinout, IFT 5–15 mA). With MOC3023 at IFT ≤ 5 mA, the 220 R/≈9 mA drive already works.

### S5. PTC F1 is rated 30 V, and 28–30 VAC lines exist
- **Evidence.** The C12559 listing for SMD1812P050TF/30 says "30V". A tripped PTC has to hold off the full secondary.
- **Fix.** Use the drop-in 1812, 0.5 A, **60 V** part: **mSMD050-60V C70113** (120k in stock) or SMD1812P050TF/60 (C462518).

### S6. USB-C J2 sits 1.85 mm inside the board edge
- **Evidence.** J2's F.Fab body runs y 154.85–162.15 against the board edge at 164.0.
- **Impact.** The HRO TYPE-C-31-M-12 has no overhang tab. With the mouth recessed that far, many plug overmolds hit the PCB edge and the plug doesn't fully seat. USB is the only bench flashing and recovery path.
- **Fix.** Move J2 so the shell front sits flush with the edge, or up to 0.5 mm past it. Then re-check the CPL J2 offset (+1.575 mm in y) in JLC's preview. That offset is the designer's correction and I could not verify it independently.

### S7. AC-side copper to the GND pour is only 0.4 mm
- **Evidence.** A copper scan shows the GND pour 0.4 mm from OPTO_MT2_x and GATE_x pads (U6–U8 pin 4/6 and R23/33/43), and 0.6 mm from the HVAC_x/R_FUSED pads. Those nets swing about 40 V (up to 64 V in a surge) against GND.
  - IPC-2221B table 6-1, B2 (uncoated external), 31–100 V: 0.6 mm.
  - The pour also runs across the opto isolation gap on the output side.
- **Fix.** Give the HVAC netclass a clearance of at least 1.0 mm to other nets (a zone/netclass rule). Keep the GND pour out of the opto output and triac region (left of x ≈ 129 around U6–U8). It isn't a safety barrier, since GND is tied to R/C through the bridge, but it improves surge robustness.

---
## NICE-TO-HAVE
- **N1. TVS margin.** At 9.3 A the SMBJ40A clamp (64.5 V) exceeds the LMR16006's recommended 60 V V_IN; absolute max is 65 V. Real Class-2 surges through the PTC and transformer impedance are much smaller, so this is acceptable. You could add 2–10 Ω in series ahead of the bulk caps, or accept it.
- **N2. L1 saturation.** SWPA4030S220MT Isat ≈1.0 A, below the LMR16006 I_LIMIT of 1.2–1.7 A. TI §9.2.2.2 suggests ≈1.6 A. A 5040-size 22 µH part avoids saturation on a shorted output.
- **N3. Buck layout.**
  - The hot loop (C2 GND → pour → D4 anode) is about 10 mm long. Rotate D4 and C2 so the GND ends of C2 and D4 meet at U1 pin 2.
  - R1/R2 sit about 5 mm from L1 (TI §11.1 item 1). Fine at this power level.
- **N4. No local decoupling for the SHT40.** The nearest 100 nF is C11, about 12 mm away and off the island. Sensirion recommends 100 nF close to VDD. A 0402 cap on the island adds no heat.
- **N5. C40 boost cap voltage.** C40 (4.7 µF 25 V X5R 0805) sees about VGH (≈20 V) and keeps only about 1 µF effective. 35/50 V would be better.
- **N6. Schematic symbol pin names.** D1's symbol labels AC pins 3/4 "+" and "−". The connectivity is correct (EasyEDA C2488: 1 = +, 2 = −, 3/4 = AC), but the labels mislead reviewers. D2's symbol names are A1/A2, but the pad mapping is correct (pad 1 = cathode = VIN_HV).
- **N7. Triac heat.** Each BT136S dissipates about 0.6 W at 0.5 A, so Y+G together put about 1.2 W in the case. That is far from the island, but the vents and the case thermal path matter.
- **N8. Firmware comment.** The `hw_watchdog.c` comment says "either edge". The STWD100 triggers on a high-to-low WDI edge. Toggling every 250 ms still gives a falling edge every 500 ms, under the 1.12 s minimum tWD, so it works. Make sure long blocking operations (OTA flash erase, EPD full refresh) still kick it.
- **N9. Rear-side height.** The J1 header + plug height (~10.5 mm) is estimated, against a 12 mm standoff. Confirm with the 15EDGRC/15EDGK drawings once J1's final position is fixed.
- **N10. JLC preview.** Check J2, J4 (FPC, rot 90), U5 (KiCad SHT4x footprint) and the SOT/SMD-6 parts in JLC's 3D placement preview. The rotations match the KiCad orientation one for one, which suits EasyEDA-origin footprints. I have not verified them against JLC's own models.

---
## VERIFIED OK (with evidence)
**ERC/DRC (re-run, kicad-cli 9.0.2)**
- ERC: 0 errors, 0 warnings.
- DRC with schematic parity: 0 errors, 0 unconnected, 0 footprint errors. The 12 warnings are all `lib_footprint_mismatch`, from local overrides.
- I refilled all zones in a copy (pcbnew ZONE_FILLER) and DRC was still clean.

**Power path**
- LMR16006X (TI SNVSA24):
  - Pinout SOT-23-6: 1 CB, 2 GND, 3 FB, 4 SHDN, 5 VIN, 6 SW. Matches.
  - V_OUT = 0.765 × (1 + 33k/10k) = 3.29 V. Over the V_FB tolerance 0.747–0.782 V that is 3.21–3.36 V, inside the ESP's 3.0–3.6 V.
  - Cboot 100 nF (§9.2.2.6). C_IN 4.7 µF 100 V placed about 2.7 mm from VIN (§9.2.2.5, 1–10 µF).
  - L = 22 µH and C_OUT = 2 × 10 µF, plus 10 µF at the module (Table 2 / typical-characteristics conditions).
  - Catch diode SS210, 100 V 2 A.
  - Minimum on-time: at 40 V, D ≈ (3.3 + 0.4)/40.4 ≈ 9.2 %, so t_on ≈ 131 ns at 700 kHz. At 64.5 V, ≈ 85 ns. The datasheet's only figure is 80 ns typ (at 2.1 MHz), so this is OK.
  - USB-only operation: ≈4.5 V in, above UVLO (4 V rising), D_max 96 %, so 3.3 V holds at modest load.
- Diode-OR topology is sensible: USB 5 V feeds the **buck input** (VIN_HV), not the 3.3 V rail, and the bridge is the other "diode". Only D3's rating is a problem (C2).
- MB10S bridge (1 kV, 1 A, 35 A surge, C2488): pad 1 = +/VIN_HV, pad 2 = −/GND, pads 3/4 = R_FUSED/HVAC_C. Matches the EasyEDA symbol (1 +, 2 −, 3/4 AC).
- SMBJ40A: unidirectional, cathode (pad 1, EasyEDA pin 1 = C) to VIN_HV, with a silk band on the cathode side. Standoff 40 V is above the ≈38 V peak at 28 VAC, and V_BR min 44.4 V covers 30 VAC unloaded.

**HVAC outputs**
- Triac switches **R → W/Y/G**: Q2–Q4 MT2 (tab, pad 2) = HVAC_R, MT1 = HVAC_W/Y/G. The load returns through the equipment to C. C only feeds the bridge.
- BT136S TO-252 pinout: 1 = T1, 2 = T2/tab, 3 = G (EasyEDA C18186224). Matches.
- Opto: MOC3063 pin 6 → 100 R → R, pin 4 → gate, 1 k gate–MT1. Pin 5 (substrate) NC. Gate drive works in quadrants I and III.
  - 100 R limits the opto peak to ≈0.34 A, below the 1 A rating.
  - The 600 V parts have a large margin.
- LED drive: (3.3 − 1.2 V)/220 Ω ≈ 9 mA. That is above MOC3063 IFT max 5 mA and well inside the GPIO's 20 mA default drive. The 10 k pull-downs keep outputs off while the pins are high-Z.
- The PTC protects only the bridge/PSU, by design. Output current is fused by the equipment.
- Firmware disables GPIO sleep-select on the drive pins (`gpio_sleep_sel_dis`), so light sleep doesn't drop outputs.

**ESP32-C6-WROOM-1 (EasyEDA C5366877 symbol, pins 1–28 + EP)**
- EN: 10 k + 1 µF (Espressif recommendation), with STWD100 WDO (open-drain) and RESET wired to EN.
- Straps:
  - IO8: 10 k pull-up plus the LED path pulls it high. Required high for download mode.
  - IO9: 10 k pull-up + BOOT switch.
  - IO15: NC, defaults are fine.
  - IO4/IO5: pulled-up buttons. MTMS/MTDI only affect SDIO timing, which is harmless.
- USB: D− → IO12 (pin 13), D+ → IO13 (pin 14). USBLC6 pins 1/6 = D−, 3/4 = D+, 5 = VBUS, 2 = GND.
- CC1/CC2: 5.1 k each to GND (UFP).
- Decoupling: 10 µF + 100 nF next to pin 2 (C7/C8), plus 100 nF at C10.
- Module edge is flush with the board edge, there is no copper inside the footprint keepout, and nothing on the board is under the panel area.

**Watchdog**
- STWD100**NY**: N = open-drain, Y = tWD 1.12/1.6/2.24 s, tPW 210 ms (ST datasheet ordering table). Pinout 1 WDO, 2 GND, 3 ~EN, 4 WDI, 5 VCC matches EasyEDA C46043.
- ~EN is pulled up by 10 k (disarmed), and firmware arms it 10 s after boot. After a WDO reset the ESP's pins go high-Z, which disarms it again.
- Firmware kicks every 250 ms (LOOP_MS), well under 1.12 s.

**Sensor**
- SHT40-AD1B, address 0x44 (matches firmware), 4.7 k pull-ups.
- Island: 4 slots. Only +3V3/GND/SDA/SCL enter, enforced by a DRC rule and confirmed.
- No filled copper in the island interior on either layer (zone hit-test).
- 24VAC nets are more than 80 mm away.

**E-paper (GDEY029T94, SSD1680)**
- FPC mapping: 1 NC, 2 GDR, 3 RESE, 4 NC, 5 VSH2, 6 TSCL (NC), 7 TSDA (NC), 8 BS1 = GND (4-wire SPI), 9 BUSY, 10 RES#, 11 D/C#, 12 CS#, 13 SCL, 14 SDA, 15 VDDIO, 16 VCI, 17 VSS, 18 VDD, 19 VPP (NC), 20 VSH1, 21 VGH, 22 VSL, 23 VGL, 24 VCOM. Matches the GD/Waveshare 24-pin pinout.
- Boost:
  - L2 47 µH 3V3 → Q1 drain. Q1 is an AO3400A (30 V, V_GS(th) ≤ 1.45 V). Source to the 2.2 Ω RESE sense resistor, 1 M gate pull-down.
  - D6: SW → VGH.
  - Negative pump: C40, D7 (NEG → GND), D8 (VGL → NEG).
  - 1 µF/50 V on VSH1/VSH2/VGH/VGL/VSL/VCOM, 1 µF on VDD, 10 µF + 1 µF on VCI.
- The bottom-contact AFC07 choice and its pin-1 alignment rely on the designer's fold analysis (part-plan "Display fit"). **Confirm with a physical panel**, as already noted.

**Netlist ↔ firmware (`components/board/include/board.h`)**
All match: W/Y/G = IO0/1/2, UP/DOWN/MODE = IO4/5/3, SDA/SCL = IO6/7, LED = IO8 (active-low), BOOT = IO9, WDI/WD_EN = IO10/11, EPD MOSI/SCK/CS/DC/RST/BUSY = IO18–23, USB = IO12/13.

**PCB/fab**
- Outline 140.0 × 64.0 mm, 1.6 mm. Island slots are 1.2 mm wide, above JLC's 1.0 mm minimum.
- Vias 0.6/0.3 and 1.0/0.5 mm. Minimum track/clearance 0.15 mm, and the HVAC nets use 1.0 mm (more than 2 A capacity at 1 oz).
- Mounting holes: 4 × 4.3 mm at (6.4, 6.4)/(133.6, 6.4)/(6.4, 57.6)/(133.6, 57.6). They match the case `holes` exactly, and no courtyard or track is within 4.5 mm of a hole centre (standoff Ø8, clamp post Ø6).
- Case cross-check: buttons (112, 30/40/50), RESET (91, 5), LED (63.3, 21.2), island and panel outline all agree with the PCB. **J1 does not (C1).**
- Front height under the panel (5 mm gap): tallest is MOC3063S at ≈3.6 mm, then L1 2.0 mm and J4 2.0 mm. Outside the panel, the 6.2 mm to the shell clears USB 3.2, 1210 2.5 and TO-252 2.3.
- Silk: diode bands on D2/D3/D4/D6–D8, polarity marks on D1, pin-1 dots on the opto/IC parts, and a "NO USB WHILE 24VAC IS WIRED" warning.
- Gerber/drill origin is the board's bottom-left (aux origin abs 100,164). NPTH and PTH files are separate.
- The fab copper/mask/paste/edge layers are byte-identical to a fresh export. The silk differs only in text plotting.
- BOM ↔ CPL: 81 designators in each with no differences. Every BOM line has an LCSC number that matches the footprint field. DNP parts (C20–22, R24/34/44, J3) are excluded. J1, the holes and the logo are correctly omitted from both. CPL coordinates and rotations equal the board placement, apart from the intentional J2 offset (S6).
- The ground-reference hazard (USB plus an earthed C) is understood and handled: there is no USB opening in the case, plus the silk warning.

**Live JLC stock (checked 2026-10-13 ~10:50 PT, qty for 2 boards)**
Every line is in stock, far above what 2 boards need. The tightest are:

| Part | LCSC | In stock |
|---|---|---|
| ESP32-C6-WROOM-1-N8 | C5366877 | 3,821 |
| SMBJ40A | C114003 | 3,355 |
| J1 plug 15EDGK | C7603433 | 797 |
| J1 header 15EDGRC | C7603439 | 1,234 |

Everything else has more than 14k (L2 C7427088 14,778; SHT40 20,383; STWD100 20,709; BT136S 25,450; LMR16006 32,481; USBLC6 41,474; AFC07 49,099; MOC3063S 71,630; FS32X475K101 153,406).
