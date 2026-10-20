"""ESPHome component for Daikin Altherma heat pumps on the X10A serial port.

Port of ESPAltherma (https://github.com/raomin/ESPAltherma, MIT License, Copyright (c) 2020 Raomin).
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

from . import definitions

CODEOWNERS = ["@waverider93"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

CONF_DAIKIN_X10A_ID = "daikin_x10a_id"
CONF_MODEL = "model"
CONF_PROTOCOL = "protocol"
CONF_REFRIGERANT = "refrigerant"
CONF_LOG_RAW = "log_raw"
CONF_LABEL = "label"
CONF_REGISTER = "register"
CONF_OFFSET = "offset"
CONF_CONVERSION = "conversion"
CONF_DATA_SIZE = "data_size"

daikin_x10a_ns = cg.esphome_ns.namespace("daikin_x10a")
DaikinX10A = daikin_x10a_ns.class_("DaikinX10A", cg.PollingComponent, uart.UARTDevice)

REFRIGERANTS = {"R410A": 801, "R32": 802, "R22": 803, "R134A": 804}


def _validate_model(value):
    value = cv.string_strict(value)
    try:
        definitions.load_model(value)
    except ValueError as err:
        raise cv.Invalid(f"{err}. Known models: {', '.join(definitions.model_names())}") from err
    return value


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DaikinX10A),
            cv.Optional(CONF_MODEL): _validate_model,
            cv.Optional(CONF_PROTOCOL, default="I"): cv.one_of("I", "S", upper=True),
            cv.Optional(CONF_REFRIGERANT, default="R32"): cv.enum(REFRIGERANTS, upper=True),
            cv.Optional(CONF_LOG_RAW, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("30s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "daikin_x10a", baud_rate=9600, require_tx=True, require_rx=True, parity="EVEN", stop_bits=1
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_protocol(cg.RawExpression(f"'{config[CONF_PROTOCOL]}'")))
    cg.add(var.set_refrigerant(config[CONF_REFRIGERANT]))
    cg.add(var.set_log_raw(config[CONF_LOG_RAW]))


# --- Shared by the sensor, binary_sensor and text_sensor platforms ---

VALUE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DAIKIN_X10A_ID): cv.use_id(DaikinX10A),
        cv.Optional(CONF_LABEL): cv.string_strict,
        cv.Optional(CONF_REGISTER): cv.hex_uint8_t,
        cv.Optional(CONF_OFFSET): cv.uint8_t,
        cv.Optional(CONF_CONVERSION): cv.int_range(min=100, max=999),
        cv.Optional(CONF_DATA_SIZE, default=1): cv.int_range(min=1, max=32),
    }
)


def _hub_config(full_config, hub_id):
    for conf in full_config.get("daikin_x10a", []):
        if conf[CONF_ID] == hub_id:
            return conf
    raise cv.Invalid("daikin_x10a hub not found")


def resolve_value(config, full_config):
    """Returns (register, offset, conversion, size, label) for a value from its label or raw fields."""
    if CONF_LABEL in config:
        hub = _hub_config(full_config, config[CONF_DAIKIN_X10A_ID])
        if CONF_MODEL not in hub:
            raise cv.Invalid("'label' needs 'model' on the daikin_x10a hub", [CONF_LABEL])
        try:
            d = definitions.find_label(hub[CONF_MODEL], config[CONF_LABEL], config.get(CONF_REGISTER))
        except ValueError as err:
            raise cv.Invalid(str(err), [CONF_LABEL]) from err
        return d.registry, d.offset, d.conv, d.size, d.label
    for key in (CONF_REGISTER, CONF_OFFSET, CONF_CONVERSION):
        if key not in config:
            raise cv.Invalid(f"Set 'label' or all of register, offset, conversion (missing {key})")
    return config[CONF_REGISTER], config[CONF_OFFSET], config[CONF_CONVERSION], config[CONF_DATA_SIZE], None


def is_bool_conversion(conv):
    return 300 <= conv <= 307 or conv == 200


def is_number_conversion(conv):
    return (101 <= conv <= 119) or (151 <= conv <= 165) or conv in (211, 310, 311, 312, 317, 323, 336) or (
        401 <= conv <= 406
    )


def final_validate_value(kind):
    """Checks at validation time that the label exists and its conversion fits the entity kind."""

    def validate(config):
        import esphome.final_validate as fv

        reg, offset, conv, size, label = resolve_value(config, fv.full_config.get())
        if kind == "sensor" and not is_number_conversion(conv) and not is_bool_conversion(conv):
            raise cv.Invalid(f"Conversion {conv} gives a text, use text_sensor")
        if kind == "binary_sensor" and not is_bool_conversion(conv):
            raise cv.Invalid(f"Conversion {conv} gives no on/off state, use sensor or text_sensor")
        return config

    return validate


async def register_value(var, config, method):
    from esphome.core import CORE

    hub = await cg.get_variable(config[CONF_DAIKIN_X10A_ID])
    reg, offset, conv, size, _ = resolve_value(config, CORE.config)
    cg.add(getattr(hub, method)(var, reg, offset, conv, size))
