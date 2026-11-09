#!/usr/bin/env python3
"""Generate experimental sweep plans; offline drafts with explicit placeholders."""
import argparse
import hashlib
import json
import math
from pathlib import Path

DEFAULT = Path('/srv/storage/Projects/SkyCRPG/session-p55/census/routes')
# Provisional protocol and runner extension, centralized for integration.
PROBE = {'request_kind': 'request_view', 'event': 'view_sample', 'available': 'available',
         'assertion': 'state("{tag}").available == true'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(census_path, out, seconds_per_cell, budget, cells_per_batch, headings, key, include=None, exclude=None, limit=None, overrides=None, tolerance=5.0, stage="REPLACE_STAGE", dist="REPLACE_DIST", save="REPLACE_COPIED_SAVE", probe_dll="REPLACE_PROBE/SkyCRPG_TestProbe.dll", probe_ini="REPLACE_PROBE/SkyCRPG_TestProbe.ini", ready=False):
    if not 0 < budget <= 1200 or seconds_per_cell <= 0 or cells_per_batch <= 0 or len(headings) != 4 or not key or not 0 < tolerance < 180:
        raise ValueError('require budget <= 1200, positive estimates/batch size, four headings and a snap key')
    if ready and any('REPLACE' in str(value) or not value for value in (stage, dist, save, probe_dll, probe_ini, key)):
        raise ValueError('cannot mark ready while placeholders remain')
    # Reserve 180 seconds for process startup, load, and cleanup; estimate remains unmeasured.
    capacity = min(4, cells_per_batch, int((budget - 180) // seconds_per_cell))
    if capacity < 1:
        raise ValueError('budget cannot hold startup reserve and one cell')
    census = json.loads(census_path.read_text())
    out.mkdir(parents=True, exist_ok=True)
    if (out / 'manifest.json').exists():
        raise ValueError('refusing to overwrite existing manifest')
    entries, selected = [], []
    for c in census['cells']:
        cid = c['id']
        reason = None
        if include and cid not in include:
            reason = 'excluded by include list'
        elif exclude and cid in exclude:
            reason = 'excluded by exclude list'
        elif c.get('utility_reason'):
            reason = c['utility_reason']
        elif not c.get('editor_id'):
            reason = 'no editor ID; safe cross-cell entry not validated'
        elif limit is not None and len(selected) >= limit:
            reason = 'pilot limit'
        row = dict(cell=cid, classification=c['classification'], batch=None, eligible=reason is None,
                   reason=reason, entry_method='coc' if not reason else None,
                   pose_source='coc engine arrival; unverified' if not reason else None,
                   poses=c.get('arrival_poses', [])[:2], tags=[], expected_headings=[], editor_id=c.get('editor_id'))
        entries.append(row)
        if not reason:
            selected.append((c, row))
    routes = []
    for start in range(0, len(selected), capacity):
        chunk = selected[start:start + capacity]
        name = 'sweep-%04d' % (len(routes) + 1)
        steps = []
        for cell, row in chunk:
            row['batch'] = name
            label = '%s-%s' % (name, cell['id'].replace(':', '-'))
            sub = [dict(kind='console', command='coc ' + cell['editor_id'], wait_s=0.5, cell=cell['editor_id']), dict(kind='sleep', s=5)]
            for idx, heading in enumerate(headings):
                tag = '%s-h%d' % (label, idx)
                row['tags'].append(tag)
                row['expected_headings'].append(float(heading) % 360)
                expected = float(heading) % 360
                yaw = 'state(\"%s\").camera.yaw_deg' % tag
                diff = 'abs(%s - %s)' % (yaw, expected)
                sub.extend([dict(kind='key', key=key, hold_s=0.1), dict(kind=PROBE['request_kind'], tag=tag), dict(kind='assert_probe', note='heading assertion', expr='(%s <= %s or %s >= %s)' % (diff, tolerance, diff, 360 - tolerance)), dict(kind='assert_probe', note='cell assertion', expr='state(\"%s\").player.cell.editor_id == %s' % (tag, json.dumps(cell['editor_id']))), dict(kind='screenshot', name=tag, method='xwd_window')])
            # A second pose is a teleport, not a claimed walk. A walk requires a validated path.
            if len(row['poses']) > 1:
                pose = row['poses'][1]['pose']
                for axis, value in zip('xyz', pose[:3]):
                    sub.append(dict(kind='console', command='player.setpos %s %.6f' % (axis, value), wait_s=0.5))
                row['second_pose_method'] = 'console displacement; path and walk NOT_RUN'
                sub.append(dict(kind='sleep', s=5))
                for idx, heading in enumerate(headings):
                    tag = '%s-p2-h%d' % (label, idx)
                    row['tags'].append(tag)
                    row['expected_headings'].append(float(heading) % 360)
                    expected = float(heading) % 360
                    yaw = 'state(\"%s\").camera.yaw_deg' % tag
                    diff = 'abs(%s - %s)' % (yaw, expected)
                    sub.extend([dict(kind='key', key=key, hold_s=0.1), dict(kind=PROBE['request_kind'], tag=tag), dict(kind='assert_probe', note='heading assertion', expr='(%s <= %s or %s >= %s)' % (diff, tolerance, diff, 360 - tolerance)), dict(kind='assert_probe', note='cell assertion', expr='state(\"%s\").player.cell.editor_id == %s' % (tag, json.dumps(cell['editor_id']))), dict(kind='screenshot', name=tag, method='xwd_window')])
            steps.append(dict(kind='section', name=label, requires=[], recover=dict(menus='close', load=None), estimate_s=seconds_per_cell, steps=sub))
        route = dict(name=name, status='ready' if ready else 'draft', profile='camera-only', stage=stage, dist=dist, save=save, budget_s=budget, device='keyboard_mouse', settle_s=5, load=dict(method='console', timeout_s=180), install_probe=dict(dll=probe_dll, ini=probe_ini), ini_overrides=overrides or [], manual=['Runtime NOT_RUN; validate entry, headings, rendered visibility and walk.'], notes=['Placeholder timing; Candidate configuration and runtime validation required.'], steps=steps)
        path = out / (name + '.json')
        path.write_text(json.dumps(route, indent=2) + '\n')
        routes.append(dict(name=name, file=path.name, sha256=sha(path)))
    manifest = dict(census_sha256=sha(census_path), generator_sha256=sha(Path(__file__)), probe_contract=PROBE, timing='PLACEHOLDER; measure before execution', seconds_per_cell=seconds_per_cell, budget_s=budget, startup_reserve_s=180, headings=headings, snap_key=key, routes=routes, cells=entries, heading_tolerance_deg=tolerance, runner_status='BLOCKED: candidate paths and calibrated oracle required')
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('census', type=Path)
    p.add_argument('--out', type=Path, default=DEFAULT)
    p.add_argument('--seconds-per-cell', type=float, default=90, help='unmeasured placeholder')
    p.add_argument('--budget', type=int, default=1200)
    p.add_argument('--cells-per-batch', type=int, default=10)
    p.add_argument('--base-heading', type=float, default=0)
    p.add_argument('--heading-tolerance', type=float, default=5)
    p.add_argument('--snap-key', required=True)
    p.add_argument('--stage', default='REPLACE_STAGE')
    p.add_argument('--dist', default='REPLACE_DIST')
    p.add_argument('--save', default='REPLACE_COPIED_SAVE')
    p.add_argument('--probe-dll', default='REPLACE_PROBE/SkyCRPG_TestProbe.dll')
    p.add_argument('--probe-ini', default='REPLACE_PROBE/SkyCRPG_TestProbe.ini')
    p.add_argument('--ready', action='store_true')
    p.add_argument('--include', type=Path)
    p.add_argument('--exclude', type=Path)
    p.add_argument('--limit', type=int)
    p.add_argument('--ini-override', action='append', default=[], help='FILE,SECTION,KEY,VALUE')
    a = p.parse_args()
    def ids(path):
        return {s.strip() for s in path.read_text().splitlines() if s.strip() and not s.startswith('#')} if path else None
    overrides = [dict(zip(('file','section','key','value'), s.split(',', 3))) for s in a.ini_override]
    m = generate(a.census, a.out, a.seconds_per_cell, a.budget, a.cells_per_batch, [a.base_heading + n for n in (0, 90, 180, 270)], a.snap_key, ids(a.include), ids(a.exclude), a.limit, overrides, a.heading_tolerance, a.stage, a.dist, a.save, a.probe_dll, a.probe_ini, a.ready)
    print('routes:', len(m['routes']), 'selected:', sum(c['batch'] is not None for c in m['cells']))


if __name__ == '__main__':
    main()
