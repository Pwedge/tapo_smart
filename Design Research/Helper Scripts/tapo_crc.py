#!/usr/bin/python3

# Credit to: https://voidcomputing.hu/blog/crc-reversing/

import crc
from itertools import product

msgs = ['5a5a000a000401000000371a',
        '5a5a000b0004011b7a53ad19',
        '5a5a000b0004f70101c4bc68',
        '5a5a000b0004f7800044a438',
        '5a5a000b0004011b59704441',
        '5a5a02080006434d530200007442',
        '5a5a010a00060100000f5000b034',
        '5a5a010b000601165672000073cd',
        '5a5a010b0006011770550000f04b',
        '5a5a010b00060118200a0000e33e',
        '5a5a010b000601186c460000a4e9',
        '5a5a010b0006011a51790000c4ad',
        '5a5a010b0006011b7b52000014e8',
        '5a5a010b0006051b7b56000051a8',
        '5a5a010b00067700014500005c2e',
        '5a5a010b000677000246000018de',
        '5a5a010b0006770003470000248e',
        '5a5a010b0006770004400000913e',
        '5a5a010b0006770005410000ad6e',
        '5a5a010b00067701004500006012',
        '5a5a010b000677010346000024e2']

for msg in msgs:
    data = bytes.fromhex(msg)

    expected_checksum = data[-1] | (data[-2] << 8)

    for poly, init, revi, revo, with_header in product(
        [0x0589, 0x080b, 0x1021, 0x1dcf, 0x3d65, 0x5935, 0x6f63, 0x755b, 0x8005, 0x8bb7, 0xa097, 0xc867],
        [0x0000, 0x1d0f, 0x800d, 0x89ec, 0xb2aa, 0xc6c6, 0xffff],
        [False, True],
        [False, True],
        [False, True]
    ):
        config = crc.Configuration(
            width=16,
            polynomial=poly,
            init_value=init,
            final_xor_value=0,
            reverse_input=revi,
            reverse_output=revo,
        )

        calculator = crc.Calculator(config)
        chk_data = data[:-2] if with_header else data[2:-2]
        checksum = calculator.checksum(chk_data)
        if checksum == expected_checksum or checksum ^ 0xffff == expected_checksum:
            print('Found solution: ', config)
            print('Original message: {}'.format(''.join('{:02x}'.format(x) for x in data)))
            print('Calculated over:  {}'.format(''.join('{:02x}'.format(x) for x in chk_data)))
            break
