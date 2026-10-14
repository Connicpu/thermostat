# ZHA quirk: Connie Thermostat V3 boost controls

`connie_thermostat_v3.py` is a **quirks v2** quirk. It adds the manufacturer-specific Boost
cluster (0xFC57, manufacturer code 0x131B) to the device and creates these entities:

| Entity | Platform | Attribute |
|---|---|---|
| Boost target temperature | number (°C, HA shows °F if your system is imperial) | 0x0000 |
| Boost duration | number, 1–120 min | 0x0001 |
| Boost mode | select: Auto / Heat / Cool | 0x0002 |
| Start boost to target / Start timed boost / Cancel boost | button (writes 1 / 2 / 0 to 0x0003) | 0x0003 |
| Boost | sensor: Off / Heating to target / Cooling to target / Timed heat / Timed cool | 0x0004 |
| Boost active | binary_sensor (running) | 0x0004 ≠ Off |
| Boost time remaining | sensor, min | 0x0005 |
| Last boost result (diagnostic) | sensor, e.g. *Target reached*, *Rejected already at target* | 0x0006 |
| Temperature swing (config) | number slider, 0.1–1.0 °C, step 0.1 (raw uint16 in 0.01 °C) | 0x0010 |

**Why swing isn't on the thermostat card itself:** HA's climate entity has no hysteresis
property (its "swing mode" means air-louver swing, and `target_temp_step` is only the setpoint
button increment), and ZHA doesn't map any Thermostat-cluster attribute to one. ZCL also has no
standard hysteresis attribute (0x0019 is *MinSetpointDeadBand*, which means something else). So
the swing is a number entity on the manufacturer cluster, and `dashboard-card.yaml` puts it as
a slider tile right under the thermostat card. It's a temperature *difference*, so it has no
temperature device class and always shows in °C (HA would otherwise turn 0.5 °C into 32.9 °F).

The thermostat, fan and humidity entities still come from ZHA's built-in handling of the
standard clusters. The quirk also **replaces the Thermostat cluster** with `ConnieThermostatCluster`, which
makes ZHA's climate entity offer only **off / heat / cool** (no heat_cool/auto) and refuses an Auto write;
see the main README (*Climate modes without Auto*) for why and the alternatives. Fan modes are auto/on.
The program-schedule number entities and the *Schedule period* sensor are listed in `connie_thermostat_v3.py`.

Tests: `python -m pytest test_quirk.py` in a venv with `zha` + `zha-quirks` (tested with zha 2.3.1 /
zha-quirks 2.3.0); includes a check against ZHA's real climate entity `hvac_modes`.

## Install

1. In `configuration.yaml`:
   ```yaml
   zha:
     custom_quirks_path: /config/custom_zha_quirks/
   ```
2. Copy `connie_thermostat_v3.py` into `/config/custom_zha_quirks/`.
3. Restart Home Assistant.
4. Pair the thermostat. If it's already paired: Device page → ⋮ → *Reconfigure*, or remove it and
   pair again (needed after updating the quirk so the replaced Thermostat cluster is used). The device page should list the quirk as "(Connie / thermostat-v3)" and show the
   boost entities.
5. Optional: install the custom [Lovelace card](../lovelace-card/) (auto-discovers the entities), or add
   `../dashboard-card.yaml` as a manual card (fix the entity IDs first).

The quirk imports from `zhaquirks.builder` (current zha-quirks). On older HA releases it falls
back to `zigpy.quirks.v2`.

## Test

```sh
python -m venv .venv && .venv/bin/pip install zha zha-quirks pytest
.venv/bin/python -m pytest -q test_quirk.py
```

The test checks that the quirk matches the device, adds the cluster without touching the
standard ones, declares all eleven entities, sends writes with manufacturer code 0x131B, and that
every attribute ID and enum value matches the firmware header `zb_boost.h`.
