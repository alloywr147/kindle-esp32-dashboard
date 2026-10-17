"""Create a LOCAL USB-install tree; never download, connect to, or alter a device."""
import argparse
import hashlib
import json
import shutil
import stat
import tempfile
import zipfile
from pathlib import Path
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]


def validate_origin(value):
    parsed = urlsplit(value)
    if (parsed.scheme != 'http' or not parsed.hostname or parsed.username or
            parsed.password or parsed.path or parsed.query or parsed.fragment):
        raise ValueError('Use the ESP HTTP origin, e.g. http://192.168.1.50 (no final slash)')
    # This widget supports IPv4 / hostname origins, not bracketed IPv6.
    import re
    if not re.fullmatch(r'http://[A-Za-z0-9.-]+(?::[0-9]+)?', value):
        raise ValueError('Use an IPv4 address or hostname, with an optional port')
    if parsed.port is not None and not 1 <= parsed.port <= 65535:
        raise ValueError('Invalid port')
    return value


def prepare(archive, output, esp_url):
    esp_url = validate_origin(esp_url)
    metadata = json.loads((ROOT / 'kindle/koreader-release.json').read_text(encoding='utf-8'))
    expected = metadata['digest'].split(':', 1)[1]
    if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
        raise ValueError('KOReader archive SHA256 mismatch; obtain the pinned official release')
    if output.exists():
        raise FileExistsError('Output already exists; choose a new empty output path')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent) as scratch:
        stage = Path(scratch)
        target = stage / 'kt2-dashboard/native'
        target.mkdir(parents=True)
        with zipfile.ZipFile(archive) as z:
            if sum(i.file_size for i in z.infolist()) > 250 * 1024 * 1024:
                raise ValueError('Unexpected archive size')
            for item in z.infolist():
                parts = item.filename.replace('\\', '/').split('/')
                if (not parts or parts[0] not in {'koreader', 'extensions'} or '..' in parts or
                        ':' in item.filename or stat.S_ISLNK(item.external_attr >> 16)):
                    raise ValueError('Unexpected or unsafe archive entry')
            # Upstream also ships KUAL entries pointing to its default install.
            # We install only the runtime; our own isolated launcher replaces those entries.
            z.extractall(target, members=[i for i in z.infolist()
                                         if i.filename.startswith('koreader/')])
        engine = target / 'koreader'
        if not (engine / 'luajit').is_file() or not (engine / 'koreader.sh').is_file():
            raise ValueError('KOReader runtime not found at the expected archive location')
        shutil.copytree(ROOT / 'kindle/plugin/kt2board.koplugin',
                        engine / 'plugins/kt2board.koplugin')
        (engine / 'kt2-config.json').write_text(json.dumps({
            'base_url': esp_url, 'poll_seconds': 5, 'test_duration': 0,
            'enable_test_commands': False,
        }, indent=2), encoding='utf-8')
        for name in ('start-native.sh', 'stop-native.sh'):
            shutil.copy2(ROOT / 'kindle' / name, target / name)
        docs = stage / 'documents'
        docs.mkdir()
        for action in ('Start', 'Stop'):
            script = action.lower() + '-native.sh'
            (docs / (action + ' Native KT2 Board.sh')).write_text(
                '#!/bin/sh\nnohup sh /mnt/us/kt2-dashboard/native/' + script +
                ' >/mnt/us/kt2-dashboard/native/' + action.lower() +
                '.log 2>&1 </dev/null &\n', encoding='utf-8', newline='\n')
        # rename within the same parent; an existing destination is never overwritten.
        stage.rename(output)
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--koreader-zip', type=Path, required=True)
    parser.add_argument('--esp-url', required=True)
    parser.add_argument('--output', type=Path, required=True,
                        help='Private local output, e.g. private/kindle-usb')
    args = parser.parse_args()
    print('Prepared local USB tree:', prepare(args.koreader_zip, args.output, args.esp_url))

