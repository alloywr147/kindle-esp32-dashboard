"""Offline tests: no serial devices, LAN calls or flash writes."""
import hashlib
import importlib.util
import json
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class InstallTreeTests(unittest.TestCase):
    def setUp(self):
        self.module = load('prepare_kindle')

    def test_origin_validation(self):
        self.assertEqual(self.module.validate_origin('http://192.168.1.50'), 'http://192.168.1.50')
        self.module.validate_origin('http://kt2-demo.local:8080')
        for value in ('https://example.com', 'http://example.com/', 'http://user:pw@example.com',
                      'http://example.com/path', 'http://example.com#key=x',
                      'http://example.com:99999', 'http://[::1]'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.module.validate_origin(value)

    def fixture(self, folder, unsafe=False):
        fake = folder / 'source'
        (fake / 'kindle/plugin/kt2board.koplugin').mkdir(parents=True)
        (fake / 'kindle/plugin/kt2board.koplugin/main.lua').write_text('return {}')
        for name in ('start-native.sh', 'stop-native.sh'):
            (fake / 'kindle' / name).write_text('#!/bin/sh\n')
        archive = folder / 'runtime.zip'
        with zipfile.ZipFile(archive, 'w') as z:
            z.writestr('koreader/luajit', 'fixture')
            z.writestr('koreader/koreader.sh', '#!/bin/sh\n')
            z.writestr('extensions/koreader/menu.json', '{}')
            if unsafe:
                z.writestr('../escaped.txt', 'must not extract')
        (fake / 'kindle/koreader-release.json').write_text(json.dumps({
            'digest': 'sha256:' + hashlib.sha256(archive.read_bytes()).hexdigest(),
        }))
        return fake, archive

    def test_isolated_tree_config_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as scratch:
            folder = Path(scratch)
            fake, archive = self.fixture(folder)
            output = folder / 'install'
            with patch.object(self.module, 'ROOT', fake):
                self.module.prepare(archive, output, 'http://192.168.1.50')
                config = json.loads((output / 'kt2-dashboard/native/koreader/kt2-config.json').read_text())
                self.assertEqual(config['base_url'], 'http://192.168.1.50')
                self.assertEqual(config['test_duration'], 0)
                self.assertFalse(config['enable_test_commands'])
                self.assertNotIn('access_key', config)
                self.assertFalse((output / 'kt2-dashboard/native/extensions').exists())
                self.assertTrue((output / 'documents/Start Native KT2 Board.sh').is_file())
                with self.assertRaises(FileExistsError):
                    self.module.prepare(archive, output, 'http://192.168.1.50')

    def test_archive_traversal_rejected(self):
        with tempfile.TemporaryDirectory() as scratch:
            folder = Path(scratch)
            fake, archive = self.fixture(folder, unsafe=True)
            with patch.object(self.module, 'ROOT', fake), self.assertRaises(ValueError):
                self.module.prepare(archive, folder / 'install', 'http://192.168.1.50')
            self.assertFalse((folder / 'escaped.txt').exists())

    def test_checksum_mismatch_rejected_before_output(self):
        with tempfile.TemporaryDirectory() as scratch:
            folder = Path(scratch)
            fake, archive = self.fixture(folder)
            with archive.open('ab') as f:
                f.write(b'changed')
            with patch.object(self.module, 'ROOT', fake), self.assertRaises(ValueError):
                self.module.prepare(archive, folder / 'install', 'http://192.168.1.50')
            self.assertFalse((folder / 'install').exists())


class FlashPlanTests(unittest.TestCase):
    def test_update_only_targets_application_and_checks_identity(self):
        module = load('esp_flash')
        with tempfile.TemporaryDirectory() as scratch:
            folder = Path(scratch)
            backup, build = folder / 'backup', folder / 'build'
            backup.mkdir()
            build.mkdir()
            original = bytes(0x400000)
            for i in (1, 2):
                (backup / f'original-{i}.bin').write_bytes(original)
            # Synthetic test MAC only; not derived from any physical board.
            identity = '00:00:00:00:00:00'
            (backup / 'manifest.json').write_text(json.dumps({
                'verified': True, 'flash_bytes': 0x400000, 'mac': identity,
                'sha256': hashlib.sha256(original).hexdigest(),
                'secure_boot': False, 'flash_encryption': False,
            }))
            (build / 'firmware.bin').write_bytes(b'fixture')
            with patch.object(module, 'mac', return_value=identity), \
                    patch.object(module, 'run', return_value='verified') as command, \
                    patch.object(module, 'reset'):
                module.flash('TEST', backup, build, 'update', False)
                command.assert_called_once_with('TEST', 'write-flash', '0x10000', build / 'firmware.bin')
            with patch.object(module, 'mac', return_value='different'), \
                    patch.object(module, 'run') as command, self.assertRaises(ValueError):
                module.flash('TEST', backup, build, 'update', False)
            command.assert_not_called()
            with self.assertRaises(ValueError):
                module.flash('TEST', backup, build, 'install', False)


if __name__ == '__main__':
    unittest.main()
