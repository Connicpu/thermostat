# Rev 10: L1 off the display, case 2 mm deeper (no hacks)

## Board
- L1 SWPA6045 (4.5 mm) moved to the top side at (24.1, 56.5) rot 180, left of the EPD outline (panel x>=29.46). The body edge is 2.4 mm outside the panel in X. The buck stage shifted about 7 mm left with it: U1 (32,52), C2 (28,50), D4 (31,57.3) vertical, C3 (34.2,55.7), R50/R1/R2 next to U1, and C4/C5 below L1. The input caps C1/C41/C42 (VIN_HV, 2.5 mm tall) moved under the panel where L1 was.
- Hot loop (C2 -> U1 VIN -> SW -> D4 -> GND -> C2): enclosed area ~6 mm^2, down from ~23 mm^2. U1 SW to D4 K is 3.4 mm of trace. Added GND stitching vias at C2 GND and U1 pin 2. The SW node now runs U1 -> D4 -> L1, 13.3 mm of copper in total incl. the C3 bootstrap branch; L1 pad to U1 SW is about 10 mm, longer than rev9's 7 mm.
- New pre-routes in gen_pcb2.py, protected in fix_dsn.py. Each one is a connection the autorouter dropped intermittently after the move: VIN_RECT (was rev9_fix_vrect.py, now a no-op); DRV_G (rev9 path); the triac MT1 hops; the VIN_HV C43->D3 back run; and the J4 +3V3 escape via.
- ERC 0. DRC (--schematic-parity --severity-all): 0 errors, 0 unconnected, 0 parity, 0 silk. Only the 17 benign lib_footprint_mismatch warnings remain.
- Unchanged: J1 is still B.Cu at (70,58), plus the antenna keepout, island nets, 1 mm HV pour rule, MOC3063 + DNP snubbers, cat logo, no serial header, and the bottom-left aux origin.

## Case
- standoff_h is now 14 (was 12). The plug pocket and the C43 lip notch are removed, and the lip is now 5.5 mm all the way round (was 6). panel_gap is back to 5.0 (it was 5.2 only to clear L1). Outline 147x71, inner r1.2. Total depth is 26.8 mm (was 25.0).

## Fit (mm, min clearance)
```
U3 ESP32-C6 module 18x25.5x3.15            vs shell            1.10 
J2 USB-C 8.94x7.3x3.26 (flush)             vs shell            1.10 
L1 SWPA6045 6.0x6.0x4.5                    vs shell            1.11 
U7                                         vs e-paper panel    1.55 
U8                                         vs e-paper panel    1.55 
D3                                         vs shell            1.97 
U3 ESP32-C6 module 18x25.5x3.15            vs e-paper panel    1.99 
C43 8x9 (9.5 incl. seat)                   vs backplate        2.06 
U7                                         vs shell            2.32 
L1 SWPA6045 6.0x6.0x4.5                    vs e-paper panel    2.41 
C1/C41 1210 x2.5                           vs e-paper panel    2.50 
C42 1210 x2.5                              vs e-paper panel    2.50 
J1 header (model)                          vs shell            2.50 
C2 1210 x2.5                               vs e-paper panel    2.51 
C2 1210 x2.5                               vs shell            2.70 
U6                                         vs shell            2.75 
U8                                         vs shell            2.75 
C43 8x9 (9.5 incl. seat)                   vs shell            2.80 
J1 plug 15EDGK (18.3 x 15.6 x 11.1)        vs backplate        2.90 
J3                                         vs shell            2.90 
SW4                                        vs shell            2.92 
SW5                                        vs shell            2.92 
SW3                                        vs shell            2.92 
C1/C41 1210 x2.5                           vs shell            2.93 
J4                                         vs e-paper panel    3.00 
J1/C43 THT tails front (2.0)               vs e-paper panel    3.00 
D4                                         vs e-paper panel    3.01 
D4                                         vs shell            3.29 
D2                                         vs backplate        3.37 
SW1                                        vs shell            3.50 
SW2                                        vs shell            3.50 
J1/C43 THT tails front (2.0)               vs shell            3.69 
Q2                                         vs shell            3.91 
Q3                                         vs shell            3.91 
Q4                                         vs shell            3.91 
D2                                         vs shell            3.93 
J4                                         vs shell            4.20 
C42 1210 x2.5                              vs shell            4.22 
SW3                                        vs e-paper panel    4.26 
SW4                                        vs e-paper panel    4.26 
SW5                                        vs e-paper panel    4.26 
J3                                         vs backplate        4.26 
J1 header (model)                          vs backplate        4.37 
J2 USB-C 8.94x7.3x3.26 (flush)             vs backplate        4.43 
wire 5                                     vs backplate        4.55 
wire 1                                     vs backplate        4.65 
J1 header body (datasheet 18.3x9.2x7.6)    vs backplate        5.27 
J1 header body (datasheet 18.3x9.2x7.6)    vs shell            5.85 
PCB outline vs shell 0.0
PCB vs backplate (standoff seat expected 0) -0.0
```
