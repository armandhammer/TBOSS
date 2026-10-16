# SPDX-License-Identifier: GPL-3.0-or-later
"""Back up, install and restore TheoryBoard THY333 firmware over SWD with OpenOCD.

Needs an SWD adapter (ST-Link V2/V3 or a CMSIS-DAP probe) wired to the board's J2
header, OpenOCD 0.12 or newer, and Python 3.9+. See docs/flashing.md first.

  python scripts/tboss_flash.py check                      # identify the chip; reads only
  python scripts/tboss_flash.py backup                     # 1 MiB internal flash, read twice
  python scripts/tboss_flash.py install build/tboss/theoryboard-usb.bin --backup backups/<file>.bin
  python scripts/tboss_flash.py restore --backup backups/<file>.bin

Rules this tool enforces:
- It writes only flash sectors 6-11 (0x08040000-0x080FFFFF). Sectors 0-5 hold the
  board's resident loader, which runs first after every reset; they are never erased.
- Before writing, the board's sectors 0-5 must equal those in your backup (proof that
  the backup belongs to this board), and the chip must be readable (protection level 0).
- It never changes option bytes, read/write protection, or the debug pins.
- Every write is verified by reading the whole flash back.
- J2 has no reset line, so OpenOCD resets the chip in software (SYSRESETREQ).
"""
import argparse
import datetime
import hashlib
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FLASH_BASE, FLASH_SIZE = 0x08000000, 0x100000
# STM32F407, 1 MiB: sectors 0-3 16 KiB, 4 64 KiB, 5-11 128 KiB (RM0090 table 5).
SECTORS = ([(i, i * 0x4000, 0x4000) for i in range(4)] + [(4, 0x10000, 0x10000)] +
           [(i, 0x20000 * (i - 4), 0x20000) for i in range(5, 12)])
FIRST_SECTOR, APP_OFFSET = 6, 0x40000
APP_LIMIT = 0x0E0000 - APP_OFFSET  # an image must end below sector 11 (settings)
assert SECTORS[FIRST_SECTOR][1] == APP_OFFSET and sum(s[2] for s in SECTORS) == FLASH_SIZE
INTERFACES = {
    'stlink': ['-f', 'interface/stlink.cfg'],
    'stlink-dap': ['-f', 'interface/stlink-dap.cfg', '-c', 'transport select dapdirect_swd'],
    'cmsis-dap': ['-f', 'interface/cmsis-dap.cfg', '-c', 'transport select swd'],
}
# Every OpenOCD command this tool may send. Anything else is a bug: refuse it.
ALLOWED = [r'init', r'reset halt', r'reset run', r'shutdown', r'flash probe 0',
           r'mdw 0x(e0042000|40023c14) 1', r'mdh 0x1fff7a22 1',
           r'dump_image \{[^{}]+\} 0x08000000 0x100000',
           rf'flash erase_sector 0 {FIRST_SECTOR} 11',
           r'flash write_bank 0 \{[^{}]+\} 0x40000', r'flash verify_bank 0 \{[^{}]+\} 0x40000']
FAILURE = re.compile(r'^Error:|failed|timed out|not halted', re.I | re.M)


class Stop(Exception):
    pass


def require(ok, message):
    if not ok:
        raise Stop(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def tcl_path(path):
    return '{' + Path(path).resolve().as_posix() + '}'


class OpenOCD:
    def __init__(self, executable, interface, speed, serial=None):
        if Path(executable).is_file():
            executable = str(Path(executable).resolve())
        self.executable, self.interface, self.speed, self.serial = executable, interface, speed, serial

    def run(self, *commands, timeout=900):
        for c in commands:
            require(any(re.fullmatch(p, c) for p in ALLOWED), f'internal error: command not allowed: {c}')
        # A .py "OpenOCD" is the simulator used by the tests (tests/tools/fake_openocd.py).
        base = [sys.executable, self.executable] if str(self.executable).endswith('.py') else [self.executable]
        args = [*base, *INTERFACES[self.interface]]
        if self.serial:
            args += ['-c', f'adapter serial {self.serial}']
        # The standard target file expects a reset line; J2 has none: reset in software.
        args += ['-f', 'target/stm32f4x.cfg', '-c', 'reset_config none', '-c', f'adapter speed {self.speed}']
        for c in ('init', *commands, 'shutdown'):
            args += ['-c', c]
        try:
            done = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
        except FileNotFoundError:
            raise Stop(f'OpenOCD not found ({self.executable}); install it or pass --openocd')
        except subprocess.TimeoutExpired:
            raise Stop('OpenOCD did not finish in time; check the wiring and power, then try again')
        log = done.stdout + done.stderr
        if done.returncode or FAILURE.search(log):
            tail = '\n'.join(log.strip().splitlines()[-8:])
            raise Stop('OpenOCD reported a problem; nothing more was done:\n' + tail)
        return log


def word(log, address):
    m = re.search(rf'0x{address:08x}:\s+([0-9a-f]+)', log, re.I)
    require(m, f'no value read at 0x{address:08X}')
    return int(m.group(1), 16)


def identify(ocd):
    """Reads only. Returns a description; stops unless it is a readable 1 MiB STM32F40x/41x."""
    log = ocd.run('mdw 0xe0042000 1', 'mdh 0x1fff7a22 1', 'mdw 0x40023c14 1')
    idcode, size_kib, optcr = word(log, 0xE0042000), word(log, 0x1FFF7A22), word(log, 0x40023C14)
    require(idcode & 0xFFF == 0x413, f'not an STM32F405/407/415/417 (DBGMCU_IDCODE 0x{idcode:08X})')
    require(size_kib == 1024, f'flash is {size_kib} KiB, expected 1024')
    rdp = (optcr >> 8) & 0xFF
    require(rdp == 0xAA, 'the chip is read-protected (RDP level 1 or 2); this tool does not change '
            'protection, because removing it erases the whole chip including the loader')
    wrp = (optcr >> 16) & 0xFFF
    require(wrp == 0xFFF, f'write protection is set on some sectors (nWRP 0x{wrp:03X}); not supported')
    return f'STM32F40x/41x rev 0x{idcode >> 16:04X}, 1 MiB flash, readable (RDP level 0), no write protection'


def read_flash(ocd, folder, name):
    path = Path(folder) / name
    log = ocd.run('reset halt', f'dump_image {tcl_path(path)} 0x08000000 0x100000', 'reset run')
    require(re.search(r'dumped 1048576 bytes', log), 'flash read incomplete')
    data = path.read_bytes()
    require(len(data) == FLASH_SIZE, 'flash read length mismatch')
    return data


def differing_sectors(a, b):
    return [n for n, start, size in SECTORS if a[start:start + size] != b[start:start + size]]


def load_backup(path):
    path = Path(path)
    require(path.is_file(), f'backup {path} not found (make one with: tboss_flash.py backup)')
    data = path.read_bytes()
    require(len(data) == FLASH_SIZE, 'a backup must be exactly 1 MiB')
    note = path.with_suffix('.sha256')
    if note.exists():
        require(note.read_text().split()[0].lower() == sha256(data), f'{path.name} does not match {note.name}')
    return data


def check_image(data):
    require(0 < len(data) <= APP_LIMIT, f'image must be 1..{APP_LIMIT} bytes (it ends below the settings sector)')
    sp, reset = struct.unpack_from('<II', data)
    require(0x20000000 < sp <= 0x20020000 and sp % 8 == 0, 'not a firmware image for 0x08040000 (stack pointer)')
    require(reset & 1 and FLASH_BASE + APP_OFFSET < reset < FLASH_BASE + APP_OFFSET + len(data),
            'not a firmware image for 0x08040000 (reset vector)')


def confirm(text, answer=input):
    print(text)
    reply = answer('Type GO to continue (anything else stops): ')
    require(reply.strip().upper() == 'GO', 'stopped; nothing was written')


def write_region(ocd, folder, region, keep_loader):
    path = Path(folder) / 'region.bin'
    path.write_bytes(region)
    log = ocd.run('reset halt', 'flash probe 0', f'flash erase_sector 0 {FIRST_SECTOR} 11',
                  f'flash write_bank 0 {tcl_path(path)} 0x40000', f'flash verify_bank 0 {tcl_path(path)} 0x40000',
                  f'dump_image {tcl_path(Path(folder) / "after.bin")} 0x08000000 0x100000', 'reset run')
    require(re.search(rf'erased sectors {FIRST_SECTOR} through 11 on flash bank 0', log), 'erase not confirmed')
    require(re.search(rf'wrote {len(region)} bytes', log), 'write not confirmed')
    require(re.search(r'contents match|verified \d+ bytes', log), 'verify not confirmed')
    after = (Path(folder) / 'after.bin').read_bytes()
    require(len(after) == FLASH_SIZE, 'read-back incomplete')
    require(after[:APP_OFFSET] == keep_loader, 'read-back: sectors 0-5 changed (should be impossible)')
    end = APP_OFFSET + len(region)
    require(after[APP_OFFSET:end] == region, 'read-back: written data differs')
    require(after[end:] == b'\xff' * (FLASH_SIZE - end), 'read-back: rest of sectors 6-11 not erased')
    return after


def main(argv=None, answer=input):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('action', choices=('check', 'backup', 'install', 'restore'))
    p.add_argument('image', nargs='?', help='install: the firmware .bin (e.g. build/tboss/theoryboard-usb.bin)')
    p.add_argument('--backup', help='install/restore: your 1 MiB backup of this board')
    p.add_argument('--out', default=str(ROOT / 'backups'), help='backup: folder for the files (default backups/)')
    p.add_argument('--openocd', default=shutil.which('openocd') or 'openocd', help='OpenOCD executable')
    p.add_argument('--interface', choices=sorted(INTERFACES), default='stlink', help='SWD adapter type')
    p.add_argument('--serial', help='adapter serial number (only with several adapters attached)')
    p.add_argument('--speed', type=int, default=1000, help='SWD clock in kHz (default 1000)')
    args = p.parse_args(argv)
    ocd = OpenOCD(args.openocd, args.interface, args.speed, args.serial)
    try:
        if args.action == 'install':
            require(args.image and args.backup, 'install needs the image and --backup (your backup of this board)')
            image = Path(args.image).read_bytes()
            check_image(image)
        if args.action in ('install', 'restore'):
            require(args.backup, f'{args.action} needs --backup')
            backup = load_backup(args.backup)
        print('Connecting...')
        print('  ' + identify(ocd))
        if args.action == 'check':
            print('OK: nothing was written.')
            return 0
        with tempfile.TemporaryDirectory() as tmp:
            print('Reading the whole internal flash (about 20 s)...')
            first = read_flash(ocd, tmp, 'read1.bin')
            if args.action == 'backup':
                print('Reading it again to compare...')
                second = read_flash(ocd, tmp, 'read2.bin')
                require(first == second, 'the two reads differ; check the wiring and try again (nothing saved)')
                out = Path(args.out)
                out.mkdir(parents=True, exist_ok=True)
                name = 'thy333-flash-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
                (out / (name + '.bin')).write_bytes(first)
                (out / (name + '.sha256')).write_text(f'{sha256(first)}  {name}.bin\n')
                print(f'OK: both reads identical. Saved {out / (name + ".bin")}\n    SHA256 {sha256(first)}\n'
                      '    Keep a copy somewhere safe: it is the only way back to this board\'s original firmware.')
                return 0
            loader = differing_sectors(first[:APP_OFFSET] + bytes(FLASH_SIZE - APP_OFFSET),
                                       backup[:APP_OFFSET] + bytes(FLASH_SIZE - APP_OFFSET))
            require(not loader, f'sectors {loader} (the resident loader) differ from your backup: '
                    'the backup is from another board or another loader version. Nothing written.')
            region = image if args.action == 'install' else backup[APP_OFFSET:]
            what = (f'the firmware {Path(args.image).name} ({len(image)} bytes, SHA256 {sha256(image)[:16]}...)'
                    if args.action == 'install' else f'the original firmware from {Path(args.backup).name}')
            confirm(f'\nLoader sectors 0-5 match your backup.\nThis erases sectors {FIRST_SECTOR}-11 '
                    f'(0x08040000-0x080FFFFF; this clears saved settings) and writes {what}.', answer)
            print('Erasing, writing and verifying (about 30 s)...')
            after = write_region(ocd, tmp, region, first[:APP_OFFSET])
            if args.action == 'restore':
                require(after == backup, 'read-back differs from the backup')
            print('OK: written and verified by reading the whole flash back.\n'
                  'Unplug the board (and the adapter) for a moment, then reconnect it normally.')
            return 0
    except Stop as stop:
        print(f'STOPPED: {stop}')
        return 1


if __name__ == '__main__':
    sys.exit(main())
