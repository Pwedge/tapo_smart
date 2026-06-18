import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ID
from esphome.components import uart
from esphome.components import output

DEPENDENCIES = ['uart']

tapo_smart_ns = cg.esphome_ns.namespace('tapo_smart')
TapoSmart = tapo_smart_ns.class_('TapoSmart', cg.PollingComponent, uart.UARTDevice, output.FloatOutput)

CONFIG_SCHEMA = (
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(TapoSmart),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await output.register_output(var, config)
