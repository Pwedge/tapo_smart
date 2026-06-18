import sys
import serial
import subprocess

# Using subprocess instead to be blocking
#import asyncio
#from kasa import Discover


uart = serial.Serial(
    port='/dev/ttyUSB0', 
    baudrate=115200, 
    timeout=5,
    rtscts=False,
    dsrdtr=False,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS
)

def check_frame_type(buf):
    idle_frame = list(bytes.fromhex('5a5a010a00060100000f5000b034'))
    pwm_set_prefix = list(bytes.fromhex('5a5a010b000601'))
    
    #print('{} vs {}'.format(buf, idle_frame))
    
    if buf == idle_frame:
        return 'idle'
    elif buf[:len(pwm_set_prefix)] == pwm_set_prefix:
        return 'dimmer'
    else:
        return 'other'

buf = []
dimmer_val = 1
state = 'wait_for_idle' # 'send_cmd', 'wait_for_uart'
idle_count = 0
while dimmer_val <= 100:
    buf.append(int.from_bytes(uart.read()))
    
    if len(buf) > 3 and buf[-2] == 0x5a and buf[-1] == 0x5a:
        frame_type = check_frame_type(buf[:-2])
        #print('{} : {} ({})'.format(buf[:-2], frame_type, state))
        
        if state == 'wait_for_idle':
            if frame_type == 'idle':
                idle_count += 1
                if idle_count >= 10:
                    state = 'send_cmd'
            else:
                idle_count = 0
        elif state == 'send_cmd':
            # Change dimmer value
            cmd = 'kasa --host=10.1.1.200 --username=redacted --password=redacted light brightness {}'.format(dimmer_val)
            subprocess.run(cmd, capture_output=True, shell=True, check=True)
            state = 'wait_for_uart'
        elif state == 'wait_for_uart':
            if frame_type == 'dimmer':
                print('DEBUG: ', end='')
                for b in buf[:-2]:
                    print('{:02x} '.format(b), end='')
                print('    // {}%'.format(dimmer_val))
                dimmer_val += 1
                state = 'wait_for_idle'
                idle_count = 0
        buf = buf[-2:]
