import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT

from . import CONF_ALTHERMA_CONTROL_ID, AlthermaControl

DEPENDENCIES = ["altherma_control"]

# type: (setter, unit, decimals, device class)
TYPES = {
    "deficit": ("set_deficit_sensor", "kWh", 2, None),
    "ua": ("set_ua_sensor", "W/K", 0, None),
    "heat_loss": ("set_loss_sensor", "kW", 2, DEVICE_CLASS_POWER),
    "heat_delivered": ("set_heat_sensor", "kW", 2, DEVICE_CLASS_POWER),
    "calibration_left": ("set_calibration_sensor", "h", 1, None),
}

CONFIG_SCHEMA = sensor.sensor_schema(state_class=STATE_CLASS_MEASUREMENT).extend(
    {
        cv.GenerateID(CONF_ALTHERMA_CONTROL_ID): cv.use_id(AlthermaControl),
        cv.Required(CONF_TYPE): cv.one_of(*TYPES, lower=True),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ALTHERMA_CONTROL_ID])
    setter, unit, decimals, device_class = TYPES[config[CONF_TYPE]]
    config = dict(config)
    config.setdefault("unit_of_measurement", unit)
    config.setdefault("accuracy_decimals", decimals)
    if device_class:
        config.setdefault("device_class", sensor.validate_device_class(device_class))
    var = await sensor.new_sensor(config)
    cg.add(getattr(parent, setter)(var))
