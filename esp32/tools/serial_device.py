"""Read STATUS or setup-hotspot details locally. Never request home Wi-Fi passwords."""
import argparse
import json
import time
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('action', choices=('status', 'setup'))
parser.add_argument('--port', required=True)
args = parser.parse_args()
connection = serial.Serial(port=None, baudrate=115200, timeout=0.3, dsrdtr=False)
connection.dtr = connection.rts = False
connection.port = args.port
with connection:
    connection.reset_input_buffer()
    command = 'STATUS' if args.action == 'status' else 'CONFIG'
    connection.write((command + '\n').encode())
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        line = connection.readline().decode('utf-8', 'replace').strip()
        if not line.startswith(command + ' '):
            continue
        data = json.loads(line[len(command) + 1:])
        if args.action == 'setup':
            data = {k: data.get(k) for k in ('setup_ssid', 'setup_password', 'setup_url', 'ip')}
        print(json.dumps(data, ensure_ascii=False, indent=2))
        break
    else:
        raise SystemExit('No response. Close serial monitors/bridges and check application boot.')

