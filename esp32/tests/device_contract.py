"""USB tests exercise the same dispatch used by HTTP; do not expose secrets."""
import argparse
import json
import sys
import time
import serial

parser = argparse.ArgumentParser()
parser.add_argument('--port', required=True)
parser.add_argument('--timeout', type=float, default=8)
args = parser.parse_args()
connection = serial.Serial(port=None, baudrate=115200, timeout=0.25, dsrdtr=False)
connection.dtr = False
connection.rts = False
connection.port = args.port
with connection:
    time.sleep(0.5)
    connection.reset_input_buffer()
    connection.write(b'SELFTEST\n')
    end = time.monotonic() + args.timeout
    report = None
    while time.monotonic() < end:
        line = connection.readline().decode('utf-8', errors='replace').strip()
        if line.startswith('SELFTEST '):
            report = json.loads(line[9:])
            break
    assert report is not None, 'ESP32 待办固件尚未提供 SELFTEST 业务验证接口'
    assert report['failed'] == 0, report
    assert report['passed'] >= 8, report
    print(json.dumps(report, ensure_ascii=False))
