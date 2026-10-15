# Thermostat V3

Shared as-is — not seeking contributions or pull requests.

ESP32-C6 Zigbee thermostat for single-stage HVAC (R/C/W/Y/G), with e-paper display, SHT40 temperature/humidity sensor, and triac outputs. Designed for JLCPCB assembly.

- kicad/: KiCad 9 project
- fab/: Gerbers plus JLC BOM/CPL
- part-plan.md: part choices, sources, cost estimate
- v2-original/: the previous V2 design

Warning: don't connect USB while 24VAC is wired unless you use a USB isolator.

This project is shared as-is. It is not seeking contributions or pull requests.

## License

MIT — see [LICENSE](LICENSE). Third-party notices (Lit, Material Design Icons, DejaVu/Bitstream,
Espressif, Good Display sequences, LCSC STEP models, …) are in [NOTICE](NOTICE).

## Security

This is a DIY thermostat that switches 24 VAC HVAC loads. Treat the Zigbee network perimeter as
the security boundary:

- **OTA is unsigned.** The device accepts Zigbee OTA images and installs them with `esp_ota_*`
  (rollback via pending-verify exists). Anyone who can run an OTA server the device trusts can
  push firmware. There is no image signature check.
- **Join policy is open.** `install_code_policy` is false, so pairing uses the Zigbee 3.0
  well-known trust-center link-key path (normal for Home Assistant / ZHA). Don't leave the
  network open to joining when you aren't pairing.
- **USB Serial/JTAG console** is enabled in `sdkconfig.defaults`. Physical USB access to the
  board gives a console.
- Factory reset from the on-device menu is intentional.

See also [MANUFACTURER_ID.md](MANUFACTURER_ID.md) for the Zigbee manufacturer code (`0x131B`
today = Espressif's sample default, not a CSA code assigned to this project).

## Third-party / display provenance

E-paper driver sequences in `firmware/components/display/ssd1680.c` follow Good Display's
GDEY029T94 sample/datasheet (not a verbatim copy of their sample tree). Full attribution is
in [NOTICE](NOTICE).

