import serial
import time
import crc

uart = serial.Serial(
    port='/dev/ttyACM0', 
    baudrate=115200, 
    timeout=5,
    rtscts=False,
    dsrdtr=False,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS
)

crc_config = crc.Configuration(width=16, polynomial=32773, init_value=65535, final_xor_value=0, reverse_input=True, reverse_output=True)

crc_calc = crc.Calculator(crc_config)

buf = []
prev_ts = time.time()
cur_ts = prev_ts
while True:
    buf.append(int.from_bytes(uart.read()))
    
    # Wait for sync
    if len(buf) == 1 and buf[0] != 0x5a or len(buf) == 2 and buf[1] != 0x5a:
        buf = []
        continue
    
    # Capture time stamp after message sync
    if len(buf) == 2:
        cur_ts = time.time()
        delta_ts = cur_ts - prev_ts
        prev_ts = cur_ts
    
    # Filter out bad messages due to UART issues
    if len(buf) > 14:
        print('{:.04f} [*BAD_LEN*] '.format(delta_ts), end='')
        for b in buf:
            print('0x{:02x} '.format(b), end='')
        print('')
        buf = []
    elif len(buf) > 5 and len(buf) == (6 + 2 + buf[5]):
        exp_crc = crc_calc.checksum(bytes(buf[2:-2]))
        exp_crc = [(exp_crc >> 8) & 0xff, exp_crc & 0xff]
        
        if exp_crc != buf[-2:]:
            print('{:.04f} [*BAD_CRC*] '.format(delta_ts), end='')
        elif buf[2] == 0x02:
            # Startup announcement
            print('\n\n\n\n\n')
            print('{:.04f} [CMS TX] '.format(0), end='')
        elif buf[2] == 0x01:
            # Message from CMS ---> RTL
            print('{:.04f} [CMS TX] '.format(delta_ts), end='')
        elif buf[2] == 0x00:
            # Message from RTL ---> CMS
            print('{:.04f} [RTL TX] '.format(delta_ts), end='')
        else:
            print('{:.04f} [*BAD_SRC*] '.format(delta_ts), end='')
        
        for b in buf:
            print('0x{:02x} '.format(b), end='')
        print('')
        buf = []

