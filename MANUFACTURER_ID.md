# Zigbee manufacturer ID

## What we use

This project's Zigbee manufacturer code is **`0x434E`** (ASCII `"CN"`):

| Location | Constant |
|---|---|
| `firmware/components/zigbee/Kconfig` | `THERMO_OTA_MANUFACTURER` (default `0x434E`) |
| `firmware/components/zigbee/include/zb_boost.h` | `ZB_BOOST_MANUF_CODE` |
| `firmware/ha/zha_quirk/connie_thermostat_v3.py` | `MANUF_CODE` |
| `firmware/z2m/thermostat-v3.js` | `manufacturerCode: 0x434E` |
| `firmware/test/hootl/fake/include/sdkconfig.h` | `CONFIG_THERMO_OTA_MANUFACTURER` |

The Basic cluster manufacturer *name* string remains `"Connie"`; the model is `thermostat-v3`.

**`0x434E` is this project's DIY code. It is not CSA-assigned and the device is not
Zigbee-certified.** It was chosen from the quiet `0x3000`–`0x5FFF` band (empty in both
zigbee-herdsman's manufacturer-code list and the live Matter DCL vendor registry as of
2026-10-15), above CSA's sequential frontier (~`0x19xx`), and is not a common vanity value.
CSA could assign it to a member later; for a single uncertified home device the collision
risk is negligible. If this product is ever certified, switch to a CSA-assigned code.

Earlier drafts of this tree used Espressif's sample default `0x131B`. No devices had been
built or paired yet, so the cutover was a straight replace (no field migration).

## Why not the alternatives

| Option | Why not (for this project) |
|---|---|
| Keep Espressif `0x131B` | Espressif's real CSA code; every esp-zigbee sample uses it → collisions with other DIY ESP32 devices on the same network. |
| Matter/Zigbee test vendors `0xFFF1`–`0xFFF4` | Shared by every developer; CSA/Google say not for field or OTA use. |
| Reserved `0xFFF5`–`0xFFFF` | Reserved; `0xFFFF` is also the OTA "match any" wildcard. |
| CSA Adopter membership (~US$7k–7.5k/yr) | Only needed to certify or sell under an official code. |

## Community practice (researched 2026-10-15)

There is **no hobbyist / DIY manufacturer-code range**. CSA assigns codes only to members.
Hobby projects usually keep the silicon vendor's sample code, or pick an unused value (as here).
A few DIY vendors use vanity codes listed as "Non-CSA-assigned" in zigbee-herdsman
(Sprut `0x6666`, Lytko `0x7777`, Livolo `0x1AD2`, Perenio `0x007B`). zigpy quirks match on the
manufacturer *name* string and pass through whatever numeric code the device sends.

Shared Zigbee/Matter ID space (same registry):

| Range | Meaning |
|---|---|
| `0x1000`–`0x1994`-ish | Sequentially assigned CSA member codes (dense). |
| `0xC5A0`–`0xC5AF` | CSA's own codes. |
| `0xFFF1`–`0xFFF4` | Test vendor IDs (development only). |
| `0xFFF5`–`0xFFFF` | Reserved. |
| `0x3000`–`0x5FFF` | No assignments found in herdsman or the Matter DCL (checked 2026-10-15). |

## If the code ever changes again

Keep these in lockstep: firmware Kconfig + `ZB_BOOST_MANUF_CODE`, quirk `MANUF_CODE` + tests,
Z2M `manufacturerCode`, OTA image-builder `-m`, HOOTL `sdkconfig.h`, and this file. Rebuild
OTA images; already-paired devices need a re-interview or re-pair. Lovelace entity IDs are
unaffected (they key off the manufacturer *name* `"Connie"`).
