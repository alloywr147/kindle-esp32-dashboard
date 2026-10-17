"""Check source-only publication contents; do not read external private directories."""
import json
import ipaddress
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP = {'.git', '.pio', '__pycache__', '.venv', 'node_modules', '.playwright-cli'}


def check(root=ROOT):
    problems, count = [], 0
    for p in root.rglob('*'):
        rel = p.relative_to(root)
        if any(x in SKIP for x in rel.parts) or not p.is_file():
            continue
        count += 1
        if (any(x in {'private', 'private-backup', 'backups'} for x in rel.parts) or
                p.suffix.lower() in {'.bin', '.elf', '.raw', '.log'} or
                p.name in {'kt2-config.json', 'kt2-last-state.json', 'state.json', 'weather.json',
                           'known_hosts', '.env', 'id_rsa', 'id_ed25519'}):
            problems.append(str(rel) + ': local/device/generated file')
        raw = p.read_bytes()
        if b'PRIVATE' + b' KEY-----' in raw:
            problems.append(str(rel) + ': private-key material')
        if p.suffix not in {'.dat', '.idx'}:
            text = raw.decode('utf-8', 'replace')
            for address in re.findall(r'(?<![\w.])(?:\d{1,3}\.){3}\d{1,3}(?![\w.])', text):
                try:
                    ip = ipaddress.ip_address(address)
                except ValueError:
                    continue
                if ip.is_private and address not in {'192.168.1.50', '192.168.4.1', '127.0.0.1', '0.0.0.0'}:
                    problems.append(str(rel) + ': unexpected private address')
            if re.search(r'(?i)[A-Z]:[\\/]Users[\\/][^\\/\s]+', text):
                problems.append(str(rel) + ': personal absolute path')
            if re.search(r'(?i)(?:access_key|wifi_password|wifi_pass)\s*[=:]\s*[\"\x27][^\"\x27]{8,}', text):
                problems.append(str(rel) + ': credential-looking literal')
        if p.suffix == '.sh' and b'\r' in raw:
            problems.append(str(rel) + ': shell script must use LF')
    data = root / 'esp32/data'
    raw, idx = (data / 'calendar.dat').read_bytes(), (data / 'calendar.idx').read_bytes()
    offsets = struct.unpack('<' + 'I' * (len(idx) // 4), idx)
    if offsets[0] != 0 or offsets[-1] != len(raw):
        problems.append('calendar index mismatch')
    for a, b in zip(offsets, offsets[1:]):
        record = json.loads(raw[a:b])
        if 'tasks' in record or 'password' in record or 'ssid' in record:
            problems.append('calendar contains non-calendar records')
            break
    return count, problems


if __name__ == '__main__':
    count, problems = check()
    for problem in problems:
        print(problem)
    print(f'Checked {count} source/data files; {len(problems)} findings')
    sys.exit(bool(problems))

