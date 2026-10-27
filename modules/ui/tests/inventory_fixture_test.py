"""Generate console lines from a read-only *copy* of Skyrim.esm; never launch/deploy.

With no CLI arguments this is a portable unittest under discovery. The owner
creates a new output directory outside the repository and reviews the manifest.
No game binary, localized string table, name or art is copied to the output.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import unittest
import zlib


def subrecords(data):
    offset = 0
    extended = None
    fields = {}
    while offset < len(data):
        if offset + 6 > len(data):
            raise ValueError('truncated subrecord header')
        tag, size = struct.unpack_from('<4sH', data, offset)
        offset += 6
        if tag == b'XXXX':
            if size != 4 or extended is not None or offset + 4 > len(data):
                raise ValueError('invalid extended length')
            extended = struct.unpack_from('<I', data, offset)[0]
            offset += 4
            continue
        size = size if extended is None else extended
        extended = None
        if offset + size > len(data):
            raise ValueError('truncated subrecord')
        fields[tag] = data[offset:offset + size]
        offset += size
    if extended is not None:
        raise ValueError('dangling extended length')
    return fields


def records(data, begin=0, end=None):
    end = len(data) if end is None else end
    offset = begin
    while offset < end:
        if offset + 24 > end:
            raise ValueError('truncated record header')
        tag, size, flags, form = struct.unpack_from('<4sIII', data, offset)
        if tag == b'GRUP':
            if size < 24 or offset + size > end:
                raise ValueError('invalid group extent')
            yield from records(data, offset + 24, offset + size)
            offset += size
        else:
            stop = offset + 24 + size
            if stop > end:
                raise ValueError('truncated record')
            payload = data[offset + 24:stop]
            if flags & 0x40000:
                if len(payload) < 4:
                    raise ValueError('missing decompressed size')
                expected = struct.unpack_from('<I', payload)[0]
                payload = zlib.decompress(payload[4:])
                if len(payload) != expected:
                    raise ValueError('decompressed size mismatch')
            yield tag, flags, form, payload
            offset = stop


def fixture_ids(data, count=500):
    # Base-game forms only: load-order-independent 00 IDs. Skip deleted,
    # non-playable/ignored records, scripts and records without a native name.
    ids = set()
    for tag, flags, form, payload in records(data):
        if tag not in (b'WEAP', b'ARMO', b'BOOK', b'ALCH') or flags & (0x4 | 0x20 | 0x1000):
            continue
        if form == 0 or form >= 0x01000000:
            continue
        fields = subrecords(payload)
        if fields.get(b'EDID') and any(fields.get(b'FULL', b'')) and b'VMAD' not in fields:
            ids.add(form)
    if len(ids) < count:
        raise ValueError(f'need {count} distinct eligible records; found {len(ids)}')
    return sorted(ids)[:count]


class InventoryFixtureTest(unittest.TestCase):
    @staticmethod
    def field(tag, payload):
        return struct.pack('<4sH', tag, len(payload)) + payload

    @staticmethod
    def record(tag, form, payload, flags=0):
        return struct.pack('<4sIII8x', tag, len(payload), flags, form) + payload

    def test_500_distinct_additem_lines_from_nested_compressed_records(self):
        fields = self.field(b'EDID', b'fixture\0') + self.field(b'FULL', b'\1\0\0\0')
        packed = struct.pack('<I', len(fields)) + zlib.compress(fields)
        content = b''.join(self.record(b'WEAP', form, packed, 0x40000) for form in range(1, 502))
        # Duplicates, scripts, deleted, non-playable, ignored, mod IDs, no name
        # and non-inventory records cannot pad out the requested entry count.
        content += self.record(b'WEAP', 1, fields)
        for flag in (0x4, 0x20, 0x1000):
            content += self.record(b'WEAP', 502, fields, flag)
        content += self.record(b'ARMO', 502, fields + self.field(b'VMAD', b'script'))
        content += self.record(b'BOOK', 0x01000001, fields)
        content += self.record(b'ALCH', 503, self.field(b'EDID', b'unnamed'))
        content += self.record(b'NPC_', 504, fields)
        group = struct.pack('<4sI16x', b'GRUP', len(content) + 24) + content
        ids = fixture_ids(group)
        lines = [f'player.additem {form:08X} 1' for form in ids]
        self.assertEqual(len(lines), 500)
        self.assertEqual(lines[0], 'player.additem 00000001 1')
        self.assertEqual(lines[-1], 'player.additem 000001F4 1')
        self.assertEqual(len(set(lines)), 500)
        with self.assertRaises(ValueError):
            fixture_ids(group, 502)

    def test_truncation_and_extended_lengths(self):
        for malformed in (b'bad', struct.pack('<4sI16x', b'GRUP', 50),
                          self.record(b'WEAP', 1, b'x', 0x40000)):
            with self.assertRaises(ValueError):
                list(records(malformed))
        extended = self.field(b'XXXX', struct.pack('<I', 10)) + struct.pack('<4sH', b'FULL', 0) + b'0123456789'
        self.assertEqual(subrecords(extended)[b'FULL'], b'0123456789')
        with self.assertRaises(ValueError):
            subrecords(extended[:-1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='Read-only offline copy of Skyrim.esm')
    parser.add_argument('--out', type=Path, required=True, help='New external directory; never a game/profile path')
    args = parser.parse_args()
    if args.out.exists():
        parser.error('--out must be new')
    data = args.source.read_bytes()
    if data[:4] != b'TES4':
        parser.error('not a TES4 plugin')
    ids = fixture_ids(data)
    lines = ''.join(f'player.additem {form:08X} 1\n' for form in ids)
    args.out.mkdir(parents=True)
    (args.out / 'skycrpg-grid-500.txt').write_text(lines)
    manifest = dict(source_sha256=hashlib.sha256(data).hexdigest(), count=len(ids),
                    commands_sha256=hashlib.sha256(lines.encode()).hexdigest(),
                    forms=[f'{form:08X}' for form in ids], runtime='NOT_RUN')
    (args.out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
