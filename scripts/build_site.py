# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the TBOSS website into one folder: the project page, the manual as one styled
page with a table of contents, and the installer and web editor (GitHub Pages publishes it).

  python scripts/build_site.py --out site

The manual stays docs/manual.md; a small converter here turns the Markdown it uses
(headings, paragraphs, lists, tables, code blocks, quotes, links, bold, italic, code)
into HTML, so the build needs nothing beyond Python. Headings get GitHub's anchors, so a
link into the manual works on GitHub and on the site alike. "Try it", "Why use it" and
"When not to" become boxes. scripts/tboss_editor.py serves the same pages while you
work on them (render()).
"""
import argparse
import html
from pathlib import Path
import re
import shutil
import unicodedata

ROOT = Path(__file__).resolve().parents[1]
WEBAPP = ROOT / 'webapp'
TEMPLATES = WEBAPP / 'site'
REPO = 'https://github.com/armandhammer/TBOSS'
SITE_URL = 'https://armandhammer.github.io/TBOSS/'


def _first(*paths):
    return next((p for p in paths if p.exists()), paths[0])


# The public repository keeps the manual and logo in docs/; the development tree in docs/tboss/.
MANUAL = _first(ROOT / 'docs/manual.md', ROOT / 'docs/tboss/manual.md')
ROADMAP = ROOT / 'docs/roadmap.md'
LOGO = _first(ROOT / 'docs/assets/tboss-logo.jpg', ROOT / 'docs/tboss/assets/tboss-logo.jpg')
PAGES = ('index.html', 'manual.html')  # rendered; everything else in webapp/ is copied as it is

# --- inline Markdown ------------------------------------------------------------------------
LABEL = re.compile(r'(?:<a id="[\w-]+"></a>)?\*\*(Try it[^*]*|Why use it|When not to):\*\*')
KINDS = {'Try it': 'try', 'Why use it': 'why', 'When not to': 'whennot'}


def url_for(url, anchors_in):
    """Where a link in the manual goes on the site: its own anchors on the page they are on,
    the other docs on GitHub, the site's own pages relative (so a local preview stays local)."""
    if url.startswith('#'):
        return anchors_in + url
    if url.startswith(SITE_URL):
        return url[len(SITE_URL):] or 'index.html'
    m = re.fullmatch(r'([\w-]+)\.md(#.*)?', url)
    if m:
        if m.group(1) == 'manual':
            return anchors_in + (m.group(2) or '#')
        if m.group(1) == 'roadmap' and not m.group(2):
            return anchors_in + '#roadmap'  # the manual's Roadmap chapter
        return f'{REPO}/blob/main/docs/{m.group(1)}.md{m.group(2) or ""}'
    return url


def inline(text, anchors_in=''):
    kept = []

    def keep(fragment):
        kept.append(fragment)
        return f'\x00{len(kept) - 1}\x00'

    text = re.sub(r'`([^`]+)`', lambda m: keep(f'<code>{html.escape(m.group(1), quote=False)}</code>'), text)
    text = re.sub(r'<a id="([\w-]+)"></a>', lambda m: keep(f'<a id="{m.group(1)}"></a>'), text)
    text = re.sub(r'\[([^\]]+)\]\(([^)\s]+)\)', lambda m: keep(
        f'<a href="{html.escape(url_for(m.group(2), anchors_in))}">{emphasis(html.escape(m.group(1), quote=False))}</a>'),
        text)
    text = html.escape(text, quote=False)

    def autolink(m):
        url = m.group(0).rstrip('.,;:')
        return keep(f'<a href="{html.escape(url_for(url, anchors_in))}">{url}</a>') + m.group(0)[len(url):]
    text = re.sub(r'https?://[^\s<>\x00*]+', autolink, text)
    text = emphasis(text)
    while '\x00' in text:
        text = re.sub(r'\x00(\d+)\x00', lambda m: kept[int(m.group(1))], text)
    return text


def emphasis(text):
    text = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', text)
    return re.sub(r'(?<![\w*])\*(?![\s*])(.+?)(?<![\s*])\*(?![\w*])', r'<em>\1</em>', text)


def plain(fragment):
    return html.unescape(re.sub(r'<[^>]+>', '', fragment))


class Slugger:
    """GitHub's heading anchors: lower case, letters, digits, '-' and '_' kept, each space a
    '-', everything else dropped; a repeated one gets -1, -2 ..."""
    def __init__(self):
        self.seen = {}

    def __call__(self, text):
        base = ''.join('-' if c == ' ' else c for c in text.strip().lower()
                       if c in ' -_' or unicodedata.category(c)[0] in 'LN' or unicodedata.category(c) in ('Mn', 'Mc'))
        n = self.seen.get(base, 0)
        self.seen[base] = n + 1
        return base if not n else f'{base}-{n}'


# --- blocks ------------------------------------------------------------------------------------
LIST = re.compile(r'^( {0,3})([-*+]|(\d+)([a-z]?)\.)\s+(.*)$')
HEADING = re.compile(r'^(#{1,6})\s+(.*?)\s*#*\s*$')


def indent(line):
    return len(line) - len(line.lstrip(' '))


def starts_block(line):
    s = line.lstrip()
    return bool(HEADING.match(line) or s.startswith('```') or s.startswith('|') or s.startswith('>') or LIST.match(line))


class Converter:
    def __init__(self, anchors_in=''):
        self.slug = Slugger()
        self.toc = []          # (level, id, html of the heading text)
        self.title = ''
        self.anchors_in = anchors_in

    def inline(self, text):
        return inline(text, self.anchors_in)

    def paragraph(self, text, tight):
        """A paragraph, split into boxes where Try it / Why use it / When not to start."""
        parts, last = [], 0
        for m in LABEL.finditer(text):
            parts.append((None, text[last:m.start()]))
            last = m.start()
        parts.append((None, text[last:]))
        out = []
        for _, chunk in parts:
            chunk = chunk.strip()
            if not chunk:
                continue
            m = LABEL.match(chunk)
            if m:
                kind = KINDS[next(k for k in KINDS if m.group(1).startswith(k))]
                out.append(f'<div class="note {kind}"><p>{self.inline(chunk)}</p></div>')
            else:
                out.append(self.inline(chunk) if tight and len(parts) == 1 else f'<p>{self.inline(chunk)}</p>')
        return '\n'.join(out)

    def heading(self, level, text):
        rendered = self.inline(text)
        anchor = self.slug(plain(rendered))
        # *(alpha)*: a small tag after the heading text; *(optional)* left out (the section says so)
        shown = re.sub(r'\s*<em>\(([^)]+)\)</em>',
                       lambda m: '' if m.group(1).lower() == 'optional' else f' <span class="tag">{m.group(1)}</span>', rendered)
        if level == 1:
            self.title = plain(shown)
            return f'<h1 id="{anchor}">{shown}</h1>'
        if level in (2, 3):
            self.toc.append((level, anchor, shown))
        return f'<h{level} id="{anchor}"><a class="anchor" href="#{anchor}" aria-label="Link to this section">#</a>{shown}</h{level}>'

    def table(self, rows):
        cells = [[c.strip() for c in r.strip().strip('|').split('|')] for r in rows]
        align = ['center' if c.startswith(':') and c.endswith(':') else 'right' if c.endswith(':') else ''
                 for c in cells[1]]
        def row(r, tag):
            styles = [f' style="text-align:{align[i]}"' if i < len(align) and align[i] else '' for i in range(len(r))]
            return '<tr>' + ''.join(f'<{tag}{styles[i]}>{self.inline(c)}</{tag}>' for i, c in enumerate(r)) + '</tr>'
        body = '\n'.join(row(r, 'td') for r in cells[2:])
        return f'<div class="table"><table>\n<thead>{row(cells[0], "th")}</thead>\n<tbody>\n{body}\n</tbody></table></div>'

    def list(self, lines, i):
        m = LIST.match(lines[i])
        base, ordered = len(m.group(1)), m.group(3) is not None
        items, loose = [], False
        while i < len(lines):
            m = LIST.match(lines[i])
            if not m or len(m.group(1)) != base or (m.group(3) is not None) != ordered:
                break
            col = m.start(5)
            content, label = [m.group(5)], m.group(2)
            i += 1
            while i < len(lines):
                line = lines[i]
                if not line.strip():
                    j = i
                    while j < len(lines) and not lines[j].strip():
                        j += 1
                    if j < len(lines) and indent(lines[j]) >= col:
                        content.extend([''] * (j - i))
                        i = j
                        loose = True
                        continue
                    break
                inner = LIST.match(line)
                if inner and len(inner.group(1)) <= base:
                    break  # the next item, or the outer list's
                if indent(line) >= col or inner:
                    content.append(line[min(indent(line), col):])
                elif starts_block(line):
                    break
                else:
                    content.append(line.strip())  # a wrapped line of the item's text
                i += 1
            items.append((label, content))
            j = i
            while j < len(lines) and not lines[j].strip():
                j += 1
            nxt = LIST.match(lines[j]) if j < len(lines) else None
            if j > i and nxt and len(nxt.group(1)) == base and (nxt.group(3) is not None) == ordered:
                i, loose = j, True
        html_items = []
        for label, content in items:
            body = self.blocks(content, tight=not loose)
            if ordered:
                html_items.append(f'<li><span class="n">{html.escape(label.rstrip("."))}</span><div>{body}</div></li>')
            else:
                html_items.append(f'<li>{body}</li>')
        tag = 'ol class="steps"' if ordered else 'ul'
        return f'<{tag}>\n' + '\n'.join(html_items) + f'\n</{tag.split()[0]}>', i

    def blocks(self, lines, tight=False):
        out, i = [], 0
        while i < len(lines):
            line = lines[i]
            stripped = line.strip()
            if not stripped:
                i += 1
                continue
            if stripped.startswith('```'):
                j = i + 1
                while j < len(lines) and not lines[j].strip().startswith('```'):
                    j += 1
                code = '\n'.join(lines[i + 1:j])
                out.append(f'<pre><code>{html.escape(code, quote=False)}</code></pre>')
                i = j + 1
                continue
            m = HEADING.match(line)
            if m:
                out.append(self.heading(len(m.group(1)), m.group(2)))
                i += 1
                continue
            if re.fullmatch(r'-{3,}|\*{3,}', stripped):
                out.append('<hr>')
                i += 1
                continue
            if stripped.startswith('|') and i + 1 < len(lines) and re.match(r'^\s*\|?\s*:?-{3,}', lines[i + 1]):
                j = i
                while j < len(lines) and lines[j].strip().startswith('|'):
                    j += 1
                out.append(self.table(lines[i:j]))
                i = j
                continue
            if stripped.startswith('>'):
                j = i
                while j < len(lines) and lines[j].strip().startswith('>'):
                    j += 1
                quoted = [re.sub(r'^\s*> ?', '', l) for l in lines[i:j]]
                out.append(f'<blockquote>{self.blocks(quoted)}</blockquote>')
                i = j
                continue
            if LIST.match(line):
                fragment, i = self.list(lines, i)
                out.append(fragment)
                continue
            j = i + 1
            while j < len(lines) and lines[j].strip() and not starts_block(lines[j]):
                j += 1
            out.append(self.paragraph(' '.join(l.strip() for l in lines[i:j]), tight))
            i = j
        return '\n'.join(out)


# --- pages -----------------------------------------------------------------------------------
def manual_parts(markdown):
    """The manual without its own contents line (the sidebar has one)."""
    lines = markdown.replace('\r\n', '\n').split('\n')
    out, skipping = [], False
    for line in lines:
        if line.startswith('Contents: '):
            skipping = True
        if skipping:
            if not line.strip():
                skipping = False
            continue
        out.append(line)
    return out


def toc_html(toc):
    out, open_sub = ['<ul>'], False
    for level, anchor, shown in toc:
        if level == 2:
            if open_sub:
                out.append('</ul></li>')
                open_sub = False
            elif len(out) > 1:
                out.append('</li>')
            out.append(f'<li><a href="#{anchor}">{shown}</a>')
        else:
            if not open_sub:
                out.append('<ul>')
                open_sub = True
            out.append(f'<li><a href="#{anchor}">{shown}</a></li>')
    out.append('</ul></li></ul>' if open_sub else '</li></ul>')
    return '\n'.join(out)


def roadmap_sections(markdown):
    """roadmap.md's sections, each {'title' (without its dates), 'planned', 'built', 'ideas'}: the text
    of its "- [ ]", "- [x]" and plain "- " items (wrapped lines joined)."""
    sections, item = [], None
    for line in markdown.replace('\r\n', '\n').split('\n'):
        if line.startswith('## '):
            sections.append({'title': re.sub(r'\s*\([^)]*\)', '', line[3:]).strip(), 'planned': [], 'built': [], 'ideas': []})
            item = None
            continue
        m = re.match(r'^- (?:\[([ xX])\] )?(.*)$', line)
        if m and sections:
            item = [m.group(2)]
            sections[-1]['ideas' if m.group(1) is None else 'planned' if m.group(1) == ' ' else 'built'].append(item)
        elif item is not None and line.startswith('  ') and line.strip():
            item.append(line.strip())
        else:
            item = None
    return [{k: v if k == 'title' else [' '.join(i) for i in v] for k, v in s.items()} for s in sections]


def roadmap_html(c):
    """The manual's Roadmap chapter, from docs/roadmap.md: what is planned, the ideas not yet scheduled,
    and (folded away) everything already built, with where to ask for something new."""
    sections = roadmap_sections(ROADMAP.read_text(encoding='utf-8')) if ROADMAP.exists() else []

    def group(title, items, kind):
        return (f'<p class="group">{c.inline(title)}</p>\n<ul class="roadmap {kind}">\n' +
                '\n'.join(f'<li>{c.inline(i)}</li>' for i in items) + '\n</ul>')
    out = [c.heading(2, 'Roadmap'),
           '<div class="note request"><p><strong>Missing a feature?</strong> Look here first: <strong>Planned</strong> '
           'is on its way, <strong>Ideas</strong> may come later, and <strong>Already built</strong> is in TBOSS now '
           f'(the contents on the left find it in this manual). Not here? Search the <a href="{REPO}/discussions">'
           f'Discussions</a> and <a href="{REPO}/issues">Issues</a>, then '
           f'<a href="{REPO}/issues/new?template=feature_request.yml">request it</a>.</p></div>',
           c.heading(3, 'Planned')]
    out += [group(s['title'], s['planned'], 'planned') for s in sections if s['planned']]
    ideas = [i for s in sections if s['title'].lower().startswith('other ideas') for i in s['ideas']]
    if ideas:
        out += [c.heading(3, 'Ideas, not scheduled'), group('Possible later; nothing decided yet', ideas, 'ideas')]
    built = [s for s in sections if s['built']]
    out += [c.heading(3, 'Already built'),
            f'<details class="built"><summary>Show all {sum(len(s["built"]) for s in built)}, release by release'
            '</summary>\n' + '\n'.join(group(s['title'], s['built'], 'built') for s in built) + '\n</details>',
            f'<p>The whole roadmap, with the reasons behind each release: '
            f'<a href="{REPO}/blob/main/docs/roadmap.md">roadmap.md</a>.</p>']
    return '\n'.join(out)


def manual_html(markdown):
    """(article html, toc html, list of (level, id, heading html)). The Roadmap chapter comes first,
    right after the introduction."""
    c = Converter()
    lines = manual_parts(markdown)
    first = next((i for i, l in enumerate(lines) if l.startswith('## ')), len(lines))
    body = '\n'.join([c.blocks(lines[:first]), roadmap_html(c), c.blocks(lines[first:])])
    body = body.replace('<hr>\n<h2', '<h2')  # a chapter has its own rule
    return body, toc_html(c.toc), c.toc


def news(markdown):
    """The manual's newest "New in" section for the project page: (title, html)."""
    lines = markdown.replace('\r\n', '\n').split('\n')
    start = next(i for i, l in enumerate(lines) if l.startswith('## New in'))
    end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith('## ')), len(lines))
    c = Converter(anchors_in='manual.html')
    title = plain(inline(lines[start][3:]))
    return title, c.blocks(lines[start + 1:end])


def render(page):
    """The HTML of a rendered page (index.html or manual.html)."""
    markdown = MANUAL.read_text(encoding='utf-8')
    if page == 'manual.html':
        body, toc, _ = manual_html(markdown)
        return (TEMPLATES / 'manual.html').read_text(encoding='utf-8').replace('{{toc}}', toc).replace('{{content}}', body)
    title, news_html = news(markdown)
    anchor = Slugger()(title)
    return (TEMPLATES / 'home.html').read_text(encoding='utf-8').replace('{{news_title}}', html.escape(title)) \
        .replace('{{news_anchor}}', anchor).replace('{{news}}', news_html)


def build(out):
    if out.exists():
        shutil.rmtree(out)
    shutil.copytree(WEBAPP, out, ignore=lambda folder, names: [n for n in names if n == '__pycache__'])
    for template in ('home.html', 'manual.html'):
        (out / 'site' / template).unlink()
    shutil.copyfile(LOGO, out / 'site' / 'tboss-logo.jpg')
    for page in PAGES:
        (out / page).write_text(render(page), encoding='utf-8', newline='\n')


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--out', type=Path, required=True, help='folder to write the site into (replaced)')
    args = p.parse_args()
    build(args.out)
    print(f'site written to {args.out}: ' + ', '.join(PAGES) + ', the installer and the editor')


if __name__ == '__main__':
    main()
