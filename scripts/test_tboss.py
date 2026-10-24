# SPDX-License-Identifier: GPL-3.0-or-later
"""TBOSS checks that need no hardware: the flash-safety rule (with negative cases), the
read-only external-memory rule, the web editor's protocol, the pad notes, the scale
library's generated files, and the website (the project page and the manual).

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


class Website(unittest.TestCase):
    """The project page and the manual as built for GitHub Pages (scripts/build_site.py)."""
    @classmethod
    def setUpClass(cls):
        import build_site
        cls.site = build_site
        cls.out = scratch() / 'site'
        build_site.build(cls.out)
        cls.manual = (cls.out / 'manual.html').read_text(encoding='utf-8')
        cls.home = (cls.out / 'index.html').read_text(encoding='utf-8')

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.out.parent, ignore_errors=True)

    def ids(self, page):
        import re
        return set(re.findall(r'id="([^"]+)"', page))

    def test_the_site_has_every_page_and_no_templates(self):
        for name in ('index.html', 'manual.html', 'editor.html', 'install.html', 'site/site.css', 'site/site.js',
                     'site/tboss-logo.jpg'):
            self.assertTrue((self.out / name).exists(), name)
        for name in ('site/home.html', 'site/manual.html'):
            self.assertFalse((self.out / name).exists(), name)
        for page in (self.manual, self.home):
            self.assertNotIn('{{', page)

    def test_every_link_into_the_manual_lands(self):
        import re
        ids = self.ids(self.manual)
        for target in re.findall(r'href="#([^"]*)"', self.manual):
            self.assertIn(target, ids)
        for target in re.findall(r'href="manual\.html#([^"]*)"', self.home):
            self.assertIn(target, ids)
        # Links from the other documents (as GitHub resolves them) work on the site too.
        docs = self.site.MANUAL.parent
        for doc in [*docs.glob('*.md'), docs.parent / 'README.md', ROOT / 'README.md']:
            if doc.exists():
                for target in re.findall(r'manual\.md#([\w-]+)', doc.read_text(encoding='utf-8')):
                    self.assertIn(target, ids, f'{doc.name}: manual.md#{target}')

    def test_github_anchors(self):
        slug = self.site.Slugger()
        self.assertEqual(slug('Chords on the right (optional)'), 'chords-on-the-right-optional')
        self.assertEqual(slug('New in 1.0 (beta)'), 'new-in-10-beta')
        self.assertEqual(slug('Voicing I / II / III'), 'voicing-i--ii--iii')
        self.assertEqual(slug('Octave V (−3 to +3)'), 'octave-v-3-to-3')
        self.assertEqual(slug('Try it'), 'try-it')
        self.assertEqual(slug('Try it'), 'try-it-1')

    def test_markdown_is_all_converted(self):
        import re
        article = self.manual.split('<article>')[1]
        prose = re.sub(r'<pre>.*?</pre>|<code>.*?</code>', '', article, flags=re.S)
        self.assertNotIn('**', prose)
        self.assertNotIn('](', prose)
        self.assertNotIn('Contents: ', prose)
        self.assertGreater(prose.count('class="note try"'), 50)
        toc = self.manual.split('<nav class="toc">')[1].split('</nav>')[0]
        self.assertEqual(toc.count('<a '), len(re.findall(r'<h[23] id=', article)))

    def test_converter(self):
        c = self.site.Converter()
        out = c.blocks(['- one', '  wrapped', '  - nested', '- two', '', '22a. step', '    more', '',
                        '| A | B |', '| --- | ---: |', '| `x` *i* | **b** |', '',
                        'Text. **Try it:** this. **Why use it:** that. **When not to:** never.', '',
                        'See https://example.com/a. And [this](roadmap.md#later).'])
        self.assertIn('<li>one wrapped\n<ul>\n<li>nested</li>', out)
        self.assertIn('<span class="n">22a</span><div>step more</div>', out)
        self.assertIn('<td style="text-align:right"><strong>b</strong></td>', out)
        self.assertIn('<p>Text.</p>\n<div class="note try">', out)
        self.assertIn('<div class="note whennot"><p><strong>When not to:</strong> never.</p></div>', out)
        self.assertIn('<a href="https://example.com/a">https://example.com/a</a>.', out)
        self.assertIn('href="https://github.com/armandhammer/TBOSS/blob/main/docs/roadmap.md#later"', out)


class ScaleLibrary(unittest.TestCase):
    def test_generated_files_match_the_list(self):
        import subprocess
        import sys
        run = subprocess.run([sys.executable, str(ROOT / 'scripts/gen_scales.py'), '--check'], capture_output=True, text=True,
                             timeout=60)
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
