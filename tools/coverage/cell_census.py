#!/usr/bin/env python3
"""Read-only, load-order-aware TES4 interior census; no game assets extracted."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import struct
import zlib

from classify import classify_detail, utility_reason, RULES

NAMES = ['Skyrim.esm', 'Update.esm', 'Dawnguard.esm', 'HearthFires.esm', 'Dragonborn.esm']
DEFAULT = Path.home() / '.local/share/Steam/steamapps/common/Skyrim Special Edition/Data'


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def subrecords(data):
    pos = 0
    extended = None
    while pos < len(data):
        if pos + 6 > len(data):
            raise ValueError('truncated subrecord header')
        tag, size = struct.unpack_from('<4sH', data, pos)
        pos += 6
        if tag == b'XXXX':
            if size != 4 or pos + size > len(data):
                raise ValueError('bad XXXX')
            extended = struct.unpack_from('<I', data, pos)[0]
            pos += size
            continue
        if extended is not None:
            size, extended = extended, None
        if pos + size > len(data):
            raise ValueError('truncated subrecord')
        yield tag.decode('ascii'), data[pos:pos + size]
        pos += size


def text(b):
    return b.rstrip(b'\0').decode('utf-8', errors='replace')


def scan(data_dir, names=NAMES):
    records = {}
    sources = []
    for plugin in names:
        path = data_dir / plugin
        blob = path.read_bytes()
        sources.append(dict(name=plugin, size=len(blob), sha256=hashlib.sha256(blob).hexdigest()))
        masters = []
        def key(fid):
            if not fid:
                return None
            index = fid >> 24
            if index > len(masters):
                return 'UNRESOLVED:%08X@%s' % (fid, plugin)
            return '%s:%06X' % ((masters + [plugin])[index], fid & 0xffffff)
        def walk(start, end, cell=None, group_type=None, world=None):
            pos = start
            while pos < end:
                if pos + 24 > end:
                    raise ValueError('truncated record in ' + plugin)
                tag, size = struct.unpack_from('<4sI', blob, pos)
                if tag == b'GRUP':
                    label, typ = struct.unpack_from('<Ii', blob, pos + 8)
                    if size < 24 or pos + size > end:
                        raise ValueError('bad group in ' + plugin)
                    walk(pos + 24, pos + size, key(label) if typ == 6 else cell, typ, key(label) if typ == 1 else world)
                    pos += size
                    continue
                flags, fid = struct.unpack_from('<II', blob, pos + 8)
                if pos + 24 + size > end:
                    raise ValueError('truncated record in ' + plugin)
                raw = blob[pos + 24:pos + 24 + size]
                pos += 24 + size
                typ = tag.decode('ascii')
                if typ not in ('TES4', 'CELL', 'REFR', 'ACHR', 'STAT', 'DOOR', 'LCTN', 'KYWD', 'LGTM', 'ACTI', 'MSTT', 'FURN', 'WRLD'):
                    continue
                if flags & 0x40000:
                    length = struct.unpack_from('<I', raw)[0]
                    raw = zlib.decompress(raw[4:])
                    if len(raw) != length:
                        raise ValueError('bad compressed record length')
                pairs = list(subrecords(raw))
                if typ == 'TES4':
                    masters[:] = [text(v) for t, v in pairs if t == 'MAST']
                    continue
                fields = dict(pairs)
                row = dict(id=key(fid), type=typ, editor_id=text(fields.get('EDID', b'')), flags=flags, source=plugin)
                def form(name):
                    v = fields.get(name, b'')
                    return key(struct.unpack_from('<I', v)[0]) if len(v) >= 4 else None
                if typ == 'WRLD':
                    row.update(parent=form('WNAM'), world_flags=int.from_bytes(fields.get('DATA', b'\0')[:1], 'little'))
                if typ == 'CELL':
                    row['worldspace'] = world
                    row.update(cell_flags=int.from_bytes(fields.get('DATA', b'\0')[:2], 'little'), location=form('XLCN'), encounter_zone=form('XEZN'), lighting_template=form('LTMP'))
                    x = fields.get('XCLL', b'')
                    row['fog_near_far'] = list(struct.unpack_from('<2f', x, 12)) if len(x) >= 20 else None
                if typ == 'LCTN':
                    raw_kw = fields.get('KWDA', b'')
                    row['keywords'] = [key(n[0]) for n in struct.iter_unpack('<I', raw_kw[:len(raw_kw)//4*4])]
                if typ in ('REFR', 'ACHR'):
                    row.update(cell=cell, group_type=group_type, base=form('NAME'))
                    d = fields.get('DATA', b'')
                    row['pose'] = list(struct.unpack('<6f', d)) if len(d) == 24 else None
                    x = fields.get('XTEL', b'')
                    row['teleport'] = dict(destination=key(struct.unpack_from('<I', x)[0]), pose=list(struct.unpack_from('<6f', x, 4))) if len(x) >= 28 else None
                records[row['id']] = row
        walk(0, len(blob))
        del blob
    cells = {k: v for k, v in records.items() if v['type'] == 'CELL' and v['cell_flags'] & 1 and not v['flags'] & 0x20}
    refs = [v for v in records.values() if v['type'] in ('REFR', 'ACHR') and not v['flags'] & 0x20]
    bycell = defaultdict(list)
    arrivals = defaultdict(list)
    for ref in refs:
        bycell[ref['cell']].append(ref)
        dest = records.get((ref.get('teleport') or {}).get('destination'))
        if dest and dest.get('cell') in cells and ref['teleport']['pose']:
            arrivals[dest['cell']].append(dict(source_ref=ref['id'], destination_ref=dest['id'], pose=ref['teleport']['pose']))
    marker_bases = {k: v for k, v in records.items() if v['type'] not in ('REFR', 'ACHR', 'CELL') and ('roommarker' in v['editor_id'].lower() or 'portalmarker' in v['editor_id'].lower())}
    rows = []
    flags = {1:'interior', 2:'has_water', 4:'cannot_travel', 8:'no_lod_water', 32:'public_area', 64:'hand_changed', 128:'show_sky', 256:'use_sky_lighting'}
    for cell in cells.values():
        cid = cell['id']
        loc = records.get(cell['location'], {})
        cell['location_editor_id'] = loc.get('editor_id') or None
        cell['location_keywords'] = [records.get(k, {}).get('editor_id') or k for k in loc.get('keywords', [])]
        cell['lighting_template_editor_id'] = records.get(cell['lighting_template'], {}).get('editor_id') or None
        cell['decoded_flags'] = [name for bit, name in flags.items() if cell['cell_flags'] & bit]
        rr = bycell[cid]
        cell['persistent_references'] = sum(r['group_type'] == 8 for r in rr)
        cell['temporary_references'] = sum(r['group_type'] == 9 for r in rr)
        cell['room_marker_references'] = sum('roommarker' in marker_bases.get(r['base'], {}).get('editor_id', '').lower() for r in rr)
        cell['portal_marker_references'] = sum('portalmarker' in marker_bases.get(r['base'], {}).get('editor_id', '').lower() for r in rr)
        cell['door_references'] = sum(records.get(r['base'], {}).get('type') == 'DOOR' for r in rr)
        cell['arrival_poses'] = arrivals[cid]
        cell['markers'] = [dict(ref=r['id'], base=r['base'], editor_id=records.get(r['base'], {}).get('editor_id'), pose=r['pose']) for r in rr if r['pose'] and any(x in records.get(r['base'], {}).get('editor_id', '').lower() for x in ('cocmarker', 'headingmarker'))]
        statics = [r for r in rr if records.get(r['base'], {}).get('type') == 'STAT']
        cell['static_bases'] = dict(Counter(records[r['base']]['editor_id'] for r in statics if records[r['base']]['editor_id']).most_common(20))
        z = [r['pose'][2] for r in statics if r['pose']]
        cell['static_z_extent'] = [min(z), max(z)] if z else None
        cell['storey_count'] = None
        cell['classification'], cell['classification_rule'] = classify_detail(cell)
        cell['utility_reason'] = utility_reason(cell)
        cell['review_flags'] = ([] if cell['editor_id'] else ['no_editor_id']) + ([] if cell['arrival_poses'] or cell['markers'] else ['no_known_arrival_pose']) + (['possible_test_or_unused'] if any(x in cell['editor_id'].lower() for x in ('test', 'qasmoke', 'unused', 'unreachable')) else [])
        rows.append(cell)
    worlds = [r.copy() for r in records.values() if r['type'] == 'WRLD' and not r['flags'] & 0x20 and r['editor_id'] != 'Tamriel']
    world_counts = Counter(r.get('worldspace') for r in records.values() if r['type'] == 'CELL' and not r['cell_flags'] & 1 and not r['flags'] & 0x20)
    for world in worlds:
        world['parent_editor_id'] = records.get(world['parent'], {}).get('editor_id') or None
        world['cell_count'] = world_counts[world['id']]
        name = world['editor_id'].lower()
        if name in ('blackreach', 'dlc01soulcairn', 'dlc2apocryphaworld', 'dlc01falmervalley', 'dlc01boneyard') or any(token in name for token in ('cave', 'cavern', 'grotto', 'crypt', 'passage', 'holdout', 'alftand')):
            world['category'] = 'enclosed_candidate'
        elif any(token in name for token in ('whiterun', 'solitude', 'windhelm', 'markarth', 'riften')):
            world['category'] = 'city_exterior'
        elif name == 'dlc2solstheimworld':
            world['category'] = 'exterior'
        else:
            world['category'] = 'review'

    return dict(worldspaces=sorted(worlds, key=lambda w:w['id']), worldspace_method='Hypothesis: known enclosed EDIDs and cave-like tokens are enclosed candidates; known city EDIDs are exterior; Solstheim is exterior. Parent alone is insufficient: Blackreach and Soul Cairn have Tamriel parents. Other worlds require review; flags and parents retained.', masters=sources, tool_sha256=digest(Path(__file__)), classifier_sha256=digest(Path(__file__).with_name('classify.py')), marker_identification='Base EDID contains RoomMarker or PortalMarker; conservative name search, not a proven exhaustive engine type mapping.', marker_bases=list(marker_bases.values()), classification_rules=[dict(source=source, name=n, values=v, matched=sum(c['classification_rule'] == i for c in rows)) for i,(source,n,v) in enumerate(RULES)], distribution=dict(Counter(c['classification'] for c in rows)), cells=sorted(rows, key=lambda r:r['id']))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--data', type=Path, default=DEFAULT)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    result = scan(a.data)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')
    print('cells:', len(result['cells']), 'sha256:', digest(a.out))


if __name__ == '__main__':
    main()
