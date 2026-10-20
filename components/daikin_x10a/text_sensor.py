from esphome.components import text_sensor

from . import VALUE_SCHEMA, final_validate_value, register_value

DEPENDENCIES = ["daikin_x10a"]

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(VALUE_SCHEMA)
FINAL_VALIDATE_SCHEMA = final_validate_value("text_sensor")


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await register_value(var, config, "add_text_sensor")
