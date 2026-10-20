from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ENTITY_CATEGORY, ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_DAIKIN_X10A_ID, VALUE_SCHEMA, default_name, final_validate_value, register_value

DEPENDENCIES = ["daikin_x10a"]

CONF_INFO = "info"
# What the component found out about the heat pump, instead of a value read from it
INFO = {
    "model": "set_model_sensor",  # the model read with (configured or detected)
    "detection": "set_detection_sensor",  # detected model, confidence and method
    "layout": "set_layout_sensor",  # registers read with the layout of another model
}

CONFIG_SCHEMA = cv.All(
    default_name,
    text_sensor.text_sensor_schema().extend(VALUE_SCHEMA).extend({cv.Optional(CONF_INFO): cv.one_of(*INFO, lower=True)}),
)


def _final_validate(config):
    if CONF_INFO in config:
        return config
    return final_validate_value("text_sensor")(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    import esphome.codegen as cg

    if CONF_INFO in config:
        config = dict(config)
        config.setdefault(CONF_ENTITY_CATEGORY, cv.entity_category(ENTITY_CATEGORY_DIAGNOSTIC))
        var = await text_sensor.new_text_sensor(config)
        hub = await cg.get_variable(config[CONF_DAIKIN_X10A_ID])
        cg.add(getattr(hub, INFO[config[CONF_INFO]])(var))
        return
    var = await text_sensor.new_text_sensor(config)
    await register_value(var, config, "add_text_sensor")
