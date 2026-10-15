# Zigbee manufacturer ID

## What we use today

The firmware, ZHA quirk and Z2M converter all use **`0x131B`**:

| Location | Constant |
|---|---|
| `firmware/components/zigbee/Kconfig` | `THERMO_OTA_MANUFACTURER` (default `0x131B`) |
| `firmware/components/zigbee/include/zb_boost.h` | `ZB_BOOST_MANUF_CODE` |
| `firmware/ha/zha_quirk/connie_thermostat_v3.py` | `MANUF_CODE` |
| `firmware/z2m/thermostat-v3.js` | `manufacturerCode: 0x131b` |

**`0x131B` is Espressif's sample / SDK default** (`ESP_ZB_OTA_UPGRADE_MANUFACTURER_CODE_DEF_VALUE`
in esp-zigbee-lib). It is **not** a Connectivity Standards Alliance (CSA) manufacturer code
assigned to Connicpu / Connie. The Basic cluster manufacturer *name* string is still `"Connie"`.

CSA policy: a company may only use another party's manufacturer identification with that party's
consent. Using Espressif's sample default is common for uncertified hobby devices that follow
Espressif examples; it is not appropriate if you ever claim Zigbee certification under this code.

**No code change yet** — Connie will pick an option below before we flip the constants.

## Options for her own ID

### A. Official CSA-assigned manufacturer code (only clean path for certification)

- Codes are assigned **only to CSA members**, on request, and kept in the CSA Manufacturer Code
  Database (Zigbee document 05-3874).
- Practical minimum membership that can certify products: **Adopter**, about **US$7,000–7,500 / year**
  (CSA public tiers as of 2025–2026; confirm on https://csa-iot.org/become-member/).
  Product certification fees are extra (~US$2,500–3,000 / product) plus authorized-lab testing.
- **Associate** membership is free but is for white-label / Certification Transfer of someone
  else's already-certified product — it does **not** get you a DIY manufacturer code for your
  own uncertified design.
- **Cost for a hobby thermostat you don't certify:** not justified. Only worth it if you plan
  to sell certified product or need a conflict-free official ID for other business reasons.

### B. Keep `0x131B` (status quo)

- Works with current quirk / Z2M / OTA image builder flags.
- Document clearly (this file + README) that it is Espressif's sample default.
- Risk: theoretical collision with other Espressif-sample devices / Espressif's own products on
  the same network when matching on manufacturer code alone; ZHA matching here also uses
  manufacturer name `"Connie"` + model `"thermostat-v3"`, which reduces practical conflict.
- **Not** CSA-clean if you later certify.

### C. Pick an unassigned 16-bit code for uncertified DIY (recommended)

See "Community practice and reserved ranges" below for the research behind this.

### D. Ask Espressif whether sample-default use is OK to document

- Optional courtesy if staying on `0x131B`. Does not make the code "yours."

## Community practice and reserved ranges (researched 2026-10-15)

**There is no hobbyist / DIY manufacturer-code range.** Nothing in the CSA policy, the ZCL spec,
Silicon Labs / Nordic / Espressif docs, ESPHome, Tasmota, Zigbee2MQTT or zigpy defines one.

What actually exists in the 16-bit space (CSA keeps one database shared by Zigbee manufacturer
codes and Matter vendor IDs):

| Range / value | Meaning |
|---|---|
| `0x1000`-`0x1994`-ish | Sequentially assigned CSA member codes (dense: ~680 entries in zigbee-herdsman's copy of the database). |
| `0xC5A0`-`0xC5AF` | CSA's own codes. |
| `0xFFF1`-`0xFFF4` | **Test vendor IDs**: shared by every developer, for development only; CSA/Google say not for field or OTA use. |
| `0xFFF5`-`0xFFFF` | Reserved (`0xFFFF` is also the OTA "match any" wildcard). |
| `0x3000`-`0x5FFF` | **No assignments at all**, both in zigbee-herdsman's list and in the live Matter DCL vendor registry (871 vendors, highest `0x6006`, checked 2026-10-15). |

Sample defaults people ship with: Espressif `0x131B`, Nordic `0x127F`, DSR/ZBOSS `0x1234`,
Silicon Labs/Ember `0x1002`/`0x1049`, NXP `0x1037`. Hobby projects mostly just keep whatever the
SDK sample used. A few DIY vendors picked vanity codes (zigbee-herdsman lists them as
"Non-CSA-assigned": Sprut `0x6666`, Lytko `0x7777`, Livolo `0x1AD2`, Perenio `0x007B`). zigpy
quirks don't pick codes; they match on the manufacturer *name* string and pass through whatever
code the device sends.

Why not the obvious choices:
- `0x131B`: it's Espressif's real assigned code and every esp-zigbee sample uses it, so another
  ESP32 DIY device on the same network can share it.
- `0xFFF1`-`0xFFF4`: everyone's test firmware uses them, and they're explicitly not for devices in
  service.
- Vanity repeats (`0x6666`, `0xBEEF`, `0x1234`...): already taken or common.

**Suggested code: `0x434E`** ("CN" in ASCII). It's in the empty `0x3000`-`0x5FFF` band, far
above CSA's sequential frontier (~`0x19xx`), absent from both registries above, and not a common
vanity value. That isn't a guarantee: CSA could assign it some day. For a single uncertified
home device the collision risk is negligible, and if you ever certify you'd switch to an
assigned code anyway.

## Recommendation (for Connie)

No devices exist yet, so changing the code now costs nothing. Later it means reflashing,
rebuilding OTA images and re-interviewing paired devices.

| Goal | Choice |
|---|---|
| Uncertified HA DIY, published (her case) | **C with `0x434E`**: switch now, before any board exists. |
| Least effort | B (keep `0x131B`); works, but it's Espressif's code. |
| Sell / certify | A (CSA Adopter, assigned code). |

## What changing the code breaks (migration plan — do not apply yet)

Touches (must stay in lockstep):

1. Firmware: `CONFIG_THERMO_OTA_MANUFACTURER` / `ZB_BOOST_MANUF_CODE` (and any image-builder
   invocation that passes `-m 0x131B`).
2. ZHA quirk: `MANUF_CODE` in `connie_thermostat_v3.py` (manufacturer-specific attribute
   reads/writes and `manufacturer_id_override`); tests in `test_quirk.py` assert `0x131B`.
3. Z2M: `manufacturerCode: 0x131b` in `thermostat-v3.js`.
4. OTA: images built for the old code are **not** accepted by a device advertising the new code
   (and vice versa). Rebuild OTA with the new manufacturer field.
5. Already-paired devices: after a firmware flash that changes the code, ZHA/Z2M may need a
   re-interview / quirk reload so manufacturer-specific frames use the new code. Worst case:
   re-pair.
6. Cards / entity IDs: **unaffected** (they key off HA entity IDs / manufacturer *name*
   `"Connie"`, not the numeric code).

Suggested migration steps when Connie picks a value `0xNNNN`:

1. Land a single PR/commit that changes firmware + quirk + Z2M + README/MANUFACTURER_ID.md together.
2. Bump firmware version; build a new OTA image with `-m 0xNNNN`.
3. Flash or OTA one test device; confirm boost cluster R/W and OTA query still work under ZHA.
4. Document “devices on 0x131B need upgrade path X” for any boards already in the field.
5. Tag a card release only if quirk packaging/HACS notes need it (usually quirk is copied manually).
