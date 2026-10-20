"""Heat deficit controller on the ESP for a Daikin Altherma driven through its room thermostat input.

Uses only values of the heat pump (read by daikin_x10a): it runs without network and calibrates itself.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, switch
from esphome.const import CONF_ID

CODEOWNERS = ["@waverider93"]
DEPENDENCIES = ["sensor", "switch"]
MULTI_CONF = False

CONF_ALTHERMA_CONTROL_ID = "altherma_control_id"
CONF_ROOM = "room_temperature"
CONF_SETPOINT = "room_setpoint"
CONF_OUTSIDE = "outside_temperature"
CONF_FLOW = "flow"
CONF_LEAVING = "leaving_water_temperature"
CONF_INLET = "inlet_water_temperature"
CONF_HOT_WATER = "hot_water_valve"
CONF_HEAT_REQUEST = "heat_request"
CONF_COOL_REQUEST = "cool_request"
CONF_UA = "ua"
CONF_ROOM_LOW = "room_low"
CONF_ROOM_HIGH = "room_high"
CONF_CALIBRATION_WINDOW = "calibration_window"
CONF_DIAGRAM_ID = "diagram_id"

altherma_control_ns = cg.esphome_ns.namespace("altherma_control")
AlthermaControl = altherma_control_ns.class_("AlthermaControl", cg.PollingComponent)
Setting = altherma_control_ns.enum("Setting", is_class=True)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AlthermaControl),
        cv.Required(CONF_ROOM): cv.use_id(sensor.Sensor),
        cv.Required(CONF_SETPOINT): cv.use_id(sensor.Sensor),
        cv.Required(CONF_OUTSIDE): cv.use_id(sensor.Sensor),
        cv.Required(CONF_FLOW): cv.use_id(sensor.Sensor),
        cv.Required(CONF_LEAVING): cv.use_id(sensor.Sensor),
        cv.Required(CONF_INLET): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_HOT_WATER): cv.use_id(binary_sensor.BinarySensor),
        # The relay it switches (on = heat request), and the cool request it leaves alone while on
        cv.Required(CONF_HEAT_REQUEST): cv.use_id(switch.Switch),
        cv.Optional(CONF_COOL_REQUEST): cv.use_id(switch.Switch),
        # Start value of the heat loss coefficient, until it has learned its own
        cv.Optional(CONF_UA, default=90): cv.float_range(min=20, max=400),
        cv.Optional(CONF_ROOM_LOW, default=0.5): cv.float_range(min=0.1, max=3),
        cv.Optional(CONF_ROOM_HIGH, default=1.0): cv.float_range(min=0.1, max=3),
        cv.Optional(CONF_CALIBRATION_WINDOW, default="72h"): cv.positive_time_period_minutes,
        # daikin_x10a hub whose diagram page shows the controller with an explanation
        cv.Optional(CONF_DIAGRAM_ID): cv.use_id(cg.esphome_ns.namespace("daikin_x10a").class_("DaikinX10A")),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    for key, setter in (
        (CONF_ROOM, "set_room"),
        (CONF_SETPOINT, "set_setpoint"),
        (CONF_OUTSIDE, "set_outside"),
        (CONF_FLOW, "set_flow"),
        (CONF_LEAVING, "set_leaving"),
        (CONF_INLET, "set_inlet"),
        (CONF_HOT_WATER, "set_hot_water"),
        (CONF_HEAT_REQUEST, "set_heat_request"),
        (CONF_COOL_REQUEST, "set_cool_request"),
    ):
        if key in config:
            cg.add(getattr(var, setter)(await cg.get_variable(config[key])))
    cg.add(var.set_default_ua(config[CONF_UA]))
    cg.add(var.set_room_limits(config[CONF_ROOM_LOW], config[CONF_ROOM_HIGH]))
    cg.add(var.set_calibration_hours(config[CONF_CALIBRATION_WINDOW].total_minutes / 60.0))
    if CONF_DIAGRAM_ID in config:
        hub = await cg.get_variable(config[CONF_DIAGRAM_ID])
        cg.add(hub.set_control_json(cg.RawExpression(f"[]() -> std::string {{ return {var}->json(); }}")))
