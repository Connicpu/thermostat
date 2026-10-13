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
    dev.node_desc = MagicMock(manufacturer_code=0x131B, is_end_device=True)
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
    # standard clusters untouched (ZHA climate entity keeps working)
    assert Thermostat.cluster_id in ep.in_clusters


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
    ])


def test_writes_are_manufacturer_specific():
    dev = apply(make_device())
    cl = dev.endpoints[1].in_clusters[q.BOOST_CLUSTER_ID]
    cl.request = AsyncMock(return_value=[[]])
    asyncio.run(cl.write_attributes({"boost_command": q.BoostCommand.Start_timed}))
    kwargs = cl.request.call_args.kwargs
    assert kwargs.get("manufacturer") == 0x131B
    asyncio.run(cl.write_attributes({"boost_setpoint": 2222}))
    assert cl.request.call_args.kwargs.get("manufacturer") == 0x131B


def test_enum_values_match_firmware_header():
    import pathlib, re
    hdr = (pathlib.Path(__file__).parents[2] / "components/zigbee/include/zb_boost.h").read_text()
    def val(name):
        return int(re.search(rf"{name}\s*=\s*(0x[0-9A-Fa-f]+|\d+)", hdr).group(1), 0)
    assert int(re.search(r"ZB_BOOST_CLUSTER_ID\s+(0x\w+)", hdr).group(1), 16) == q.BOOST_CLUSTER_ID
    assert int(re.search(r"ZB_BOOST_MANUF_CODE\s+(0x\w+)", hdr).group(1), 16) == q.MANUF_CODE
    assert val("ZB_BOOST_CMD_START_TIMED") == q.BoostCommand.Start_timed
    assert val("ZB_BOOST_CMD_START_TARGET") == q.BoostCommand.Start_to_target
    assert val("ZB_BOOST_STATE_COOL_TIMED") == q.BoostState.Timed_cool
    assert val("ZB_BOOST_RESULT_REJECTED_NO_DIRECTION") == q.BoostResult.Rejected_no_direction
    attrs = q.ConnieBoostCluster.AttributeDefs
    assert val("ZB_BOOST_ATTR_LAST_RESULT") == attrs.boost_last_result.id
    assert val("ZB_BOOST_ATTR_REMAINING") == attrs.boost_remaining.id
