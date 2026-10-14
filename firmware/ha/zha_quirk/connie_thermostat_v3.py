"""ZHA quirk (quirks v2) for Connie's Super Awesome Thermostat V3.

Adds the manufacturer-specific Boost cluster 0xFC57 (boost, swing and the
day/night program schedule) and exposes it as clean Home Assistant entities. The standard Thermostat / Fan / Humidity clusters are
left alone, so ZHA's normal climate entity keeps working.

Install: copy this file into the folder configured as `zha: custom_quirks_path:`
in configuration.yaml, restart Home Assistant, then reconfigure (or re-pair)
the device. The wire protocol is defined in firmware/components/zigbee/include/zb_boost.h.
"""

from __future__ import annotations

try:
    from zhaquirks.clusters import CustomCluster
except ImportError:  # older zha-quirks
    from zigpy.quirks import CustomCluster  # type: ignore[no-redef]
import zigpy.types as t
from zigpy.zcl.foundation import BaseAttributeDefs, ZCLAttributeDef

try:  # zha-quirks >= 2.x: canonical location
    from zhaquirks.builder import (
        BinarySensorDeviceClass,
        EntityPlatform,
        EntityType,
        NumberDeviceClass,
        QuirkBuilder,
        SensorDeviceClass,
    )
    from zha.units import UnitOfTemperature, UnitOfTime
except ImportError:  # older Home Assistant / zha-quirks 0.0.1xx
    from zigpy.quirks.v2 import (  # type: ignore[no-redef]
        BinarySensorDeviceClass,
        EntityPlatform,
        EntityType,
        NumberDeviceClass,
        QuirkBuilder,
        SensorDeviceClass,
    )
    from zigpy.quirks.v2.homeassistant import UnitOfTemperature, UnitOfTime  # type: ignore[no-redef]

MANUFACTURER = "Connie"
MODEL = "thermostat-v3"
BOOST_CLUSTER_ID = 0xFC57
MANUF_CODE = 0x131B


class BoostMode(t.enum8):
    """Which way a boost drives the HVAC (attribute 0x0002)."""

    Auto = 0x00
    Heat = 0x01
    Cool = 0x02


class BoostCommand(t.enum8):
    """Write-only trigger (attribute 0x0003)."""

    Cancel = 0x00
    Start_to_target = 0x01
    Start_timed = 0x02


class BoostState(t.enum8):
    """Current boost (attribute 0x0004)."""

    Off = 0x00
    Heating_to_target = 0x01
    Cooling_to_target = 0x02
    Timed_heat = 0x03
    Timed_cool = 0x04


class BoostResult(t.enum8):
    """Outcome of the last boost request / why the last boost ended (0x0006)."""

    None_ = 0x00
    Started = 0x01
    Target_reached = 0x02
    Time_expired = 0x03
    Cancelled = 0x04
    Mode_changed = 0x05
    Stopped_by_failsafe = 0x06
    Temperature_limit = 0x07
    Rejected_invalid = 0x10
    Rejected_sensor_fault = 0x11
    Rejected_already_at_target = 0x12
    Rejected_no_direction = 0x13


class ScheduleState(t.enum8):
    """Which program period is active (attribute 0x002A)."""

    No_clock_holding_day = 0x00
    Day = 0x01
    Night = 0x02
    Day_morning_boost = 0x03


class ConnieBoostCluster(CustomCluster):
    """Manufacturer-specific boost control cluster."""

    cluster_id = BOOST_CLUSTER_ID
    name = "Connie Boost"
    ep_attribute = "connie_boost"
    manufacturer_id_override = MANUF_CODE

    class AttributeDefs(BaseAttributeDefs):
        boost_setpoint = ZCLAttributeDef(id=0x0000, type=t.int16s, is_manufacturer_specific=True)
        boost_duration = ZCLAttributeDef(id=0x0001, type=t.uint16_t, is_manufacturer_specific=True)
        boost_mode = ZCLAttributeDef(id=0x0002, type=BoostMode, is_manufacturer_specific=True)
        boost_command = ZCLAttributeDef(id=0x0003, type=BoostCommand, is_manufacturer_specific=True)
        boost_state = ZCLAttributeDef(id=0x0004, type=BoostState, access="rp", is_manufacturer_specific=True)
        boost_remaining = ZCLAttributeDef(id=0x0005, type=t.uint16_t, access="rp", is_manufacturer_specific=True)
        boost_last_result = ZCLAttributeDef(id=0x0006, type=BoostResult, access="rp", is_manufacturer_specific=True)
        # Temperature swing (hysteresis), 0.01 °C each side of the setpoint, 10..100.
        temperature_swing = ZCLAttributeDef(id=0x0010, type=t.uint16_t, is_manufacturer_specific=True)
        # Program schedule. Start times: minutes after local midnight. Temps: 0.01 °C.
        day_cool_start = ZCLAttributeDef(id=0x0020, type=t.uint16_t, is_manufacturer_specific=True)
        day_cool_temp = ZCLAttributeDef(id=0x0021, type=t.int16s, is_manufacturer_specific=True)
        night_cool_start = ZCLAttributeDef(id=0x0022, type=t.uint16_t, is_manufacturer_specific=True)
        night_cool_temp = ZCLAttributeDef(id=0x0023, type=t.int16s, is_manufacturer_specific=True)
        day_heat_start = ZCLAttributeDef(id=0x0024, type=t.uint16_t, is_manufacturer_specific=True)
        day_heat_temp = ZCLAttributeDef(id=0x0025, type=t.int16s, is_manufacturer_specific=True)
        night_heat_start = ZCLAttributeDef(id=0x0026, type=t.uint16_t, is_manufacturer_specific=True)
        night_heat_temp = ZCLAttributeDef(id=0x0027, type=t.int16s, is_manufacturer_specific=True)
        morning_boost_duration = ZCLAttributeDef(id=0x0028, type=t.uint16_t, is_manufacturer_specific=True)
        morning_boost_offset = ZCLAttributeDef(id=0x0029, type=t.int16s, is_manufacturer_specific=True)
        schedule_state = ZCLAttributeDef(id=0x002A, type=ScheduleState, access="rp", is_manufacturer_specific=True)


def _schedule(builder):
    """Number entities for the program schedule (CONFIG category)."""
    a = ConnieBoostCluster.AttributeDefs
    # Quirks v2 has no time-of-day entity, so start times are hours with 15-minute steps
    # (6.5 = 06:30). The firmware accepts any minute.
    for attr, name in (
        (a.day_heat_start, "Day heating start"),
        (a.night_heat_start, "Night heating start"),
        (a.day_cool_start, "Day cooling start"),
        (a.night_cool_start, "Night cooling start"),
    ):
        builder = builder.number(
            attr.name, ConnieBoostCluster.cluster_id,
            min_value=0.0, max_value=23.75, step=0.25, multiplier=1 / 60, unit=UnitOfTime.HOURS,
            mode="box", entity_type=EntityType.CONFIG, translation_key=attr.name, fallback_name=name,
        )
    for attr, name in (
        (a.day_heat_temp, "Day heating temperature"),
        (a.night_heat_temp, "Night heating temperature"),
        (a.day_cool_temp, "Day cooling temperature"),
        (a.night_cool_temp, "Night cooling temperature"),
    ):
        builder = builder.number(
            attr.name, ConnieBoostCluster.cluster_id,
            min_value=10.0, max_value=32.0, step=0.5, multiplier=0.01, unit=UnitOfTemperature.CELSIUS,
            device_class=NumberDeviceClass.TEMPERATURE, mode="box", entity_type=EntityType.CONFIG,
            translation_key=attr.name, fallback_name=name,
        )
    builder = builder.number(
        a.morning_boost_duration.name, ConnieBoostCluster.cluster_id,
        min_value=0, max_value=240, step=15, unit=UnitOfTime.MINUTES, device_class=NumberDeviceClass.DURATION,
        mode="box", entity_type=EntityType.CONFIG, translation_key="morning_boost_duration",
        fallback_name="Morning heating boost duration",
    )
    # a temperature *difference*: no temperature device_class (see temperature_swing)
    builder = builder.number(
        a.morning_boost_offset.name, ConnieBoostCluster.cluster_id,
        min_value=0.0, max_value=3.0, step=0.1, multiplier=0.01, unit=UnitOfTemperature.CELSIUS,
        mode="box", entity_type=EntityType.CONFIG, translation_key="morning_boost_offset",
        fallback_name="Morning heating boost offset",
    )
    return builder.enum(
        a.schedule_state.name, ScheduleState, ConnieBoostCluster.cluster_id,
        entity_platform=EntityPlatform.SENSOR, entity_type=EntityType.STANDARD,
        translation_key="schedule_state", fallback_name="Schedule period",
    )


QUIRK_ENTRY = (
    _schedule(QuirkBuilder(MANUFACTURER, MODEL).adds(ConnieBoostCluster))
    # --- settings for the next boost ---
    .number(
        ConnieBoostCluster.AttributeDefs.boost_setpoint.name,
        ConnieBoostCluster.cluster_id,
        min_value=10.0,
        max_value=32.0,
        step=0.5,
        multiplier=0.01,                      # attribute is in 0.01 °C
        unit=UnitOfTemperature.CELSIUS,       # HA converts to °F for display
        device_class=NumberDeviceClass.TEMPERATURE,
        mode="box",
        entity_type=EntityType.STANDARD,
        translation_key="boost_setpoint",
        fallback_name="Boost target temperature",
    )
    .number(
        ConnieBoostCluster.AttributeDefs.boost_duration.name,
        ConnieBoostCluster.cluster_id,
        min_value=1,
        max_value=120,
        step=1,
        unit=UnitOfTime.MINUTES,
        device_class=NumberDeviceClass.DURATION,
        mode="slider",
        entity_type=EntityType.STANDARD,
        translation_key="boost_duration",
        fallback_name="Boost duration",
    )
    .enum(
        ConnieBoostCluster.AttributeDefs.boost_mode.name,
        BoostMode,
        ConnieBoostCluster.cluster_id,
        entity_type=EntityType.STANDARD,
        translation_key="boost_mode",
        fallback_name="Boost mode",
    )
    # --- control tuning ---
    # Neither HA's climate entity nor ZHA has a hysteresis/swing concept (HA "swing
    # mode" is louver swing), so this is a plain number entity. It is a temperature
    # *difference*: no temperature device_class, otherwise HA would convert 0.5 °C
    # to 32.9 °F on imperial systems.
    .number(
        ConnieBoostCluster.AttributeDefs.temperature_swing.name,
        ConnieBoostCluster.cluster_id,
        min_value=0.1,
        max_value=1.0,
        step=0.1,
        multiplier=0.01,                      # attribute is in 0.01 °C
        unit=UnitOfTemperature.CELSIUS,
        mode="slider",
        entity_type=EntityType.CONFIG,
        translation_key="temperature_swing",
        fallback_name="Temperature swing",
    )
    # --- triggers ---
    .write_attr_button(
        ConnieBoostCluster.AttributeDefs.boost_command.name,
        BoostCommand.Start_to_target,
        ConnieBoostCluster.cluster_id,
        entity_type=EntityType.STANDARD,
        unique_id_suffix="boost_start_target",
        translation_key="boost_start_target",
        fallback_name="Start boost to target",
    )
    .write_attr_button(
        ConnieBoostCluster.AttributeDefs.boost_command.name,
        BoostCommand.Start_timed,
        ConnieBoostCluster.cluster_id,
        entity_type=EntityType.STANDARD,
        unique_id_suffix="boost_start_timed",
        translation_key="boost_start_timed",
        fallback_name="Start timed boost",
    )
    .write_attr_button(
        ConnieBoostCluster.AttributeDefs.boost_command.name,
        BoostCommand.Cancel,
        ConnieBoostCluster.cluster_id,
        entity_type=EntityType.STANDARD,
        unique_id_suffix="boost_cancel",
        translation_key="boost_cancel",
        fallback_name="Cancel boost",
    )
    # --- status (the device reports these on change) ---
    .enum(
        ConnieBoostCluster.AttributeDefs.boost_state.name,
        BoostState,
        ConnieBoostCluster.cluster_id,
        entity_platform=EntityPlatform.SENSOR,
        entity_type=EntityType.STANDARD,
        translation_key="boost_state",
        fallback_name="Boost",
    )
    .binary_sensor(
        ConnieBoostCluster.AttributeDefs.boost_state.name,
        ConnieBoostCluster.cluster_id,
        device_class=BinarySensorDeviceClass.RUNNING,
        attribute_converter=lambda v: v != BoostState.Off,
        entity_type=EntityType.STANDARD,
        unique_id_suffix="boost_active",
        translation_key="boost_active",
        fallback_name="Boost active",
    )
    .sensor(
        ConnieBoostCluster.AttributeDefs.boost_remaining.name,
        ConnieBoostCluster.cluster_id,
        unit=UnitOfTime.MINUTES,
        device_class=SensorDeviceClass.DURATION,
        suggested_display_precision=0,
        entity_type=EntityType.STANDARD,
        translation_key="boost_remaining",
        fallback_name="Boost time remaining",
    )
    .enum(
        ConnieBoostCluster.AttributeDefs.boost_last_result.name,
        BoostResult,
        ConnieBoostCluster.cluster_id,
        entity_platform=EntityPlatform.SENSOR,
        entity_type=EntityType.DIAGNOSTIC,
        translation_key="boost_last_result",
        fallback_name="Last boost result",
    )
    .add_to_registry()
)
