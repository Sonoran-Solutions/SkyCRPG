"""Synthetic TES4 fixtures and conservative sweep contracts; no installed game needed."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'coverage'))
import cell_census as cc
import classify
import make_sweep_routes as routes
import sweep_report


def sub(tag, data):
    return tag.encode() + struct.pack('<H', len(data)) + data


def rec(tag, fid, fields, flags=0, compressed=False):
    data = b''.join(sub(*f) for f in fields)
    if compressed:
        data = struct.pack('<I', len(data)) + zlib.compress(data)
        flags |= 0x40000
    return tag.encode() + struct.pack('<IIIII', len(data), flags, fid, 0, 0) + data


def group(label, typ, contents):
    return b'GRUP' + struct.pack('<IIiII', len(contents) + 24, label, typ, 0, 0) + contents


def f(n):
    return struct.pack('<I', n)


class CensusTests(unittest.TestCase):
    def test_override_compressed_and_door_arrival(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            pose = [1., 2., 3., 0., 0., 1.]
            cell = rec('CELL', 0x100, [('EDID', b'Old\0'), ('DATA', b'\1')], compressed=True)
            door = rec('DOOR', 0x200, [('EDID', b'Door\0')])
            dest = rec('REFR', 0x301, [('NAME', f(0x200)), ('DATA', struct.pack('<6f', *pose))])
            source = rec('REFR', 0x300, [('NAME', f(0x200)), ('XTEL', f(0x301) + struct.pack('<6f', *pose))])
            nested = group(0x100, 6, group(0, 9, dest + source))
            (root / 'A.esm').write_bytes(rec('TES4', 0, []) + cell + door + nested)
            override = rec('CELL', 0x100, [('EDID', b'New\0'), ('DATA', b'\1')])
            (root / 'B.esm').write_bytes(rec('TES4', 0, [('MAST', b'A.esm\0')]) + override)
            result = cc.scan(root, ['A.esm', 'B.esm'])
            row = result['cells'][0]
            self.assertEqual(row['id'], 'A.esm:000100')
            self.assertEqual(row['editor_id'], 'New')
            self.assertEqual(row['arrival_poses'][0]['pose'], pose)
            self.assertEqual(row['door_references'], 2)

    def test_classifier(self):
        self.assertEqual(classify.classify({'editor_id': 'BleakFallsBarrow01'}), 'nordic_ruin')
        self.assertEqual(classify.classify({'editor_id': 'Unrelated'}), 'unclassified')

    def test_keyword_priority_and_boundaries(self):
        self.assertEqual(classify.classify({'editor_id': 'Cave01', 'location_keywords': ['LocTypeInn']}), 'multi_storey_house_inn')
        for name in ('Beginning01', 'Determine01', 'Attention01', 'Fortune01'):
            self.assertEqual(classify.classify({'editor_id': name}), 'unclassified')
        self.assertIsNotNone(classify.utility_reason({'editor_id': 'WarehouseTest01'}))

    def test_worldspace_group(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            world = rec('WRLD', 0x100, [('EDID', b'Blackreach\0'), ('DATA', b'\0')])
            cell = rec('CELL', 0x200, [('EDID', b'BlackreachCell\0'), ('DATA', b'\0')])
            (root / 'A.esm').write_bytes(rec('TES4', 0, []) + world + group(0x100, 1, cell))
            result = cc.scan(root, ['A.esm'])
            self.assertEqual(result['worldspaces'][0]['cell_count'], 1)
            self.assertEqual(result['worldspaces'][0]['category'], 'enclosed_candidate')
            self.assertEqual(result['cells'], [])

    def test_batch_hashes_and_not_run(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            census = root / 'census.json'
            cells = [dict(id='A.esm:%06X' % i, editor_id='Cell%d' % i, classification='unclassified', arrival_poses=[]) for i in range(5)]
            census.write_text(json.dumps({'cells': cells}))
            m = routes.generate(census, root / 'routes', 100, 400, 10, ['0','90','180','270'], 'F1')
            self.assertEqual(len(m['routes']), 3)
            for r in m['routes']:
                self.assertEqual(r['sha256'], routes.sha(root / 'routes' / r['file']))
            self.assertEqual(m['census_sha256'], routes.sha(census))
            route = json.loads((root / 'routes' / m['routes'][0]['file']).read_text())
            steps = route['steps'][0]['steps']
            self.assertEqual([s['kind'] for s in steps[2:5]], ['key', 'request_view', 'assert_probe'])
            self.assertIn('camera.yaw_deg', steps[4]['expr'])
            self.assertIn('player.cell.editor_id', steps[5]['expr'])
            with self.assertRaises(ValueError):
                routes.generate(census, root / 'ready', 100, 400, 10, [0,90,180,270], 'F1', ready=True)
            report = sweep_report.aggregate(m, [{'name':m['routes'][0]['name']}], [], 0.97)
            self.assertEqual(report['totals']['NOT_RUN'], 5)
            events = [dict(ev='view_sample', requested=True, tag=t, available=False) for t in m['cells'][0]['tags']]
            report = sweep_report.aggregate(m, [{'name':m['routes'][0]['name']}], events)
            self.assertEqual(report['cells'][0]['status'], 'BLOCKED')
            self.assertNotIn('PASS', report['totals'])
            events = [dict(ev='view_sample', requested=True, tag=t, available=True, player={'cell': {'editor_id':'Wrong'}}, camera={'yaw_deg': 0}) for t in m['cells'][0]['tags']]
            report = sweep_report.aggregate(m, [{'name':m['routes'][0]['name']}], events)
            self.assertEqual(report['cells'][0]['result_reason'], 'cell assertion failed')
            for event in events:
                event['player']['cell']['editor_id'] = 'Cell0'
                event['camera']['yaw_deg'] = 180
            report = sweep_report.aggregate(m, [{'name':m['routes'][0]['name']}], events)
            self.assertEqual(report['cells'][0]['result_reason'], 'heading assertion failed')


if __name__ == '__main__':
    unittest.main()
