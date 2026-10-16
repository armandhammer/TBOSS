# SPDX-License-Identifier: GPL-3.0-or-later
"""Flash-safety check for a built TheoryBoard image.

With no NRST on J2, recovery depends on the debugger always being able to attach.
So firmware must never touch PA13/PA14 (SWDIO/SWCLK), DBGMCU, low-power Stop/
Standby, option bytes, or flash programming - with one exception (the
settings store): src/tb/flash_store.c may erase/program flash sector 11
(0x080E0000) only. Two independent layers:

1. Source: every project file the build compiled (validation.json input list).
2. Binary: aligned literal words of the image (peripheral addresses and unlock keys
   the forbidden operations would need, whatever the source spelling).

Usage: python scripts/firmware_safety.py build/tboss
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SWD_PINS = {13, 14}

FORBIDDEN_SOURCE = (
    (r'\bDBGMCU\b', 'debug MCU configuration'),
    (r'\bSLEEPDEEP\b|SCB->SCR', 'deep sleep (Stop/Standby)'),
    (r'__WFE\b', 'wait-for-event low power'),
    (r'\bPWR\b\s*->|RCC_APB1ENR_PWREN', 'power controller'),
    (r'\bFLASH\s*->\s*(?!ACR\b)\w+', 'flash programming/option registers (only ACR allowed)'),
    (r'\bOPTCR\b|\bOPTKEY|\bFLASH_KEY', 'option bytes / flash unlock'),
    (r'\bHAL_FLASH|\bHAL_PWR|\bHAL_DBGMCU', 'HAL flash/power/debug'),
)
# The one permitted flash writer, and what it must pin down to be trusted.
SECTOR11_WRITER = 'src/tb/flash_store.c'
SECTOR11_REQUIRED = (r'#define FS_SECTOR 11u\b', r'FS_BASE 0x080E0000u', r'FLASH_CR_SNB_Pos')
SECTOR11_WORDS = {0x40023C04, 0x40023C10, 0x45670123, 0xCDEF89AB}  # KEYR, CR, KEY1, KEY2
# Words that would have to appear in the image to reach the forbidden operations.
FORBIDDEN_WORDS = {
    0xE0042000: 'DBGMCU_IDCODE', 0xE0042004: 'DBGMCU_CR', 0xE0042008: 'DBGMCU_APB1_FZ',
    0xE004200C: 'DBGMCU_APB2_FZ', 0x40007000: 'PWR_CR', 0x40007004: 'PWR_CSR',
    0x40023C04: 'FLASH_KEYR', 0x40023C08: 'FLASH_OPTKEYR', 0x40023C10: 'FLASH_CR',
    0x40023C14: 'FLASH_OPTCR', 0x45670123: 'flash unlock KEY1', 0xCDEF89AB: 'flash unlock KEY2',
    0x08192A3B: 'option unlock OPTKEY1', 0x4C5D6E7F: 'option unlock OPTKEY2',
}
# The external W25Q128 NOR is not backed up: firmware may only read it. Only the
# read-only driver may select it (PA15 low = BSRR bit 31), and that driver may only
# contain the read-type opcodes 0xAB (wake), 0x9F (JEDEC ID), 0x03 (read) plus 0xFF
# (dummy byte clocked out while reading).
NOR_DRIVER = 'src/tb/nor.c'
NOR_HEX = {0xAB, 0x9F, 0x03, 0xFF}
GPIOA_REG = re.compile(r'GPIOA\s*->\s*(MODER|OTYPER|OSPEEDR|PUPDR|AFR\s*\[\s*([01])\s*\]|BSRR|ODR|LCKR|IDR)')
SHIFT = re.compile(r'<<\s*(\d+)\s*u?\)?')


def project_sources(build):
    receipt = json.loads((build / 'validation.json').read_text())
    files = []
    for name in receipt['input_sha256']:
        path = Path(name)
        path = path if path.is_absolute() else ROOT / path
        try:
            rel = path.resolve().relative_to(ROOT).as_posix()
        except ValueError:
            continue  # compiler/system headers outside the repository
        if rel.startswith('src/') and path.suffix in ('.c', '.h'):
            files.append(path)
    return sorted(files)


def gpioa_pins(register, afr, line):
    """Pins named by explicit shifts on a GPIOA line; None if they can't be resolved."""
    shifts = [int(s) for s in SHIFT.findall(line)]
    if not shifts:
        return None
    if register.startswith('AFR'):
        return {int(afr) * 8 + s // 4 for s in shifts}
    if register in ('MODER', 'OSPEEDR', 'PUPDR'):
        return {s // 2 for s in shifts}
    return {s % 16 for s in shifts}


def sector11_writer(files):
    """True when the build contains the permitted sector-11 writer and it pins sector 11."""
    for path in files:
        if path.relative_to(ROOT).as_posix() == SECTOR11_WRITER:
            header = path.with_suffix('.h')
            text = path.read_text(encoding='utf-8') + (header.read_text(encoding='utf-8') if header.exists() else '')
            return all(re.search(p, text) for p in SECTOR11_REQUIRED)
    return False


def check_sources(files):
    failures = []
    for path in files:
        rel = path.relative_to(ROOT).as_posix()
        text = re.sub(r'/\*.*?\*/|//[^\n]*', lambda m: '\n' * m.group(0).count('\n'), path.read_text(encoding='utf-8'), flags=re.S)
        for number, line in enumerate(text.splitlines(), 1):
            for pattern, what in FORBIDDEN_SOURCE:
                if rel == SECTOR11_WRITER and what.startswith('flash programming') and \
                        not re.search(r'FLASH\s*->\s*OPT', line):
                    continue  # KEYR/CR/SR in the pinned sector-11 writer only
                if re.search(pattern, line):
                    failures.append(f'{rel}:{number}: {what}: {line.strip()}')
            if rel == NOR_DRIVER:
                for literal in re.findall(r'\b0[xX][0-9A-Fa-f]+', line):
                    if int(literal, 16) not in NOR_HEX:
                        failures.append(f'{rel}:{number}: NOR driver literal {literal} is not a read opcode')
            for m in GPIOA_REG.finditer(line):
                register = m.group(1).replace(' ', '')
                if register == 'IDR':
                    continue
                if register == 'BSRR' and rel != NOR_DRIVER and re.search(r'<<\s*31\b', line):
                    failures.append(f'{rel}:{number}: selects the external NOR (PA15 low) outside {NOR_DRIVER}')
                pins = gpioa_pins(register, m.group(2), line)
                if pins is None:
                    failures.append(f'{rel}:{number}: GPIOA {register} write with unresolved pins: {line.strip()}')
                elif pins & SWD_PINS:
                    failures.append(f'{rel}:{number}: GPIOA {register} touches PA13/PA14: {line.strip()}')
            if 'ts_pin' in text:  # scanner {port,bit} tables; port 0 = GPIOA
                for port, bit in re.findall(r'\{\s*(\d+)\s*,\s*(\d+)\s*\}', line):
                    if int(port) == 0 and int(bit) in SWD_PINS:
                        failures.append(f'{rel}:{number}: pin table names PA{bit}')
    return failures


def check_binary(binary, allow_sector11=False):
    failures = []
    for offset in range(0, len(binary) - 3, 4):
        word = int.from_bytes(binary[offset:offset + 4], 'little')
        if allow_sector11 and word in SECTOR11_WORDS:
            continue
        if word in FORBIDDEN_WORDS:
            failures.append(f'image +0x{offset:05x}: literal 0x{word:08X} ({FORBIDDEN_WORDS[word]})')
    return failures


def check(build):
    build = Path(build)
    files = project_sources(build)
    return (check_sources(files) +
            check_binary((build / 'theoryboard-usb.bin').read_bytes(), sector11_writer(files)))


def main():
    build = Path(sys.argv[1]) if len(sys.argv) > 1 else None
    if build is None:
        raise SystemExit(__doc__)
    failures = check(build if build.is_absolute() else ROOT / build)
    if failures:
        print(f'FAIL: {len(failures)} flash-safety violations')
        for f in failures:
            print('  ' + f)
        raise SystemExit(1)
    print(f'PASS: {build}: no PA13/PA14, DBGMCU, Stop/Standby, PWR, option bytes; flash writes '
          'only via the pinned sector-11 writer (sources and image literals); external NOR read-only.')


if __name__ == '__main__':
    main()
