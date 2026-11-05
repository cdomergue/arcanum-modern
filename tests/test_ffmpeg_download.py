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
helper_spec = importlib.util.spec_from_file_location(
    'uap_builder', Path(__file__).resolve().parents[1] / 'tools/build-uap.py')
helper = importlib.util.module_from_spec(helper_spec)
helper_spec.loader.exec_module(helper)


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


class HelperDownloadTests(unittest.TestCase):
    def test_gateway_timeout_fallback_and_cached_result(self):
        payload = b'verified build tool'
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            digest = hashlib.sha256(payload).hexdigest()

            def fallback(command, **kwargs):
                self.assertTrue(kwargs['check'])
                self.assertIn('--retry-all-errors', command)
                Path(command[command.index('--output') + 1]).write_bytes(payload)

            with patch.object(helper.urllib.request, 'urlopen',
                              side_effect=urllib.error.HTTPError('https://example.test/tool', 504, 'timeout', {}, None)) as fetch, \
                    patch.object(helper.shutil, 'which', return_value='/usr/bin/curl'), \
                    patch.object(helper.subprocess, 'run', side_effect=fallback):
                archive = helper.download(cache, 'tool.tar.gz', 'https://example.test/tool', digest)
                self.assertEqual(archive.read_bytes(), payload)
                self.assertEqual(helper.download(cache, archive.name, 'https://example.test/tool', digest), archive)
                fetch.assert_called_once()
            self.assertEqual(list(cache.iterdir()), [archive])

    def test_fallback_checksum_failure_does_not_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)

            def fallback(command, **kwargs):
                Path(command[command.index('--output') + 1]).write_bytes(b'wrong')

            with patch.object(helper.urllib.request, 'urlopen', side_effect=urllib.error.URLError('reset')), \
                    patch.object(helper.shutil, 'which', return_value='/usr/bin/curl'), \
                    patch.object(helper.subprocess, 'run', side_effect=fallback):
                with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
                    helper.download(cache, 'tool.tar.gz', 'https://example.test/tool', '0' * 64)
            self.assertEqual(list(cache.iterdir()), [])


if __name__ == '__main__':
    unittest.main()
