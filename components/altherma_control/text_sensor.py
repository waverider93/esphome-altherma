import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from . import CONF_ALTHERMA_CONTROL_ID, AlthermaControl

DEPENDENCIES = ["altherma_control"]

# Why the controller does what it does ("deficit reached", "minimum pause", ...)
CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {cv.GenerateID(CONF_ALTHERMA_CONTROL_ID): cv.use_id(AlthermaControl)}
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ALTHERMA_CONTROL_ID])
    var = await text_sensor.new_text_sensor(config)
    cg.add(parent.set_reason_sensor(var))
