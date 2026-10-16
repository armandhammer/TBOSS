# SPDX-License-Identifier: GPL-3.0-or-later
"""TBOSS checks that need no hardware: the flash-safety rule (with negative cases), the
read-only external-memory rule, the web editor's protocol, and the pad notes.

  python -m unittest test_tboss        (from scripts/; the last two need gcc and node)
"""
from pathlib import Path
import shutil
import tempfile
import unittest

import firmware_safety as safety

ROOT = Path(__file__).resolve().parents[1]


def scratch():
    (ROOT / 'build').mkdir(exist_ok=True)
    return Path(tempfile.mkdtemp(dir=ROOT / 'build'))


class SafetyRule(unittest.TestCase):
    """The single flash-write exception: flash_store.c, sector 11, never option bytes."""
    def setUp(self):
        self.dir = scratch()
        self.addCleanup(shutil.rmtree, self.dir)

    def source(self, name, text):
        path = self.dir / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def test_flash_registers_only_in_the_pinned_writer(self):
        writer = (ROOT / safety.SECTOR11_WRITER)
        self.assertEqual(safety.check_sources([writer]), [])
        self.assertTrue(safety.sector11_writer([writer]))
        other = self.source('other.c', 'void f(void) { FLASH->CR = 1; FLASH->KEYR = 2; }\n')
        self.assertEqual(len(safety.check_sources([other])), 1)
        self.assertEqual(safety.check_sources([self.source('acr.c', 'void f(void) { FLASH->ACR = 2; }\n')]), [])

    def test_option_bytes_banned_even_in_the_writer(self):
        text = (ROOT / safety.SECTOR11_WRITER).read_text() + '\nvoid g(void) { FLASH->OPTCR = 0; }\n'
        fake = ROOT / 'build' / self.dir.name / 'src/tb/flash_store.c'
        fake.parent.mkdir(parents=True)
        fake.write_text(text)
        original = safety.SECTOR11_WRITER
        safety.SECTOR11_WRITER = fake.relative_to(ROOT).as_posix()
        try:
            self.assertTrue(any('option' in f or 'flash programming' in f for f in safety.check_sources([fake])))
        finally:
            safety.SECTOR11_WRITER = original

    def test_unlock_literals_need_the_writer_and_option_keys_never_pass(self):
        word = lambda w: w.to_bytes(4, 'little')
        keys = word(0x45670123) + word(0xCDEF89AB)
        self.assertEqual(len(safety.check_binary(keys)), 2)
        self.assertEqual(safety.check_binary(keys, allow_sector11=True), [])
        for banned in (0x08192A3B, 0x4C5D6E7F, 0x40023C14, 0x40023C08, 0xE0042004, 0x40007000):
            self.assertEqual(len(safety.check_binary(word(banned), allow_sector11=True)), 1, hex(banned))

    def test_writer_must_pin_sector_11(self):
        fake = self.source('src/tb/flash_store.c', (ROOT / safety.SECTOR11_WRITER).read_text()
                           .replace('#define FS_SECTOR 11u', '#define FS_SECTOR 5u'))
        original, root = safety.SECTOR11_WRITER, safety.ROOT
        safety.SECTOR11_WRITER, safety.ROOT = 'src/tb/flash_store.c', self.dir
        try:
            self.assertFalse(safety.sector11_writer([fake]))
        finally:
            safety.SECTOR11_WRITER, safety.ROOT = original, root


class NorReadOnly(unittest.TestCase):
    """The external NOR is not backed up: only nor.c may select it, with read opcodes only."""
    def setUp(self):
        self.dir = scratch()
        self.addCleanup(shutil.rmtree, self.dir)

    def driver(self, text):
        fake = self.dir / 'src/tb/nor.c'
        fake.parent.mkdir(parents=True, exist_ok=True)
        fake.write_text(text)
        original = safety.NOR_DRIVER
        safety.NOR_DRIVER = fake.relative_to(ROOT).as_posix()
        self.addCleanup(setattr, safety, 'NOR_DRIVER', original)
        return fake

    def test_real_driver_passes(self):
        self.assertEqual(safety.check_sources([ROOT / safety.NOR_DRIVER]), [])

    def test_write_or_erase_opcodes_fail(self):
        text = (ROOT / 'src/tb/nor.c').read_text()
        for opcode in ('0x06', '0x02', '0x20', '0xD8', '0xC7', '0x01', '0xB9'):
            fake = self.driver(text.replace('NR_READ = 0x03', f'NR_READ = {opcode}'))
            self.assertEqual(len(safety.check_sources([fake])), 1, opcode)

    def test_only_the_driver_may_select_the_chip(self):
        other = self.dir / 'other.c'
        other.write_text('void f(void) { GPIOA->BSRR = 1u << 31; }\n')
        self.assertTrue(any('external NOR' in f for f in safety.check_sources([other])))
        other.write_text('void f(void) { GPIOA->BSRR = 1u << 15; }\n')  # deselect is fine
        self.assertEqual(safety.check_sources([other]), [])


class WebEditor(unittest.TestCase):
    def test_protocol_module_against_firmware(self):
        node = shutil.which('node')
        if not node:
            self.skipTest('node not on PATH')
        import subprocess
        run = subprocess.run([node, str(ROOT / 'tests/webapp/test_protocol.mjs')], capture_output=True, text=True,
                             timeout=600)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertIn('PASS', run.stdout)

    def test_installer_against_a_model_of_the_loader(self):
        node = shutil.which('node')
        if not node:
            self.skipTest('node not on PATH')
        import subprocess
        run = subprocess.run([node, str(ROOT / 'tests/webapp/test_installer.mjs')], capture_output=True, text=True,
                             timeout=600)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertIn('PASS', run.stdout)


class NoteTable(unittest.TestCase):
    def test_every_pad_and_chord_edit_matches_music_theory(self):
        if not shutil.which('gcc'):
            self.skipTest('gcc not on PATH; run under WSL')
        import check_notes as notes
        failures, *_ = notes.check(notes.build_and_run())
        self.assertEqual(failures, [])


if __name__ == '__main__':
    unittest.main()
