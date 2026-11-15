#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
  namespace tapo_smart {
    typedef enum {
      no_error = 0,
      timeout = -1,
      bad_crc = -2,
      bad_xor = -3,
      bad_compare = -4,
      not_inited = -5,
      other = -6
    } error_t;
    
    class TapoSmart : public PollingComponent, public uart::UARTDevice, public output::FloatOutput {
      public:
        /* PollingComponent methods - start */
        void setup() override;
        void loop() override;
        void dump_config() override;
        void update() override;
        /* PollingComponent methods - end */
        
        /* UARTDevice methods - start */
        /* UARTDevice methods - end */
        
        /* FloatOutput methods - start */
        void write_state(float state) override;
        /* FloatOutput methods - end */
        
        /* Custom methods - start */
        float get_temperature();
        uint8_t get_errors();
        /* Custom methods - end */
        
      protected:
        uint16_t calc_crc16_(const uint8_t *data, size_t len);
        uint8_t calc_xor_(const uint8_t *data, size_t len);
        void update_crc16_(uint8_t *msg, size_t len);
        void update_xor_(uint8_t *msg, size_t len);
        void process_message_(const uint8_t *msg, size_t len);
        error_t flush_rx_(int8_t timeout_ms);
        error_t message_rx_(uint8_t *rx_msg, uint8_t rx_msg_len, uint16_t timeout_ms);
        error_t message_tx_(uint8_t *tx_msg, uint8_t tx_msg_len, bool update_xor, bool update_crc);
        error_t message_compare_(uint8_t *rx_msg, uint8_t rx_msg_len, uint8_t *exp_msg, uint8_t *msg_mask);
        error_t set_dimmer_(float dimmer_val);
        error_t get_status_();
        void bad_state_check_();
        
        bool init_complete_{false};
        uint32_t status_ts_prev_{0};
        float dimmer_val_target_{0.0};
        float dimmer_val_handled_{0.0};
        uint16_t temperature_{0};
        uint8_t error_count_{0};
        HighFrequencyLoopRequester high_freq_;
    };
  }  // namespace tapo_smart
}  // namespace esphome