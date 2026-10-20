import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]

MULTI_CONF = True

CONF_BMS_COUNT = "bms_count"
CONF_ADDRESS_BASE = "address_base"
CONF_UPDATE_INTERVAL = "update_interval"
CONF_SEPLOS_PARSER_ID = "seplos_parser_id"

seplos_parser_ns = cg.esphome_ns.namespace("seplos_parser")

SeplosParser = seplos_parser_ns.class_("SeplosParser", cg.Component)

HUB_CHILD_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SEPLOS_PARSER_ID): cv.use_id(SeplosParser),
    }
)

CONFIG_SCHEMA = ( 
    cv.Schema({
        cv.GenerateID(): cv.declare_id(SeplosParser),
        cv.Optional(CONF_UPDATE_INTERVAL, default=5): cv.int_,
        cv.Optional(CONF_BMS_COUNT, default=1): cv.int_,
        cv.Optional(CONF_ADDRESS_BASE, default=1): cv.int_range(min=0, max=1),
        cv.Optional("replay_only", default=False): cv.boolean,
        cv.Optional("transaction_tracking", default=False): cv.boolean,
        cv.Optional("status_profile", default="strict"): cv.one_of(
            "strict", "v3_sp18b_fw15", "v3_sp18b_fw15_balancing"
        ),
    })
        .extend(cv.COMPONENT_SCHEMA)
        .extend(uart.UART_DEVICE_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
#    uart_device = await cg.get_variable(config[CONF_UART_ID])
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_bms_count(config[CONF_BMS_COUNT]))
    cg.add(var.set_address_base(config[CONF_ADDRESS_BASE]))
    cg.add(var.set_replay_only(config["replay_only"]))
    cg.add(var.set_transaction_tracking(config["transaction_tracking"]))
    cg.add(var.set_status_profile(config["status_profile"]))
