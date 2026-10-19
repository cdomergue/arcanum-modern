#!/usr/bin/env python3
import argparse
import json
import struct
import zlib
from pathlib import Path


def verify_png(path, width, height):
    data = path.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError(f'Invalid PNG: {path}')
    offset, chunks = 8, []
    while offset < len(data):
        length, = struct.unpack_from('>I', data, offset)
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        expected, = struct.unpack_from('>I', data, offset + 8 + length)
        if zlib.crc32(kind + payload) != expected:
            raise ValueError(f'CRC mismatch: {path}')
        if kind == b'IHDR' and struct.unpack('>IIBBBBB', payload) != (width, height, 8, 2, 0, 0, 0):
            raise ValueError(f'Wrong tile geometry: {path}')
        chunks.append(kind)
        offset += length + 12
    if chunks != [b'IHDR', b'IDAT', b'IEND'] or offset != len(data):
        raise ValueError(f'Unexpected chunks (text or incomplete image): {path}')
    return len(data)


def verify(root):
    total_maps = total_tiles = total_bytes = 0
    for manifest_path in sorted(root.glob('*/manifest.json')):
        manifest = json.loads(manifest_path.read_text())
        if manifest.get('roofs') is not True:
            raise ValueError(f'Cache does not preserve roofs: {manifest_path}')
        module_tiles = 0
        for info in manifest['maps']:
            folder = manifest_path.parent / str(info['id'])
            expected_marker = f"1 {info['width']} {info['height']} 1024 4\n"
            if (folder / 'complete').read_text() != expected_marker:
                raise ValueError(f'Incomplete map: {folder}')
            expected_files = set()
            for y in range(0, info['height'], 1024):
                for x in range(0, info['width'], 1024):
                    for level in range(4):
                        step = 1 << level
                        filename = f'{level}_{x // 1024}_{y // 1024}.png'
                        expected_files.add(filename)
                        width = (min(1024, info['width'] - x) + step - 1) // step
                        height = (min(1024, info['height'] - y) + step - 1) // step
                        total_bytes += verify_png(folder / filename, width, height)
            actual_files = {p.name for p in folder.glob('*_*_*.png')}
            if actual_files != expected_files:
                raise ValueError(f'Unexpected tiles: {folder}')
            module_tiles += len(expected_files)
            total_maps += 1
        total_tiles += module_tiles
        print(f"{manifest['module']}: {len(manifest['maps'])} maps, {module_tiles} verified PNGs", flush=True)
    if not total_maps:
        raise ValueError('No map manifests found')
    report = dict(maps=total_maps, png_tiles=total_tiles, bytes=total_bytes,
                  png_crc='all passed', dimensions='all passed', png_text_chunks=0, roofs=True)
    (root / 'verification.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('root', type=Path, nargs='?', default=Path('dist/local-map-cache'))
    verify(parser.parse_args().root)
