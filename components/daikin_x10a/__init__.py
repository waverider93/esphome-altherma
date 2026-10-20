"""ESPHome component for Daikin Altherma heat pumps on the X10A serial port.

Port of ESPAltherma (https://github.com/raomin/ESPAltherma, MIT License, Copyright (c) 2020 Raomin).
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_NAME
from esphome.core import CORE

from . import catalog

CODEOWNERS = ["@waverider93"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

CONF_DAIKIN_X10A_ID = "daikin_x10a_id"
CONF_MODEL = "model"
CONF_LANGUAGE = "language"
CONF_DETECT = "detect"
CONF_LAYOUT_CHECK = "layout_check"
CONF_DIAGRAM = "diagram"
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

AUTO = "auto"
REFRIGERANTS = {"AUTO": 0, "R410A": 801, "R32": 802, "R22": 803, "R134A": 804}


def _validate_model(value):
    value = cv.string_strict(value)
    if value.lower() == AUTO:
        return AUTO
    try:
        return catalog.find_model(value).name
    except ValueError as err:
        raise cv.Invalid(f"{err}. Known models: {', '.join(catalog.model_names())}") from err


def _validate_hub(config):
    if config[CONF_MODEL] == AUTO and not config[CONF_DETECT]:
        raise cv.Invalid("model: auto needs detect: true")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DaikinX10A),
            # Definition name, or auto: detected from the heat pump after boot
            cv.Optional(CONF_MODEL, default=AUTO): _validate_model,
            # Language of the entity names derived from the labels, and of the diagram page
            cv.Optional(CONF_LANGUAGE, default="en"): cv.one_of(*catalog.LANGUAGES, lower=True),
            # Reads every known register once after boot to detect (or check) the model
            cv.Optional(CONF_DETECT, default=True): cv.boolean,
            # Corrects registers whose layout does not match the unit (as ESPAltherma's layoutfix.h)
            cv.Optional(CONF_LAYOUT_CHECK, default=True): cv.boolean,
            # Serves a drawing of the installation on /altherma (needs web_server)
            cv.Optional(CONF_DIAGRAM, default=False): cv.boolean,
            # Only for raw register values without a model; a model sets the protocol itself
            cv.Optional(CONF_PROTOCOL, default="I"): cv.one_of("I", "S", upper=True),
            cv.Optional(CONF_REFRIGERANT, default="AUTO"): cv.enum(REFRIGERANTS, upper=True),
            cv.Optional(CONF_LOG_RAW, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("30s"))
    .extend(uart.UART_DEVICE_SCHEMA),
    _validate_hub,
)


def _final_validate_hub(config):
    uart.final_validate_device_schema(
        "daikin_x10a", baud_rate=9600, require_tx=True, require_rx=True, parity="EVEN", stop_bits=1
    )(config)
    import esphome.final_validate as fv

    if config[CONF_DIAGRAM] and "web_server_base" not in fv.full_config.get():
        raise cv.Invalid("diagram: true needs web_server (or another component serving web pages)")
    return config


FINAL_VALIDATE_SCHEMA = _final_validate_hub


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    if config[CONF_MODEL] != AUTO:
        cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_language(config[CONF_LANGUAGE]))
    cg.add(var.set_detect(config[CONF_DETECT]))
    cg.add(var.set_layout_check(config[CONF_LAYOUT_CHECK]))
    cg.add(var.set_protocol(cg.RawExpression(f"'{config[CONF_PROTOCOL]}'")))
    cg.add(var.set_refrigerant(config[CONF_REFRIGERANT]))
    cg.add(var.set_log_raw(config[CONF_LOG_RAW]))
    if config[CONF_DIAGRAM]:
        cg.add_define("USE_DAIKIN_X10A_DIAGRAM")
        base = await cg.get_variable(CORE.config["web_server_base"][CONF_ID])
        cg.add(var.set_web_server_base(base))


# --- Shared by the sensor, binary_sensor and text_sensor platforms ---

VALUE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DAIKIN_X10A_ID): cv.use_id(DaikinX10A),
        # Label of the definition files, in English or in one of the translated languages
        cv.Optional(CONF_LABEL): cv.string_strict,
        cv.Optional(CONF_REGISTER): cv.hex_uint8_t,
        cv.Optional(CONF_OFFSET): cv.uint8_t,
        cv.Optional(CONF_CONVERSION): cv.int_range(min=100, max=999),
        cv.Optional(CONF_DATA_SIZE, default=1): cv.int_range(min=1, max=32),
    }
)


def _raw_hub(hub_id=None):
    """The hub config as written in the YAML (platform schemas run before the hub is validated)."""
    hubs = (CORE.raw_config or {}).get("daikin_x10a") or []
    if isinstance(hubs, dict):
        hubs = [hubs]
    for hub in hubs:
        if hub_id is None or str(hub.get(CONF_ID)) == str(hub_id):
            return hub
    return hubs[0] if hubs else {}


def default_name(config):
    """Entities with a label get the label's name in the hub language when the YAML gives no name."""
    if isinstance(config, dict) and CONF_LABEL in config and CONF_NAME not in config:
        hub = _raw_hub(config.get(CONF_DAIKIN_X10A_ID))
        en = catalog.english(str(config[CONF_LABEL]))
        if en is not None:
            config = dict(config)
            config[CONF_NAME] = catalog.translate(en, str(hub.get(CONF_LANGUAGE, "en")).lower())
    return config


def _hub_config(full_config, hub_id):
    for conf in full_config.get("daikin_x10a", []):
        if conf[CONF_ID] == hub_id:
            return conf
    raise cv.Invalid("daikin_x10a hub not found")


def resolve_value(config, full_config):
    """The catalog entries of a label (one per model that has it), or None for a raw register value."""
    if CONF_LABEL in config:
        hub = _hub_config(full_config, config[CONF_DAIKIN_X10A_ID])
        model = None if hub[CONF_MODEL] == AUTO else catalog.find_model(hub[CONF_MODEL])
        try:
            return catalog.find_label(model, config[CONF_LABEL], config.get(CONF_REGISTER))
        except ValueError as err:
            raise cv.Invalid(str(err), [CONF_LABEL]) from err
    for key in (CONF_REGISTER, CONF_OFFSET, CONF_CONVERSION):
        if key not in config:
            raise cv.Invalid(f"Set 'label' or all of register, offset, conversion (missing {key})")
    return None


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

        found = resolve_value(config, fv.full_config.get())
        convs = {e.conv for e in found} if found else {config[CONF_CONVERSION]}
        for conv in convs:
            if kind == "sensor" and not is_number_conversion(conv) and not is_bool_conversion(conv):
                raise cv.Invalid(f"Conversion {conv} gives a text, use text_sensor")
            if kind == "binary_sensor" and not is_bool_conversion(conv):
                raise cv.Invalid(f"Conversion {conv} gives no on/off state, use sensor or text_sensor")
        return config

    return validate


async def register_value(var, config, method):
    hub = await cg.get_variable(config[CONF_DAIKIN_X10A_ID])
    found = resolve_value(config, CORE.config)
    if found:
        # Resolved on the ESP against the model it reads (configured or detected, with layout corrections)
        cg.add(getattr(hub, method)(var, found[0].label, config.get(CONF_REGISTER, -1), 0, 0, 0, 0))
    else:
        cg.add(getattr(hub, method)(var, cg.nullptr, -1, config[CONF_REGISTER], config[CONF_OFFSET],
                                    config[CONF_CONVERSION], config[CONF_DATA_SIZE]))
