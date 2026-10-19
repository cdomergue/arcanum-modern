import re
import struct
import zlib
from pathlib import Path


class Archive:
    def __init__(self, path):
        self.path = Path(path)
        self.entries = {}
        with self.path.open('rb') as f:
            f.seek(-4, 2)
            table_offset, = struct.unpack('<I', f.read(4))
            f.seek(-4 - table_offset, 2)
            table_size, count = struct.unpack('<II', f.read(8))
            base = self.path.stat().st_size - table_size - table_offset
            for _ in range(count):
                length, = struct.unpack('<I', f.read(4))
                name = f.read(length).rstrip(b'\0').decode('cp1252').replace('\\', '/').lower()
                _, flags, size, packed, offset = struct.unpack('<IIIII', f.read(20))
                if not flags & (0x400 | 0x800):
                    self.entries[name] = (flags, size, packed, base + offset)

    def read(self, name):
        flags, size, packed, offset = self.entries[name.lower()]
        with self.path.open('rb') as f:
            f.seek(offset)
            data = f.read(packed if flags & 2 else size)
        if flags & 2:
            data = zlib.decompress(data)
        if len(data) != size:
            raise ValueError(f'Invalid length: {self.path}: {name}')
        return data


def messages(data):
    return {int(k): v.strip() for k, v in re.findall(r'\{(\d+)\}\s*\{([^}]*)\}', data.decode('cp1252'))}


def sector_townmap(data):
    integer = lambda offset: struct.unpack_from('<I', data, offset)[0]
    offset = 4 + integer(0) * 48 + 4096 * 4
    empty = integer(offset)
    offset += 4 + (0 if empty else 256 * 4)
    version = integer(offset)
    if version < 0xAA0003:
        return 0
    if version > 0xAA0004:
        raise ValueError(f'Unknown sector format {version:x}')
    offset += 4
    offset += 4 + integer(offset) * 24
    offset += 12
    return integer(offset)


class Module:
    def __init__(self, game, name):
        game = Path(game)
        self.name = name
        self.files = {}
        self.source_paths = set()
        for archives, root in (
            (sorted(game.glob('*.[dD][aA][tT]'), key=lambda p: p.name.lower()), game / 'data'),
            (sorted((game / 'modules').glob(name + '.*'), key=lambda p: p.suffix.lower()), game / 'modules' / name),
        ):
            for path in archives:
                if path.suffix.lower() != '.dat' and not re.fullmatch(r'\.patch\d+', path.suffix.lower()):
                    continue
                archive = Archive(path)
                self.source_paths.add(path)
                self.files.update({key: (archive, key) for key in archive.entries})
            for path in root.rglob('*'):
                if path.is_file() and path.relative_to(root).parts[0].lower() not in ('save', 'tigcache', 'screenshots'):
                    self.source_paths.add(path)
                    self.files[path.relative_to(root).as_posix().lower()] = path

    def source_snapshot(self):
        return {str(p): (p.stat().st_size, p.stat().st_mtime_ns) for p in sorted(self.source_paths)}

    def read(self, path):
        entry = self.files[path.lower()]
        return entry.read_bytes() if isinstance(entry, Path) else entry[0].read(entry[1])

    def townmaps(self):
        result = []
        for map_id, name in messages(self.read('rules/townmap.mes')).items():
            name = re.split(r'\[w:', name, flags=re.I)[0].strip()
            key = f'townmap/{name}/{name}.tmi'.lower()
            if key not in self.files:
                continue
            fields = struct.unpack('<8iqfi', self.read(key))
            result.append(dict(id=map_id, name=name, map=fields[2], width=fields[3], height=fields[4],
                               columns=fields[5], rows=fields[6], location=fields[8], scale=fields[9]))
        return result

    def catalog(self):
        maps = self.townmaps()
        map_names = [v.split(',')[0].strip() for k, v in sorted(messages(self.read('rules/maplist.mes')).items()) if k >= 5000]
        associations = {}
        for info in maps:
            x, y = info['location'] & 0xffffffff, info['location'] >> 32
            sector = (x // 64) | ((y // 64) << 26)
            for name in map_names:
                key = f'maps/{name}/{sector}.sec'.lower()
                if key in self.files and sector_townmap(self.read(key)) == info['id']:
                    associations[info['id']] = name
        missing = {m['id'] for m in maps} - associations.keys()
        if missing:
            for name in map_names:
                prefix = f'maps/{name}/'.lower()
                for key in self.files:
                    if key.startswith(prefix) and key.endswith('.sec'):
                        map_id = sector_townmap(self.read(key))
                        if map_id in missing:
                            associations[map_id] = name
                            missing.remove(map_id)
                if not missing:
                    break
        for info in maps:
            info['source_map'] = associations.get(info['id'])
        return maps
