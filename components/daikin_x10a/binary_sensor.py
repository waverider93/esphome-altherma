from esphome.components import binary_sensor
import esphome.config_validation as cv

from . import VALUE_SCHEMA, default_name, final_validate_value, register_value

DEPENDENCIES = ["daikin_x10a"]

CONFIG_SCHEMA = cv.All(default_name, binary_sensor.binary_sensor_schema().extend(VALUE_SCHEMA))
FINAL_VALIDATE_SCHEMA = final_validate_value("binary_sensor")


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await register_value(var, config, "add_binary_sensor")
