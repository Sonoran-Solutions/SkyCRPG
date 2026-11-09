#!/usr/bin/env python3
"""Generate experimental sweep plans; runner view support is still required."""
import argparse
import hashlib
import json
import math
from pathlib import Path

DEFAULT = Path('/srv/storage/Projects/SkyCRPG/session-p55/census/routes')
# Provisional protocol and runner extension, centralized for integration.
PROBE = {'request_kind': 'request_view', 'event': 'view_sample', 'available': 'available',
         'assertion': 'last(where(events(view_sample), "tag", "{tag}")).available == true'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(census_path, out, seconds_per_cell, budget, cells_per_batch, headings, key, include=None, exclude=None, limit=None, overrides=None):
    if not 0 < budget <= 1200 or seconds_per_cell <= 0 or cells_per_batch <= 0 or len(headings) != 4 or not key:
        raise ValueError('require budget <= 1200, positive estimates/batch size, four headings and a snap key')
    # Reserve 180 seconds for process startup, load, and cleanup; estimate remains unmeasured.
    capacity = min(cells_per_batch, int((budget - 180) // seconds_per_cell))
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
        elif not c.get('editor_id'):
            reason = 'no editor ID; safe cross-cell entry not validated'
        elif limit is not None and len(selected) >= limit:
            reason = 'pilot limit'
        row = dict(cell=cid, classification=c['classification'], batch=None, eligible=reason is None,
                   reason=reason, entry_method='coc' if not reason else None,
                   pose_source='coc engine arrival; unverified' if not reason else None,
                   poses=c.get('arrival_poses', [])[:2], tags=[])
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
                sub.extend([dict(kind=PROBE['request_kind'], tag=tag), dict(kind='assert_probe', expr=PROBE['assertion'].format(tag=tag)), dict(kind='screenshot', name=tag, method='xwd_window'), dict(kind='key', key=key, hold_s=0.1)])
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
                    sub.extend([dict(kind=PROBE['request_kind'], tag=tag), dict(kind='assert_probe', expr=PROBE['assertion'].format(tag=tag)), dict(kind='screenshot', name=tag, method='xwd_window'), dict(kind='key', key=key, hold_s=0.1)])
            steps.append(dict(kind='section', name=label, requires=[], estimate_s=seconds_per_cell, steps=sub))
        route = dict(name=name, status='draft', profile='camera-only', stage='REPLACE_STAGE', dist='REPLACE_DIST', save='REPLACE_COPIED_SAVE', budget_s=budget, device='keyboard_mouse', settle_s=5, load=dict(method='console', timeout_s=180), install_probe=dict(dll='REPLACE_PROBE_DLL', ini='REPLACE_PROBE_INI'), ini_overrides=overrides or [], manual=['Runtime NOT_RUN; validate entry, headings, rendered visibility and walk.'], notes=['Placeholder timing; runner request_view support and candidate configuration required.'], steps=steps)
        path = out / (name + '.json')
        path.write_text(json.dumps(route, indent=2) + '\n')
        routes.append(dict(name=name, file=path.name, sha256=sha(path)))
    manifest = dict(census_sha256=sha(census_path), generator_sha256=sha(Path(__file__)), probe_contract=PROBE, timing='PLACEHOLDER; measure before execution', seconds_per_cell=seconds_per_cell, budget_s=budget, startup_reserve_s=180, headings=headings, snap_key=key, routes=routes, cells=entries, runner_status='BLOCKED: request_view step requires runner implementation; candidate paths and calibrated oracle required')
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('census', type=Path)
    p.add_argument('--out', type=Path, default=DEFAULT)
    p.add_argument('--seconds-per-cell', type=float, default=90, help='unmeasured placeholder')
    p.add_argument('--budget', type=int, default=1200)
    p.add_argument('--cells-per-batch', type=int, default=10)
    p.add_argument('--headings', nargs=4, default=['0', '90', '180', '270'])
    p.add_argument('--snap-key', required=True)
    p.add_argument('--include', type=Path)
    p.add_argument('--exclude', type=Path)
    p.add_argument('--limit', type=int)
    p.add_argument('--ini-override', action='append', default=[], help='FILE,SECTION,KEY,VALUE')
    a = p.parse_args()
    def ids(path):
        return {s.strip() for s in path.read_text().splitlines() if s.strip() and not s.startswith('#')} if path else None
    overrides = [dict(zip(('file','section','key','value'), s.split(',', 3))) for s in a.ini_override]
    m = generate(a.census, a.out, a.seconds_per_cell, a.budget, a.cells_per_batch, a.headings, a.snap_key, ids(a.include), ids(a.exclude), a.limit, overrides)
    print('routes:', len(m['routes']), 'selected:', sum(c['batch'] is not None for c in m['cells']))


if __name__ == '__main__':
    main()
