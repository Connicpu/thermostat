# Thermostat V3 – Part Plan (rev 2, 2026-10-12 night)
Stock, prices and basic/extended status come from the JLCPCB parts API, checked 2026-10-12 ~23:00–23:40 PT. Re-check before ordering.
Previous revision: part-plan-v1.md

## Architecture (as implemented in kicad/thermostat-v3.kicad_sch)
- **24VAC input:** 24VAC R/C goes through a 0.5 A PTC (F1), an MB10S bridge (D1), an SMBJ40A TVS (D2) and a 100 µF/63 V bulk cap (C1) to VIN_HV.
- **3.3 V supply:** an **LMR16006X buck (60 V max, 0.6 A, 700 kHz)** with a 22 µH inductor and SS210 catch diode makes +3V3.
  - The X (700 kHz) version was chosen over the Y (2.1 MHz) version because the minimum on-time at ~40 V in → 3.3 V out is too short at 2.1 MHz.
- **USB:** USB-C VBUS goes through a B5819W into VIN_HV (diode-OR; the bridge itself is the other "diode"). USB D+/D- go straight to the C6's native USB, with USBLC6 ESD protection.
- **MCU:** ESP32-C6-WROOM-1-N8 (Zigbee). **Firmware default: Zigbee end device** (rx-off-when-idle / light-sleep between polls) to limit self-heating. A router role is optional later.
- **HVAC outputs, ×3 (W, Y, G):**
  - GPIO → 220R → MOC3063 zero-cross opto (10k pull-down on each drive line, so outputs are off when the GPIO is high-Z)
  - opto → 100R from R, 1k gate-MT1 → BT136S-600D triac switching R onto W/Y/G
  - **DNP RC snubber footprint per channel** (100R 0805 + 100 nF 50 V 0805, across MT2–MT1)
- **Watchdog:** STWD100NY (1.6 s), open-drain WDO wired to ESP EN.
  - Its ~EN pin has a 10k pull-up, so the watchdog is **disarmed at boot and while flashing**.
  - Firmware arms it by driving IO11 low and kicks it on IO10.
  - If firmware hangs → watchdog resets the ESP → GPIOs go high-Z → triacs off.
- **Display:** 2.9" 296×128 SSD1680 panel (GDEY029T94 / Waveshare 2.9" V2) on a 24-pin 0.5 mm FPC.
  - Onboard boost per the Waveshare reference schematic: **68 µH**, Si1308EDL, **3 Ω sense resistor (B/W 2.9")**, 10k gate pull-down, 3 Schottky pump diodes (B5819W, which is basic, instead of MBR0530), and 1 µF caps on VSH1/VSH2/VSL/VGH/VGL/VCOM/VDD/VPP.
  - BS1 is tied to GND (4-wire SPI).
- **Sensor:** SHT40 on a slotted thermal island (4 slots, power/I2C enter through one bridge, no GND pour on the island), plus a Stemma QT port.
- **UI:** UP / DOWN / MODE buttons, RESET and BOOT buttons, and one red status LED (IO8; pull-up keeps the strap pin high).

## Verified changes since rev 1
| Item | Decision | LCSC | Type | Stock | Unit $ |
|---|---|---|---|---|---|
| Buck | **LMR16006XDDCR** (4–60 V in, 0.6 A, async) | C87080 | ext | 33,642 | 0.57 |
| Buck inductor | SWPA4030S220MT 22 µH | C83472 | ext | 40,683 | 0.06 |
| Catch diode | SS210 (100 V 2 A) | C14996 | basic | 708,928 | 0.03 |
| Input bulk cap | **100 µF 63 V** 10×10.5 SMD (RVT63V100M10X10) | C51953411 | ext | 26,682 | 0.18 |
| Input ceramic | 2.2 µF 100 V 1206 | C170101 | ext | 166,291 | 0.13 |
| TVS | SMBJ40A (40 V standoff, so a 26–28 VAC line won't conduct) | C114003 | ext | 3,355 | 0.05 |
| PTC | SMD1812P050TF/30 | C12559 | ext | 21,463 | 0.11 |
| EPD boost L | **68 µH** 3×3 (ANR3015T680M) – per Waveshare 2.9" schematic | C7427091 | ext | 4,085 | 0.04 |
| EPD sense R | **3 Ω** 0402 (Waveshare: 3R for 2.9" B/W; 0.47R only for B/C) | C423196 | ext | 36,784 | <0.01 |
| EPD FPC | **Jushuo AFC07-S24FCA-00, bottom contact, flip-lock** (KiCad footprint exists) | C262567 | ext | 49,144 | 0.13 |
| Triac | BT136S-600D TO-252 | C18186224 | ext | 25,450 | 0.10 |
| Watchdog | STWD100NYWY3F (switched from TPS3823 because it has an ENABLE pin, so it doesn't fire during flashing) | C46043 | ext | 21,379 | 0.39 |
| HVAC terminal | 15EDGRC-3.5-05P header (hand-solder) + 15EDGK-3.5-05P plug | C7603439 / C7603433 | ext | 1,234 / 800 | 0.27 / 0.84 |

The full priced BOM is in fab/thermostat-v3-BOM-JLC.csv and fab/bom_prices.json: 38 lines, **18 unique extended parts**.

### FPC contact orientation
- Waveshare's FAQ specifies a "0.5 mm 24-pin rear-flip 2.0H" socket.
- I chose a **bottom-contact** flip connector: with the panel lying face-up on the board's top side and its tail running straight out into the connector, the gold fingers face down (toward the PCB).
- **Before ordering, confirm on a physical panel that the GDEY029T94 fingers are on the side opposite the display face.**
- If they aren't, swap to the double-sided-contact HDGC 0.5K-HX-24PWB (C2919499, 3,685 in stock, contacts on both sides). That part needs a custom footprint.

## Pin map (ESP32-C6-WROOM-1)
| Pin | GPIO | Net |
|---|---|---|
| 8 / 9 / 27 | IO0 / IO1 / IO2 | DRV_W / DRV_Y / DRV_G |
| 4 / 5 / 26 | IO4 / IO5 / IO3 | BTN_UP / BTN_DOWN / BTN_MODE (10k pull-ups) |
| 6 / 7 | IO6 / IO7 | I2C SDA / SCL (4.7k) – SHT40 + Stemma QT |
| 10 | IO8 | LED_STAT (active-low, 10k pull-up, strap) |
| 15 | IO9 | BOOT button (strap) |
| 11 / 12 | IO10 / IO11 | WD_WDI / WD_EN (active-low arm) |
| 13 / 14 | IO12 / IO13 | USB D- / D+ |
| 16–21 | IO18–23 | EPD MOSI, SCK, CS, DC, RST, BUSY |
| 23, 24, 25 | IO15, RXD0, TXD0 | spare / NC |

## Cost estimate – 5 assembled boards + 2 panels
| Item | Basis | USD |
|---|---|---|
| PCB, 2-layer 140×64 mm, qty 5 | JLC standard price for >100 mm boards (estimate) | ~7 |
| PCBA setup + stencil (Economic) | $8 + $1.50 | 9.50 |
| Extended-part feeder fees | 18 × $3 | 54.00 |
| Components ×5 boards | priced from JLC API (bom_prices.json); +~5% for attrition/min-qty | ~53 |
| Solder joints | ~420 joints × 5 × $0.0017 | ~3.60 |
| **JLC subtotal** | | **~127** |
| Shipping to US incl. tariff/duty | **Assumption:** standard/DHL ~$20–30; using $30 | 30 |
| HVAC headers + plugs ×5 | C7603439 + C7603433 at ~$1.11/set; add to the same JLC order as unassembled parts (or LCSC + ~$9 ship) | ~6 (≤15) |
| 2× GDEY029T94 2.9" panels | buy-lcd.com $6.46 each – https://buy-lcd.com/products/29-inch-e-paper-screen-296x128-raspberry-pi-spi-display-gdey029t94 ; shipping not verified, assume ~$10 | ~23 |
| **Total** | | **~$186 (worst case ~$195)** |

Under the $200 target, but with little margin. If needed, these trims save ~$3 per removed extended part:
- Replace the PTC with a 0 Ω jumper (lose fusing).
- Replace the 3 Ω sense resistor with a basic value. Check the boost first.
- Drop U4 (STWD100) and rely on the C6's internal watchdogs plus the default-off pull-downs.

The ESP32 module ($21) and SHT40 ($9.50) dominate the parts cost; SHT40 → AHT20 would save ~$7 at the cost of accuracy.

## Concerns / to verify
1. **USB + 24VAC ground hazard.** Board GND is the bridge's negative node, not HVAC C. Never connect USB to a mains-earthed PC while 24VAC is attached, unless the transformer secondary is floating; one rectifier diode can then conduct through the PC's ground. Flash on the bench, or use a USB isolator. Optionally add a silkscreen warning.
2. **LMR16006 output caps:** 2×10 µF is used. The TI datasheet table may recommend more for 3.3 V at 700 kHz; check and consider 2×22 µF (basic 0805 parts exist).
3. **ESP32-C6-WROOM-1 footprint** is custom (tstat.pretty), built from Espressif datasheet v1.4 Fig. 11-1. The pad rows (±8.75 mm, 1.27 mm pitch, pin 1 7.49 mm from the top) follow the drawing; **the exposed-pad position is approximate.** Verify against Espressif's land-pattern source or JLC's footprint before ordering.
4. **JLC CPL rotations** for the SOT-23, SOT-323, TO-252, SMDIP, FPC and USB-C parts usually need offset fixes. Check them in JLC's placement preview.
5. **Triac leakage:** zero-cross optos plus snubberless triacs leak µA-level current when off; this is normally fine with contactor/relay-based furnace and AC control boards. Snubber footprints are present but not populated (DNP), so they can be fitted if a control board chatters.
6. Firmware must interlock W/Y, enforce minimum compressor off-time, arm the watchdog only after boot, and refresh the e-paper ≥180 s apart (Waveshare guidance) with the panel put to sleep in between.

---
## Rev 3 (phase 4 verification, 2026-10-13 PT)
Verified:
- ESP32-C6-WROOM-1: Espressif datasheet v1.4 (Fig. 11-1 / p.44) vs EasyEDA C5366877 footprint; switched to EasyEDA footprint (EP centre corrected to x-1.5,y+0.42). Module top flush to board edge, antenna keepout zone, no copper under antenna.
- All custom footprints replaced with EasyEDA versions via easyeda2kicad (MOC3063S wide 10.16 mm, terminal drill 1.4 mm, MB10S pinout fixed, etc).
- GDEY029T94 (Good Display datasheet p.7, p.29): contacts on display-face side; FPC folded 180 deg under panel -> contacts face down -> bottom-contact Jushuo AFC07-S24FCA-00 (C262567, 49k stock). Boost per GD reference: 47 uH (C7427088), 2.2 R, 1 M, 4.7 uF 25 V, VPP NC.
- LMR16006 (TI SNVSA24): internal compensation, Vref 0.765 V; 22 uH + 2x10 uF, 33k/10k, 0.1 uF boot OK; Ton(min) margin at 40 V OK. No change.
- CPL rotations: EasyEDA footprints = JLC orientation (0 offset); KiCad USB-C HRO position corrected by local y -1.575 mm (EasyEDA pad comparison); KiCad passives/switch/SHT4x 0.
Results: ERC 0; DRC 0 errors, 0 unconnected, schematic parity clean (104 cosmetic silk warnings + 1 lib_footprint_mismatch warning on U3).
Cost (5 boards): parts $50.59 + 18 extended x $3 = $54 + PCB/assembly setup ~ $22 + shipping ~$30 + terminals ~$6 + panels ~$23 => ~$186.

## Display fit check (2026-10-13 PT)
Source: GD GDEY029T94 datasheet p.7 (mechanical drawing), p.6 (79.0x36.7x1.2, AA 66.896x29.056).
- FPC tail: 12.50 wide, 14.40 free length beyond outline, 24 contacts at 0.5 pitch (span 11.50), contact length 3.66 with stiffener, 0.3 thick; contacts on display-face side. Tail edge is 11.50 mm from the outline edge nearest pin 24; pin 1 is 23.5 mm from that edge.
- Board: J4 at (100,44) rot 90, mouth faces +x at x~103.8, pad 1 y=49.74, pad 24 y=38.24.
- Panel placement (face up, FPC on right edge, 180 deg fold under): outline x 29.46..108.46, y 26.24..62.94, panel bottom 5.0 mm above PCB (M2.5x5 standoffs or case ledge). Bend r~2.15 mm, ~6.8 mm of tail in the bend, ~3.0 mm inserted. Pin 1 / pad 1 y match exactly (49.74). Contacts face down -> bottom-contact AFC07 correct.
- Panel standoff height must be >= ~4.5 mm (MOC3063S optos ~4 mm under panel). Every +1 mm of height pulls the tail ~1.6 mm out of the connector; keep 4.6-5.2 mm.
- No copper change; only the Cmts.User panel outline moved (-0.54, +1.24). Image: fab/display-fit.png

## Rev 4 – JLC upload fixes (2026-10-13 PT)
- CPL/gerber origin: aux/drill/grid origin set at board bottom-left (abs 100,164). Gerbers exported with --use-drill-file-origin, drill with --drill-origin plot, CPL = (x-100, 164-y). Verified: CPL range x 13.5..128, y 3.5..59; U5 pad 1 at (127.3, 32.4) flashes at X127.3 Y32.4 in F_Cu/F_Paste gerbers.
- Switches: EasyEDA footprint for C318884 (TS-1187A-B-A-B, 5.1x5.1 4-pad, pads 1/2 -> pin 1, 3/4 -> pin 2); BOM grouped by LCSC+footprint -> one line "TS-1187A-B-A-B" SW1..SW5.
- Extended->basic swaps: Q1 SI1308EDL (C469327) -> AO3400A (C20917, SOT-23, 30 V/5.7 A, same G/S/D pinout); R16 2.2R 0402 (C25101) -> 2.2R 0603 (C22939); C2 2.2uF 100V (C170101) -> 2.2uF 50V X7R 1206 (C50254, VIN_HV <= ~37 V, TVS standoff 40 V); J3 Stemma QT (C51940130) -> DNP (hand-solder if wanted).
- Kept extended (no basic equivalent with same function): ESP32-C6, SHT40, LMR16006, MOC3063S, BT136S, STWD100, USBLC6, USB-C, FPC conn, 22uH/47uH inductors, SMBJ40A, PTC, 100uF/63V.
- Silkscreen: refs 0.8/0.15 mm auto-placed, footprint silk trimmed at pads/edge; 0 silk DRC warnings. Cat logo + "Connie's Super Awesome Thermostat / VERSION 3.0" on bottom silk.
