# SPDX-License-Identifier: GPL-3.0-or-later
"""scripts/tboss_flash.py against a simulated OpenOCD (tests/tools/fake_openocd.py).

  python -m unittest test_tboss_flash      (from scripts/)
"""
import contextlib
import io
import os
from pathlib import Path
import re
import shutil
import struct
import tempfile
import unittest

import tboss_flash as f

ROOT = Path(__file__).resolve().parents[1]
FAKE = str(ROOT / 'tests/tools/fake_openocd.py')
FORBIDDEN = re.compile(r'protect|unlock|lock|option|mass_erase|stm32f2x |erase_sector 0 [0-5]\b|write_image|'
                       r'mww|mwh|mwb|program', re.I)


def image(n=40000):
    data = bytearray(struct.pack('<II', 0x20020000, 0x08040189)) + bytes((i * 13) & 255 for i in range(n - 8))
    return bytes(data)


class FlashTool(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.dir)
        self.original = bytes((i * 7 + (i >> 12)) & 255 for i in range(f.FLASH_SIZE))
        self.flash = self.dir / 'flash.bin'
        self.flash.write_bytes(self.original)
        self.log = self.dir / 'commands.txt'
        self.env = {'FAKE_FLASH': str(self.flash), 'FAKE_LOG': str(self.log)}
        os.environ.update(self.env)
        self.addCleanup(lambda: [os.environ.pop(k, None) for k in
                                 ('FAKE_FLASH', 'FAKE_LOG', 'FAKE_OPTCR', 'FAKE_IDCODE', 'FAKE_FLIP')])

    def tool(self, *args, answer='GO'):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = f.main([*args, '--openocd', FAKE], answer=lambda prompt: answer)
        self.output = out.getvalue()
        return code

    def commands(self):
        return self.log.read_text().split('\n') if self.log.exists() else []

    def backup(self):
        self.assertEqual(self.tool('backup', '--out', str(self.dir / 'backups')), 0, self.output)
        files = sorted((self.dir / 'backups').glob('*.bin'))
        self.assertEqual(len(files), 1)
        return files[0]

    def test_check_reads_only(self):
        self.assertEqual(self.tool('check'), 0, self.output)
        self.assertIn('RDP level 0', self.output)
        self.assertEqual(self.flash.read_bytes(), self.original)
        self.assertFalse(any('erase' in c or 'write' in c for c in self.commands()))

    def test_backup_reads_twice_and_records_sha256(self):
        path = self.backup()
        self.assertEqual(path.read_bytes(), self.original)
        self.assertIn(f.sha256(self.original), path.with_suffix('.sha256').read_text())
        self.assertEqual(sum(c.startswith('dump_image') for c in self.commands()), 2)
        self.assertEqual(self.flash.read_bytes(), self.original)

    def test_backup_of_a_board_running_tboss_says_so(self):
        flash = bytearray(self.original)
        flash[0x50000:0x50010] = b'TheoryBoard MIDI'
        self.flash.write_bytes(flash)
        self.backup()
        self.assertIn('running TBOSS', self.output)
        self.flash.write_bytes(self.original)
        self.assertEqual(self.tool('backup', '--out', str(self.dir / 'b2')), 0)
        self.assertIn('original firmware', self.output)
        self.assertNotIn('running TBOSS', self.output)

    def test_inconsistent_reads_save_nothing(self):
        os.environ['FAKE_FLIP'] = '1'
        real = f.read_flash
        calls = []

        def flaky(ocd, folder, name):  # second read differs from the first
            calls.append(name)
            os.environ['FAKE_FLIP'] = '' if len(calls) == 1 else '1'
            return real(ocd, folder, name)
        f.read_flash = flaky
        try:
            self.assertEqual(self.tool('backup', '--out', str(self.dir / 'b')), 1)
        finally:
            f.read_flash = real
        self.assertIn('two reads differ', self.output)
        self.assertFalse((self.dir / 'b').exists() and any((self.dir / 'b').iterdir()))

    def test_install_writes_sectors_6_to_11_only_and_verifies(self):
        backup = self.backup()
        img = self.dir / 'tboss.bin'
        img.write_bytes(image())
        self.assertEqual(self.tool('install', str(img), '--backup', str(backup)), 0, self.output)
        flash = self.flash.read_bytes()
        self.assertEqual(flash[:f.APP_OFFSET], self.original[:f.APP_OFFSET])
        self.assertEqual(flash[f.APP_OFFSET:f.APP_OFFSET + len(image())], image())
        self.assertEqual(flash[f.APP_OFFSET + len(image()):], b'\xff' * (f.FLASH_SIZE - f.APP_OFFSET - len(image())))
        for c in self.commands():
            self.assertFalse(FORBIDDEN.search(re.sub(r'\{[^}]*\}', '{}', c)), c)  # not in a {file path}: tmp names are random
        self.assertIn('flash erase_sector 0 6 11', self.commands())
        # ...and restore brings back the original exactly.
        self.assertEqual(self.tool('restore', '--backup', str(backup)), 0, self.output)
        self.assertEqual(self.flash.read_bytes(), self.original)

    def test_refusals_write_nothing(self):
        backup = self.backup()
        img = self.dir / 'tboss.bin'
        img.write_bytes(image())
        cases = [
            (['install', str(img), '--backup', str(backup)], 'stop', {}, 'nothing was written'),
            (['install', str(img)], 'GO', {}, 'needs the image and --backup'),
            (['install', str(img), '--backup', str(backup)], 'GO', {'FAKE_OPTCR': '0x0FFF55ED'}, 'read-protected'),
            (['install', str(img), '--backup', str(backup)], 'GO', {'FAKE_OPTCR': '0x0FFEAAED'}, 'write protection'),
            (['install', str(img), '--backup', str(backup)], 'GO', {'FAKE_IDCODE': '0x10006411'}, 'not an STM32F4'),
        ]
        for args, answer, env, message in cases:
            with self.subTest(message=message):
                os.environ.update(env)
                before = self.flash.read_bytes()
                self.log.write_text('')
                self.assertEqual(self.tool(*args, answer=answer), 1, self.output)
                self.assertIn(message, self.output)
                self.assertEqual(self.flash.read_bytes(), before)
                self.assertFalse(any('erase' in c or 'write' in c for c in self.commands()))
                for k in env:
                    os.environ.pop(k)

    def test_backup_from_another_board_is_refused(self):
        backup = self.backup()
        other = bytearray(backup.read_bytes())
        other[0x8000] ^= 0xFF  # sector 2: a different loader
        bad = self.dir / 'other.bin'
        bad.write_bytes(other)
        img = self.dir / 'tboss.bin'
        img.write_bytes(image())
        self.assertEqual(self.tool('install', str(img), '--backup', str(bad)), 1)
        self.assertIn('sectors [2]', self.output)
        self.assertEqual(self.flash.read_bytes(), self.original)

    def test_bad_images_and_tampered_backups(self):
        backup = self.backup()
        for data, message in ((b'\0' * 100, 'stack pointer'), (image(f.APP_LIMIT + 4), 'ends below the settings')):
            path = self.dir / 'x.bin'
            path.write_bytes(data)
            self.assertEqual(self.tool('install', str(path), '--backup', str(backup)), 1)
            self.assertIn(message, self.output)
        tampered = bytearray(backup.read_bytes())
        tampered[-1] ^= 1
        backup.write_bytes(tampered)
        self.assertEqual(self.tool('restore', '--backup', str(backup)), 1)
        self.assertIn('does not match', self.output)

    def test_only_listed_commands_can_be_sent(self):
        ocd = f.OpenOCD(FAKE, 'stlink', 1000)
        for bad in ('flash protect 0 0 11 on', 'stm32f2x unlock 0', 'flash erase_sector 0 0 11', 'mww 0x40023c14 0'):
            with self.assertRaises(f.Stop):
                ocd.run(bad)


if __name__ == '__main__':
    unittest.main()
