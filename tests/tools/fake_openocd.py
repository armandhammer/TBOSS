# SPDX-License-Identifier: GPL-3.0-or-later
"""A stand-in for OpenOCD used by scripts/test_tboss_flash.py: a 1 MiB STM32F407 flash
kept in a file, with OpenOCD-like messages. Environment:
  FAKE_FLASH  path of the 1 MiB flash image (read and updated)
  FAKE_LOG    every -c command is appended here, one per line
  FAKE_OPTCR  option register value (default 0x0FFFAAED: RDP level 0, no WRP)
  FAKE_IDCODE DBGMCU_IDCODE (default 0x10076413)
  FAKE_FLIP   if set, dump_image flips one byte (a bad read)
"""
import os
from pathlib import Path
import re
import sys


def main():
    commands = [sys.argv[i + 1] for i, a in enumerate(sys.argv) if a == '-c']
    with open(os.environ['FAKE_LOG'], 'a') as log:
        log.write('\n'.join(commands) + '\n')
    flash_path = Path(os.environ['FAKE_FLASH'])
    flash = bytearray(flash_path.read_bytes())
    out = []
    for c in commands:
        m = re.fullmatch(r'dump_image \{(.+)\} 0x08000000 0x100000', c)
        w = re.fullmatch(r'flash (write|verify)_bank 0 \{(.+)\} 0x40000', c)
        if c in ('init', 'shutdown', 'reset run', 'reset_config none') or c.startswith(('adapter ', 'transport ')):
            continue
        elif c == 'reset halt':
            out.append('[stm32f4x.cpu] halted due to debug-request, current mode: Thread')
        elif c == 'mdw 0xe0042000 1':
            out.append(f'0xe0042000: {int(os.environ.get("FAKE_IDCODE", "0x10076413"), 16):08x}')
        elif c == 'mdh 0x1fff7a22 1':
            out.append('0x1fff7a22: 0400')
        elif c == 'mdw 0x40023c14 1':
            out.append(f'0x40023c14: {int(os.environ.get("FAKE_OPTCR", "0x0FFFAAED"), 16):08x}')
        elif m:
            data = bytearray(flash)
            if os.environ.get('FAKE_FLIP'):
                data[0x12345] ^= 1
            Path(m.group(1)).write_bytes(data)
            out.append('dumped 1048576 bytes in 16.000s (64.000 KiB/s)')
        elif c == 'flash probe 0':
            out.append("flash 'stm32f2x' found at 0x08000000")
        elif c == 'flash erase_sector 0 6 11':
            flash[0x40000:] = b'\xff' * (len(flash) - 0x40000)
            out.append('erased sectors 6 through 11 on flash bank 0 in 11.2s')
        elif w and w.group(1) == 'write':
            data = Path(w.group(2)).read_bytes()
            flash[0x40000:0x40000 + len(data)] = data
            out.append(f'wrote {len(data)} bytes from file {w.group(2)} to flash bank 0 at offset 0x00040000')
        elif w:
            data = Path(w.group(2)).read_bytes()
            ok = flash[0x40000:0x40000 + len(data)] == data
            out.append(f'read {len(data)} bytes from file' + ('\ncontents match' if ok else '\nError: contents differ'))
        else:
            sys.stderr.write(f'Error: fake OpenOCD does not know "{c}"\n')
            sys.exit(1)
    flash_path.write_bytes(flash)
    sys.stderr.write('\n'.join(out) + '\n')


if __name__ == '__main__':
    main()
