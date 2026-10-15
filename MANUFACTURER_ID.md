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

### C. Pick a random unused 16-bit code for uncertified DIY

- There is **no official private / hobby / test reserved range** published for end products.
  Cluster IDs `0xFC00–0xFFFF` are the manufacturer-*specific cluster* range; that is separate
  from the 16-bit *manufacturer code* carried in ZCL frames.
- Some stacks treat `0xFFFF` as a wildcard in OTA matching — do **not** use `0xFFFF` as “yours.”
- Picking a random value (e.g. derived from a hash of `"Connicpu"`) and documenting it can work
  for a private HA network **if** you accept:
  - No CSA standing; you must not imply certification.
  - Non-zero collision risk with a real member's assigned code (low for a single home, not zero
    if the code is published and others copy it).
  - If CSA ever contacts you or you sell at scale, you still need option A.
- Community practice for uncertified DIY is mixed (silicon-vendor sample codes, or ad-hoc codes).
  **Recommended only with eyes open**, and only after grepping public lists / common SDK defaults
  so you don't pick Espressif (`0x131B`), Nordic (`0x127F`), etc.

### D. Ask Espressif whether sample-default use is OK to document

- Optional courtesy if staying on `0x131B`. Does not make the code “yours.”

## Recommendation (for Connie)

| Goal | Choice |
|---|---|
| Publish DIY, stay on HA, no certification | **B** (keep `0x131B`) or **C** (new DIY code) — her call. |
| Sell / certify as Zigbee | **A** (CSA Adopter + assigned code). |
| Change later | Feasible; see migration plan below. Do it in one coordinated release. |

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
