#!/usr/bin/env python3
"""Portable checks for rejecting incompatible/non-standalone macOS releases."""
import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location(
    'package_macos', Path(__file__).resolve().parents[1]/'tools/package-macos.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class MacPackageTests(unittest.TestCase):
    def test_native_arm64_executable(self):
        package.validate_macho(struct.pack('<8I', 0xfeedfacf, 0x100000c, 0, 2, 0, 0, 0, 0))

    def test_reject_incompatible_binaries(self):
        for data in (b'', b'\x7fELF'+bytes(64), b'MZ'+bytes(64),
                     struct.pack('<8I', 0xfeedfacf, 0x1000007, 3, 2, 0, 0, 0, 0),
                     struct.pack('<8I', 0xfeedfacf, 0x100000c, 0, 6, 0, 0, 0, 0)):
            with self.subTest(data=data[:16]), self.assertRaises(ValueError):
                package.validate_macho(data)

    def test_system_libraries(self):
        output = ('app:\n'
                  '\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1.0.0)\n'
                  '\t/System/Library/Frameworks/Metal.framework/Versions/A/Metal (compatibility version 1.0.0)\n')
        self.assertEqual(len(package.validate_dependencies(output)), 2)

    def test_reject_development_libraries(self):
        for name in ('/opt/homebrew/lib/libSDL3.dylib', '@rpath/libSDL3.dylib',
                     '@executable_path/libSDL3.dylib', '/usr/local/lib/libSDL3.dylib'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                package.validate_dependencies('app:\n\t'+name+' (compatibility version 1.0.0)\n')


if __name__ == '__main__':
    unittest.main()
