import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_TYPE, ENTITY_CATEGORY_CONFIG

from . import CONF_ALTHERMA_CONTROL_ID, AlthermaControl, Setting, altherma_control_ns

DEPENDENCIES = ["altherma_control"]

SettingNumber = altherma_control_ns.class_("SettingNumber", number.Number)

# type: (enum, min, max, step, unit)
TYPES = {
    "start": ("START", 0.5, 15, 0.5, "kWh"),
    "stop": ("STOP", -5, 5, 0.5, "kWh"),
    "min_run": ("MIN_RUN", 0, 6, 0.25, "h"),
    "min_pause": ("MIN_PAUSE", 0, 6, 0.25, "h"),
    "room_gain": ("ROOM_GAIN", 0, 20, 0.5, "kWh/K"),
    "ua": ("UA", 20, 400, 1, "W/K"),
}

CONFIG_SCHEMA = number.number_schema(SettingNumber, entity_category=ENTITY_CATEGORY_CONFIG).extend(
    {
        cv.GenerateID(CONF_ALTHERMA_CONTROL_ID): cv.use_id(AlthermaControl),
        cv.Required(CONF_TYPE): cv.one_of(*TYPES, lower=True),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ALTHERMA_CONTROL_ID])
    setting, low, high, step, unit = TYPES[config[CONF_TYPE]]
    config = dict(config)
    config.setdefault("unit_of_measurement", unit)
    var = cg.new_Pvariable(config["id"], parent, getattr(Setting, setting))
    await number.register_number(var, config, min_value=low, max_value=high, step=step)
    cg.add(parent.add_setting_number(getattr(Setting, setting), var))
