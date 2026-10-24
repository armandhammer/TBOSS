# SPDX-License-Identifier: GPL-3.0-or-later
"""Start the TBOSS web editor and installer: serve webapp/ on this computer only and open it.

  python scripts/tboss_editor.py            (works from any folder)
  python scripts/tboss_editor.py 8000 install.html     (open the installer instead)
  python scripts/tboss_editor.py 8000 manual.html      (the manual; ./ is the project page)
  python scripts/tboss_editor.py 8000 --no-browser     (serve only)

The installer finds the firmware at firmware/tboss.bin: this server provides the image
built by scripts/tboss_build.py (build/tboss/theoryboard-usb.bin) there. The project page
and the manual are rendered from docs/manual.md on every request (scripts/build_site.py),
so an edit to the manual shows on the next reload.

Chrome and Edge allow MIDI with SysEx only on https:// or http://localhost pages,
which is why the editor is served locally rather than opened as a file.
Press Ctrl+C to stop.
"""
import functools
import hashlib
import http.server
import json
from pathlib import Path
import sys
import threading
import webbrowser

ROOT = Path(__file__).resolve().parents[1]
WEBAPP = ROOT / 'webapp'
IMAGE = ROOT / 'build/tboss/theoryboard-usb.bin'
sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_site  # noqa: E402  (scripts/build_site.py: the project page and the manual)
if {'-h', '--help'} & set(sys.argv[1:]):
    raise SystemExit(__doc__)
ARGS = [a for a in sys.argv[1:] if a != '--no-browser']
OPEN = '--no-browser' not in sys.argv
PORT = int(ARGS[0]) if ARGS else 8000
PAGE = ARGS[1] if len(ARGS) > 1 else 'editor.html'


class Handler(http.server.SimpleHTTPRequestHandler):
    def send_bytes(self, data, content_type):
        self.send_response(200)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        path = self.path.split('?')[0].split('#')[0]
        if path in ('/', '/index.html', '/manual.html'):
            self.send_bytes(build_site.render('manual.html' if path == '/manual.html' else 'index.html').encode(),
                            'text/html; charset=utf-8')
            return
        if path == '/site/tboss-logo.jpg':
            self.send_bytes(build_site.LOGO.read_bytes(), 'image/jpeg')
            return
        if self.path.split('?')[0] in ('/firmware/tboss.json', '/firmware/tboss.bin'):
            if not IMAGE.exists():
                self.send_error(404, 'no local build (python scripts/tboss_build.py)')
                return
            data = IMAGE.read_bytes()
            if self.path.startswith('/firmware/tboss.json'):
                data = json.dumps({'version': 'local build', 'file': 'tboss.bin',
                                   'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)}).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json' if self.path.startswith('/firmware/tboss.json')
                             else 'application/octet-stream')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        super().do_GET()

    def end_headers(self):
        # Never cached: after an update the browser must not mix new pages with old scripts.
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()


def main():
    if not (WEBAPP / 'editor.html').exists():
        raise SystemExit(f'web editor not found in {WEBAPP}')
    Handler.extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, '.js': 'text/javascript'}
    handler = functools.partial(Handler, directory=str(WEBAPP))
    try:
        server = http.server.ThreadingHTTPServer(('127.0.0.1', PORT), handler)
    except OSError:
        raise SystemExit(f'port {PORT} is busy (is the editor already running?). '
                         f'Try another: python scripts/tboss_editor.py {PORT + 1}')
    url = f'http://localhost:{PORT}/{PAGE}'
    print(f'TBOSS editor: {url}  (open it in Chrome or Edge; Ctrl+C stops the server)')
    if OPEN:
        threading.Timer(0.5, webbrowser.open, (url,)).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print('stopped')


if __name__ == '__main__':
    main()
