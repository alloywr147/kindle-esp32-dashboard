"""Backup and identity-checked application/first-install flashing. No default COM port."""
import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def run(port, *arguments):
    command = [sys.executable, '-m', 'esptool', '--chip', 'esp32c3', '--port', port,
               '--baud', '460800', '--after', 'no-reset', *map(str, arguments)]
    result = subprocess.run(command, capture_output=True, text=True,
                            encoding='utf-8', errors='replace', timeout=240)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout


def mac(port):
    text = run(port, 'read-mac')
    value = re.search(r'(?i)MAC:\s*([0-9a-f:]{17})', text)
    if not value:
        raise RuntimeError('Could not read board MAC')
    return value[1].lower()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def backup(port, folder):
    if folder.exists():
        raise FileExistsError('Choose a new private backup folder')
    identity = mac(port)
    security = run(port, 'get-security-info')
    if not (re.search(r'Secure Boot:\s*Disabled', security, re.I) and
            re.search(r'Flash Encryption:\s*Disabled', security, re.I)):
        raise RuntimeError('Only unencrypted boards with Secure Boot disabled are supported')
    flash = run(port, 'flash-id')
    if not re.search(r'Detected flash size:\s*4\s*MB', flash, re.I):
        raise RuntimeError('This partition layout is only for 4 MiB boards')
    folder.mkdir(parents=True)
    for i in (1, 2):
        run(port, 'read-flash', '0x0', '0x400000', folder / f'original-{i}.bin')
    first, second = folder / 'original-1.bin', folder / 'original-2.bin'
    if first.stat().st_size != 0x400000 or first.read_bytes() != second.read_bytes():
        raise RuntimeError('The two original flash reads differ; do not flash')
    (folder / 'manifest.json').write_text(json.dumps({
        'mac': identity, 'sha256': digest(first), 'verified': True,
        'flash_bytes': 0x400000, 'secure_boot': False, 'flash_encryption': False,
    }, indent=2), encoding='utf-8')
    reset(port)
    print('Two identical backups saved. Keep the folder private.')


def reset(port):
    subprocess.run([sys.executable, '-m', 'esptool', '--chip', 'esp32c3', '--port', port,
                    '--after', 'watchdog-reset', 'read-mac'], check=True,
                   stdout=subprocess.DEVNULL, timeout=40)


def flash(port, folder, build, mode, first_install):
    data = json.loads((folder / 'manifest.json').read_text(encoding='utf-8'))
    if not data.get('verified') or data.get('flash_bytes') != 0x400000:
        raise ValueError('A verified full backup is required')
    if data.get('secure_boot') is not False or data.get('flash_encryption') is not False:
        raise ValueError('Unsupported board security configuration')
    for i in (1, 2):
        path = folder / f'original-{i}.bin'
        if path.stat().st_size != 0x400000 or digest(path) != data['sha256']:
            raise ValueError('Backup verification failed')
    if mode == 'install' and not first_install:
        raise ValueError('First install overwrites LittleFS. Pass --first-install only for initial setup')
    if mac(port) != data['mac']:
        raise ValueError('Different board; use that board\'s own backup')
    binary = build / 'firmware.bin'
    if not 0 < binary.stat().st_size <= 0x1f0000:
        raise ValueError('Application does not fit the supported partition')
    parts = [('0x10000', binary)]
    if mode == 'install':
        if (build / 'littlefs.bin').stat().st_size != 0x200000:
            raise ValueError('Unexpected filesystem image size')
        parts = [('0x0', build / 'bootloader.bin'), ('0x8000', build / 'partitions.bin'),
                 *parts, ('0x200000', build / 'littlefs.bin')]
    arguments = ['write-flash']
    for offset, path in parts:
        if not path.is_file():
            raise FileNotFoundError(path)
        arguments += [offset, path]
    print(run(port, *arguments))
    reset(port)
    print('Written. Update mode preserves NVS and LittleFS; verify runtime state next.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('backup', 'install', 'update'))
    parser.add_argument('--port', required=True)
    parser.add_argument('--backup', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'esp32/.pio/build/c3mini')
    parser.add_argument('--first-install', action='store_true')
    args = parser.parse_args()
    if args.action == 'backup':
        backup(args.port, args.backup)
    else:
        flash(args.port, args.backup, args.build_dir, args.action, args.first_install)

