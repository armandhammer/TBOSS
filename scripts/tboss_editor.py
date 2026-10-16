# SPDX-License-Identifier: GPL-3.0-or-later
"""Start the TBOSS web editor: serve webapp/ on this computer only and open it.

  python scripts/tboss_editor.py            (works from any folder)

Chrome and Edge allow MIDI with SysEx only on https:// or http://localhost pages,
which is why the editor is served locally rather than opened as a file.
Press Ctrl+C to stop.
"""
import functools
import http.server
from pathlib import Path
import sys
import threading
import webbrowser

WEBAPP = Path(__file__).resolve().parents[1] / 'webapp'
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8000


def main():
    if not (WEBAPP / 'index.html').exists():
        raise SystemExit(f'web editor not found in {WEBAPP}')
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(WEBAPP))
    handler.extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, '.js': 'text/javascript'}
    try:
        server = http.server.ThreadingHTTPServer(('127.0.0.1', PORT), handler)
    except OSError:
        raise SystemExit(f'port {PORT} is busy (is the editor already running?). '
                         f'Try another: python scripts/tboss_editor.py {PORT + 1}')
    url = f'http://localhost:{PORT}/'
    print(f'TBOSS editor: {url}  (open it in Chrome or Edge; Ctrl+C stops the server)')
    threading.Timer(0.5, webbrowser.open, (url,)).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print('stopped')


if __name__ == '__main__':
    main()
