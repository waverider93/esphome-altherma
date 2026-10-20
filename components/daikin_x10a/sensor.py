from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_DEVICE_CLASS, CONF_STATE_CLASS, CONF_UNIT_OF_MEASUREMENT

from . import VALUE_SCHEMA, default_name, final_validate_value, register_value, resolve_value

DEPENDENCIES = ["daikin_x10a"]

CONFIG_SCHEMA = cv.All(default_name, sensor.sensor_schema(accuracy_decimals=1).extend(VALUE_SCHEMA))
FINAL_VALIDATE_SCHEMA = final_validate_value("sensor")


def _defaults(label, conv):
    """Unit and device class guessed from the ESPAltherma label, when the YAML sets none."""
    low = label.lower()
    if 401 <= conv <= 406 or "temp" in low or "(r1t)" in low or low.endswith("setpoint"):
        return "°C", "temperature"
    if "l/min" in low:
        return "L/min", None
    if "pressure" in low:
        return "bar", "pressure"
    if "current" in low or "(a)" in low:
        return "A", "current"
    if "(kw)" in low:
        return "kW", "power"
    if "(rps)" in low:
        return "rps", None
    return None, None


async def to_code(config):
    from esphome.core import CORE

    found = resolve_value(config, CORE.config)
    if found:
        unit, device_class = _defaults(found[0].label, found[0].conv)
        config = dict(config)
        if unit:
            config.setdefault(CONF_UNIT_OF_MEASUREMENT, unit)
            config.setdefault(CONF_STATE_CLASS, sensor.validate_state_class("measurement"))
        if device_class:
            config.setdefault(CONF_DEVICE_CLASS, sensor.validate_device_class(device_class))
    var = await sensor.new_sensor(config)
    await register_value(var, config, "add_sensor")
