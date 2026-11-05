#!/usr/bin/env python3
"""Check FFmpeg download recovery without network access."""
import hashlib
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import urllib.error

spec = importlib.util.spec_from_file_location(
    'ffmpeg_builder', Path(__file__).resolve().parents[1] / 'tools/build-ffmpeg.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class DownloadTests(unittest.TestCase):
    def test_connection_reset_uses_verified_fallback(self):
        payload = b'verified source archive'
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / 'source.tar.xz'

            def fallback(command, **kwargs):
                self.assertTrue(kwargs['check'])
                self.assertIn('--retry-all-errors', command)
                self.assertEqual(command[-1], builder.URL)
                Path(command[command.index('--output') + 1]).write_bytes(payload)

            with patch.object(builder, 'SHA256', hashlib.sha256(payload).hexdigest()), \
                    patch.object(builder.urllib.request, 'urlretrieve',
                                 side_effect=urllib.error.URLError('connection reset')), \
                    patch.object(builder.shutil, 'which', return_value='/usr/bin/curl'), \
                    patch.object(builder.subprocess, 'run', side_effect=fallback):
                builder.download_source(archive)
            self.assertEqual(archive.read_bytes(), payload)
            self.assertFalse(archive.with_suffix('.download').exists())

    def test_bad_checksum_is_not_cached(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / 'source.tar.xz'
            with patch.object(builder.urllib.request, 'urlretrieve',
                              side_effect=lambda url, path: path.write_bytes(b'wrong')):
                with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
                    builder.download_source(archive)
            self.assertFalse(archive.exists())
            self.assertFalse(archive.with_suffix('.download').exists())

    def test_failed_fallback_removes_partial_download(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / 'source.tar.xz'
            temporary = archive.with_suffix('.download')
            temporary.write_bytes(b'partial')
            with patch.object(builder.urllib.request, 'urlretrieve',
                              side_effect=urllib.error.URLError('connection reset')), \
                    patch.object(builder.shutil, 'which', return_value='/usr/bin/curl'), \
                    patch.object(builder.subprocess, 'run',
                                 side_effect=subprocess.CalledProcessError(56, 'curl')):
                with self.assertRaises(subprocess.CalledProcessError):
                    builder.download_source(archive)
            self.assertFalse(archive.exists())
            self.assertFalse(temporary.exists())


if __name__ == '__main__':
    unittest.main()
