"""Generate the exact PC almanac fields as compact, indexed, read-only data."""
import argparse
import json
import struct
import sys
from dataclasses import asdict
from datetime import date, timedelta
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from almanac_source import calculate_almanac, ALMANAC_CALCULATION_VERSION


def generate(target: Path, start: date, end: date):
    if end < start:
        raise ValueError('end before start')
    target.mkdir(parents=True, exist_ok=True)
    offsets = [0]
    with (target / 'calendar.dat').open('wb') as stream:
        current = start
        while current <= end:
            value = asdict(calculate_almanac(current))
            value.pop('displayed_on')
            value['date'] = current.isoformat()
            encoded = json.dumps(value, ensure_ascii=False, separators=(',', ':')).encode('utf-8')
            if len(encoded) >= 2048:
                raise ValueError('calendar record exceeds device bound')
            stream.write(encoded)
            offsets.append(stream.tell())
            current += timedelta(days=1)
    (target / 'calendar.idx').write_bytes(struct.pack(f'<{len(offsets)}I', *offsets))
    metadata = {'start': start.isoformat(), 'end': end.isoformat(), 'days': len(offsets)-1,
                'bytes': offsets[-1], 'calculation': ALMANAC_CALCULATION_VERSION}
    (target / 'calendar-meta.json').write_text(json.dumps(metadata, ensure_ascii=False), encoding='utf-8')
    print(json.dumps(metadata, ensure_ascii=False))
    return metadata


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'data')
    args = parser.parse_args()
    generate(args.output, date(2026, 1, 1), date(2035, 12, 31))
