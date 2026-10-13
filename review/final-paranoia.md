# Final paranoia review (2026-10-13, board @ 04ccf01 / rev10)

Verdict: **CLEAN for ordering, with 1 medium decision and 2 verify-in-JLC-preview items.** There are no errors and no fixes were needed.

## Checks run against the files (not the notes)
- ERC: 0 errors, 0 warnings. DRC with schematic parity: 0 errors, 0 unconnected, 0 footprint errors. The 17 warnings are all lib_footprint_mismatch, which is cosmetic: board footprints were edited after import.
- Fab freshness: regenerated F/B Cu, Mask, Paste and Edge.Cuts with the drill-origin setting. They are byte-identical to fab/thermostat-v3-gerbers.zip, ignoring header and date lines. Silk differs only because the fab export subtracts soldermask. The zip, BOM, CPL and .kicad_pcb all date from 13:34 PT. CPL positions of L1 (24.1, 7.5, rot 180) and R51 (20.0, 15.4, rot 90) match the board. J1 is THT on the back and is correctly left out of the CPL; C43 and J3 are also hand-soldered or DNP.
- Pinout (netlist):
  - USB D-/D+ go to IO12/IO13. CC1 and CC2 each have 5.1k.
  - EN is 10k/1uF, plus the STWD100 open-drain WDO. WD ~EN has a 10k pull-up to IO11 and WDI goes to IO10.
  - IO8 has the LED (active-low) and a 10k pull-up, so the strap is high. IO9 has a 10k pull-up and BOOT. IO15 is unconnected.
  - SHT40-AD1B is at 0x44 with 4.7k pull-ups on IO6/IO7.
  - EPD: the 24-pin map matches GDEY029T94. BS1 is tied to GND (4-wire SPI). The GDR/RESE booster uses AO3400A (30V), 2.2R and 1M, with B5819W diodes (40V). Caps on VGH, VGL and the other rails are 1uF 50V; C40 is 4.7uF 25V.
  - Buttons on IO4/IO5/IO3 have pull-ups. Drives on IO0/1/2 have 10k pull-downs.
  - Firmware board.h matches every GPIO.
  - Buck FB is 33k/10k with Vref 0.765, giving 3.29V.
  - D3 SS210 runs from VBUS to VIN_HV, so the 24V side can't backfeed USB.
  - Power chain: R goes to F1 (PTC, 60V/0.5A), then MB10S, R51 3.3R, then VIN_HV with SMBJ40A, 4x 4.7uF 100V and the C43 bulk cap.
- Opto/triac:
  - LED current is (3.3-1.2)/220R, about 9.5mA, against an IFT max of 5mA, so there is 2x margin.
  - Each MOC3063 drives the gate from R through 100R, with 1k from gate to MT1.
  - T435 DPAK: pad 2/tab is MT2 to R, pad 1 is MT1 to the load terminal, pad 3 is the gate. This matches the ST pinout.
- J1 pad order R, C, W, Y, G sits at x = 163, 166.5, 170, 173.5, 177. The back silk letters are at the same x. The cat logo (LOGO1) is present.
- Layout:
  - HVAC traces are 1.0mm. Vias are 0.5mm drill / 1.0mm pad, which is fine for 1A AC.
  - The ISLAND keepout has only GND, 3V3 and SDA/SCL in it.
  - The ANT_KEEPOUT zone is present on both layers.
  - Board is 140x64x1.6mm. The M4 holes at 6.4/133.6 by 6.4/57.6 match `holes` in case scad rev10.
  - No fiducials; JLC doesn't need them for this.
  - The hazard silk "NO USB WHILE 24VAC IS WIRED" is present.
- Live stock checked: T435-600B C145128 (1,744), 4.7uF 100V 1210 C381466 (89k), PTC C70113 (114k), 3.3R 0.75W C844038 (27k), 47uH C7427088 (14.6k). The 9 extended parts in bom_prices.json were all in stock this morning; the lowest is SMBJ40A at 3,355. C83454 (SWPA6045S220MT, extended) is listed on JLC, but its stock couldn't be fetched live.

## Findings
- **MEDIUM (decision):** The T435-600B holding and latching current is 35mA max, against about 15mA for V1's BT136. If a furnace or air-handler input (W, Y or G) draws less than about 35-50mA, which can happen with modern control boards or ECM blowers, the triac may not stay latched and the output could buzz or drop. V1 works on her system, but its triac was more forgiving. Options:
  - Keep it, and measure each input's current on a bench or in the wall.
  - Swap to a lower-IH snubberless triac (e.g. a T405/T410 "logic-level" 10mA-class part) if one is in stock.
- **LOW, verify in JLC placement preview:**
  - MB10S D1: the + mark must sit at the VIN_RECT pad (board 119.1,154.7). Vendors disagree on MB10S pin numbering, and the AC/DC sides are correct in our footprint, but a +/- swap would reverse-bias the supply.
  - Check pin-1 dots on U3 (C6), U5 (SHT40), U6-U8, J2 and the triac tabs in the same preview.
- **LOW:** C40 is 25V on the EPD boost. That is the reference-design value but has thin margin; 50V would be nicer. It is not required.
- **INFO:** fab/bom_prices.json is stale and is missing the 12 parts changed since this morning. The BOM itself is correct.

## Cost / parts
37 BOM lines. About 22 basic and 15 extended: the 9 already priced, plus T435, 1210 100V cap, PTC, R51, L1 and L2. At about $3 per extended part per order that adds roughly $45 in loading fees. The parts price for 2 boards plus attrition was about $44, before the new parts.
