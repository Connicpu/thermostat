"""Smoke test for the ZHA quirk: python -m pytest test_quirk.py (in a venv with zha + zha-quirks)."""
import asyncio
from unittest.mock import AsyncMock, MagicMock

import pytest
import zigpy.device
import zigpy.types as t
from zigpy.zcl.clusters.general import Basic, Identify, Ota
from zigpy.zcl.clusters.hvac import Fan, Thermostat
from zigpy.zcl.clusters.measurement import RelativeHumidity, TemperatureMeasurement

import connie_thermostat_v3 as q


def make_device():
    app = MagicMock()
    app.get_sequence = MagicMock(return_value=1)
    dev = zigpy.device.Device(app, t.EUI64.convert("aa:bb:cc:dd:ee:ff:00:11"), 0x1234)
    dev.node_desc = MagicMock(manufacturer_code=0x434E, is_end_device=True)
    ep = dev.add_endpoint(1)
    ep.profile_id = 0x0104
    ep.device_type = 0x0301
    for c in (Basic, Identify, Thermostat, Fan, TemperatureMeasurement, RelativeHumidity):
        ep.add_input_cluster(c.cluster_id)
    ep.add_output_cluster(Ota.cluster_id)
    dev.manufacturer = q.MANUFACTURER
    dev.model = q.MODEL
    ep.manufacturer = q.MANUFACTURER
    ep.model = q.MODEL
    return dev


def apply(dev):
    for op in q.QUIRK_ENTRY.zigpy_transforms:
        dev = op(dev)
    return dev


def test_matches_and_adds_cluster():
    dev = apply(make_device())
    ep = dev.endpoints[1]
    assert q.BOOST_CLUSTER_ID in ep.in_clusters
    assert isinstance(ep.in_clusters[q.BOOST_CLUSTER_ID], q.ConnieBoostCluster)
    # Thermostat replaced by the no-Auto variant (still the standard cluster id, so ZHA's climate entity is used)
    assert isinstance(ep.in_clusters[Thermostat.cluster_id], q.ConnieThermostatCluster)
    assert Fan.cluster_id in ep.in_clusters


def test_entities_declared():
    md = q.QUIRK_ENTRY.zha_device_factory
    entities = None
    for attr in ("definition", "quirk_definition", "_definition"):
        d = getattr(md, attr, None)
        if d is not None:
            entities = d.entity_metadata
    if entities is None:  # fall back: search the factory's attributes
        entities = next(v.entity_metadata for v in vars(md).values() if hasattr(v, "entity_metadata"))
    names = sorted(e.fallback_name for e in entities)
    assert names == sorted([
        "Boost target temperature", "Boost duration", "Boost mode",
        "Start boost to target", "Start timed boost", "Cancel boost",
        "Boost", "Boost active", "Boost time remaining", "Last boost result",
        "Temperature swing",
        "Day heating start", "Night heating start", "Day cooling start", "Night cooling start",
        "Day heating temperature", "Night heating temperature", "Day cooling temperature",
        "Night cooling temperature", "Morning heating boost duration", "Morning heating boost offset",
        "Schedule period",
    ])


def test_writes_are_manufacturer_specific():
    dev = apply(make_device())
    cl = dev.endpoints[1].in_clusters[q.BOOST_CLUSTER_ID]
    cl.request = AsyncMock(return_value=[[]])
    asyncio.run(cl.write_attributes({"boost_command": q.BoostCommand.Start_timed}))
    kwargs = cl.request.call_args.kwargs
    assert kwargs.get("manufacturer") == 0x434E
    asyncio.run(cl.write_attributes({"boost_setpoint": 2222}))
    assert cl.request.call_args.kwargs.get("manufacturer") == 0x434E


def test_enum_values_match_firmware_header():
    import pathlib, re
    hdr = (pathlib.Path(__file__).parents[2] / "components/zigbee/include/zb_boost.h").read_text()
    def val(name):
        return int(re.search(rf"{name}\s*=\s*(0x[0-9A-Fa-f]+|\d+)", hdr).group(1), 0)
    assert int(re.search(r"ZB_BOOST_CLUSTER_ID\s+(0x\w+)", hdr).group(1), 16) == q.BOOST_CLUSTER_ID
    assert int(re.search(r"ZB_BOOST_MANUF_CODE\s+(0x\w+)", hdr).group(1), 16) == q.MANUF_CODE
    assert val("kZbBoostCmdStartTimed") == q.BoostCommand.Start_timed
    assert val("kZbBoostCmdStartTarget") == q.BoostCommand.Start_to_target
    assert val("kZbBoostStateCoolTimed") == q.BoostState.Timed_cool
    assert val("kZbBoostResultRejectedNoDirection") == q.BoostResult.Rejected_no_direction
    attrs = q.ConnieBoostCluster.AttributeDefs
    assert val("kZbBoostAttrLastResult") == attrs.boost_last_result.id
    assert val("kZbBoostAttrRemaining") == attrs.boost_remaining.id
    assert val("kZbAttrSwing") == attrs.temperature_swing.id
    for hdr_name, attr in (("kZbAttrDayCoolStart", attrs.day_cool_start), ("kZbAttrNightHeatTemp", attrs.night_heat_temp),
                           ("kZbAttrMorningBoostMin", attrs.morning_boost_duration),
                           ("kZbAttrMorningBoostOfs", attrs.morning_boost_offset), ("kZbAttrSchedState", attrs.schedule_state)):
        assert val(hdr_name) == attr.id


def test_climate_modes_off_heat_cool_only():
    from zha.application.platforms.climate.const import SEQ_OF_OPERATION, HVACMode
    dev = apply(make_device())
    th = dev.endpoints[1].in_clusters[Thermostat.cluster_id]
    # before any read, and after the device reports the standard 0x04
    for _ in range(2):
        seq = th.get(Thermostat.AttributeDefs.ctrl_sequence_of_oper.name, 0xFF)
        assert sorted(SEQ_OF_OPERATION[seq]) == sorted([HVACMode.OFF, HVACMode.HEAT, HVACMode.COOL])
        assert HVACMode.HEAT_COOL not in SEQ_OF_OPERATION[seq]
        th.update_attribute(Thermostat.AttributeDefs.ctrl_sequence_of_oper.id,
                            Thermostat.ControlSequenceOfOperation.Cooling_and_Heating)
    # other attributes pass through untouched
    th.update_attribute(Thermostat.AttributeDefs.system_mode.id, Thermostat.SystemMode.Heat)
    assert th.get("system_mode") == Thermostat.SystemMode.Heat


def test_auto_write_refused_other_writes_pass():
    dev = apply(make_device())
    th = dev.endpoints[1].in_clusters[Thermostat.cluster_id]
    th.request = AsyncMock(return_value=[[]])
    with pytest.raises(ValueError):
        asyncio.run(th.write_attributes({"system_mode": Thermostat.SystemMode.Auto}))
    with pytest.raises(ValueError):
        asyncio.run(th.write_attributes({Thermostat.AttributeDefs.system_mode.id: 0x01}))
    th.request.assert_not_called()
    asyncio.run(th.write_attributes({"system_mode": Thermostat.SystemMode.Cool}))
    th.request.assert_called()


def test_offset_is_zha_standard_number_not_a_quirk_duplicate():
    """ZHA already exposes LocalTemperatureCalibration. The quirk must not add a second one."""
    from zha.application.platforms.number import ThermostatLocalTempCalibration

    md = q.QUIRK_ENTRY.zha_device_factory
    entities = None
    for attr in ("definition", "quirk_definition", "_definition"):
        d = getattr(md, attr, None)
        if d is not None:
            entities = d.entity_metadata
    if entities is None:
        entities = next(v.entity_metadata for v in vars(md).values() if hasattr(v, "entity_metadata"))
    assert all(getattr(e, "attribute_name", None) != "local_temperature_calibration" for e in entities)

    ent = ThermostatLocalTempCalibration
    assert ent._unique_id_suffix == "local_temperature_calibration"
    assert ent._attribute_name == "local_temperature_calibration"
    assert ent._attr_translation_key == "local_temperature_calibration"
    assert ent._cluster_id == Thermostat.cluster_id
    assert ent._attr_native_min_value == -2.5
    assert ent._attr_native_max_value == 2.5
    assert ent._attr_native_step == 0.1
    assert ent._multiplier == 0.1
    assert ent._attr_native_unit_of_measurement == "°C"
    # A temperature device class would add 32 °F. This is a difference, so ZHA leaves it in °C.
    assert "_attr_device_class" not in ent.__dict__
    assert ent._attr_entity_category.name == "CONFIG"

    attr = Thermostat.AttributeDefs.local_temperature_calibration
    assert attr.id == 0x0010
    assert attr.type == t.int8s

    dev = apply(make_device())
    th = dev.endpoints[1].in_clusters[Thermostat.cluster_id]
    found = th.find_attribute(attr.id)
    assert found.id == 0x0010
    # The replaced cluster still answers the real attribute (only the control sequence is overridden).
    th.update_attribute(attr.id, 4)
    assert th.get("local_temperature_calibration") == 4


def test_zha_climate_entity_modes():
    """End to end through ZHA's real climate entity class (zha library)."""
    from zha.application.platforms.climate import Thermostat as ZhaThermostat
    from zha.application.platforms.climate.const import HVACMode
    dev = apply(make_device())
    th = dev.endpoints[1].in_clusters[Thermostat.cluster_id]
    th.update_attribute(Thermostat.AttributeDefs.ctrl_sequence_of_oper.id, 0x04)
    ent = ZhaThermostat.__new__(ZhaThermostat)
    ent._cluster = th
    assert set(ZhaThermostat.hvac_modes.fget(ent)) == {HVACMode.OFF, HVACMode.HEAT, HVACMode.COOL}
