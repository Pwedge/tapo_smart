messages = [
    '5a5a010b00067701004500006012',
    '5a5a010b00067701014400005c42',
    '5a5a010b000677010247000018b2',
    '5a5a010b000677010346000024e2',
    '5a5a010b00067701044100009152',
    '5a5a010b0006770105400000ad02',
    '5a5a010b0006770106430000e9f2',
    '5a5a010b0006770107420000d5a2',
    '5a5a010b0006011b7a53000028b8',
    '5a5a010b0006011b597000006642',
    '5a5a010b0006011b38110000440c',
    '5a5a010b0006011b173e00005935',
    '5a5a010b0006011a765e00007b17',
    '5a5a010b0006011a557d000035ed',
    '5a5a010b0006011a341c000017a3',
    '5a5a010b0006011a133b0000a819',
    '5a5a010b0006011a725a00008a57',
    '5a5a010b0006011a51790000c4ad',
    '5a5a010b0006011a30180000e6e3',
    '5a5a010b0006011a0f270000fedf',
    '5a5a010b000601196e450000dc25'
]

start_byte = 6
end_byte = 8

for msg in messages:
    xor_val = 0x33
    byte_str = ''
    
    byte_vals = bytearray.fromhex(msg)
    for i in range(start_byte, end_byte + 1):
        xor_val ^= byte_vals[i]
        byte_str += '{:02x}'.format(byte_vals[i])
    
    print('{} : capture={:02x} vs calc={:02x}'.format(byte_str, byte_vals[end_byte + 1], xor_val))
    