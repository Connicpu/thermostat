# Case fit check, rev9 board (fab8653)

Method: kicad-cli STL of the populated board (real 3D models) + datasheet envelopes for parts without installed models (L1, J2, U3, C43, 1210s, J1 plug 15EDGK-3.5 18.3x15.6x11.1, wires d1.8 bend r4), signed distance against OpenSCAD-exported backplate/shell solids (scripts/case_fit.py). Min clearance per part (mm, negative = interference):

```
L1 SWPA6045 6.0x6.0x4.5                    vs e-paper panel    0.70 TIGHT
U3 ESP32-C6 module 18x25.5x3.15            vs shell            1.10 
J2 USB-C 8.94x7.3x3.26 (flush)             vs shell            1.10 
J1 plug 15EDGK (18.3 x 15.6 x 11.1)        vs backplate        1.31 
C43 8x9 (9.5 incl. seat)                   vs backplate        1.58 
L1 SWPA6045 6.0x6.0x4.5                    vs shell            1.69 
U7                                         vs e-paper panel    1.75 
U8                                         vs e-paper panel    1.75 
J1 header (model)                          vs backplate        1.92 
D3                                         vs shell            1.97 
U3 ESP32-C6 module 18x25.5x3.15            vs e-paper panel    2.18 
U7                                         vs shell            2.48 
J1 header (model)                          vs shell            2.50 
C2 1210 x2.5                               vs e-paper panel    2.70 
C43 8x9 (9.5 incl. seat)                   vs shell            2.80 
SW4                                        vs shell            2.92 
SW5                                        vs shell            2.92 
SW3                                        vs shell            2.92 
U8                                         vs shell            2.95 
U6                                         vs shell            2.95 
J3                                         vs shell            3.10 
J4                                         vs e-paper panel    3.19 
J1/C43 THT tails front (2.0)               vs e-paper panel    3.20 
D4                                         vs e-paper panel    3.20 
C1/C41/C42 1210 x2.5                       vs shell            3.34 
D2                                         vs backplate        3.37 
SW1                                        vs shell            3.50 
SW2                                        vs shell            3.50 
J1/C43 THT tails front (2.0)               vs shell            3.81 
J1 header body (datasheet 18.3x9.2x7.6)    vs backplate        3.82 
D2                                         vs shell            3.93 
Q2                                         vs shell            4.11 
Q3                                         vs shell            4.11 
Q4                                         vs shell            4.11 
C1/C41/C42 1210 x2.5                       vs e-paper panel    4.16 
J3                                         vs backplate        4.26 
C2 1210 x2.5                               vs shell            4.31 
J2 USB-C 8.94x7.3x3.26 (flush)             vs backplate        4.39 
J4                                         vs shell            4.39 
D4                                         vs shell            4.41 
SW3                                        vs e-paper panel    4.44 
SW4                                        vs e-paper panel    4.44 
SW5                                        vs e-paper panel    4.44 
wire 5                                     vs backplate        4.55 
wire 1                                     vs backplate        4.65 
J1 header body (datasheet 18.3x9.2x7.6)    vs shell            5.85 
PCB outline vs shell 0.0
PCB vs backplate (standoff seat expected 0) -0.0
```
