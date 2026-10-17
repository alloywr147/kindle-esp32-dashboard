import importlib.util
import json
import struct
import tempfile
import unittest
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class CalendarContract(unittest.TestCase):
    def test_leap_day_records_are_random_access_and_keep_yi_ji(self):
        path = ROOT / 'tools' / 'generate_calendar.py'
        self.assertTrue(path.exists(), '黄历数据生成器尚未实现')
        spec = importlib.util.spec_from_file_location('calendar_generator', path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as folder:
            module.generate(Path(folder), date(2028, 2, 28), date(2028, 3, 1))
            raw = (Path(folder) / 'calendar.dat').read_bytes()
            index = (Path(folder) / 'calendar.idx').read_bytes()
            self.assertEqual(len(index), 4 * 4)
            offsets = struct.unpack('<4I', index)
            records = [json.loads(raw[offsets[i]:offsets[i+1]]) for i in range(3)]
            self.assertEqual([x['date'] for x in records], ['2028-02-28', '2028-02-29', '2028-03-01'])
            self.assertTrue(all('yi' in x and 'ji' in x and x['lunar_date'] for x in records))
            self.assertTrue(all(len(x['yi']) <= 4 and len(x['ji']) <= 4 for x in records))
            self.assertTrue(all(len(raw[offsets[i]:offsets[i+1]]) < 2048 for i in range(3)))


if __name__ == '__main__':
    unittest.main()
