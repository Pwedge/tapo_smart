#include "tapo_smart.h"
#include "esphome/core/application.h"


 /*
 *    1.8406 [RTL TX] 0x5a 0x5a 0x00 0x0a 0x00 0x04 0x01 0x00 0x00 0x00 0x37 0x1a             // RTL status request
 *    0.0014 [CMS TX] 0x5a 0x5a 0x01 0x0a 0x00 0x06 0x01 0x00 0x00 0x1c 0x7d 0x00 0x25 0xd8   // CMS status response
 *    2.0495 [RTL TX] 0x5a 0x5a 0x00 0x0a 0x00 0x04 0x01 0x00 0x00 0x00 0x37 0x1a             // RTL status request
 *    0.0015 [CMS TX] 0x5a 0x5a 0x01 0x0a 0x00 0x06 0x01 0x00 0x00 0x1c 0x7e 0x00 0xd5 0xd8   // CMS status response
 */

namespace esphome {
  namespace tapo_smart {
    static const char *TAG = "tapo_smart.component";
    static const uint8_t preamble[2] = {0x5a, 0x5a};
    
    typedef struct frame_header_s {
      uint8_t preamble[2];  /* Should always be 0x5a 0x5a */     
      uint8_t source;       /* 0=RTL8710CF, 1=CMS8S5880, 2=Broadcast */
      uint8_t type;         /* 8=Hello, 10=Status, 11=Config */
      uint8_t padding;      /* Always 0x00 */
      uint8_t size;         /* Remaining bytes, after this (not including CRC bytes) */
    } frame_header_t;
    
    typedef struct frame_footer_s {
      uint8_t crc16[2];     /* All previous bytes except for preamble bytes.  https://www.crccalc.com/?crc=123456789&method=CRC-16/MODBUS&datatype=hex&outtype=hex (Poly=0x8005, Init=0xffff, RefIn=true, RefOut=true, XorOut=0x0) */
    } frame_footer_t;
    
    
    uint16_t TapoSmart::calc_crc16_(const uint8_t *data, size_t len) {
      const uint16_t  poly = 0xa001;
      const uint16_t  init = 0xffff;
      
      return crc16(data, len, init, poly, false, false);
    }
    
    uint8_t TapoSmart::calc_xor_(const uint8_t *data, size_t len) {
      const uint8_t xor_check_val = 0x33;
      
      uint8_t       xor_byte = xor_check_val;
      int8_t        i;
      
      for (i = 0; i < len; i ++) {
        xor_byte ^= data[i];
      }
      
      return xor_byte;
    }
    
    void TapoSmart::update_crc16_(uint8_t *msg, size_t len) {
      uint16_t  crc16_val;
      
      crc16_val = this->calc_crc16_(msg + 2, len - 4);
      msg[len - 2] = (crc16_val >> 8) & 0xff;
      msg[len - 1] = (crc16_val >> 0) & 0xff;
    }
    
    void TapoSmart::update_xor_(uint8_t *msg, size_t len) {
      uint8_t xor_val;
      
      xor_val = this->calc_xor_(msg + (len - 6), 3);
      msg[len - 3] = xor_val;
    }
    
    error_t TapoSmart::flush_rx_(int8_t timeout_ms)
    {
      uint32_t      start_ts;
      uint8_t       rx_discard;
      const int8_t  timeout_ms_def = 10;
      
      if (timeout_ms <= 0) {
        timeout_ms = timeout_ms_def;
      }
      
      ESP_LOGV(TAG, "Waiting up to %dms to flush receive FIFO", timeout_ms);
      start_ts = millis();
      
      while (this->available()) {
        this->read_array(&rx_discard, 1);
        
        if (start_ts + timeout_ms < millis()) {
          ESP_LOGW(TAG, "Timeout during receive FIFO flush");
          return timeout;
        }
      }
      
      return no_error;
    }
    
    error_t TapoSmart::message_rx_(uint8_t *rx_msg, uint8_t rx_msg_len, uint16_t timeout_ms)
    {
      const int8_t  timeout_ms_def = 5;
      
      uint32_t      start_ts;
      uint8_t       rx_cnt;
      uint8_t       xor_val;
      uint16_t      crc16_val;
      
      if (timeout_ms <= 0) {
        timeout_ms = timeout_ms_def;
      }
      
      ESP_LOGV(TAG, "Waiting up to %dms to receive %d bytes", timeout_ms, rx_msg_len);
      start_ts = millis();
      
      /* Wait for premable sync, then receive rest of message */
      rx_cnt = 0;
      while (rx_cnt != rx_msg_len) {
        if (this->available()) {
          this->read_array(rx_msg + rx_cnt++, 1);
          
          if (rx_cnt <= 2 && memcmp(rx_msg, preamble, rx_cnt)) {
            ESP_LOGV(TAG, "Waiting for preamble sync");
            rx_cnt = 0;
            continue;
          }
        } else if (start_ts + timeout_ms < millis()) {
          ESP_LOGW(TAG, "Timeout while waiting to receive rest of message");
          this->error_count_++;
          return timeout;
        }
      }
      
      ESP_LOGV(TAG, "Full message received: %s", format_hex(rx_msg, rx_msg_len).c_str());
      
      crc16_val = this->calc_crc16_(rx_msg + 2, rx_msg_len - 4);
      if (crc16_val != ((rx_msg[rx_msg_len - 2] << 8) | rx_msg[rx_msg_len - 1])) {
        ESP_LOGW(TAG, "Receive CRC check failed");
        ESP_LOGV(TAG, "Received message: %s (%x)", format_hex(rx_msg, rx_msg_len).c_str(), crc16_val);
        this->error_count_++;
        return bad_crc;
      }
      
      return no_error;
    }
    
    error_t TapoSmart::message_tx_(uint8_t *tx_msg, uint8_t tx_msg_len, bool update_xor, bool update_crc)
    {
      if (update_xor) {
        ESP_LOGV(TAG, "Updating XOR check byte for transmit message");
        this->update_xor_(tx_msg, tx_msg_len);
      }
      
      if (update_crc) {
        ESP_LOGV(TAG, "Updating CRC16 for transmit message");
        this->update_crc16_(tx_msg, tx_msg_len);
      }
      
      ESP_LOGV(TAG, "Transmitting message: %s", format_hex(tx_msg, tx_msg_len).c_str());
      this->write_array(tx_msg, tx_msg_len);
      
      return no_error;
    }
    
    error_t TapoSmart::message_compare_(uint8_t *rx_msg, uint8_t rx_msg_len, uint8_t *exp_msg, uint8_t *msg_mask)
    {
      uint8_t mask;
      uint8_t i;
      
      for (i = 0; i < rx_msg_len; i++) {
        mask = msg_mask ? msg_mask[i] : 0xff;
        if ((rx_msg[i] & mask) != (exp_msg[i] & mask)) {
          return bad_compare;
        }
      }
      
      return no_error;
    }
    
    error_t TapoSmart::set_dimmer_(float dimmer_val)
    {
      const uint16_t  pwm_clamp = 128;
      
      float           range;
      uint8_t         num_led;
      uint16_t        pwm_val;
      uint8_t         dim_msg[] = {0x5a, 0x5a, 0x00, 0x0b, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
      uint8_t         led_msg[] = {0x5a, 0x5a, 0x00, 0x0b, 0x00, 0x04, 0xf7, 0x01, 0x00, 0x00, 0x00, 0x00};
      uint8_t         rx_msg[14];
      error_t         err_val;
      
      ESP_LOGV(TAG, "Setting dimmer to %f", dimmer_val);
      
      if (dimmer_val == 0.0) {
        num_led = 0;
        pwm_val = 8191;
      } else {
        range = this->max_power_ - this->min_power_;
        if (range <= 0.0) {
          num_led = 7;
          pwm_val = 0;
        } else {
          num_led = ((dimmer_val - this->min_power_) / (range / 6.0f)) + 1;
          pwm_val = 8191 - (8191.0f * dimmer_val);
        }
        
        if (pwm_val < pwm_clamp) {
          ESP_LOGV(TAG, "Clamped PWM value to %u", pwm_clamp);
          pwm_val = pwm_clamp;
        }
      }
      
      dim_msg[7] = (pwm_val >> 8) & 0xff;
      dim_msg[8] = (pwm_val >> 0) & 0xff;
      this->message_tx_(dim_msg, sizeof(dim_msg), true, true);
      err_val = this->message_rx_(rx_msg, sizeof(rx_msg), 1000);
      if (err_val != no_error) {
        ESP_LOGE(TAG, "Received unexpected response to dimmer set message (%d)", err_val);
        return err_val;
      }
      delayMicroseconds(5000);
      
      led_msg[8] = num_led;
      this->message_tx_(led_msg, sizeof(led_msg), true, true);
      err_val = this->message_rx_(rx_msg, sizeof(rx_msg), 1000);
      if (err_val != no_error) {
        ESP_LOGE(TAG, "Received unexpected response to LED set message (%d)", err_val);
        return err_val;
      }
      delayMicroseconds(50000);
      
      this->dimmer_val_handled_ = dimmer_val;
      
      return no_error;
    }
    
    error_t TapoSmart::get_status_()
    {
      uint8_t status_req[] = {0x5a, 0x5a, 0x00, 0x0a, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x37, 0x1a};
      uint8_t rx_msg[14];
      error_t err_val;
      
      ESP_LOGV(TAG, "Requesting a status update from CMS");
      
      this->message_tx_(status_req, sizeof(status_req), false, false);
      err_val = this->message_rx_(rx_msg, sizeof(rx_msg), 0);
      if (err_val != no_error) {
        ESP_LOGE(TAG, "Received unexpected response to status request message (%d)", err_val);
        return err_val;
      }
      
      this->temperature_ = (rx_msg[9] << 8) | rx_msg[10];
      ESP_LOGV(TAG, "Temperature is now 0x%04x", this->temperature_);
      
      return no_error;
    }
    
    void TapoSmart::bad_state_check_()
    {
      /* 
       * Testing shows that after some random time the RTL starts to unreliably
       * receive data on the UART.  Not sure if the issue is in ESPHome,
       * libretiny, or elsewhere.  Monitoring the heap for an extended amount
       * of time doesn't show anything bad happening there.  As of libretiny 
       * v1.9.1 the page at:
       *   https://docs.libretiny.eu/docs/status/supported/#feature-support
       * shows the stability of the RTL8720C family as "2/5", so suspecting 
       * that there is something very low-level that is causing the bad 
       * behavior.  Unfortunately, for now I am just going to swing the heavy
       * hammer :( 
       */
      
      if (this->error_count_ >= 1) {
        ESP_LOGE(TAG, "Too many errors seen.  Rebooting :(");
        App.safe_reboot();
      }
    }
    
    void TapoSmart::setup() {
      /* 
       * Startup sequence capture:
       *    0.0000 [CMS TX] 0x5a 0x5a 0x02 0x08 0x00 0x06 0x43 0x4d 0x53 0x02 0x00 0x00 0x74 0x42   // CMS hello (broadcast)
       *
       *    0.0108 [RTL TX] 0x5a 0x5a 0x00 0x08 0x00 0x01 0x00 0xf0 0x27                            // RTL hello (unicast)
       *    0.0014 [CMS TX] 0x5a 0x5a 0x01 0x08 0x00 0x06 0x43 0x4d 0x53 0x02 0x00 0x00 0x7b 0xb2   // CMS hello (unicast)
       *
       *    0.1082 [RTL TX] 0x5a 0x5a 0x00 0x0b 0x00 0x04 0xf2 0x1b 0x7a 0xa0 0xac 0x6a             // RTL set dimmer range?
       *    0.0013 [CMS TX] 0x5a 0x5a 0x01 0x0b 0x00 0x06 0x72 0x1b 0x7a 0x20 0x00 0x00 0xf0 0x42   // CMS echo dimmer range
       *
       *    0.0328 [RTL TX] 0x5a 0x5a 0x00 0x0b 0x00 0x04 0x01 0x1b 0x7b 0x52 0xfd 0xd9             // RTL set dimmer value
       *    0.0014 [CMS TX] 0x5a 0x5a 0x01 0x0b 0x00 0x06 0x01 0x1b 0x7b 0x52 0x00 0x00 0x14 0xe8   // CMS echo minimum dimmer value
       *
       *    0.0336 [RTL TX] 0x5a 0x5a 0x00 0x0b 0x00 0x04 0xf7 0x80 0x07 0x43 0x56 0x7b             // RTL set number of LEDs?
       *    0.0016 [CMS TX] 0x5a 0x5a 0x01 0x0b 0x00 0x06 0x77 0x00 0x07 0x43 0x00 0x00 0xd5 0xce   // CMS echo configuration complete
       */
      
      uint8_t rx_msg[14];
      error_t err_val;
      
      this->init_complete_ = false;
      
      /* Flush the FIFO */
      this->flush_rx_(0);
      
      /* Check for broadcast announce from CMS */
      err_val = this->message_rx_(rx_msg, 14, 100);
      if (err_val == timeout) {
        ESP_LOGV(TAG, "Possible reboot of RTL.  CMS may already be initialized.  Checking for a status response");
        uint8_t tx_msg[] = {0x5a, 0x5a, 0x00, 0x0a, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x37, 0x1a};
        this->message_tx_(tx_msg, sizeof(tx_msg), false, false);
        
        err_val = this->message_rx_(rx_msg, 14, 0);
        if (err_val == timeout) {
          ESP_LOGE(TAG, "CMS failed to respond during setup");
          this->mark_failed();
          return;
        }
        
        uint8_t exp_msg[] =   {0x5a, 0x5a, 0x01, 0x0a, 0x00, 0x06, 0x01, 0x00, 0x00, 0x1c, 0x7d, 0x00, 0x25, 0xd8};
        uint8_t msg_mask[] =  {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00};
        if (message_compare_(rx_msg, 14, exp_msg, msg_mask) != no_error) {
          ESP_LOGE(TAG, "Unexpected status response");
          ESP_LOGV(TAG, "Received message: %s", format_hex(rx_msg, 14).c_str());
          this->mark_failed();
          return;
        }
        
        ESP_LOGI(TAG, "CMS was already initialized.  No setup needed");
        this->init_complete_ = true;
        this->error_count_ = 0;
        return;
      } else if (err_val != no_error) {
        ESP_LOGE(TAG, "Error seen during reception of CMS broadcast message during setup (%d)", err_val);
        this->mark_failed();
        return;
      }
      
      ESP_LOGI(TAG, "Performing initialization of CMS");
      {
        ESP_LOGV(TAG, "Received broadcast hello from CMS.  Transmitting hello from RTL");
        uint8_t tx_msg[] =  {0x5a, 0x5a, 0x00, 0x08, 0x00, 0x01, 0x00, 0xf0, 0x27};
        uint8_t exp_msg[] = {0x5a, 0x5a, 0x01, 0x08, 0x00, 0x06, 0x43, 0x4d, 0x53, 0x02, 0x00, 0x00, 0x7b, 0xb2};
        this->message_tx_(tx_msg, sizeof(tx_msg), false, false);
        err_val = this->message_rx_(rx_msg, sizeof(exp_msg), 0);
        if (err_val || message_compare_(rx_msg, sizeof(exp_msg), exp_msg, NULL)) {
          ESP_LOGE(TAG, "Received unexpected response to hello handshake (%d)", err_val);
          this->mark_failed();
          return;
        }
      }
      
      {
        ESP_LOGV(TAG, "Received unicast hello from CMS.  Transmitting set dimmer range from RTL");
        uint8_t tx_msg[] =  {0x5a, 0x5a, 0x00, 0x0b, 0x00, 0x04, 0xf2, 0x1f, 0xfd, 0x00, 0x00, 0x00};
        uint8_t exp_msg[] = {0x5a, 0x5a, 0x01, 0x0b, 0x00, 0x06, 0x72, 0x1f, 0x7d, 0x23, 0x00, 0x00, 0x44, 0x42};
        this->message_tx_(tx_msg, sizeof(tx_msg), true, true);
        err_val = this->message_rx_(rx_msg, sizeof(exp_msg), 0);
        if (err_val || message_compare_(rx_msg, sizeof(exp_msg), exp_msg, NULL)) {
          ESP_LOGE(TAG, "Received unexpected response to set dimmer range message (%d)", err_val);
          this->mark_failed();
          return;
        }
      }
      
      {
        ESP_LOGV(TAG, "Received dimmer range acknowledge from CMS.  Transmitting set minimum dimmer value from RTL");
        uint8_t tx_msg[] =  {0x5a, 0x5a, 0x00, 0x0b, 0x00, 0x04, 0x01, 0x1f, 0xfe, 0x00, 0x00, 0x00};
        uint8_t exp_msg[] = {0x5a, 0x5a, 0x01, 0x0b, 0x00, 0x06, 0x01, 0x1f, 0x7e, 0x53, 0x00, 0x00, 0xd8, 0x48};
        this->message_tx_(tx_msg, sizeof(tx_msg), true, true);
        err_val = this->message_rx_(rx_msg, sizeof(exp_msg), 0);
        if (err_val || message_compare_(rx_msg, sizeof(exp_msg), exp_msg, NULL)) {
          ESP_LOGE(TAG, "Received unexpected response to set minimum dimmer value message (%d)", err_val);
          this->mark_failed();
          return;
        }
      }
      
      {
        ESP_LOGV(TAG, "Received dimmer range acknowledge from CMS.  Transmitting set number of LEDs from RTL");
        uint8_t tx_msg[] =  {0x5a, 0x5a, 0x00, 0x0b, 0x00, 0x04, 0xf7, 0x80, 0x07, 0x43, 0x56, 0x7b};
        uint8_t exp_msg[] = {0x5a, 0x5a, 0x01, 0x0b, 0x00, 0x06, 0x77, 0x00, 0x07, 0x43, 0x00, 0x00, 0xd5, 0xce};
        this->message_tx_(tx_msg, sizeof(tx_msg), false, false);
        err_val = this->message_rx_(rx_msg, sizeof(exp_msg), 0);
        if (err_val || message_compare_(rx_msg, sizeof(exp_msg), exp_msg, NULL)) {
          ESP_LOGE(TAG, "Received unexpected response to configuration complete message (%d)", err_val);
          this->mark_failed();
          return;
        }
      }
      
      ESP_LOGI(TAG, "CMS initialization complete");
      this->init_complete_ = true;
      this->error_count_ = 0;
      
      return;
    }
    
    void TapoSmart::loop() {
      const uint32_t  status_poll_ms = 2000;
      const uint32_t  overtemp_encoded = 4635;  /* Roughly 95C.  Temperature has an inverse relationship, so any value below this is too hot */
      
      uint32_t        poll_ts;
      
      if (!this->init_complete_) {
        return;
      }
      
      poll_ts = millis();
      
      if (!this->status_has_error() && this->dimmer_val_target_ != this->dimmer_val_handled_) {
        ESP_LOGV(TAG, "Dimmer values don't match.  Setting new target dimmer value");
        this->set_dimmer_(this->dimmer_val_target_);
      } else if (poll_ts > status_poll_ms + this->status_ts_prev_) {
        ESP_LOGV(TAG, "Performing status update");
        this->get_status_();
        this->status_ts_prev_ = poll_ts;
      }
      
      if (!this->status_has_error() && this->temperature_ > 0 && this->temperature_ < overtemp_encoded) {
        ESP_LOGE(TAG, "Over temperature detected.  Disabling output and setting error state");
        this->status_set_error("Over temperature condition");
        this->set_dimmer_(0.0);
      } else if (this->status_has_error() && this->temperature_ > overtemp_encoded) {
        ESP_LOGI(TAG, "Over temperature resolved");
        this->status_clear_error();
      }
      
      //ESP_LOGV(TAG, "Performing bad state check");
      this->bad_state_check_();
      
      return;
    }

    void TapoSmart::dump_config() {
      ESP_LOGCONFIG(TAG, "TP-Link Tapo Smart Dimmer: \n");
    }
    
    void TapoSmart::update() {
      return;
    }
    
    void TapoSmart::write_state(float state) {
      ESP_LOGV(TAG, "Storing dimmer target of: %f", state);
      this->dimmer_val_target_ = state;
    }
    
    float TapoSmart::get_temperature() {
      /*  Measured data:
            chamber_temp_bme280(f),cms_temp_val(raw)
              81.6,7234
              83,7221
              84.3,7209
              86.1,7195
              87.9,7180
              89.9,7036
              91.5,7007
              92.5,7009
              94.9,6990
              96.4,6979
              98.8,6953
              101.2,6940
              103.1,6782
              104.9,6767
              106.9,6754
              108.9,6721
              111,6703
              112.6,6685
              114.4,6666
              116,6656
              118.8,6493
              120.8,6471
              123.8,6445
              125.7,6419
              128.1,6242
              131,6202
              133.4,6175
              135.2,6150
              139.5,5967
              143.1,5936
              149.3,5691
              152.9,5653
              157.8,5431
              162.8,5392 */
      
      /* first order linear approximation */
      return -0.0253 * this->temperature_ + 212.27;
    }
    
    uint8_t TapoSmart::get_errors() {
      return this->error_count_;
    }
  }  // namespace tapo_smart
}  // namespace esphome
