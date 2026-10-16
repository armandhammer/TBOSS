# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the TBOSS firmware image (src/tb) for the TheoryBoard THY333.

Needs Python 3.9+, git, and the Arm GNU Toolchain (arm-none-eabi-gcc). With
version 14.3.Rel1 the image is byte-for-byte reproducible (see docs/building.md).

  python scripts/tboss_build.py --fetch            # first time: download pinned TinyUSB/CMSIS
  python scripts/tboss_build.py                    # -> build/tboss/theoryboard-usb.bin
  python scripts/tboss_build.py --toolchain "C:/Program Files/Arm GNU Toolchain/14.3 rel1/bin"

The image goes at 0x08040000 (flash sectors 6-10), where the board's resident
loader starts it. Every build is checked: vector table, size below the settings
sector, RAM layout, and scripts/firmware_safety.py (no SWD pins, option bytes,
low-power modes or flash writes outside the settings sector).
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'src/tb'
sys.path.insert(0, str(Path(__file__).resolve().parent))
import firmware_safety  # noqa: E402

# Exact upstream commits. TinyUSB (MIT); ST CMSIS device header and Arm CMSIS core (Apache-2.0).
DEPS = {
    'tinyusb': ('https://github.com/hathach/tinyusb.git', '1a0e4020ecbf439b75b6b6fd7e8f4127c873d070'),
    'device': ('https://github.com/STMicroelectronics/cmsis_device_f4.git', '3c77349ce04c8af401454cc51f85ea9a50e34fc1'),
    'core': ('https://github.com/ARM-software/CMSIS_5.git', '2b7495b8535bdcb306dac29b9ded4cfb679d7e5c'),
}
MIDI_SHA256 = '948896790fcac43f8f05a332d1652dd4ad576cd667605fcb648e0ec443b34c71'
TOOLCHAIN = '14.3.1'
APP_BASE, SETTINGS_SECTOR = 0x08040000, 0x080E0000
RAM_BASE, RAM_TOP, DIAGNOSTICS = 0x20000000, 0x20020000, 0x20018000
VECTORS = 98
PRODUCT = 'TheoryBoard MIDI'
REQUIRED = ('tb_flash_reset', 'tc_bus_clear', 'tc_hardware_init', 'tc_poll', 'tc_arm', 'tc_step',
            'tc_notes', 'tc_mode_buttons', 'tp_step', 'ts_step', 'usb_main',
            'tm_packet', 'sx_handle', 'st_requests', 'tn_pad', 'nr_read')
COMMON = ['-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=soft', '-g0', '-Os', '-fshort-enums',
          '-ffunction-sections', '-fdata-sections', '-fno-common', '-fno-unwind-tables',
          '-fno-asynchronous-unwind-tables', '-Wall', '-Wextra', '-Werror', '-DSTM32F407xx']
OWN_C = ['-std=c11', '-Wpedantic', '-Wconversion', '-Wshadow']


def require(ok, message):
    if not ok:
        raise SystemExit('BUILD FAILED: ' + message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def git(path, *args):
    return subprocess.check_output(['git', '-C', str(path), *args], text=True).strip()


def fetch(deps):
    deps.mkdir(parents=True, exist_ok=True)
    for name, (url, pin) in DEPS.items():
        path = deps / name
        if not (path / '.git').exists():
            subprocess.check_call(['git', 'init', '-q', str(path)])
            subprocess.check_call(['git', '-C', str(path), 'remote', 'add', 'origin', url])
        print(f'fetching {name} {pin[:12]} ...')
        subprocess.check_call(['git', '-C', str(path), 'fetch', '-q', '--depth', '1', 'origin', pin])
        subprocess.check_call(['git', '-C', str(path), 'checkout', '-q', '--force', pin])


def check_deps(deps):
    for name, (_, pin) in DEPS.items():
        path = deps / name
        require((path / '.git').exists(), f'{path} missing: run with --fetch first')
        require(git(path, 'rev-parse', 'HEAD') == pin, f'{name} is not at the pinned commit {pin}')
        require(not git(path, 'status', '--porcelain'), f'{name} has local changes')


def pinned_text(tinyusb, name):
    """Upstream file exactly as committed (Windows checkouts may hold CRLF)."""
    text = (tinyusb / 'src' / name).read_bytes().replace(b'\r\n', b'\n')
    committed = subprocess.check_output(['git', '-C', str(tinyusb), 'show', DEPS['tinyusb'][1] + ':src/' + name])
    require(text == committed, f'TinyUSB {name} differs from the pinned commit')
    return text.decode('utf-8')


def patch_tinyusb(tinyusb, out):
    """Small, checked edits to local copies (originals untouched, notices kept):
    - usbd.c: report each USB event to tb_usb_event_queued() before queueing it, so
      the firmware can match MIDI transmit completions to the packet it sent;
    - dcd_dwc2.c, dwc2_common.c: every busy-wait gets an upper bound and calls
      tb_usb_wait_fault() instead of hanging; the RX FIFO drain loop is bounded;
    - midi_device.c: report each MIDI transmit completion to tb_midi_tx_result()."""
    replaced = {}
    notice = '/* TBOSS: modified local copy of the pinned TinyUSB file; original notices retained. */\n'
    for name in ('device/usbd.c', 'portable/synopsys/dwc2/dcd_dwc2.c',
                 'portable/synopsys/dwc2/dwc2_common.c', 'portable/synopsys/dwc2/dwc2_common.h'):
        text = pinned_text(tinyusb, name)
        if name == 'device/usbd.c':
            anchor = '  TU_ASSERT(osal_queue_send(_usbd_q, event, in_isr));'
            require(text.count(anchor) == 1, 'usbd.c event anchor')
            text = text.replace(anchor, '  extern void tb_usb_event_queued(dcd_event_t const*);\n'
                                        '  tb_usb_event_queued(event);\n' + anchor)
        else:
            pattern = r'while\s*\(([^\n]+)\)\s*\{\s*\}'
            expected = 4 if name.endswith(('dwc2_common.c', 'dcd_dwc2.c')) else 2
            require(len(re.findall(pattern, text)) == expected, 'unexpected busy-wait shape in ' + name)
            text = re.sub(pattern, lambda m: 'for (uint32_t tb_wait = 0; ' + m[1] + '; ++tb_wait) {\n'
                          '    if (tb_wait >= 1000000u) tb_usb_wait_fault(0x202u);\n  }', text)
            text = 'extern void tb_usb_wait_fault(unsigned int code) __attribute__((noreturn));\n' + text
            if name.endswith('dcd_dwc2.c'):
                anchor = ('    do {\n      handle_rxflvl_irq(rhport); // read all packets\n'
                          '    } while(dwc2->gintsts & GINTSTS_RXFLVL);')
                require(text.count(anchor) == 1, 'RX FIFO anchor')
                text = text.replace(anchor, '    uint32_t tb_rx = 0;\n    do {\n'
                                    '      if (++tb_rx > 256u) tb_usb_wait_fault(0x203u);\n'
                                    '      handle_rxflvl_irq(rhport); // read all packets\n'
                                    '    } while(dwc2->gintsts & GINTSTS_RXFLVL);')
        dest = out / Path(name).name
        dest.write_text(notice + text, encoding='utf-8', newline='\n')
        replaced[name] = dest
    text = (tinyusb / 'src/class/midi/midi_device.c').read_bytes().replace(b'\r\n', b'\n')
    require(sha256(text) == MIDI_SHA256, 'midi_device.c differs from the pinned commit')
    text = text.decode('utf-8')
    anchor = '  uint8_t idx = find_midi_itf(ep_addr);'
    require(text.count(anchor) == 1, 'MIDI hook anchor')
    text = text.replace(anchor, '  extern void tb_midi_tx_result(uint8_t, uint32_t, bool);\n' + anchor)
    anchor = '  if (ep_addr == ep_st_rx->ep_addr) {'
    require(text.count(anchor) == 1, 'MIDI TX anchor')
    text = text.replace(anchor, '  if (ep_addr == ep_st_tx->ep_addr) {\n'
                        '    tb_midi_tx_result(idx, xferred_bytes, result == XFER_RESULT_SUCCESS);\n'
                        '  }\n' + anchor)
    dest = out / 'midi_device.c'
    dest.write_text(notice + text, encoding='utf-8', newline='\n')
    replaced['class/midi/midi_device.c'] = dest
    return replaced


def symbols(elf):
    h = struct.unpack_from('<16sHHIIIIIHHHHHH', elf)
    require(h[0][:7] == b'\x7fELF\x01\x01\x01' and h[1] == 2 and h[2] == 40, 'ARM ELF32 executable')
    require(h[7] >> 24 == 5 and not h[7] & 0x400, 'EABI5 soft-float')
    shoff, count = h[6], h[12]
    sections = [struct.unpack_from('<10I', elf, shoff + i * 40) for i in range(count)]
    found = {}
    for s in sections:
        if s[1] != 2:
            continue
        strings = elf[sections[s[6]][4]:sections[s[6]][4] + sections[s[6]][5]]
        raw = elf[s[4]:s[4] + s[5]]
        for i in range(0, len(raw), 16):
            n, value, _, _, _, index = struct.unpack_from('<IIIBBH', raw, i)
            if n:
                name = strings[n:strings.index(b'\0', n)].decode('ascii')
                require(index != 0, 'undefined symbol ' + name)
                found[name] = value
    return found


def validate(elf, binary):
    """What the resident loader and the diagnostics rely on, from the ELF and the image."""
    sym = symbols(elf)
    require(sym['vectors'] == APP_BASE, 'vector table at 0x08040000')
    require(sym['_image_end'] - APP_BASE == len(binary), 'image spans vectors .. end of .data')
    require(len(binary) <= SETTINGS_SECTOR - APP_BASE, 'image reaches the settings sector')
    words = struct.unpack_from(f'<{VECTORS}I', binary)
    require(words[0] == RAM_TOP == sym['_estack'], 'initial stack pointer 0x20020000')
    require(words[1] == sym['Reset_Handler'] | 1, 'reset vector -> Reset_Handler (Thumb)')
    for i, w in enumerate(words[2:], 2):
        require(w == 0 or (w & 1 and APP_BASE < w < sym['_image_end']), f'vector {i} outside the image')
    require(APP_BASE < sym['_sidata'] <= sym['_image_end'] and sym['_sdata'] == RAM_BASE, '.data load/run address')
    require(sym['_ebss'] <= DIAGNOSTICS, 'RAM state below the diagnostics block')
    for name, address in (('tb_status', 0x20018000), ('ts_status', 0x20018100), ('tc_status', 0x20018400)):
        require(sym.get(name) == address, f'{name} at 0x{address:08X}')
    for name in REQUIRED:
        require(name in sym, 'missing ' + name)
    require(PRODUCT.encode() in binary, 'USB product string')
    unsafe = firmware_safety.check_binary(binary, allow_sector11=True)
    require(not unsafe, 'flash-safety literals in image: ' + '; '.join(unsafe))
    return dict(entry=hex(sym['Reset_Handler'] | 1), bytes=len(binary), ram_used=hex(sym['_ebss'] - RAM_BASE))


def build(deps, toolchain, out):
    check_deps(deps)
    tools = {}
    for name in ('gcc', 'objcopy'):
        found = shutil.which('arm-none-eabi-' + name, path=str(toolchain) if toolchain else None)
        require(found, f'arm-none-eabi-{name} not found (install the Arm GNU Toolchain or pass --toolchain)')
        tools[name] = found
    version = subprocess.check_output([tools['gcc'], '-dumpversion'], text=True).strip()
    if version != TOOLCHAIN:
        print(f'note: arm-none-eabi-gcc {version}; the published image hash needs {TOOLCHAIN} (14.3.Rel1)')
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    tinyusb, device, core = (deps / n for n in ('tinyusb', 'device', 'core'))
    patched = patch_tinyusb(tinyusb, out)
    sources = [SRC / 'startup.S', SRC / 'main.c']
    sources += sorted(p for p in SRC.glob('*.c') if p.name != 'main.c')
    sources += [tinyusb / 'src/tusb.c', tinyusb / 'src/common/tusb_fifo.c', patched['device/usbd.c'],
                patched['portable/synopsys/dwc2/dcd_dwc2.c'], patched['portable/synopsys/dwc2/dwc2_common.c'],
                patched['class/midi/midi_device.c']]
    includes = ['-I' + str(SRC)]
    for path in (tinyusb / 'src', tinyusb / 'src/class/midi', device / 'Include', core / 'CMSIS/Core/Include',
                 tinyusb / 'src/portable/synopsys/dwc2'):
        includes += ['-isystem', str(path)]
    objects, inputs = [], {}
    for i, source in enumerate(sources):
        flags = COMMON + includes
        if source.suffix == '.c':
            flags = flags + (OWN_C if source.parent == SRC else ['-std=c11'])
        deps_text = subprocess.run([tools['gcc'], *flags, '-M', str(source)], capture_output=True, text=True, check=True).stdout
        for dep in re.split(r'\s+', deps_text.replace('\\\n', ' ').split(': ', 1)[1].strip()):
            path = Path(dep.replace('\\ ', ' ')).resolve()
            inputs[str(path)] = sha256(path.read_bytes())
        obj = out / f'{i:02d}-{source.stem}.o'
        run = subprocess.run([tools['gcc'], *flags, '-c', str(source), '-o', str(obj)], capture_output=True, text=True)
        require(run.returncode == 0 and not run.stderr.strip(), f'compiling {source.name}:\n{run.stderr}')
        objects.append(str(obj))
    elf, binary = out / 'theoryboard-usb.elf', out / 'theoryboard-usb.bin'
    link = subprocess.run([tools['gcc'], '-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=soft', '-nostartfiles',
                           '--specs=nano.specs', '--specs=nosys.specs', *objects, '-T', str(SRC / 'flash.ld'),
                           '-Wl,--gc-sections,--build-id=none,--no-warn-rwx-segments,--fatal-warnings',
                           '-Wl,--orphan-handling=error,-z,noexecstack', '-Wl,-Map=' + str(out / 'theoryboard-usb.map'),
                           '-o', str(elf)], capture_output=True, text=True)
    require(link.returncode == 0 and not link.stderr.strip(), 'linking:\n' + link.stderr)
    subprocess.check_call([tools['objcopy'], '-O', 'binary', str(elf), str(binary)])
    image = binary.read_bytes()
    result = validate(elf.read_bytes(), image)
    # Same receipt shape as scripts/firmware_safety.py expects (input_sha256).
    (out / 'validation.json').write_text(json.dumps(dict(result, sha256=sha256(image), compiler=version,
        pins={n: pin for n, (_, pin) in DEPS.items()}, input_sha256=inputs), indent=1) + '\n', encoding='utf-8')
    unsafe = firmware_safety.check(out)
    require(not unsafe, 'flash-safety check failed:\n  ' + '\n  '.join(unsafe))
    return image, result


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--deps', type=Path, default=ROOT / 'deps', help='where the pinned dependencies live')
    p.add_argument('--toolchain', type=Path, help='folder holding arm-none-eabi-gcc (default: PATH)')
    p.add_argument('--out', type=Path, default=ROOT / 'build/tboss', help='output folder (replaced)')
    p.add_argument('--fetch', action='store_true', help='download/check out the pinned dependencies first')
    args = p.parse_args()
    if args.fetch:
        fetch(args.deps)
    image, result = build(args.deps.resolve(), args.toolchain, args.out.resolve())
    print(f'PASS {args.out / "theoryboard-usb.bin"}: {result["bytes"]} bytes, RAM {result["ram_used"]}, '
          f'SHA256 {sha256(image)}')


if __name__ == '__main__':
    main()
