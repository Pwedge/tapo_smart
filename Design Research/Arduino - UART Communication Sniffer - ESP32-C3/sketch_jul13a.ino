/* NOTE: Use Arduino ESP32 version 2.0.18 and ESPSoftwareSerial vserion 8.10! */

#include <SoftwareSerial.h>

/* Serial (hardware UART0) used for USB interface
 * Serial1 (hardware UART1) used for TX/RX interface #1
 * Serial2 (software UART) used for TX/RX interface #2 */

const int usb_uart_baud = 230400;
const int hw_uart_baud = 115200;
const int hw_uart_tx = 21;
const int hw_uart_rx = 0;
const int sw_uart_baud = 115200;
const int sw_uart_tx = 4;
const int sw_uart_rx = 1;

EspSoftwareSerial::UART Serial2;
uint8_t buf1[250];
uint8_t buf2[250];
uint8_t buf1_len = 0;
uint8_t buf2_len = 0;

void setup() {
  Serial.begin(usb_uart_baud);
  Serial1.begin(hw_uart_baud, SERIAL_8N1, hw_uart_rx, hw_uart_tx);
  Serial2.begin(sw_uart_baud, SWSERIAL_8N1, sw_uart_rx, sw_uart_tx, false);
}

void loop() {
  if (Serial1.available()) {
    buf1[buf1_len++] = Serial1.read();
  }

  if ((buf1_len == 1 && buf1[0] != 0x5a) ||
       buf1_len == 2 && buf1[1] != 0x5a) {
    buf1_len = 0;
  } else if (buf1_len >= 6 && buf1_len == 8 + buf1[5]) {
    Serial.write(buf1, buf1_len);
    buf1_len = 0;
  }

  if (Serial2.available()) {
    buf2[buf2_len++] = Serial2.read();
  }

  if ((buf2_len == 1 && buf2[0] != 0x5a) ||
       buf2_len == 2 && buf2[1] != 0x5a) {
    buf2_len = 0;
  } else if (buf2_len >= 6 && buf2_len >= 8 + buf2[5]) {
    Serial.write(buf2, buf2_len);
    buf2_len = 0;
  }
}
