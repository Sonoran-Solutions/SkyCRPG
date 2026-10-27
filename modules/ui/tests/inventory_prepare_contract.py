"""Check the integrator patch in memory; never edit shared working-tree files."""
import importlib.util
from pathlib import Path
import sys
import re

repo = Path(__file__).resolve().parents[3]
log = Path(sys.argv[1])
# Apply only the builder's unified-diff hunks in memory. This makes the
# integration probe reproducible without mutating an integrator-owned file.
patch = Path('/srv/storage/Projects/SkyCRPG/session-p3/integration/dyl-163.patch').read_text()
base = (repo / 'modules/ui/tools/build_scaleform.py').read_text()
if 'DYL-163: gate the existing player-only dispatch sites' in base:
    candidate = base
else:
    section = patch.split('+++ b/modules/ui/tools/build_scaleform.py\n', 1)[1].split('--- a/', 1)[0]
    original = base.splitlines(True)
    output = []
    cursor = 0
    for hunk in re.split(r'(?=^@@ )', section, flags=re.M):
        if not hunk.startswith('@@ '):
            continue
        header, *body = hunk.splitlines(True)
        start = int(re.match(r'@@ -(\d+)', header).group(1)) - 1
        output.extend(original[cursor:start])
        cursor = start
        for line in body:
            if line.startswith((' ', '-')):
                assert original[cursor] == line[1:], 'integration hunk drift'
                cursor += 1
            if line.startswith((' ', '+')):
                output.append(line[1:])
    output.extend(original[cursor:])
    candidate = ''.join(output)
(log / 'integrated_build_scaleform.py').write_text(candidate)
spec = importlib.util.spec_from_file_location('inventory_integrated_builder', log / 'integrated_build_scaleform.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)
builder.ROOT = repo / 'modules/ui'
provider = Path('/srv/storage/Projects/SkyCRPG/session-20261026/ui-skyui-v6.11/source/actionscript')
for name, expected in builder.SOURCES.items():
    source = provider / name
    assert builder.sha(source) == expected, name
    original = source.read_text()
    transformed = builder.transform(name, original)
    if name.endswith('InventoryMenu.as'):
        # Gate existing native calls; never add or remove a mutation callback.
        pattern = r'gfx\.io\.GameDelegate\.[^;]+;'
        assert re.findall(pattern, original) == re.findall(pattern, transformed)
        assert transformed.count('skycrpgInventoryAllow("activate"') == 2
        assert transformed.count('skycrpgInventoryAllow("drop"') == 2
        (log / 'InventoryMenu.as').write_text(transformed)
    if name.endswith('InventoryLists.as'):
        assert transformed.count('_root.Menu_mc.skycrpgInventoryViewChanged();') == 4
print('PASS: all pinned transforms, native dispatch preservation, player-only action gates and view invalidation anchors')
